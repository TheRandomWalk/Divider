#pragma once

#include <string>
#include <vector>

namespace divider
{

    struct SeriesDefinition
    {
        int valueCount;
        std::string name;
        double defaultTolerancePercent;
    };

    const std::vector<SeriesDefinition>& allSeries();
    const SeriesDefinition* findSeries(int valueCount); // nullptr when the series is not supported.
    std::vector<double> generateBaseValues(int valueCount);
    std::vector<double> generateSeriesValues(int valueCount);

} // namespace divider
