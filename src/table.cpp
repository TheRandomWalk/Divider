#include "divider/table.h"

#include "divider/si.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace divider
{
    namespace
    {


        struct Row
        {
            std::vector<std::string> cells;
        };

        struct ColumnFormat
        {
            SiScale scale;
            int digits = 0;
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

        std::vector<std::size_t> computeWidths(const std::vector<std::string>& headers, const std::vector<Row>& rows)
        {
            std::vector<std::size_t> widths;
            widths.reserve(headers.size());

            for (const auto& header : headers)
            {
                widths.push_back(visibleWidth(header));
            }

            for (const auto& row : rows)
            {
                for (std::size_t index = 0; index < row.cells.size(); ++index)
                {
                    widths[index] = std::max(widths[index], visibleWidth(row.cells[index]));
                }
            }

            return widths;
        }

        std::string formatSiValue(double value, int digits)
        {
            return formatSi(value, "", digits);
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

        bool formattedNumberIsZero(std::string_view text)
        {
            bool sawDigit = false;

            for (const unsigned char ch : text)
            {
                if (!std::isdigit(ch))
                {
                    continue;
                }
                sawDigit = true;
                if (ch != '0')
                {
                    return false;
                }
            }
            return sawDigit;
        }

        std::string formatSignedSiValueWithScale(double value, const SiScale& scale, int digits)
        {
            value = cleanVoltageError(value);
            std::string output = formatSiValueWithScale(std::abs(value), scale, digits);

            if (formattedNumberIsZero(output))
            {
                return output;
            }
            if (value > 0.0)
            {
                return "+" + output;
            }
            return "-" + output;
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
            if (scale.multiplier == 0.0)
            {
                return value;
            }
            return std::abs(value / scale.multiplier);
        }

        int digitsForMantissa(int seriesValue, double mantissa)
        {
            if (seriesValue <= 24)
            {
                return mantissa < 10.0 ? 1 : 0;
            }
            if (mantissa < 10.0)
            {
                return 2;
            }
            if (mantissa < 100.0)
            {
                return 1;
            }
            return 0;
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

        int columnSignificantDigits(int seriesValue)
        {
            return seriesValue >= 48 ? 3 : 2;
        }

        int resistanceDigits(int seriesValue, double value, const SiScale& scale)
        {
            return digitsForMantissa(seriesValue, siMantissa(value, scale));
        }

        ColumnFormat resistanceColumnFormat(const std::vector<DividerCandidate>& candidates, int seriesValue, double DividerCandidate::* member)
        {
            double minimum = candidates.front().*member;
            for (const auto& candidate : candidates)
            {
                minimum = std::min(minimum, candidate.*member);
            }

            const SiScale scale = chooseSiScale(minimum);
            return ColumnFormat{
                scale,
                decimalDigitsForSignificantDigits(minimum, scale, columnSignificantDigits(seriesValue)),
            };
        }

        ColumnFormat columnFormatFromMinimum(double minimum, int significantDigits)
        {
            const SiScale scale = chooseSiScale(minimum);

            return ColumnFormat{scale, decimalDigitsForSignificantDigits(minimum, scale, significantDigits),};
        }

        ColumnFormat allZeroColumnFormat(int significantDigits)
        {
            return ColumnFormat{SiScale{1.0, ""}, std::max(0, significantDigits - 1)};
        }

        ColumnFormat powerColumnFormat(const std::vector<DividerCandidate>& candidates)
        {
            double minimum = candidates.front().power;
            for (const auto& candidate : candidates)
            {
                minimum = std::min(minimum, candidate.power);
            }
            if (minimum == 0.0)
            {
                return allZeroColumnFormat(3);
            }
            return columnFormatFromMinimum(minimum, 3);
        }

        void includeAbsNonZeroValue(double value, double& minimum, bool& initialized)
        {
            value = cleanVoltageError(value);

            if (value == 0.0)
            {
                return;
            }

            const double absolute = std::abs(value);
            minimum = initialized ? std::min(minimum, absolute) : absolute;
            initialized = true;
        }

        ColumnFormat nominalVoltageColumnFormat(const std::vector<DividerCandidate>& candidates)
        {
            double minimum = 0.0;
            bool initialized = false;
            for (const auto& candidate : candidates)
            {
                includeAbsNonZeroValue(candidate.nominalVoltageError, minimum, initialized);
            }
            return initialized ? columnFormatFromMinimum(minimum, 3) : allZeroColumnFormat(3);
        }

        ColumnFormat voltageRangeColumnFormat(const std::vector<DividerCandidate>& candidates)
        {
            double minimum = 0.0;
            bool initialized = false;

            for (const auto& candidate : candidates)
            {
                includeAbsNonZeroValue(voltageErrorRange(candidate), minimum, initialized);
            }
            return initialized ? columnFormatFromMinimum(minimum, 3) : allZeroColumnFormat(3);
        }

        void printPaddedCell(std::ostream& output, const std::string& cell, std::size_t width, bool rightJustified)
        {
            const std::size_t cellWidth = visibleWidth(cell);
            const std::size_t padding = width > cellWidth ? width - cellWidth : 0;

            if (rightJustified)
            {
                output << std::string(padding, ' ') << cell;
            }
            else
            {
                output << cell << std::string(padding, ' ');
            }
        }

        void printRow(std::ostream& output, const std::vector<std::string>& cells, const std::vector<std::size_t>& widths, bool rightJustified)
        {
            for (std::size_t index = 0; index < cells.size(); ++index)
            {
                if (index > 0)
                {
                    output << "  ";
                }
                printPaddedCell(output, cells[index], widths[index], rightJustified);
            }
            output << '\n';
        }

    } // namespace

    void printSeriesTable(std::ostream& output, std::string_view seriesName, int seriesValue, double tolerancePercent, const std::vector<DividerCandidate>& candidates, int digits, bool includeVoltageColumns, bool fixedDigits)
    {
        output << "\n" << seriesName << " (tolerance +/-" << formatCompactDecimal(tolerancePercent) << "%)\n";

        if (candidates.empty())
        {
            output << "No candidates found.\n";
            return;
        }

        std::vector<std::string> headers{
            omegaHeader("top"), omegaHeader("bottom"), omegaHeader("series"), omegaHeader("parallel"), "power (W)", "nom error (%)", "error range (%)",
        };

        if (includeVoltageColumns)
        {
            headers.push_back("nom error (V)");
            headers.push_back("error range (V)");
            for (const char* header : {"VOUT min (V)", "VOUT max (V)", "top power (W)", "bottom power (W)"})
                headers.push_back(header);
        }
        else
        {
            headers.erase(headers.begin() + 4);
        }

        std::vector<Row> rows;
        rows.reserve(candidates.size());

        const int percentErrorDigits = digits;
        const auto precision = [fixedDigits, digits](int automatic) { return fixedDigits ? digits : automatic; };
        const ColumnFormat seriesColumn = resistanceColumnFormat(candidates, seriesValue, &DividerCandidate::rSeries);
        const ColumnFormat parallelColumn = resistanceColumnFormat(candidates, seriesValue, &DividerCandidate::rParallel);
        const ColumnFormat powerColumn = includeVoltageColumns ? powerColumnFormat(candidates) : ColumnFormat{SiScale{1.0, ""}, 0};
        const ColumnFormat nominalVoltageColumn = includeVoltageColumns ? nominalVoltageColumnFormat(candidates) : ColumnFormat{SiScale{1.0, ""}, 0};
        const ColumnFormat voltageRangeColumn = includeVoltageColumns ? voltageRangeColumnFormat(candidates) : ColumnFormat{SiScale{1.0, ""}, 0};

        for (const auto& candidate : candidates)
        {
            const SiScale highScale = chooseSiScale(candidate.rHigh);
            const SiScale lowScale = chooseSiScale(candidate.rLow);
            const int highDigits = resistanceDigits(seriesValue, candidate.rHigh, highScale);
            const int lowDigits = resistanceDigits(seriesValue, candidate.rLow, lowScale);

            Row row;
            row.cells.push_back(formatSiValueWithScale(candidate.rHigh, highScale, precision(highDigits)));
            row.cells.push_back(formatSiValueWithScale(candidate.rLow, lowScale, precision(lowDigits)));
            row.cells.push_back(formatSiValueWithScale(candidate.rSeries, seriesColumn.scale, precision(seriesColumn.digits)));
            row.cells.push_back(formatSiValueWithScale(candidate.rParallel, parallelColumn.scale, precision(parallelColumn.digits)));

            if (includeVoltageColumns)
            {
                row.cells.push_back(formatSiValueWithScale(candidate.power, powerColumn.scale, precision(powerColumn.digits)));
                row.cells.push_back(formatSignedDecimal(candidate.nominalRatioErrorPercent, percentErrorDigits));
                row.cells.push_back(formatDecimal(ratioErrorRangePercent(candidate), percentErrorDigits));
                row.cells.push_back(formatSignedSiValueWithScale(candidate.nominalVoltageError, nominalVoltageColumn.scale, precision(nominalVoltageColumn.digits)));
                row.cells.push_back(formatSiValueWithScale(voltageErrorRange(candidate), voltageRangeColumn.scale, precision(voltageRangeColumn.digits)));
                for (double value : {candidate.voutMin, candidate.voutMax, candidate.powerHigh, candidate.powerLow})
                {
                    const SiScale scale = chooseSiScale(value);
                    row.cells.push_back(formatSiValueWithScale(value, scale, precision(decimalDigitsForSignificantDigits(value, scale, 3))));
                }
            }
            else
            {
                row.cells.push_back(formatSignedDecimal(candidate.nominalRatioErrorPercent, percentErrorDigits));
                row.cells.push_back(formatDecimal(ratioErrorRangePercent(candidate), percentErrorDigits));
            }

            rows.push_back(std::move(row));
        }

        const auto widths = computeWidths(headers, rows);
        printRow(output, headers, widths, false);

        std::vector<std::string> separators;
        separators.reserve(widths.size());

        for (const auto width : widths)
        {
            separators.push_back(std::string(width, '-'));
        }

        printRow(output, separators, widths, false);

        for (const auto& row : rows)
        {
            printRow(output, row.cells, widths, true);
        }
    }

} // namespace divider
