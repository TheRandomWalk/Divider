#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace divider
{

    struct Range
    {
        bool enabled = false;
        double min = 0.0;
        double max = 0.0;

        bool contains(double value) const;
    };

    struct SearchConfig
    {
        double targetRatio = 0.0;
        double tolerancePercent = 0.0; // Upper resistor tolerance; also the lower one unless lowTolerancePercent is set.
        std::optional<double> lowTolerancePercent;
        double factor = 0.5;
        std::size_t top = 10;
        Range seriesRange;
        Range parallelRange;
        Range powerRange;
        Range highRange;
        Range lowRange;
        std::optional<double> maxErrorPercent;
        std::optional<double> loadResistance;
        std::optional<double> ratedPower; // Maximum worst-case power allowed in each resistor; requires VIN.
        std::optional<double> vin;
        std::optional<double> vout;
    };

    struct DividerCandidate
    {
        double rHigh = 0.0;
        double rLow = 0.0;
        double rSeries = 0.0;
        double rParallel = 0.0;
        double ratio = 0.0;
        double ratioLow = 0.0;
        double ratioHigh = 0.0;
        double nominalRatioErrorPercent = 0.0;
        double lowRatioErrorPercent = 0.0;
        double highRatioErrorPercent = 0.0;
        double ratioErrorScorePercent = 0.0;
        double nominalVoltageError = 0.0;
        double lowVoltageError = 0.0;
        double highVoltageError = 0.0;
        double power = 0.0; // Total power in the two divider resistors, excluding the load.
        double powerHigh = 0.0;
        double powerLow = 0.0;
        double powerHighMax = 0.0; // Worst-case power in the upper resistor over both tolerances.
        double powerLowMax = 0.0;  // Worst-case power in the lower resistor over both tolerances.
        double voutMin = 0.0;
        double voutMax = 0.0;
    };

    std::vector<DividerCandidate> findBestCandidates(const std::vector<double>& resistorValues, const SearchConfig& config);
    std::vector<DividerCandidate> findBestCandidates(const std::vector<double>& highValues, const std::vector<double>& lowValues, const SearchConfig& config);

} // namespace divider
