#pragma once

#include "divider/cli.h"
#include "divider/e_series.h"
#include "divider/search.h"

#include <iosfwd>
#include <vector>

namespace divider
{

    struct SeriesPair
    {
        const SeriesDefinition* high;
        const SeriesDefinition* low;
    };

    // Without --E-high/--E-low both resistors come from the same series. Otherwise every combination of the
    // upper and lower series is evaluated; --E is the fallback for a missing list, then every series.
    std::vector<SeriesPair> selectSeriesPairs(const Options& options);

    // The search for one series pair: its own tolerances, and the rating limit after derating.
    SearchConfig makeSearchConfig(const Options& options, const SeriesPair& pair);

    // Searches every selected series pair and prints the results in the requested format.
    void run(const Options& options, std::ostream& output);

} // namespace divider
