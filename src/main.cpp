#include "divider/cli.h"
#include "divider/e_series.h"
#include "divider/search.h"
#include "divider/table.h"

#include <exception>
#include <iostream>
#include <map>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{

    using SeriesPair = std::pair<const divider::SeriesDefinition*, const divider::SeriesDefinition*>;

    void configureConsoleEncoding()
    {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif
    }

    // An empty selection means every series. Results keep the order of allSeries().
    std::vector<const divider::SeriesDefinition*> selectSeries(const std::vector<int>& selected)
    {
        std::vector<const divider::SeriesDefinition*> result;
        for (const auto& series : divider::allSeries())
        {
            bool include = selected.empty();
            for (const int value : selected)
            {
                include = include || value == series.valueCount;
            }
            if (include)
            {
                result.push_back(&series);
            }
        }
        return result;
    }

    // Without --E-high/--E-low both resistors come from the same series. Otherwise every
    // combination of the upper and lower series is evaluated; --E is the fallback for a missing list.
    std::vector<SeriesPair> selectSeriesPairs(const divider::Options& options)
    {
        std::vector<SeriesPair> pairs;
        if (options.highSeries.empty() && options.lowSeries.empty())
        {
            for (const auto* series : selectSeries(options.selectedSeries))
            {
                pairs.emplace_back(series, series);
            }
            return pairs;
        }

        const auto highList = selectSeries(options.highSeries.empty() ? options.selectedSeries : options.highSeries);
        const auto lowList = selectSeries(options.lowSeries.empty() ? options.selectedSeries : options.lowSeries);
        for (const auto* high : highList)
        {
            for (const auto* low : lowList)
            {
                pairs.emplace_back(high, low);
            }
        }
        return pairs;
    }

    double toleranceFor(const divider::Options& options, const divider::SeriesDefinition& series)
    {
        const auto override = options.toleranceOverridesPercent.find(series.valueCount);
        return override != options.toleranceOverridesPercent.end() ? override->second : series.defaultTolerancePercent;
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

        std::map<int, std::vector<double>> valuesBySeries;
        const auto valuesFor = [&valuesBySeries](int valueCount) -> const std::vector<double>&
        {
            auto found = valuesBySeries.find(valueCount);
            if (found == valuesBySeries.end())
            {
                found = valuesBySeries.emplace(valueCount, divider::generateSeriesValues(valueCount)).first;
            }
            return found->second;
        };

        std::vector<divider::SeriesResult> results;
        for (const auto& [high, low] : selectSeriesPairs(options))
        {
            divider::SearchConfig config;
            config.targetRatio = options.ratio;
            config.tolerancePercent = toleranceFor(options, *high);
            config.lowTolerancePercent = toleranceFor(options, *low);
            config.factor = options.factor;
            config.top = options.top;
            config.seriesRange = options.seriesRange;
            config.parallelRange = options.parallelRange;
            config.powerRange = options.powerRange;
            config.highRange = options.highRange;
            config.lowRange = options.lowRange;
            config.maxErrorPercent = options.maxErrorPercent;
            config.loadResistance = options.loadResistance;

            if (options.ratedPower)
            {
                config.ratedPower = *options.ratedPower * (1.0 - options.deratePercent.value_or(0.0) / 100.0);
            }
            if (options.useVoltage)
            {
                config.vin = options.vin;
                config.vout = options.vout;
            }

            divider::SeriesResult result;
            result.highName = high->name;
            result.lowName = low->name;
            result.highValueCount = high->valueCount;
            result.lowValueCount = low->valueCount;
            result.highTolerancePercent = config.tolerancePercent;
            result.lowTolerancePercent = *config.lowTolerancePercent;
            result.candidates = divider::findBestCandidates(valuesFor(high->valueCount), valuesFor(low->valueCount), config);

            if (options.format == divider::OutputFormat::Table)
            {
                divider::printPairTable(std::cout, result, options.digits, options.useVoltage, options.digitsProvided, options.ratedPower.has_value());
            }
            else
            {
                results.push_back(std::move(result));
            }
        }

        if (options.format == divider::OutputFormat::Csv)
        {
            divider::printCsv(std::cout, results, options.useVoltage);
        }
        else if (options.format == divider::OutputFormat::Json)
        {
            divider::printJson(std::cout, results, options.useVoltage);
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
