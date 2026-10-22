#pragma once

#include "divider/search.h"

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace divider
{

    struct SeriesResult
    {
        std::string highName;
        std::string lowName;
        int highValueCount = 0;
        int lowValueCount = 0;
        double highTolerancePercent = 0.0;
        double lowTolerancePercent = 0.0;
        std::vector<DividerCandidate> candidates;
    };

    void printSeriesTable(std::ostream& output, std::string_view seriesName, int seriesValue, double tolerancePercent, const std::vector<DividerCandidate>& candidates, int digits, bool includeVoltageColumns, bool fixedDigits = false, bool includeWorstPowerColumns = false);
    void printPairTable(std::ostream& output, const SeriesResult& result, int digits, bool includeVoltageColumns, bool fixedDigits = false, bool includeWorstPowerColumns = false);
    void printCsv(std::ostream& output, const std::vector<SeriesResult>& results, bool includeVoltageColumns);
    void printJson(std::ostream& output, const std::vector<SeriesResult>& results, bool includeVoltageColumns);

} // namespace divider
