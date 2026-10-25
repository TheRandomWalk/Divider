#pragma once

#include "divider/series_result.h"

#include <iosfwd>
#include <string_view>
#include <vector>

namespace divider
{

    void printSeriesTable(std::ostream& output, std::string_view seriesName, int seriesValue, double tolerancePercent, const std::vector<DividerCandidate>& candidates, int digits, bool includeVoltageColumns, bool fixedDigits = false, bool includeWorstPowerColumns = false);
    void printPairTable(std::ostream& output, const SeriesResult& result, int digits, bool includeVoltageColumns, bool fixedDigits = false, bool includeWorstPowerColumns = false);

} // namespace divider
