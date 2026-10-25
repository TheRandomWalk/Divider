#include "divider/machine_output.h"

#include <iomanip>
#include <locale>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace divider
{
    namespace
    {

        struct Field
        {
            const char* name;
            double (*get)(const DividerCandidate&);
            bool voltageOnly;
        };

        const std::vector<Field>& machineFields()
        {
            static const std::vector<Field> fields{
                {"r_high_ohm", [](const DividerCandidate& c) { return c.rHigh; }, false},
                {"r_low_ohm", [](const DividerCandidate& c) { return c.rLow; }, false},
                {"r_series_ohm", [](const DividerCandidate& c) { return c.rSeries; }, false},
                {"r_parallel_ohm", [](const DividerCandidate& c) { return c.rParallel; }, false},
                {"ratio", [](const DividerCandidate& c) { return c.ratio; }, false},
                {"ratio_min", [](const DividerCandidate& c) { return c.ratioLow; }, false},
                {"ratio_max", [](const DividerCandidate& c) { return c.ratioHigh; }, false},
                {"nominal_error_pct", [](const DividerCandidate& c) { return c.nominalRatioErrorPercent; }, false},
                {"error_min_pct", [](const DividerCandidate& c) { return c.lowRatioErrorPercent; }, false},
                {"error_max_pct", [](const DividerCandidate& c) { return c.highRatioErrorPercent; }, false},
                {"vout_min_v", [](const DividerCandidate& c) { return c.voutMin; }, true},
                {"vout_max_v", [](const DividerCandidate& c) { return c.voutMax; }, true},
                {"nominal_error_v", [](const DividerCandidate& c) { return c.nominalVoltageError; }, true},
                {"error_min_v", [](const DividerCandidate& c) { return c.lowVoltageError; }, true},
                {"error_max_v", [](const DividerCandidate& c) { return c.highVoltageError; }, true},
                {"power_w", [](const DividerCandidate& c) { return c.power; }, true},
                {"power_high_w", [](const DividerCandidate& c) { return c.powerHigh; }, true},
                {"power_low_w", [](const DividerCandidate& c) { return c.powerLow; }, true},
                {"power_high_max_w", [](const DividerCandidate& c) { return c.powerHighMax; }, true},
                {"power_low_max_w", [](const DividerCandidate& c) { return c.powerLowMax; }, true},
            };
            return fields;
        }

        bool isIncluded(const Field& field, bool includeVoltageColumns)
        {
            return includeVoltageColumns || !field.voltageOnly;
        }

        std::string machineNumber(double value)
        {
            std::ostringstream text;
            text.imbue(std::locale::classic());
            text << std::setprecision(12) << value;
            return text.str();
        }

    } // namespace

    void printCsv(std::ostream& output, const std::vector<SeriesResult>& results, bool includeVoltageColumns)
    {
        output << "rank,high_series,low_series,high_tolerance_pct,low_tolerance_pct";
        for (const auto& field : machineFields())
        {
            if (isIncluded(field, includeVoltageColumns))
            {
                output << ',' << field.name;
            }
        }
        output << '\n';

        for (const auto& result : results)
        {
            std::size_t rank = 0;
            for (const auto& candidate : result.candidates)
            {
                output << ++rank << ',' << result.highName << ',' << result.lowName << ',' << machineNumber(result.highTolerancePercent) << ',' << machineNumber(result.lowTolerancePercent);
                for (const auto& field : machineFields())
                {
                    if (isIncluded(field, includeVoltageColumns))
                    {
                        output << ',' << machineNumber(field.get(candidate));
                    }
                }
                output << '\n';
            }
        }
    }

    void printJson(std::ostream& output, const std::vector<SeriesResult>& results, bool includeVoltageColumns)
    {
        output << "{\n  \"results\": [";
        for (std::size_t resultIndex = 0; resultIndex < results.size(); ++resultIndex)
        {
            const auto& result = results[resultIndex];
            output << (resultIndex > 0 ? ",\n" : "\n") << "    {\n"
                   << "      \"high_series\": \"" << result.highName << "\",\n"
                   << "      \"low_series\": \"" << result.lowName << "\",\n"
                   << "      \"high_tolerance_pct\": " << machineNumber(result.highTolerancePercent) << ",\n"
                   << "      \"low_tolerance_pct\": " << machineNumber(result.lowTolerancePercent) << ",\n"
                   << "      \"candidates\": [";

            for (std::size_t index = 0; index < result.candidates.size(); ++index)
            {
                output << (index > 0 ? ",\n" : "\n") << "        {\"rank\": " << index + 1;
                for (const auto& field : machineFields())
                {
                    if (isIncluded(field, includeVoltageColumns))
                    {
                        output << ", \"" << field.name << "\": " << machineNumber(field.get(result.candidates[index]));
                    }
                }
                output << '}';
            }
            output << (result.candidates.empty() ? "]\n" : "\n      ]\n") << "    }";
        }
        output << (results.empty() ? "]\n}\n" : "\n  ]\n}\n");
    }

} // namespace divider
