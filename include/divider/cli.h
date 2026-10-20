#pragma once

#include "divider/search.h"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace divider
{

    struct Options
    {
        bool help = false;
        bool useVoltage = false;
        double ratio = 0.0;
        double vin = 0.0;
        double vout = 0.0;
        std::size_t top = 10;
        int digits = 3;
        bool digitsProvided = false;
        double factor = 0.5;
        Range seriesRange;
        Range parallelRange;
        Range powerRange;
        Range highRange;
        Range lowRange;
        std::optional<double> maxErrorPercent;
        std::optional<double> loadResistance;
        std::vector<int> selectedSeries;
        std::map<int, double> toleranceOverridesPercent;
    };

    Options parseCommandLine(int argc, char** argv);
    std::string usage(std::string_view executableName);

} // namespace divider
