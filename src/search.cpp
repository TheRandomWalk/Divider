#include "divider/search.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace divider
{
    namespace
    {

        using ValueIterator = std::vector<double>::const_iterator;

        bool isFinite(double value)
        {
            return std::isfinite(value);
        }

        bool betterCandidate(const DividerCandidate& left, const DividerCandidate& right)
        {
            constexpr double scoreEpsilon = 1.0e-9;
            constexpr double valueEpsilon = 1.0e-15;
            if (std::abs(left.ratioErrorScorePercent - right.ratioErrorScorePercent) > scoreEpsilon)
            {
                return left.ratioErrorScorePercent < right.ratioErrorScorePercent;
            }
            if (std::abs(std::abs(left.nominalRatioErrorPercent) - std::abs(right.nominalRatioErrorPercent)) > scoreEpsilon)
            {
                return std::abs(left.nominalRatioErrorPercent) < std::abs(right.nominalRatioErrorPercent);
            }
            if (std::abs(left.rSeries - right.rSeries) > valueEpsilon * std::max(left.rSeries, right.rSeries))
            {
                return left.rSeries < right.rSeries;
            }
            if (std::abs(left.rHigh - right.rHigh) > valueEpsilon * std::max(left.rHigh, right.rHigh))
            {
                return left.rHigh < right.rHigh;
            }
            return left.rLow < right.rLow;
        }

        // Heap comparator that keeps the worst retained candidate on top.
        struct WorstFirst
        {
            bool operator()(const DividerCandidate& left, const DividerCandidate& right) const
            {
                return betterCandidate(left, right);
            }
        };

        double parallelResistance(double rHigh, double rLow)
        {
            const double small = std::min(rHigh, rLow);
            return small / (1.0 + small / std::max(rHigh, rLow));
        }

        double ratioErrorPercent(double ratio, double targetRatio)
        {
            return ((ratio - targetRatio) / targetRatio) * 100.0;
        }

        double scoreError(double nominalErrorPercent, double lowErrorPercent, double highErrorPercent, double factor)
        {
            return std::abs(nominalErrorPercent) * factor + (std::abs(highErrorPercent) + std::abs(lowErrorPercent)) * (1.0 - factor);
        }

        // The divider output ratio, with an optional fixed load across the lower resistor.
        struct DividerModel
        {
            std::optional<double> load;

            double bottom(double rLow) const
            {
                return load ? parallelResistance(rLow, *load) : rLow;
            }

            double ratio(double rHigh, double rLow) const
            {
                const double effectiveLow = bottom(rLow);
                return effectiveLow / (rHigh + effectiveLow);
            }
        };

        // Everything about a search that does not change from one candidate to the next.
        struct SearchContext
        {
            const SearchConfig& config;
            DividerModel model;
            double highTolerance;
            double lowTolerance;
        };

        // Resistor limits over the tolerance interval.
        struct Extremes
        {
            double highMin;
            double highMax;
            double lowMin;
            double lowMax;
        };

        Extremes toleranceExtremes(double rHigh, double rLow, const SearchContext& context)
        {
            return Extremes{
                rHigh * (1.0 - context.highTolerance),
                rHigh * (1.0 + context.highTolerance),
                rLow * (1.0 - context.lowTolerance),
                rLow * (1.0 + context.lowTolerance),
            };
        }

        // Fills the nominal and tolerance-extreme voltage errors, VOUT range and nominal power for one candidate.
        void fillVoltageAndPower(DividerCandidate& candidate, double vin, double vout, const SearchContext& context)
        {
            candidate.nominalVoltageError = (vin * candidate.ratio) - vout;
            candidate.lowVoltageError = (vin * candidate.ratioLow) - vout;
            candidate.highVoltageError = (vin * candidate.ratioHigh) - vout;
            candidate.voutMin = vin * candidate.ratioLow;
            candidate.voutMax = vin * candidate.ratioHigh;

            const double output = vin * candidate.ratio;
            const double dropHigh = vin - output;
            candidate.powerHigh = dropHigh * (dropHigh / candidate.rHigh);
            candidate.powerLow = output * (output / candidate.rLow);
            candidate.power = context.model.load ? candidate.powerHigh + candidate.powerLow : vin * (vin / candidate.rSeries);
        }

        // Worst case per resistor, with B the effective lower leg. P_high = VIN^2 * Rh / (Rh + B)^2 falls as B
        // grows, so B is taken at its minimum and Rh peaks at Rh = B. P_low = Vout^2 / Rl falls as Rh grows, so
        // Rh is taken at its minimum and Rl peaks at Rl = Rh || load. Each function has a single maximum, so
        // clamping the peak into the tolerance interval gives the exact worst case.
        void fillWorstCasePower(DividerCandidate& candidate, double vin, const Extremes& extremes, const SearchContext& context)
        {
            const DividerModel& model = context.model;

            const double bottomMin = model.bottom(extremes.lowMin);
            const double highAtPeak = std::clamp(bottomMin, extremes.highMin, extremes.highMax);
            candidate.powerHighMax = vin * vin * highAtPeak / ((highAtPeak + bottomMin) * (highAtPeak + bottomMin));

            const double lowPeak = model.load ? parallelResistance(extremes.highMin, *model.load) : extremes.highMin;
            const double lowAtPeak = std::clamp(lowPeak, extremes.lowMin, extremes.lowMax);
            const double bottomAtPeak = model.bottom(lowAtPeak);
            const double outputAtPeak = vin * bottomAtPeak / (extremes.highMin + bottomAtPeak);
            candidate.powerLowMax = outputAtPeak * outputAtPeak / lowAtPeak;
        }

        DividerCandidate makeCandidate(double rHigh, double rLow, const SearchContext& context)
        {
            const SearchConfig& config = context.config;
            const DividerModel& model = context.model;
            const Extremes extremes = toleranceExtremes(rHigh, rLow, context);

            DividerCandidate candidate;
            candidate.rHigh = rHigh;
            candidate.rLow = rLow;
            candidate.rSeries = rHigh + rLow;
            candidate.rParallel = parallelResistance(rHigh, rLow);
            candidate.ratio = model.ratio(rHigh, rLow);
            candidate.ratioLow = model.ratio(extremes.highMax, extremes.lowMin);
            candidate.ratioHigh = model.ratio(extremes.highMin, extremes.lowMax);
            candidate.nominalRatioErrorPercent = ratioErrorPercent(candidate.ratio, config.targetRatio);
            candidate.lowRatioErrorPercent = ratioErrorPercent(candidate.ratioLow, config.targetRatio);
            candidate.highRatioErrorPercent = ratioErrorPercent(candidate.ratioHigh, config.targetRatio);
            candidate.ratioErrorScorePercent = scoreError(candidate.nominalRatioErrorPercent, candidate.lowRatioErrorPercent, candidate.highRatioErrorPercent, config.factor);

            if (config.vin && config.vout)
            {
                fillVoltageAndPower(candidate, *config.vin, *config.vout, context);
                fillWorstCasePower(candidate, *config.vin, extremes, context);
                if (!isFinite(candidate.power) || !isFinite(candidate.powerHigh) || !isFinite(candidate.powerLow))
                {
                    throw std::invalid_argument("Power calculation out of range; reduce VIN or increase resistance.");
                }
            }

            return candidate;
        }

        bool validPercent(double value)
        {
            return isFinite(value) && value >= 0.0 && value < 100.0;
        }

        void validateConfig(const SearchConfig& config)
        {
            if (config.powerRange.enabled && !config.vin)
            {
                throw std::invalid_argument("Power filtering requires VIN.");
            }
            if (config.ratedPower && !config.vin)
            {
                throw std::invalid_argument("Rated power filtering requires VIN.");
            }
            if (config.ratedPower && (!isFinite(*config.ratedPower) || *config.ratedPower <= 0.0))
            {
                throw std::invalid_argument("Invalid rated power.");
            }

            if (!isFinite(config.targetRatio) || config.targetRatio <= 0.0 || config.targetRatio >= 1.0 || !validPercent(config.tolerancePercent) ||
                (config.lowTolerancePercent && !validPercent(*config.lowTolerancePercent)) || !isFinite(config.factor) || config.factor < 0.0 || config.factor > 1.0)
            {
                throw std::invalid_argument("Invalid search target, tolerance or factor.");
            }
            if (config.loadResistance && (!isFinite(*config.loadResistance) || *config.loadResistance <= 0.0))
            {
                throw std::invalid_argument("Invalid load resistance.");
            }
            if (config.maxErrorPercent && (!isFinite(*config.maxErrorPercent) || *config.maxErrorPercent < 0.0))
            {
                throw std::invalid_argument("Invalid maximum error.");
            }
            if (config.vin.has_value() != config.vout.has_value() || (config.vin && (!isFinite(*config.vin) || !isFinite(*config.vout) || *config.vout <= 0.0 || *config.vin <= *config.vout)))
            {
                throw std::invalid_argument("Invalid voltage pair.");
            }
            for (const Range* range : {&config.seriesRange, &config.parallelRange, &config.highRange, &config.lowRange, &config.powerRange})
            {
                const bool allowsZeroMinimum = range == &config.powerRange;
                if (range->enabled && (!isFinite(range->min) || !isFinite(range->max) || range->min > range->max || range->min < 0.0 || (!allowsZeroMinimum && range->min == 0.0)))
                {
                    throw std::invalid_argument("Invalid search range.");
                }
            }
        }

        // Sorted, de-duplicated copy of the resistor values.
        std::vector<double> prepareValues(const std::vector<double>& source)
        {
            std::vector<double> prepared = source;
            for (const double value : prepared)
            {
                if (!isFinite(value) || value <= 0.0 || value > 1.0e150)
                {
                    throw std::invalid_argument("Invalid resistor value.");
                }
            }
            std::sort(prepared.begin(), prepared.end());
            prepared.erase(std::unique(prepared.begin(), prepared.end()), prepared.end());
            return prepared;
        }

        // Narrows the sorted lower-resistor values to the interval that can satisfy every filter for this upper resistor.
        // Each expression is monotonic in the lower resistor, so binary searches discard impossible intervals
        // without approximating the ranking.
        class LowerWindow
        {
        public:
            LowerWindow(const std::vector<double>& values, double rHigh, const SearchContext& context) : m_first(values.begin()), m_last(values.end())
            {
                const SearchConfig& config = context.config;

                restrict(config.lowRange, [](double low) { return low; });
                restrict(config.seriesRange, [rHigh](double low) { return rHigh + low; });
                restrict(config.parallelRange, [rHigh](double low) { return parallelResistance(rHigh, low); });

                if (config.powerRange.enabled && !config.loadResistance)
                {
                    const auto power = [&](double low) { return *config.vin * (*config.vin / (rHigh + low)); };
                    m_first = std::partition_point(m_first, m_last, [&](double low) { return power(low) > config.powerRange.max; });
                    m_last = std::partition_point(m_first, m_last, [&](double low) { return power(low) >= config.powerRange.min; });
                }

                if (config.maxErrorPercent)
                {
                    // Both extreme ratios increase monotonically with the lower resistor.
                    const auto errorAtLowExtreme = [&](double low) { return ratioErrorPercent(context.model.ratio(rHigh * (1.0 + context.highTolerance), low * (1.0 - context.lowTolerance)), config.targetRatio); };
                    const auto errorAtHighExtreme = [&](double low) { return ratioErrorPercent(context.model.ratio(rHigh * (1.0 - context.highTolerance), low * (1.0 + context.lowTolerance)), config.targetRatio); };
                    m_first = std::partition_point(m_first, m_last, [&](double low) { return errorAtLowExtreme(low) < -*config.maxErrorPercent; });
                    m_last = std::partition_point(m_first, m_last, [&](double low) { return errorAtHighExtreme(low) <= *config.maxErrorPercent; });
                }
            }

            ValueIterator begin() const
            {
                return m_first;
            }

            ValueIterator end() const
            {
                return m_last;
            }

        private:
            template <typename Expression>
            void restrict(const Range& range, const Expression& expression)
            {
                if (!range.enabled)
                {
                    return;
                }
                m_first = std::partition_point(m_first, m_last, [&](double low) { return expression(low) < range.min; });
                m_last = std::partition_point(m_first, m_last, [&](double low) { return expression(low) <= range.max; });
            }

            ValueIterator m_first;
            ValueIterator m_last;
        };

        // Filters that need the full candidate (after the window has removed the obviously impossible ones).
        bool passesFilters(const DividerCandidate& candidate, const SearchConfig& config)
        {
            if (!config.powerRange.contains(candidate.power))
            {
                return false;
            }
            if (config.ratedPower && std::max(candidate.powerHighMax, candidate.powerLowMax) > *config.ratedPower)
            {
                return false;
            }
            if (config.maxErrorPercent && std::max(std::abs(candidate.lowRatioErrorPercent), std::abs(candidate.highRatioErrorPercent)) > *config.maxErrorPercent)
            {
                return false;
            }
            return true;
        }

    } // namespace

    bool Range::contains(double value) const
    {
        return !enabled || (value >= min && value <= max);
    }

    std::vector<DividerCandidate> findBestCandidates(const std::vector<double>& resistorValues, const SearchConfig& config)
    {
        return findBestCandidates(resistorValues, resistorValues, config);
    }

    std::vector<DividerCandidate> findBestCandidates(const std::vector<double>& highValues, const std::vector<double>& lowValues, const SearchConfig& config)
    {
        if (config.top == 0)
        {
            return {};
        }

        validateConfig(config);
        const std::vector<double> highSorted = prepareValues(highValues);
        const std::vector<double> lowSorted = prepareValues(lowValues);

        const double highTolerancePercent = config.tolerancePercent;
        const double lowTolerancePercent = config.lowTolerancePercent.value_or(config.tolerancePercent);
        const SearchContext context{config, DividerModel{config.loadResistance}, highTolerancePercent / 100.0, lowTolerancePercent / 100.0};

        std::priority_queue<DividerCandidate, std::vector<DividerCandidate>, WorstFirst> best;
        for (const double rHigh : highSorted)
        {
            if (!config.highRange.contains(rHigh))
            {
                continue;
            }

            const LowerWindow window(lowSorted, rHigh, context);
            for (auto it = window.begin(); it != window.end(); ++it)
            {
                const DividerCandidate candidate = makeCandidate(rHigh, *it, context);
                if (!passesFilters(candidate, config))
                {
                    continue;
                }
                if (!isFinite(candidate.ratioErrorScorePercent))
                {
                    throw std::invalid_argument("Error calculation out of range; increase the target ratio.");
                }

                if (best.size() < config.top)
                {
                    best.push(candidate);
                }
                else if (betterCandidate(candidate, best.top()))
                {
                    best.pop();
                    best.push(candidate);
                }
            }
        }

        std::vector<DividerCandidate> results;
        results.reserve(best.size());
        for (; !best.empty(); best.pop())
        {
            results.push_back(best.top());
        }

        std::sort(results.begin(), results.end(), betterCandidate);
        return results;
    }

} // namespace divider
