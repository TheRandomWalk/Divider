#pragma once

#include "divider/search.h"

#include <string>
#include <vector>

namespace divider
{

    // The best candidates found for one combination of upper and lower series.
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

} // namespace divider
