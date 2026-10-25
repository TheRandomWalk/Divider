#pragma once

#include "divider/series_result.h"

#include <iosfwd>
#include <vector>

namespace divider
{

    // Machine-readable output: base units, 12 significant digits, locale independent.
    void printCsv(std::ostream& output, const std::vector<SeriesResult>& results, bool includeVoltageColumns);
    void printJson(std::ostream& output, const std::vector<SeriesResult>& results, bool includeVoltageColumns);

} // namespace divider
