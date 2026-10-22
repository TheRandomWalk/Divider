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

    enum class OutputFormat
    {
        Table,
        Csv,
        Json,
    };

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
        std::optional<double> ratedPower;
        std::optional<double> deratePercent;
        OutputFormat format = OutputFormat::Table;
        std::vector<int> selectedSeries;
        std::vector<int> highSeries;
        std::vector<int> lowSeries;
        std::map<int, double> toleranceOverridesPercent;
    };

    Options parseCommandLine(int argc, char** argv);
    std::string usage(std::string_view executableName);

} // namespace divider
