#include "divider/table.h"

#include "divider/si.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace divider
{
    namespace
    {

        using Getter = double (*)(const DividerCandidate&);
        using Cells = std::vector<std::string>;

        struct ColumnFormat
        {
            SiScale scale;
            int digits = 0;
        };

        // A table column: its header and how to render one candidate in it.
        struct Column
        {
            std::string header;
            std::function<std::string(const DividerCandidate&)> cell;
        };

        // --digits, when given, overrides every automatically chosen precision except the percent columns, which always use it.
        struct Precision
        {
            int digits;
            bool fixed;

            int resolve(int automatic) const
            {
                return fixed ? digits : automatic;
            }
        };

        std::string omegaHeader(std::string_view label)
        {
            return std::string(label) + " (\xCE\xA9)";
        }

        std::size_t visibleWidth(std::string_view text)
        {
            std::size_t width = 0;
            for (const unsigned char ch : text)
            {
                if ((ch & 0xC0) != 0x80)
                {
                    ++width;
                }
            }
            return width;
        }

        std::string formatSiValueWithScale(double value, const SiScale& scale, int digits)
        {
            return formatSiWithScale(value, scale, "", digits);
        }

        double cleanVoltageError(double value)
        {
            constexpr double zeroThreshold = 1.0e-12;
            return std::abs(value) < zeroThreshold ? 0.0 : value;
        }

        std::string formatSignedSiValueWithScale(double value, const SiScale& scale, int digits)
        {
            value = cleanVoltageError(value);
            std::string output = formatSiValueWithScale(std::abs(value), scale, digits);

            if (isZeroNumber(output))
            {
                return output;
            }
            return (value > 0.0 ? "+" : "-") + output;
        }

        double ratioErrorRangePercent(const DividerCandidate& candidate)
        {
            return candidate.highRatioErrorPercent - candidate.lowRatioErrorPercent;
        }

        double voltageErrorRange(const DividerCandidate& candidate)
        {
            return candidate.highVoltageError - candidate.lowVoltageError;
        }

        double siMantissa(double value, const SiScale& scale)
        {
            return std::abs(value / scale.multiplier);
        }

        // Decimal places for a resistor value in its own unit: E48 and finer series carry three significant digits.
        int resistanceDigits(int seriesValue, double value, const SiScale& scale)
        {
            const double mantissa = siMantissa(value, scale);
            if (seriesValue <= 24)
            {
                return mantissa < 10.0 ? 1 : 0;
            }
            if (mantissa < 10.0)
            {
                return 2;
            }
            return mantissa < 100.0 ? 1 : 0;
        }

        int decimalDigitsForSignificantDigits(double value, const SiScale& scale, int significantDigits)
        {
            const double mantissa = siMantissa(value, scale);
            if (mantissa == 0.0 || !std::isfinite(mantissa))
            {
                return 0;
            }

            const int exponent = static_cast<int>(std::floor(std::log10(mantissa)));
            return std::max(0, significantDigits - exponent - 1);
        }

        // Rendering of a value in the unit that suits it best, independent of the rest of the column.
        std::string formatOwnScale(double value, int significantDigits, const Precision& precision)
        {
            const SiScale scale = chooseSiScale(value);
            return formatSiValueWithScale(value, scale, precision.resolve(decimalDigitsForSignificantDigits(value, scale, significantDigits)));
        }

        ColumnFormat columnFormatFromMinimum(double minimum, int significantDigits)
        {
            const SiScale scale = chooseSiScale(minimum);
            return ColumnFormat{scale, decimalDigitsForSignificantDigits(minimum, scale, significantDigits)};
        }

        ColumnFormat allZeroColumnFormat(int significantDigits)
        {
            return ColumnFormat{SiScale{1.0, ""}, std::max(0, significantDigits - 1)};
        }

        double minimumOf(const std::vector<DividerCandidate>& candidates, Getter get)
        {
            double minimum = get(candidates.front());
            for (const auto& candidate : candidates)
            {
                minimum = std::min(minimum, get(candidate));
            }
            return minimum;
        }

        // The whole column shares the unit of its smallest value.
        ColumnFormat resistanceColumnFormat(const std::vector<DividerCandidate>& candidates, int seriesValue, Getter get)
        {
            return columnFormatFromMinimum(minimumOf(candidates, get), seriesValue >= 48 ? 3 : 2);
        }

        ColumnFormat powerColumnFormat(const std::vector<DividerCandidate>& candidates)
        {
            const double minimum = minimumOf(candidates, [](const DividerCandidate& c) { return c.power; });
            return minimum == 0.0 ? allZeroColumnFormat(3) : columnFormatFromMinimum(minimum, 3);
        }

        // Voltage errors are often exactly zero: they must not force the whole column down to a tiny unit.
        ColumnFormat smallestNonZeroColumnFormat(const std::vector<DividerCandidate>& candidates, Getter get)
        {
            double minimum = 0.0;
            bool initialized = false;
            for (const auto& candidate : candidates)
            {
                const double value = std::abs(cleanVoltageError(get(candidate)));
                if (value != 0.0)
                {
                    minimum = initialized ? std::min(minimum, value) : value;
                    initialized = true;
                }
            }
            return initialized ? columnFormatFromMinimum(minimum, 3) : allZeroColumnFormat(3);
        }

        Column resistorColumn(std::string header, double DividerCandidate::* member, int seriesValue, const Precision& precision)
        {
            return Column{std::move(header), [member, seriesValue, precision](const DividerCandidate& candidate)
                          {
                              const double value = candidate.*member;
                              const SiScale scale = chooseSiScale(value);
                              return formatSiValueWithScale(value, scale, precision.resolve(resistanceDigits(seriesValue, value, scale)));
                          }};
        }

        Column sharedScaleColumn(std::string header, double DividerCandidate::* member, const ColumnFormat& format, const Precision& precision)
        {
            return Column{std::move(header), [member, format, precision](const DividerCandidate& candidate) { return formatSiValueWithScale(candidate.*member, format.scale, precision.resolve(format.digits)); }};
        }

        Column ownScaleColumn(std::string header, double DividerCandidate::* member, const Precision& precision)
        {
            return Column{std::move(header), [member, precision](const DividerCandidate& candidate) { return formatOwnScale(candidate.*member, 3, precision); }};
        }

        std::vector<Column> buildColumns(const SeriesResult& result, const Precision& precision, bool includeVoltageColumns, bool includeWorstPowerColumns)
        {
            const auto& candidates = result.candidates;
            const int columnSeriesValue = std::max(result.highValueCount, result.lowValueCount);
            const int percentDigits = precision.digits;

            const auto nominalErrorColumn = Column{"nom error (%)", [percentDigits](const DividerCandidate& c) { return formatSignedDecimal(c.nominalRatioErrorPercent, percentDigits); }};
            const auto errorRangeColumn = Column{"error range (%)", [percentDigits](const DividerCandidate& c) { return formatDecimal(ratioErrorRangePercent(c), percentDigits); }};

            std::vector<Column> columns;
            columns.push_back(resistorColumn(omegaHeader("top"), &DividerCandidate::rHigh, result.highValueCount, precision));
            columns.push_back(resistorColumn(omegaHeader("bottom"), &DividerCandidate::rLow, result.lowValueCount, precision));
            columns.push_back(sharedScaleColumn(omegaHeader("series"), &DividerCandidate::rSeries, resistanceColumnFormat(candidates, columnSeriesValue, [](const DividerCandidate& c) { return c.rSeries; }), precision));
            columns.push_back(sharedScaleColumn(omegaHeader("parallel"), &DividerCandidate::rParallel, resistanceColumnFormat(candidates, columnSeriesValue, [](const DividerCandidate& c) { return c.rParallel; }), precision));

            if (!includeVoltageColumns)
            {
                columns.push_back(nominalErrorColumn);
                columns.push_back(errorRangeColumn);
                return columns;
            }

            const ColumnFormat nominalVoltage = smallestNonZeroColumnFormat(candidates, [](const DividerCandidate& c) { return c.nominalVoltageError; });
            const ColumnFormat voltageRange = smallestNonZeroColumnFormat(candidates, voltageErrorRange);

            columns.push_back(sharedScaleColumn("power (W)", &DividerCandidate::power, powerColumnFormat(candidates), precision));
            columns.push_back(nominalErrorColumn);
            columns.push_back(errorRangeColumn);
            columns.push_back(Column{"nom error (V)", [nominalVoltage, precision](const DividerCandidate& c) { return formatSignedSiValueWithScale(c.nominalVoltageError, nominalVoltage.scale, precision.resolve(nominalVoltage.digits)); }});
            columns.push_back(Column{"error range (V)", [voltageRange, precision](const DividerCandidate& c) { return formatSiValueWithScale(voltageErrorRange(c), voltageRange.scale, precision.resolve(voltageRange.digits)); }});
            columns.push_back(ownScaleColumn("VOUT min (V)", &DividerCandidate::voutMin, precision));
            columns.push_back(ownScaleColumn("VOUT max (V)", &DividerCandidate::voutMax, precision));
            columns.push_back(ownScaleColumn("top power (W)", &DividerCandidate::powerHigh, precision));
            columns.push_back(ownScaleColumn("bottom power (W)", &DividerCandidate::powerLow, precision));
            if (includeWorstPowerColumns)
            {
                columns.push_back(ownScaleColumn("top max power (W)", &DividerCandidate::powerHighMax, precision));
                columns.push_back(ownScaleColumn("bottom max power (W)", &DividerCandidate::powerLowMax, precision));
            }
            return columns;
        }

        void printHeading(std::ostream& output, const SeriesResult& result)
        {
            if (result.highName == result.lowName)
            {
                output << "\n" << result.highName << " (tolerance +/-" << formatCompactDecimal(result.highTolerancePercent) << "%)\n";
            }
            else
            {
                output << "\ntop " << result.highName << " (+/-" << formatCompactDecimal(result.highTolerancePercent) << "%) / bottom " << result.lowName << " (+/-" << formatCompactDecimal(result.lowTolerancePercent) << "%)\n";
            }
        }

        void printRow(std::ostream& output, const Cells& cells, const std::vector<std::size_t>& widths, bool rightJustified)
        {
            for (std::size_t index = 0; index < cells.size(); ++index)
            {
                if (index > 0)
                {
                    output << "  ";
                }
                const std::size_t padding = widths[index] - visibleWidth(cells[index]);
                if (rightJustified)
                {
                    output << std::string(padding, ' ') << cells[index];
                }
                else
                {
                    output << cells[index] << std::string(padding, ' ');
                }
            }
            output << '\n';
        }

    } // namespace

    void printPairTable(std::ostream& output, const SeriesResult& result, int digits, bool includeVoltageColumns, bool fixedDigits, bool includeWorstPowerColumns)
    {
        printHeading(output, result);

        if (result.candidates.empty())
        {
            output << "No candidates found.\n";
            return;
        }

        const std::vector<Column> columns = buildColumns(result, Precision{digits, fixedDigits}, includeVoltageColumns, includeWorstPowerColumns);

        Cells headers;
        for (const auto& column : columns)
        {
            headers.push_back(column.header);
        }

        std::vector<Cells> rows;
        rows.reserve(result.candidates.size());
        for (const auto& candidate : result.candidates)
        {
            Cells row;
            for (const auto& column : columns)
            {
                row.push_back(column.cell(candidate));
            }
            rows.push_back(std::move(row));
        }

        std::vector<std::size_t> widths;
        for (const auto& header : headers)
        {
            widths.push_back(visibleWidth(header));
        }
        for (const auto& row : rows)
        {
            for (std::size_t index = 0; index < row.size(); ++index)
            {
                widths[index] = std::max(widths[index], visibleWidth(row[index]));
            }
        }

        Cells separators;
        for (const auto width : widths)
        {
            separators.push_back(std::string(width, '-'));
        }

        printRow(output, headers, widths, false);
        printRow(output, separators, widths, false);
        for (const auto& row : rows)
        {
            printRow(output, row, widths, true);
        }
    }

    void printSeriesTable(std::ostream& output, std::string_view seriesName, int seriesValue, double tolerancePercent, const std::vector<DividerCandidate>& candidates, int digits, bool includeVoltageColumns, bool fixedDigits, bool includeWorstPowerColumns)
    {
        SeriesResult result;
        result.highName = std::string(seriesName);
        result.lowName = result.highName;
        result.highValueCount = seriesValue;
        result.lowValueCount = seriesValue;
        result.highTolerancePercent = tolerancePercent;
        result.lowTolerancePercent = tolerancePercent;
        result.candidates = candidates;
        printPairTable(output, result, digits, includeVoltageColumns, fixedDigits, includeWorstPowerColumns);
    }

} // namespace divider
