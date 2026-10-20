#pragma once

#include "divider/search.h"

#include <iosfwd>
#include <string_view>
#include <vector>

namespace divider
{

    void printSeriesTable(std::ostream& output, std::string_view seriesName, int seriesValue, double tolerancePercent, const std::vector<DividerCandidate>& candidates, int digits, bool includeVoltageColumns, bool fixedDigits = false);

} // namespace divider
