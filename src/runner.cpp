#include "divider/runner.h"

#include "divider/machine_output.h"
#include "divider/table.h"

#include <algorithm>
#include <map>
#include <ostream>
#include <utility>

namespace divider
{
    namespace
    {

        // An empty selection means every series. Results keep the order of allSeries().
        std::vector<const SeriesDefinition*> selectSeries(const std::vector<int>& selected)
        {
            std::vector<const SeriesDefinition*> result;
            for (const auto& series : allSeries())
            {
                if (selected.empty() || std::find(selected.begin(), selected.end(), series.valueCount) != selected.end())
                {
                    result.push_back(&series);
                }
            }
            return result;
        }

        double toleranceFor(const Options& options, const SeriesDefinition& series)
        {
            const auto override = options.toleranceOverridesPercent.find(series.valueCount);
            return override != options.toleranceOverridesPercent.end() ? override->second : series.defaultTolerancePercent;
        }

        // Generates the values of each series once, however many pairs use it.
        class SeriesValueCache
        {
        public:
            const std::vector<double>& get(int valueCount)
            {
                auto found = m_values.find(valueCount);
                if (found == m_values.end())
                {
                    found = m_values.emplace(valueCount, generateSeriesValues(valueCount)).first;
                }
                return found->second;
            }

        private:
            std::map<int, std::vector<double>> m_values;
        };

    } // namespace

    std::vector<SeriesPair> selectSeriesPairs(const Options& options)
    {
        std::vector<SeriesPair> pairs;
        if (options.highSeries.empty() && options.lowSeries.empty())
        {
            for (const auto* series : selectSeries(options.selectedSeries))
            {
                pairs.push_back({series, series});
            }
            return pairs;
        }

        const auto highList = selectSeries(options.highSeries.empty() ? options.selectedSeries : options.highSeries);
        const auto lowList = selectSeries(options.lowSeries.empty() ? options.selectedSeries : options.lowSeries);
        for (const auto* high : highList)
        {
            for (const auto* low : lowList)
            {
                pairs.push_back({high, low});
            }
        }
        return pairs;
    }

    SearchConfig makeSearchConfig(const Options& options, const SeriesPair& pair)
    {
        SearchConfig config;
        config.targetRatio = options.ratio;
        config.tolerancePercent = toleranceFor(options, *pair.high);
        config.lowTolerancePercent = toleranceFor(options, *pair.low);
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
        return config;
    }

    void run(const Options& options, std::ostream& output)
    {
        SeriesValueCache values;
        std::vector<SeriesResult> results;

        for (const SeriesPair& pair : selectSeriesPairs(options))
        {
            const SearchConfig config = makeSearchConfig(options, pair);

            SeriesResult result;
            result.highName = pair.high->name;
            result.lowName = pair.low->name;
            result.highValueCount = pair.high->valueCount;
            result.lowValueCount = pair.low->valueCount;
            result.highTolerancePercent = config.tolerancePercent;
            result.lowTolerancePercent = *config.lowTolerancePercent;
            result.candidates = findBestCandidates(values.get(pair.high->valueCount), values.get(pair.low->valueCount), config);

            if (options.format == OutputFormat::Table)
            {
                // Tables are printed as they are computed so long searches show progress.
                printPairTable(output, result, options.digits, options.useVoltage, options.digitsProvided, options.ratedPower.has_value());
            }
            else
            {
                results.push_back(std::move(result));
            }
        }

        if (options.format == OutputFormat::Csv)
        {
            printCsv(output, results, options.useVoltage);
        }
        else if (options.format == OutputFormat::Json)
        {
            printJson(output, results, options.useVoltage);
        }
    }

} // namespace divider
