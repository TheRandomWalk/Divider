#include "divider/search.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <stdexcept>
#include <vector>

namespace divider
{
    namespace
    {

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

        double scoreError(double nominalErrorPercent, double lowErrorPercent, double highErrorPercent, double factor)
        {
            return std::abs(nominalErrorPercent) * factor + (std::abs(highErrorPercent) + std::abs(lowErrorPercent)) * (1.0 - factor);
        }

        double clampValue(double value, double low, double high)
        {
            return std::min(std::max(value, low), high);
        }

        DividerCandidate makeCandidate(double rHigh, double rLow, double targetRatio, double highTolerancePercent, double lowTolerancePercent, double factor, std::optional<double> vin, std::optional<double> vout, std::optional<double> load)
        {
            const double rSeries = rHigh + rLow;
            const auto effectiveLow = [load](double resistance)
            {
                return load ? parallelResistance(resistance, *load) : resistance;
            };
            const auto dividerRatio = [&effectiveLow](double high, double low)
            {
                const double bottom = effectiveLow(low);
                return bottom / (high + bottom);
            };
            const double ratio = dividerRatio(rHigh, rLow);
            const double nominalRatioErrorPercent = ((ratio - targetRatio) / targetRatio) * 100.0;
            const double highTolerance = highTolerancePercent / 100.0;
            const double lowTolerance = lowTolerancePercent / 100.0;
            const double rHighMax = rHigh * (1.0 + highTolerance);
            const double rHighMin = rHigh * (1.0 - highTolerance);
            const double rLowMax = rLow * (1.0 + lowTolerance);
            const double rLowMin = rLow * (1.0 - lowTolerance);
            const double ratioLow = dividerRatio(rHighMax, rLowMin);
            const double ratioHigh = dividerRatio(rHighMin, rLowMax);
            const double lowRatioErrorPercent = ((ratioLow - targetRatio) / targetRatio) * 100.0;
            const double highRatioErrorPercent = ((ratioHigh - targetRatio) / targetRatio) * 100.0;

            DividerCandidate candidate;

            candidate.rHigh = rHigh;
            candidate.rLow = rLow;
            candidate.rSeries = rSeries;
            candidate.rParallel = parallelResistance(rHigh, rLow);
            candidate.ratio = ratio;
            candidate.ratioLow = ratioLow;
            candidate.ratioHigh = ratioHigh;
            candidate.nominalRatioErrorPercent = nominalRatioErrorPercent;
            candidate.lowRatioErrorPercent = lowRatioErrorPercent;
            candidate.highRatioErrorPercent = highRatioErrorPercent;
            candidate.ratioErrorScorePercent = scoreError(nominalRatioErrorPercent, lowRatioErrorPercent, highRatioErrorPercent, factor);

            if (vin && vout)
            {
                candidate.nominalVoltageError = (*vin * ratio) - *vout;
                candidate.lowVoltageError = (*vin * ratioLow) - *vout;
                candidate.highVoltageError = (*vin * ratioHigh) - *vout;
                candidate.voutMin = *vin * ratioLow;
                candidate.voutMax = *vin * ratioHigh;
                const double output = *vin * ratio;
                const double dropHigh = *vin - output;
                candidate.powerHigh = dropHigh * (dropHigh / rHigh);
                candidate.powerLow = output * (output / rLow);
                candidate.power = load ? candidate.powerHigh + candidate.powerLow : *vin * (*vin / rSeries);

                // Worst case per resistor, with B the effective lower leg. P_high = VIN^2 * Rh / (Rh + B)^2 falls as B
                // grows, so B is taken at its minimum and Rh peaks at Rh = B. P_low = Vout^2 / Rl falls as Rh grows, so
                // Rh is taken at its minimum and Rl peaks at Rl = Rh || load. Each function has a single maximum, so
                // clamping the peak into the tolerance interval gives the exact worst case.
                const double bottomMin = effectiveLow(rLowMin);
                const double highAtPeak = clampValue(bottomMin, rHighMin, rHighMax);
                candidate.powerHighMax = *vin * *vin * highAtPeak / ((highAtPeak + bottomMin) * (highAtPeak + bottomMin));
                const double lowPeak = load ? parallelResistance(rHighMin, *load) : rHighMin;
                const double lowAtPeak = clampValue(lowPeak, rLowMin, rLowMax);
                const double bottomAtPeak = effectiveLow(lowAtPeak);
                const double outputAtPeak = *vin * bottomAtPeak / (rHighMin + bottomAtPeak);
                candidate.powerLowMax = outputAtPeak * outputAtPeak / lowAtPeak;
                if (!std::isfinite(candidate.power) || !std::isfinite(candidate.powerHigh) || !std::isfinite(candidate.powerLow))
                    throw std::invalid_argument("Power calculation out of range; reduce VIN or increase resistance.");
            }

            return candidate;
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

        if (config.powerRange.enabled && !config.vin)
        {
            throw std::invalid_argument("Power filtering requires VIN.");
        }
        if (config.ratedPower && !config.vin)
        {
            throw std::invalid_argument("Rated power filtering requires VIN.");
        }
        if (config.ratedPower && (!std::isfinite(*config.ratedPower) || *config.ratedPower <= 0.0))
        {
            throw std::invalid_argument("Invalid rated power.");
        }

        const auto finite = [](double value) { return std::isfinite(value); };
        if (!finite(config.targetRatio) || config.targetRatio <= 0.0 || config.targetRatio >= 1.0 ||
            !finite(config.tolerancePercent) || config.tolerancePercent < 0.0 || config.tolerancePercent >= 100.0 ||
            (config.lowTolerancePercent && (!finite(*config.lowTolerancePercent) || *config.lowTolerancePercent < 0.0 || *config.lowTolerancePercent >= 100.0)) ||
            !finite(config.factor) || config.factor < 0.0 || config.factor > 1.0)
            throw std::invalid_argument("Invalid search target, tolerance or factor.");
        if (config.loadResistance && (!finite(*config.loadResistance) || *config.loadResistance <= 0.0))
            throw std::invalid_argument("Invalid load resistance.");
        if (config.maxErrorPercent && (!finite(*config.maxErrorPercent) || *config.maxErrorPercent < 0.0))
            throw std::invalid_argument("Invalid maximum error.");
        if (config.vin.has_value() != config.vout.has_value() ||
            (config.vin && (!finite(*config.vin) || !finite(*config.vout) || *config.vout <= 0.0 || *config.vin <= *config.vout)))
            throw std::invalid_argument("Invalid voltage pair.");
        for (const Range* range : {&config.seriesRange, &config.parallelRange, &config.highRange, &config.lowRange, &config.powerRange})
        {
            if (range->enabled && (!finite(range->min) || !finite(range->max) || range->min > range->max ||
                range->min < 0.0 || (range != &config.powerRange && range->min == 0.0)))
                throw std::invalid_argument("Invalid search range.");
        }
        const auto prepare = [&finite](const std::vector<double>& source)
        {
            std::vector<double> prepared = source;
            for (double value : prepared)
                if (!finite(value) || value <= 0.0 || value > 1.0e150)
                    throw std::invalid_argument("Invalid resistor value.");
            std::sort(prepared.begin(), prepared.end());
            prepared.erase(std::unique(prepared.begin(), prepared.end()), prepared.end());
            return prepared;
        };
        const std::vector<double> highSorted = prepare(highValues);
        const std::vector<double> values = prepare(lowValues);
        const double highTolerancePercent = config.tolerancePercent;
        const double lowTolerancePercent = config.lowTolerancePercent.value_or(config.tolerancePercent);

        std::priority_queue<DividerCandidate, std::vector<DividerCandidate>, WorstFirst> best;
        for (double rHigh : highSorted)
        {
            if (!config.highRange.contains(rHigh)) continue;
            auto first = values.begin();
            auto last = values.end();
            // Each expression is monotonic in the lower resistor. Binary searches
            // discard impossible intervals without approximating the ranking.
            const auto restrictRange = [&](const Range& range, const auto& expression)
            {
                if (!range.enabled) return;
                first = std::partition_point(first, last, [&](double low) { return expression(low) < range.min; });
                last = std::partition_point(first, last, [&](double low) { return expression(low) <= range.max; });
            };
            restrictRange(config.lowRange, [](double low) { return low; });
            restrictRange(config.seriesRange, [rHigh](double low) { return rHigh + low; });
            restrictRange(config.parallelRange, [rHigh](double low) { return parallelResistance(rHigh, low); });
            if (config.powerRange.enabled && !config.loadResistance)
            {
                const auto power = [&](double low) { return *config.vin * (*config.vin / (rHigh + low)); };
                first = std::partition_point(first, last, [&](double low) { return power(low) > config.powerRange.max; });
                last = std::partition_point(first, last, [&](double low) { return power(low) >= config.powerRange.min; });
            }
            if (config.maxErrorPercent)
            {
                const double highTolerance = highTolerancePercent / 100.0;
                const double lowTolerance = lowTolerancePercent / 100.0;
                const auto ratio = [&](double high, double low)
                {
                    const double bottom = config.loadResistance ? parallelResistance(low, *config.loadResistance) : low;
                    return bottom / (high + bottom);
                };
                // Both extreme ratios increase monotonically with the lower resistor.
                first = std::partition_point(first, last, [&](double low) {
                    return ((ratio(rHigh * (1.0 + highTolerance), low * (1.0 - lowTolerance)) - config.targetRatio) / config.targetRatio) * 100.0 < -*config.maxErrorPercent;
                });
                last = std::partition_point(first, last, [&](double low) {
                    return ((ratio(rHigh * (1.0 - highTolerance), low * (1.0 + lowTolerance)) - config.targetRatio) / config.targetRatio) * 100.0 <= *config.maxErrorPercent;
                });
            }
            for (auto it = first; it != last; ++it)
            {
                DividerCandidate candidate = makeCandidate(rHigh, *it, config.targetRatio, highTolerancePercent, lowTolerancePercent, config.factor, config.vin, config.vout, config.loadResistance);
                if (!config.powerRange.contains(candidate.power)) continue;
                if (config.ratedPower && std::max(candidate.powerHighMax, candidate.powerLowMax) > *config.ratedPower) continue;
                if (config.maxErrorPercent && std::max(std::abs(candidate.lowRatioErrorPercent), std::abs(candidate.highRatioErrorPercent)) > *config.maxErrorPercent) continue;
                if (!finite(candidate.ratioErrorScorePercent))
                    throw std::invalid_argument("Error calculation out of range; increase the target ratio.");

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

        while (!best.empty())
        {
            results.push_back(best.top());
            best.pop();
        }

        std::sort(results.begin(), results.end(), betterCandidate);
        return results;
    }

} // namespace divider
