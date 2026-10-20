#include "divider/cli.h"
#include "divider/e_series.h"
#include "divider/search.h"
#include "divider/table.h"

#include <exception>
#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{

    void configureConsoleEncoding()
    {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif
    }

    bool shouldEvaluateSeries(const divider::Options& options, int valueCount)
    {
        if (options.selectedSeries.empty())
        {
            return true;
        }

        for (const int selected : options.selectedSeries)
        {
            if (selected == valueCount)
            {
                return true;
            }
        }
        return false;
    }

} // namespace

int main(int argc, char** argv)
{
    try
    {
        configureConsoleEncoding();

        const divider::Options options = divider::parseCommandLine(argc, argv);
        const std::string executableName = argc > 0 ? argv[0] : "divider";

        if (options.help)
        {
            std::cout << divider::usage(executableName);
            return 0;
        }

        for (const auto& series : divider::allSeries())
        {
            if (!shouldEvaluateSeries(options, series.valueCount))
            {
                continue;
            }

            double tolerancePercent = series.defaultTolerancePercent;
            const auto override = options.toleranceOverridesPercent.find(series.valueCount);

            if (override != options.toleranceOverridesPercent.end())
            {
                tolerancePercent = override->second;
            }

            divider::SearchConfig config;
            config.targetRatio = options.ratio;
            config.tolerancePercent = tolerancePercent;
            config.factor = options.factor;
            config.top = options.top;
            config.seriesRange = options.seriesRange;
            config.parallelRange = options.parallelRange;
            config.powerRange = options.powerRange;
            config.highRange = options.highRange;
            config.lowRange = options.lowRange;
            config.maxErrorPercent = options.maxErrorPercent;
            config.loadResistance = options.loadResistance;

            if (options.useVoltage)
            {
                config.vin = options.vin;
                config.vout = options.vout;
            }

            const auto values = divider::generateSeriesValues(series.valueCount);
            const auto candidates = divider::findBestCandidates(values, config);
            divider::printSeriesTable(std::cout, series.name, series.valueCount, tolerancePercent, candidates, options.digits, options.useVoltage, options.digitsProvided);
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        const std::string executableName = argc > 0 ? argv[0] : "divider";
        std::cerr << "Error: " << error.what() << "\n\n" << divider::usage(executableName);
        return 1;
    }
}
