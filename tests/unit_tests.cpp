#include "divider/cli.h"
#include "divider/e_series.h"
#include "divider/search.h"
#include "divider/si.h"
#include "divider/table.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    struct TestCase
    {
        std::string_view name;
        void (*run)();
    };

    void expectNear(double actual, double expected, double tolerance = 1.0e-12)
    {
        if (!std::isfinite(actual) || !std::isfinite(expected) || std::abs(actual - expected) > tolerance * std::max(1.0, std::abs(expected)))
        {
            std::ostringstream error;
            error << "Expected " << actual << " to be near " << expected;
            throw std::runtime_error(error.str());
        }
    }

    void expectTrue(bool condition, const std::string& message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    void expectEqual(const std::string& actual, const std::string& expected)
    {
        if (actual != expected)
        {
            std::ostringstream error;
            error << "Expected '" << actual << "' to equal '" << expected << "'";
            throw std::runtime_error(error.str());
        }
    }

    void expectThrows(const std::function<void()>& action, const std::string& expectedMessage)
    {
        try
        {
            action();
        }
        catch (const std::exception& error)
        {
            const std::string message = error.what();
            if (!expectedMessage.empty() && message.find(expectedMessage) == std::string::npos)
            {
                std::ostringstream failure;
                failure << "Expected exception containing '" << expectedMessage << "', got '" << message << "'";
                throw std::runtime_error(failure.str());
            }
            return;
        }

        throw std::runtime_error("Expected exception containing '" + expectedMessage + "'.");
    }

    divider::Options parseArgs(std::initializer_list<const char*> args)
    {
        std::vector<char*> argv;
        argv.reserve(args.size());
        for (const char* arg : args)
        {
            argv.push_back(const_cast<char*>(arg));
        }
        return divider::parseCommandLine(static_cast<int>(argv.size()), argv.data());
    }

    void testSiParsing()
    {
        expectNear(divider::parseSiValue("10u"), 10.0e-6);
        expectNear(divider::parseSiValue("10U"), 10.0e-6);
        expectNear(divider::parseSiValue("10\xC2\xB5"), 10.0e-6);
        expectNear(divider::parseSiValue("10\xCE\xBC"), 10.0e-6);
        expectNear(divider::parseSiValue("10m"), 10.0e-3);
        expectNear(divider::parseSiValue(" 4.7 k "), 4.7e3);
        expectNear(divider::parseSiValue("2.5"), 2.5);
        expectNear(divider::parseSiValue("10k"), 10.0e3);
        expectNear(divider::parseSiValue("10K"), 10.0e3);
        expectNear(divider::parseSiValue("10M"), 10.0e6);
        expectNear(divider::parseSiValue("10G"), 10.0e9);
        expectNear(divider::parseSiValue("10g"), 10.0e9);
        expectNear(divider::parseSiValue("10T"), 10.0e12);
        expectNear(divider::parseSiValue("10t"), 10.0e12);
        expectTrue(divider::parseSiValue("1m") != divider::parseSiValue("1M"), "Expected m and M to stay distinct.");
    }

    void testSiParsingRejectsInvalidInput()
    {
        expectThrows(
            []
            {
                divider::parseSiValue("");
            },
            "Empty SI value");
        expectThrows(
            []
            {
                divider::parseSiValue("abc");
            },
            "Invalid numeric value");
        expectThrows(
            []
            {
                divider::parseSiValue("inf");
            },
            "Invalid numeric value");
        expectThrows(
            []
            {
                divider::parseSiValue("1n");
            },
            "Unknown SI prefix");
        expectThrows(
            []
            {
                divider::parseSiValue("1MM");
            },
            "Unknown SI prefix");
    }

    void testSiFormatting()
    {
        expectEqual(divider::formatSi(0.00123, "W", 3), "1.230 mW");
        expectEqual(divider::formatSi(0.00123, "", 3), "1.230 m");
        expectEqual(divider::formatSi(123.0, "", 3), "123.000  ");
        expectEqual(divider::formatSi(1234.0, "", 1), "1.2 k");
        expectEqual(divider::formatSiWithScale(1.0e9, divider::SiScale{1.0e6, "M"}, "", 0), "1,000 M");
        expectEqual(divider::formatSi(0.0, "", 3), "0.000  ");
        expectEqual(divider::formatSi(1234.0, "ohm", 2), "1.23 kohm");
        expectEqual(divider::formatSi(1.0e6, "ohm", 3), "1.000 Mohm");
        expectEqual(divider::formatSignedDecimal(0.0004, 3), "0.000");
        expectEqual(divider::formatSignedDecimal(-0.0004, 3), "0.000");
        expectEqual(divider::formatSignedDecimal(0.0006, 3), "+0.001");
        expectEqual(divider::formatSignedDecimal(-0.0006, 3), "-0.001");
        expectEqual(divider::formatSignedDecimal(12345.6, 1), "+12,345.6");
        expectEqual(divider::formatSignedDecimal(-12345.6, 1), "-12,345.6");
        expectEqual(divider::formatSiWithScale(-0.00123, divider::SiScale{1.0e-3, "m"}, "", 2), "-1.23 m");
        expectEqual(divider::formatSi(std::numeric_limits<double>::infinity(), "", 3), "N/A");
    }

    void testSeriesGeneration()
    {
        const auto e24Base = divider::generateBaseValues(24);
        expectTrue(e24Base.size() == 24, "E24 base size should be 24.");
        expectNear(e24Base[10], 2.7);

        const auto e192 = divider::generateSeriesValues(192);
        expectTrue(!e192.empty(), "E192 should not be empty.");
        expectNear(e192.front(), 1.0e-5);
        expectNear(e192.back(), 1.0e13);
    }

    void testSeriesDefinitionsAndBounds()
    {
        const auto& series = divider::allSeries();
        expectTrue(series.size() == 7, "Expected seven supported E series.");
        expectTrue(series.front().valueCount == 3, "Expected E3 first.");
        expectTrue(series.front().defaultTolerancePercent == 40.0, "Expected E3 default tolerance.");
        expectTrue(series.back().valueCount == 192, "Expected E192 last.");
        expectNear(series.back().defaultTolerancePercent, 0.5);

        for (const auto& definition : series)
        {
            const auto base = divider::generateBaseValues(definition.valueCount);
            expectTrue(base.size() == static_cast<std::size_t>(definition.valueCount), "Expected base table size to match series.");
            expectNear(base.front(), 1.0);
            expectTrue(base.back() < 10.0, "Expected base table to stay below the next decade.");

            const auto values = divider::generateSeriesValues(definition.valueCount);
            expectTrue(!values.empty(), "Expected generated values.");
            expectNear(values.front(), 1.0e-5);
            expectNear(values.back(), 1.0e13);
            for (std::size_t index = 1; index < values.size(); ++index)
            {
                expectTrue(values[index] > values[index - 1], "Expected generated resistor values to be strictly sorted.");
            }
        }

        expectThrows(
            []
            {
                divider::generateBaseValues(5);
            },
            "Unsupported E series");
    }

    void testRangeContains()
    {
        divider::Range disabled;
        expectTrue(disabled.contains(-1.0), "Expected disabled range to accept any value.");

        const divider::Range range{true, 10.0, 20.0};
        expectTrue(range.contains(10.0), "Expected range to include minimum.");
        expectTrue(range.contains(15.0), "Expected range to include middle.");
        expectTrue(range.contains(20.0), "Expected range to include maximum.");
        expectTrue(!range.contains(9.999), "Expected range to reject below minimum.");
        expectTrue(!range.contains(20.001), "Expected range to reject above maximum.");
    }

    void testHalfRatioSearch()
    {
        const std::vector<double> values{1000.0, 2200.0};
        divider::SearchConfig config;
        config.targetRatio = 0.5;
        config.tolerancePercent = 40.0;
        config.top = 1;

        const auto candidates = divider::findBestCandidates(values, config);
        expectTrue(candidates.size() == 1, "Expected one half-ratio candidate.");
        expectNear(candidates.front().rHigh, candidates.front().rLow);
        expectNear(candidates.front().nominalRatioErrorPercent, 0.0);
        expectNear(candidates.front().lowRatioErrorPercent, -40.0);
        expectNear(candidates.front().highRatioErrorPercent, 40.0);
    }

    void testTopLimitAndTieBreakers()
    {
        const std::vector<double> values{1000.0, 2000.0, 3000.0};
        divider::SearchConfig config;
        config.targetRatio = 0.5;
        config.tolerancePercent = 1.0;
        config.top = 2;

        const auto candidates = divider::findBestCandidates(values, config);
        expectTrue(candidates.size() == 2, "Expected top limit to truncate results.");
        expectNear(candidates[0].rHigh, 1000.0);
        expectNear(candidates[0].rLow, 1000.0);
        expectNear(candidates[1].rHigh, 2000.0);
        expectNear(candidates[1].rLow, 2000.0);
    }

    void testSearchFilters()
    {
        const std::vector<double> values{1000.0, 2000.0, 3000.0};
        divider::SearchConfig config;
        config.targetRatio = 0.5;
        config.tolerancePercent = 1.0;
        config.top = 10;
        config.seriesRange = divider::Range{true, 4000.0, 4000.0};

        auto candidates = divider::findBestCandidates(values, config);
        expectTrue(!candidates.empty(), "Expected series-filtered candidates.");
        for (const auto& candidate : candidates)
        {
            expectNear(candidate.rSeries, 4000.0);
        }

        config.seriesRange = divider::Range{};
        config.parallelRange = divider::Range{true, 650.0, 700.0};
        candidates = divider::findBestCandidates(values, config);
        expectTrue(!candidates.empty(), "Expected parallel-filtered candidates.");
        for (const auto& candidate : candidates)
        {
            expectTrue(candidate.rParallel >= 650.0 && candidate.rParallel <= 700.0, "Expected parallel range to be applied.");
        }
    }

    void testPowerFilteringAndTopZero()
    {
        const std::vector<double> values{1000.0, 2000.0};
        divider::SearchConfig config;
        config.targetRatio = 0.5;
        config.tolerancePercent = 1.0;
        config.top = 10;
        config.vin = 10.0;
        config.vout = 5.0;
        config.powerRange = divider::Range{true, 0.04, 0.06};

        const auto candidates = divider::findBestCandidates(values, config);
        expectTrue(candidates.size() == 1, "Expected power filter to keep only the 1k/1k divider.");
        expectNear(candidates.front().rHigh, 1000.0);
        expectNear(candidates.front().rLow, 1000.0);
        expectNear(candidates.front().power, 0.05);

        config.top = 0;
        expectTrue(divider::findBestCandidates(values, config).empty(), "Expected top zero to return no candidates.");

        divider::SearchConfig invalidPowerConfig;
        invalidPowerConfig.targetRatio = 0.5;
        invalidPowerConfig.tolerancePercent = 1.0;
        invalidPowerConfig.powerRange = divider::Range{true, 0.0, 1.0};
        expectThrows(
            [&]
            {
                divider::findBestCandidates(values, invalidPowerConfig);
            },
            "Power filtering requires VIN");
    }

    void testVoltageColumnsAndPower()
    {
        const std::vector<double> values{6800.0, 10000.0, 22000.0};
        divider::SearchConfig config;
        config.targetRatio = 3.3 / 12.0;
        config.tolerancePercent = 1.0;
        config.top = 2;
        config.vin = 12.0;
        config.vout = 3.3;
        config.powerRange = divider::Range{true, 0.001, 0.1};

        const auto candidates = divider::findBestCandidates(values, config);
        expectTrue(!candidates.empty(), "Expected voltage candidates.");
        expectTrue(candidates.front().power > 0.001, "Expected power above lower filter limit.");

        std::ostringstream output;
        divider::printSeriesTable(output, "E-test", 96, 1.0, candidates, 3, true);
        const std::string text = output.str();
        expectTrue(text.find("\xCE\xA9") != std::string::npos, "Expected omega symbol in resistance columns.");
        expectTrue(text.find("parallel (\xCE\xA9)") < text.find("power (W)") && text.find("power (W)") < text.find("nom error (%)") && text.find("nom error (%)") < text.find("error range (%)"),
                   "Expected power column before percentage errors and nominal error first.");
        expectTrue(text.find("nom error (%)") != std::string::npos, "Expected nominal percentage error column.");
        expectTrue(text.find("error range (%)") != std::string::npos, "Expected percentage error range column.");
        expectTrue(text.find("nom error (V)") != std::string::npos, "Expected nominal voltage error column.");
        expectTrue(text.find("error range (V)") != std::string::npos, "Expected voltage error range column.");
        expectTrue(text.find("low error (V)") == std::string::npos, "Expected no low voltage error column.");
        expectTrue(text.find("high error (V)") == std::string::npos, "Expected no high voltage error column.");
        expectTrue(text.find("power (W)") != std::string::npos, "Expected power column.");
        expectTrue(text.find("ohm") == std::string::npos, "Expected no ohm suffix in values.");
    }

    void testRatioModeOmitsVoltageColumns()
    {
        const std::vector<double> values{1000.0};
        divider::SearchConfig config;
        config.targetRatio = 0.5;
        config.tolerancePercent = 1.0;
        config.top = 1;

        const auto candidates = divider::findBestCandidates(values, config);
        std::ostringstream output;
        divider::printSeriesTable(output, "E-test", 96, 1.0, candidates, 3, false);
        const std::string text = output.str();
        expectTrue(text.find("error range (V)") == std::string::npos, "Expected no voltage columns in ratio mode.");
        expectTrue(text.find("power (W)") == std::string::npos, "Expected no power column in ratio mode.");
        expectTrue(text.find("nom error (%)") != std::string::npos, "Expected nominal percentage error in ratio mode.");
        expectTrue(text.find("error range (%)") != std::string::npos, "Expected percentage error range in ratio mode.");
    }

    void testE3WorstCaseErrors()
    {
        const std::vector<double> values{1000.0};
        divider::SearchConfig config;
        config.targetRatio = 0.5;
        config.tolerancePercent = 40.0;
        config.top = 1;
        config.vin = 10.0;
        config.vout = 5.0;

        const auto candidates = divider::findBestCandidates(values, config);
        expectTrue(candidates.size() == 1, "Expected one E3 worst-case candidate.");

        expectNear(candidates.front().lowRatioErrorPercent, -40.0);
        expectNear(candidates.front().highRatioErrorPercent, 40.0);
        expectNear(candidates.front().lowVoltageError, -2.0);
        expectNear(candidates.front().highVoltageError, 2.0);
    }

    void testAverageErrorRanking()
    {
        const std::vector<double> values{22.0, 39.0, 220.0, 330.0};
        divider::SearchConfig config;
        config.targetRatio = 0.1;
        config.tolerancePercent = 5.0;
        config.top = 1;

        const auto candidates = divider::findBestCandidates(values, config);
        expectTrue(candidates.size() == 1, "Expected one average-error candidate.");
        expectNear(candidates.front().rHigh, 330.0);
        expectNear(candidates.front().rLow, 39.0);
        expectNear(candidates.front().ratioErrorScorePercent,
                   std::abs(candidates.front().nominalRatioErrorPercent) * config.factor + (std::abs(candidates.front().highRatioErrorPercent) + std::abs(candidates.front().lowRatioErrorPercent)) * (1.0 - config.factor));
    }

    void testFactorRanking()
    {
        const std::vector<double> values{22.0, 39.0, 220.0, 330.0};
        divider::SearchConfig config;
        config.targetRatio = 0.1;
        config.tolerancePercent = 5.0;
        config.factor = 0.0;
        config.top = 1;

        const auto candidates = divider::findBestCandidates(values, config);
        expectTrue(candidates.size() == 1, "Expected one factor-ranked candidate.");
        expectNear(candidates.front().rHigh, 220.0);
        expectNear(candidates.front().rLow, 22.0);
    }

    void testNominalOnlyRanking()
    {
        const std::vector<double> values{22.0, 39.0, 220.0, 330.0};
        divider::SearchConfig config;
        config.targetRatio = 0.1;
        config.tolerancePercent = 5.0;
        config.factor = 1.0;
        config.top = 1;

        const auto candidates = divider::findBestCandidates(values, config);
        expectTrue(candidates.size() == 1, "Expected one nominal-ranked candidate.");
        expectNear(candidates.front().rHigh, 330.0);
        expectNear(candidates.front().rLow, 39.0);
    }

    void testLowSeriesColumnDigits()
    {
        divider::DividerCandidate candidate;
        candidate.rHigh = 12000.0;
        candidate.rLow = 2200.0;
        candidate.rSeries = 14200.0;
        candidate.rParallel = 687.5;
        candidate.nominalRatioErrorPercent = 0.123;
        candidate.lowRatioErrorPercent = -1.234;
        candidate.highRatioErrorPercent = 2.345;
        candidate.nominalVoltageError = 0.00234;
        candidate.lowVoltageError = -0.00123;
        candidate.highVoltageError = 0.03456;
        candidate.power = 0.000789;

        std::ostringstream output;
        divider::printSeriesTable(output, "E24", 24, 5.0, {candidate}, 5, true);
        const std::string text = output.str();

        expectTrue(text.find("12 k") != std::string::npos, "Expected high column to use zero decimals for 10 to <1000.");
        expectTrue(text.find("2.2 k") != std::string::npos, "Expected low column to use one decimal.");
        expectTrue(text.find("14 k") != std::string::npos, "Expected series column to use the minimum-value column format.");
        expectTrue(text.find("688  ") != std::string::npos, "Expected parallel column to use the minimum-value column format.");
        expectTrue(text.find("789 u") != std::string::npos, "Expected power to use three significant digits from the column minimum.");
        expectTrue(text.find("+0.123") != std::string::npos, "Expected nominal percentage error column.");
        expectTrue(text.find("3.579") != std::string::npos, "Expected percentage error range to use three decimals.");
        expectTrue(text.find("+2.34 m") != std::string::npos, "Expected nominal voltage error column.");
        expectTrue(text.find("35.8 m") != std::string::npos, "Expected voltage error range column to use its own SI unit.");
    }

    void testVoltageColumnsUseIndependentTableUnits()
    {
        divider::DividerCandidate smallError;
        smallError.rHigh = 1000.0;
        smallError.rLow = 1000.0;
        smallError.rSeries = 2000.0;
        smallError.rParallel = 500.0;
        smallError.power = 0.000002;
        smallError.nominalVoltageError = 0.000002;
        smallError.lowVoltageError = -0.001;
        smallError.highVoltageError = 0.002;

        divider::DividerCandidate largeError = smallError;
        largeError.power = 0.002;
        largeError.nominalVoltageError = 0.002;
        largeError.lowVoltageError = -1.2;
        largeError.highVoltageError = 0.5;

        std::ostringstream output;
        divider::printSeriesTable(output, "E96", 96, 1.0, {smallError, largeError}, 3, true);
        const std::string text = output.str();

        expectTrue(text.find("2.00 u") != std::string::npos, "Expected small power to use table-wide uW scale.");
        expectTrue(text.find("2,000.00 u") != std::string::npos, "Expected large power to use the same table-wide uW scale.");
        expectTrue(text.find("+2.00 u") != std::string::npos, "Expected small nominal voltage to use table-wide uV scale.");
        expectTrue(text.find("+2,000.00 u") != std::string::npos, "Expected large nominal voltage to use the same table-wide uV scale.");
        expectTrue(text.find("3.00 m") != std::string::npos, "Expected small voltage range to use table-wide mV scale.");
        expectTrue(text.find("1,700.00 m") != std::string::npos, "Expected large voltage range to use the same table-wide mV scale.");
    }

    void testSeriesAndParallelUseMinimumColumnFormat()
    {
        divider::DividerCandidate candidate;
        candidate.rHigh = 150000.0;
        candidate.rLow = 30000.0;
        candidate.rSeries = 180000.0;
        candidate.rParallel = 25000.0;

        std::ostringstream output;
        divider::printSeriesTable(output, "E24", 24, 5.0, {candidate}, 3, false);
        const std::string text = output.str();

        expectTrue(text.find("180 k") != std::string::npos, "Expected series to use two significant digits from the E24 minimum.");
        expectTrue(text.find("25 k") != std::string::npos, "Expected parallel to use two significant digits from the E24 minimum.");
    }

    void testSeriesAndParallelUseColumnUnits()
    {
        divider::DividerCandidate largeRow;
        largeRow.rHigh = 1.0e9;
        largeRow.rLow = 100.0e6;
        largeRow.rSeries = 1.1e9;
        largeRow.rParallel = 90.9090909e6;

        divider::DividerCandidate smallerRow;
        smallerRow.rHigh = 100.0e6;
        smallerRow.rLow = 10.0e3;
        smallerRow.rSeries = 100010000.0;
        smallerRow.rParallel = 9999.0001;

        std::ostringstream output;
        divider::printSeriesTable(output, "E96", 96, 1.0, {largeRow, smallerRow}, 3, false);
        const std::string text = output.str();

        expectTrue(text.find("1,100 M") != std::string::npos, "Expected series column to use the minimum series row scale and decimals.");
        expectTrue(text.find("100 M") != std::string::npos, "Expected series values to share the M scale.");
        expectTrue(text.find("90,909.09 k") != std::string::npos, "Expected parallel column to use the minimum parallel row scale and decimals.");
        expectTrue(text.find("10.00 k") != std::string::npos, "Expected parallel values to share the k scale.");
    }

    void testZeroVoltageIgnoredForScale()
    {
        divider::DividerCandidate candidate;
        candidate.rHigh = 1000.0;
        candidate.rLow = 1000.0;
        candidate.rSeries = 2000.0;
        candidate.rParallel = 500.0;
        candidate.nominalVoltageError = 0.0;
        candidate.lowVoltageError = -2.0;
        candidate.highVoltageError = 3.0;

        std::ostringstream output;
        divider::printSeriesTable(output, "E96", 96, 1.0, {candidate}, 3, true);
        const std::string text = output.str();

        expectTrue(text.find("0.00  ") != std::string::npos, "Expected zero nominal voltage to print without an SI prefix.");
        expectTrue(text.find("5.00  ") != std::string::npos, "Expected voltage error range to print in the non-zero error scale.");
        expectTrue(text.find("+0.000") == std::string::npos, "Expected zero voltage to omit the sign.");
        expectTrue(text.find("+0.000 m") == std::string::npos, "Expected zero voltage not to force mV scale.");
    }

    void testNearZeroVoltageDoesNotForceFemtoScale()
    {
        divider::DividerCandidate candidate;
        candidate.rHigh = 34800.0;
        candidate.rLow = 13200.0;
        candidate.rSeries = 48000.0;
        candidate.rParallel = 9570.0;
        candidate.nominalVoltageError = 4.440892098500626e-16;
        candidate.lowVoltageError = -0.02387128959840279;
        candidate.highVoltageError = 0.02397895264344729;

        std::ostringstream output;
        divider::printSeriesTable(output, "E192", 192, 0.5, {candidate}, 3, true);
        const std::string text = output.str();

        expectTrue(text.find(" f") == std::string::npos, "Expected near-zero voltage noise not to force femto scale.");
        expectTrue(text.find("0.00  ") != std::string::npos, "Expected near-zero nominal voltage to print as zero without an SI prefix.");
        expectTrue(text.find("+0.00 m") == std::string::npos, "Expected near-zero nominal voltage to omit the sign.");
        expectTrue(text.find("47.9 m") != std::string::npos, "Expected voltage error range to remain in mV scale.");
    }

    void testHighSeriesColumnDigits()
    {
        divider::DividerCandidate candidate;
        candidate.rHigh = 1000.0;
        candidate.rLow = 12000.0;
        candidate.rSeries = 13000.0;
        candidate.rParallel = 923.0;
        candidate.lowRatioErrorPercent = -1.23456;
        candidate.highRatioErrorPercent = 2.34567;

        std::ostringstream output;
        divider::printSeriesTable(output, "E96", 96, 1.0, {candidate}, 5, false);
        const std::string text = output.str();

        expectTrue(text.find("1.00 k") != std::string::npos, "Expected E48+ mantissas from 1 to <10 to use two decimals.");
        expectTrue(text.find("12.0 k") != std::string::npos, "Expected E48+ mantissas from 10 to <100 to use one decimal.");
        expectTrue(text.find("13.0 k") != std::string::npos, "Expected series to use three significant digits for E96.");
        expectTrue(text.find("923  ") != std::string::npos, "Expected parallel to use three significant digits for E96.");
        expectTrue(text.find("3.580") != std::string::npos, "Expected percentage error range to use three decimals.");
    }

    void testHighAndLowMayUseDifferentUnits()
    {
        divider::DividerCandidate candidate;
        candidate.rHigh = 1.0e9;
        candidate.rLow = 100.0e6;
        candidate.rSeries = 1.1e9;
        candidate.rParallel = 90.9090909e6;

        std::ostringstream output;
        divider::printSeriesTable(output, "E96", 96, 1.0, {candidate}, 3, false);
        const std::string text = output.str();

        expectTrue(text.find("1.00 G") != std::string::npos, "Expected high value to use its own G scale.");
        expectTrue(text.find("100 M") != std::string::npos, "Expected low value to use M scale.");
        expectTrue(text.find("1.10 G") != std::string::npos, "Expected series to use three significant digits for E96.");
        expectTrue(text.find("90.9 M") != std::string::npos, "Expected parallel to use three significant digits for E96.");
    }

    void testEmptyTableOutput()
    {
        std::ostringstream output;
        divider::printSeriesTable(output, "E-test", 24, 5.0, {}, 3, true);
        const std::string text = output.str();

        expectTrue(text.find("E-test (tolerance +/-5%)") != std::string::npos, "Expected table title for empty results.");
        expectTrue(text.find("No candidates found.") != std::string::npos, "Expected empty table message.");
        expectTrue(text.find("top (\xCE\xA9)") == std::string::npos, "Expected no table header for empty results.");
    }

    void testUsageDocumentsImportantOptions()
    {
        const std::string text = divider::usage("divider");
        expectTrue(text.find("--top N") != std::string::npos, "Expected --top in usage.");
        expectTrue(text.find("--digits N") != std::string::npos, "Expected --digits in usage.");
        expectTrue(text.find("--factor F") != std::string::npos, "Expected --factor in usage.");
        expectTrue(text.find("score = abs(nominal error) * F") != std::string::npos, "Expected score formula in usage.");
        expectTrue(text.find("F=1 ranks only by nominal error") != std::string::npos, "Expected factor explanation in usage.");
        expectTrue(text.find("--E LIST") != std::string::npos, "Expected --E selector in usage.");
        expectTrue(text.find("--power MIN MAX") != std::string::npos, "Expected --power in usage.");
    }

    void testLoadedDividerAndLimits()
    {
        divider::SearchConfig config;
        config.targetRatio = 1.0 / 3.0;
        config.vin = 12.0;
        config.vout = 4.0;
        config.loadResistance = 1000.0;
        config.top = 10;
        auto result = divider::findBestCandidates({1000.0}, config);
        expectTrue(result.size() == 1, "Expected loaded divider.");
        const auto& c = result.front();
        expectNear(c.ratio, 1.0 / 3.0);
        expectNear(c.voutMin, 4.0);
        expectNear(c.voutMax, 4.0);
        expectNear(c.powerHigh, 0.064);
        expectNear(c.powerLow, 0.016);
        expectNear(c.power, 0.080); // The fixed load dissipates a separate 0.016 W.
        config.tolerancePercent = 10.0;
        result = divider::findBestCandidates({1000.0}, config);
        expectNear(result.front().voutMin, 12.0 * 900.0 / 2990.0);
        expectNear(result.front().voutMax, 12.0 * 1100.0 / 2990.0);
        config.maxErrorPercent = 10.0;
        expectTrue(divider::findBestCandidates({1000.0}, config).empty(), "Expected worst-case error rejection.");
        config.maxErrorPercent = 11.0;
        expectTrue(!divider::findBestCandidates({1000.0}, config).empty(), "Expected worst-case error acceptance.");
        config.powerRange = {true, 0.079, 0.081};
        expectTrue(!divider::findBestCandidates({1000.0}, config).empty(), "Expected loaded resistor power filter.");
        config.powerRange = {true, 0.09, 0.1};
        expectTrue(divider::findBestCandidates({1000.0}, config).empty(), "Power filter must exclude load dissipation.");
    }

    void testOptimizedSearchMatchesExhaustive()
    {
        const std::vector<double> values{10000, 100, 470, 1000, 2200, 3300, 6800};
        for (bool loaded : {false, true})
        for (double ratio : {0.1, 0.275, 0.5, 0.9})
        for (double tolerance : {0.0, 1.0, 40.0})
        {
            divider::SearchConfig config;
            config.targetRatio = ratio;
            config.vin = 12.0;
            config.vout = 12.0 * ratio;
            config.tolerancePercent = tolerance;
            config.top = 100;
            if (loaded) config.loadResistance = 4700.0;
            const auto exhaustive = divider::findBestCandidates(values, config);
            for (int filter = 0; filter < 6; ++filter)
            {
                auto filtered = config;
                if (filter == 0 || filter == 5) filtered.highRange = {true, 470, 6800};
                if (filter == 1 || filter == 5) filtered.lowRange = {true, 1000, 10000};
                if (filter == 2 || filter == 5) filtered.seriesRange = {true, 1470, 12000};
                if (filter == 3 || filter == 5) filtered.parallelRange = {true, 300, 3000};
                if (filter == 4 || filter == 5) filtered.powerRange = {true, 0.01, 0.1};
                filtered.maxErrorPercent = 50.0;
                filtered.top = 5;
                std::vector<divider::DividerCandidate> expected;
                for (const auto& c : exhaustive)
                {
                    if (filtered.highRange.contains(c.rHigh) && filtered.lowRange.contains(c.rLow) &&
                        filtered.seriesRange.contains(c.rSeries) && filtered.parallelRange.contains(c.rParallel) &&
                        filtered.powerRange.contains(c.power) &&
                        std::max(std::abs(c.lowRatioErrorPercent), std::abs(c.highRatioErrorPercent)) <= *filtered.maxErrorPercent)
                        expected.push_back(c);
                }
                if (expected.size() > filtered.top) expected.resize(filtered.top);
                const auto actual = divider::findBestCandidates(values, filtered);
                expectTrue(actual.size() == expected.size(), "Optimized search lost candidates.");
                for (std::size_t i = 0; i < actual.size(); ++i)
                {
                    if (actual[i].rHigh != expected[i].rHigh || actual[i].rLow != expected[i].rLow)
                        std::cerr << "Mismatch load=" << loaded << " ratio=" << ratio << " tolerance=" << tolerance << " filter=" << filter << " index=" << i << " actual=" << actual[i].rHigh << "/" << actual[i].rLow << " expected=" << expected[i].rHigh << "/" << expected[i].rLow << " scores=" << actual[i].ratioErrorScorePercent << "/" << expected[i].ratioErrorScorePercent << "\n";
                    expectNear(actual[i].rHigh, expected[i].rHigh);
                    expectNear(actual[i].rLow, expected[i].rLow);
                }
            }
        }
    }

    void testNewOptionsAndNumericValidation()
    {
        const auto options = parseArgs({"divider", "--ratio", "0.25", "--high", "1k", "10k", "--low", "100", "2k", "--load", "100k", "--max-error", "1", "--digits", "0"});
        expectNear(options.highRange.min, 1000);
        expectNear(options.lowRange.max, 2000);
        expectNear(*options.loadResistance, 100000);
        expectNear(*options.maxErrorPercent, 1);
        expectTrue(options.digitsProvided && options.digits == 0, "Expected explicit precision.");
        for (const char* value : {"nan", "inf", "-inf"})
            expectThrows([&] { parseArgs({"divider", "--ratio", "0.5", "--E96", value}); }, "Invalid tolerance");
        for (const char* value : {"1e300T", "1e-320u", "1e-999", "1e999"})
            expectThrows([&] { divider::parseSiValue(value); }, "");
        expectThrows([] { parseArgs({"divider", "--ratio", "0.5", "--load", "0"}); }, "positive");
        expectThrows([] { parseArgs({"divider", "--ratio", "0.5", "--max-error", "-1"}); }, "non-negative");
        expectThrows([] { parseArgs({"divider", "--ratio", "0.5", "--high", "10k", "1k"}); }, "Minimum");
        expectThrows([] { parseArgs({"divider", "--ratio", "0.5", "--low", "0", "1k"}); }, "positive");
    }

    void testExplicitDigitsAndNewColumns()
    {
        divider::SearchConfig config;
        config.targetRatio = 0.5;
        config.vin = 10;
        config.vout = 5;
        config.tolerancePercent = 1;
        const auto candidates = divider::findBestCandidates({1000}, config);
        for (int digits : {0, 2, 9})
        {
            std::ostringstream output;
            divider::printSeriesTable(output, "E96", 96, 1, candidates, digits, true, true);
            const auto text = output.str();
            expectTrue(text.find(divider::formatSi(1000, "", digits)) != std::string::npos, "Expected explicit resistor precision.");
            expectTrue(text.find(divider::formatDecimal(2, digits)) != std::string::npos, "Expected explicit error precision.");
            for (const char* header : {"VOUT min (V)", "VOUT max (V)", "top power (W)", "bottom power (W)"})
                expectTrue(text.find(header) != std::string::npos, "Missing new voltage/power column.");
        }
    }

    void testInclusiveErrorAndPowerLimits()
    {
        divider::SearchConfig config;
        config.targetRatio = 0.5;
        config.tolerancePercent = 40;
        config.maxErrorPercent = 40;
        config.vin = 10;
        config.vout = 5;
        config.powerRange = {true, 0.05, 0.05};
        config.highRange = {true, 1000, 1000};
        config.lowRange = {true, 1000, 1000};
        expectTrue(divider::findBestCandidates({2000, 1000, 1000}, config).size() == 1,
                   "Expected inclusive boundaries and deduplicated resistor values.");
        config.maxErrorPercent = 39.999;
        expectTrue(divider::findBestCandidates({1000}, config).empty(), "Expected error outside limit to be rejected.");
    }

    void testCliParsing()
    {
        const char* argv[] = {
            "divider", "--vin-vout", "12", "3.3", "--top", "5", "--digits", "4", "--factor", "0.25", "--series", "1k", "1M", "--parallel", "100", "100k", "--power", "0", "100m", "--E192", "0.01",
        };
        const auto options = divider::parseCommandLine(static_cast<int>(std::size(argv)), const_cast<char**>(argv));

        expectTrue(options.useVoltage, "Expected voltage mode.");
        expectTrue(options.top == 5, "Expected --top 5.");
        expectTrue(options.digits == 4, "Expected --digits 4.");
        expectNear(options.factor, 0.25);
        expectNear(options.ratio, 3.3 / 12.0);
        expectNear(options.seriesRange.min, 1000.0);
        expectNear(options.seriesRange.max, 1.0e6);
        expectNear(options.powerRange.min, 0.0);
        expectNear(options.powerRange.max, 0.1);
        expectNear(options.toleranceOverridesPercent.at(192), 0.01);
    }

    void testCliDefaultsAndHelp()
    {
        auto options = parseArgs({"divider", "--ratio", "0.25"});
        expectTrue(!options.help, "Expected normal parse not to be help.");
        expectTrue(!options.useVoltage, "Expected ratio mode.");
        expectNear(options.ratio, 0.25);
        expectTrue(options.top == 10, "Expected default top.");
        expectTrue(options.digits == 3, "Expected default digits.");
        expectNear(options.factor, 0.5);
        expectTrue(options.selectedSeries.empty(), "Expected all series by default.");

        options = parseArgs({"divider", "--help"});
        expectTrue(options.help, "Expected help option.");
    }

    void testCliSeriesSelection()
    {
        const char* argv[] = {
            "divider", "--ratio", "0.5", "--E", "96", "192", "--top", "1",
        };
        const auto options = divider::parseCommandLine(static_cast<int>(std::size(argv)), const_cast<char**>(argv));

        expectTrue(options.selectedSeries.size() == 2, "Expected two selected E series.");
        expectTrue(options.selectedSeries[0] == 96, "Expected E96 first.");
        expectTrue(options.selectedSeries[1] == 192, "Expected E192 second.");
    }

    void testCliSeriesSelectionAcceptsNamesAndDeduplicates()
    {
        const auto options = parseArgs({"divider", "--ratio", "0.5", "--E", "E24", "e96", "96", "3"});
        expectTrue(options.selectedSeries.size() == 3, "Expected duplicate selected series to be removed.");
        expectTrue(options.selectedSeries[0] == 24, "Expected E24 first.");
        expectTrue(options.selectedSeries[1] == 96, "Expected E96 second.");
        expectTrue(options.selectedSeries[2] == 3, "Expected E3 third.");
    }

    void testCliValidationErrors()
    {
        expectThrows(
            []
            {
                parseArgs({"divider"});
            },
            "Missing target");
        expectThrows(
            []
            {
                parseArgs({"divider", "--ratio", "0"});
            },
            "Invalid ratio");
        expectThrows(
            []
            {
                parseArgs({"divider", "--ratio", "1"});
            },
            "Invalid ratio");
        expectThrows(
            []
            {
                parseArgs({"divider", "--ratio", "0.5", "--vin-vout", "5", "1"});
            },
            "Use exactly one target mode");
        expectThrows(
            []
            {
                parseArgs({"divider", "--vin-vout", "5", "5"});
            },
            "Invalid voltage pair");
        expectThrows(
            []
            {
                parseArgs({"divider", "--vin-vout", "5", "0"});
            },
            "Invalid voltage pair");
        expectThrows(
            []
            {
                parseArgs({"divider", "--ratio", "0.5", "--power", "0", "1m"});
            },
            "--power is only valid");
        expectThrows(
            []
            {
                parseArgs({"divider", "--ratio", "0.5", "--top", "0"});
            },
            "Invalid positive integer");
        expectThrows(
            []
            {
                parseArgs({"divider", "--ratio", "0.5", "--digits", "10"});
            },
            "Invalid --digits");
        expectThrows(
            []
            {
                parseArgs({"divider", "--ratio", "0.5", "--factor", "-0.1"});
            },
            "Invalid --factor");
        expectThrows(
            []
            {
                parseArgs({"divider", "--ratio", "0.5", "--factor", "1.1"});
            },
            "Invalid --factor");
        expectThrows(
            []
            {
                parseArgs({"divider", "--ratio", "0.5", "--series", "10k", "1k"});
            },
            "Minimum must not exceed maximum");
        expectThrows(
            []
            {
                parseArgs({"divider", "--ratio", "0.5", "--parallel", "0", "1k"});
            },
            "Values must be positive");
        expectThrows(
            []
            {
                parseArgs({"divider", "--vin-vout", "5", "1", "--power", "-1m", "1m"});
            },
            "Values must be non-negative");
        expectThrows(
            []
            {
                parseArgs({"divider", "--ratio", "0.5", "--E"});
            },
            "Missing series list");
        expectThrows(
            []
            {
                parseArgs({"divider", "--ratio", "0.5", "--E", "95"});
            },
            "Invalid --E series");
        expectThrows(
            []
            {
                parseArgs({"divider", "--ratio", "0.5", "--E24", "100"});
            },
            "Invalid tolerance");
        expectThrows(
            []
            {
                parseArgs({"divider", "--ratio", "0.5", "--E5", "1"});
            },
            "Unknown option");
        expectThrows(
            []
            {
                parseArgs({"divider", "--ratio"});
            },
            "Missing value after --ratio");
    }

    void testIndependentToleranceAndValueLists()
    {
        divider::SearchConfig config;
        config.targetRatio = 0.5;
        config.tolerancePercent = 10.0;
        config.lowTolerancePercent = 0.0;
        const auto result = divider::findBestCandidates({1000.0}, {1000.0}, config);
        expectTrue(result.size() == 1, "Expected one mixed-tolerance candidate.");
        expectNear(result[0].ratioLow, 1000.0 / (1100.0 + 1000.0));
        expectNear(result[0].ratioHigh, 1000.0 / (900.0 + 1000.0));

        const auto separate = divider::findBestCandidates({1000.0, 2000.0}, {500.0, 1000.0}, config);
        expectTrue(separate.size() == 4, "Expected the product of both value lists.");
        divider::SearchConfig invalid = config;
        invalid.lowTolerancePercent = 100.0;
        expectThrows([&] { divider::findBestCandidates({1000.0}, {1000.0}, invalid); }, "Invalid search target");
    }

    void testMixedSearchMatchesExhaustive()
    {
        const std::vector<double> high{1000, 2200, 4700, 10000, 33000};
        const std::vector<double> low{470, 1000, 3300, 6800, 22000};
        for (double ratio : {0.1, 0.3, 0.5})
        for (bool loaded : {false, true})
        {
            divider::SearchConfig config;
            config.targetRatio = ratio;
            config.tolerancePercent = 1.0;
            config.lowTolerancePercent = 20.0;
            config.vin = 12.0;
            config.vout = 12.0 * ratio;
            config.top = 1000;
            if (loaded) config.loadResistance = 4700.0;
            const auto all = divider::findBestCandidates(high, low, config);
            expectTrue(all.size() == high.size() * low.size(), "Expected every mixed pair.");

            auto filtered = config;
            filtered.top = 4;
            filtered.maxErrorPercent = 60.0;
            filtered.lowRange = {true, 1000, 22000};
            filtered.seriesRange = {true, 3000, 40000};
            std::vector<divider::DividerCandidate> expected;
            for (const auto& c : all)
            {
                if (filtered.lowRange.contains(c.rLow) && filtered.seriesRange.contains(c.rSeries) &&
                    std::max(std::abs(c.lowRatioErrorPercent), std::abs(c.highRatioErrorPercent)) <= *filtered.maxErrorPercent)
                    expected.push_back(c);
            }
            if (expected.size() > filtered.top) expected.resize(filtered.top);
            const auto actual = divider::findBestCandidates(high, low, filtered);
            expectTrue(actual.size() == expected.size(), "Mixed optimized search lost candidates.");
            for (std::size_t i = 0; i < actual.size(); ++i)
            {
                expectNear(actual[i].rHigh, expected[i].rHigh);
                expectNear(actual[i].rLow, expected[i].rLow);
            }
        }
    }

    void testWorstCasePowerMatchesGrid()
    {
        for (bool loaded : {false, true})
        for (double tolerance : {0.0, 1.0, 20.0, 40.0})
        for (const auto& pair : {std::pair<double, double>{1000, 1000}, {1000, 3000}, {5000, 700}, {2200, 2300}})
        {
            divider::SearchConfig config;
            config.targetRatio = 0.5;
            config.tolerancePercent = tolerance;
            config.lowTolerancePercent = tolerance / 2.0;
            config.vin = 10.0;
            config.vout = 5.0;
            if (loaded) config.loadResistance = 1500.0;
            const auto result = divider::findBestCandidates({pair.first}, {pair.second}, config);
            expectTrue(result.size() == 1, "Expected one candidate.");

            const double highTol = tolerance / 100.0;
            const double lowTol = tolerance / 200.0;
            double gridHigh = 0.0;
            double gridLow = 0.0;
            constexpr int steps = 400;
            for (int i = 0; i <= steps; ++i)
            for (int j = 0; j <= steps; ++j)
            {
                const double rh = pair.first * (1.0 - highTol + 2.0 * highTol * i / steps);
                const double rl = pair.second * (1.0 - lowTol + 2.0 * lowTol * j / steps);
                const double bottom = loaded ? rl * 1500.0 / (rl + 1500.0) : rl;
                const double vout = 10.0 * bottom / (rh + bottom);
                gridHigh = std::max(gridHigh, (10.0 - vout) * (10.0 - vout) / rh);
                gridLow = std::max(gridLow, vout * vout / rl);
            }
            expectTrue(result[0].powerHighMax >= gridHigh * (1.0 - 1.0e-12) && result[0].powerHighMax <= gridHigh * (1.0 + 1.0e-4), "Worst-case upper power must match a dense grid.");
            expectTrue(result[0].powerLowMax >= gridLow * (1.0 - 1.0e-12) && result[0].powerLowMax <= gridLow * (1.0 + 1.0e-4), "Worst-case lower power must match a dense grid.");
            expectTrue(result[0].powerHighMax >= result[0].powerHigh * (1.0 - 1.0e-12), "Worst case cannot be below nominal power.");
            expectTrue(result[0].powerLowMax >= result[0].powerLow * (1.0 - 1.0e-12), "Worst case cannot be below nominal power.");
        }
    }

    void testRatedPowerFilter()
    {
        divider::SearchConfig config;
        config.targetRatio = 0.5;
        config.tolerancePercent = 10.0;
        config.vin = 10.0;
        config.vout = 5.0;
        config.top = 10;
        const auto all = divider::findBestCandidates({100.0, 1000.0}, {100.0, 1000.0}, config);
        expectTrue(all.size() == 4, "Expected four unfiltered candidates.");

        // 100 / 100 ohms dissipates 0.25 W per resistor nominally, so a 0.25 W rating fails once tolerances are included.
        config.ratedPower = 0.25;
        for (const auto& candidate : divider::findBestCandidates({100.0, 1000.0}, {100.0, 1000.0}, config))
            expectTrue(std::max(candidate.powerHighMax, candidate.powerLowMax) <= 0.25, "Rated power filter kept an overloaded resistor.");
        expectTrue(divider::findBestCandidates({100.0, 1000.0}, {100.0, 1000.0}, config).size() < all.size(), "Expected the rating to remove candidates.");

        config.ratedPower = 1.0;
        expectTrue(divider::findBestCandidates({100.0, 1000.0}, {100.0, 1000.0}, config).size() == all.size(), "Expected a generous rating to keep every candidate.");

        config.vin.reset();
        config.vout.reset();
        expectThrows([&] { divider::findBestCandidates({100.0}, {100.0}, config); }, "Rated power filtering requires VIN");
    }

    void testCliMixedSeriesPowerAndFormat()
    {
        auto options = parseArgs({"divider", "--ratio", "0.25", "--vin", "12", "--E-high", "96", "24", "--E-low", "E12", "--rating", "125m", "--derate", "50", "--format", "csv", "--power", "0", "1"});
        expectTrue(options.useVoltage, "Expected --vin to enable voltage mode.");
        expectNear(options.vin, 12.0);
        expectNear(options.vout, 3.0);
        expectTrue(options.highSeries.size() == 2 && options.highSeries[0] == 96 && options.highSeries[1] == 24, "Expected upper series list.");
        expectTrue(options.lowSeries.size() == 1 && options.lowSeries[0] == 12, "Expected lower series list.");
        expectNear(*options.ratedPower, 0.125);
        expectNear(*options.deratePercent, 50.0);
        expectTrue(options.format == divider::OutputFormat::Csv, "Expected csv format.");
        expectTrue(parseArgs({"divider", "--ratio", "0.5"}).format == divider::OutputFormat::Table, "Expected table by default.");
        expectTrue(parseArgs({"divider", "--ratio", "0.5", "--format", "json"}).format == divider::OutputFormat::Json, "Expected json format.");

        expectThrows([] { parseArgs({"divider", "--ratio", "0.5", "--format", "xml"}); }, "Invalid --format");
        expectThrows([] { parseArgs({"divider", "--ratio", "0.5", "--rating", "1"}); }, "--rating is only valid");
        expectThrows([] { parseArgs({"divider", "--ratio", "0.5", "--vin", "5", "--derate", "10"}); }, "--derate requires --rating");
        expectThrows([] { parseArgs({"divider", "--ratio", "0.5", "--vin", "5", "--rating", "1", "--derate", "100"}); }, "Invalid derating");
        expectThrows([] { parseArgs({"divider", "--vin-vout", "5", "1", "--vin", "5"}); }, "cannot be combined");
        expectThrows([] { parseArgs({"divider", "--vin-vout", "5", "1", "--rating", "0"}); }, "--rating must be positive");
        expectThrows([] { parseArgs({"divider", "--ratio", "0.5", "--E-high"}); }, "Missing series list after --E-high");
        expectThrows([] { parseArgs({"divider", "--ratio", "0.5", "--E-low", "7"}); }, "Invalid --E series");
        expectThrows([] { parseArgs({"divider", "--vin", "5"}); }, "Missing target");
    }

    void testMachineReadableOutput()
    {
        divider::SeriesResult result;
        result.highName = "E96";
        result.lowName = "E24";
        result.highValueCount = 96;
        result.lowValueCount = 24;
        result.highTolerancePercent = 1.0;
        result.lowTolerancePercent = 5.0;
        divider::SearchConfig config;
        config.targetRatio = 0.25;
        config.tolerancePercent = 1.0;
        config.lowTolerancePercent = 5.0;
        config.vin = 12.0;
        config.vout = 3.0;
        config.top = 2;
        result.candidates = divider::findBestCandidates({3000.0}, {1000.0}, config);

        std::ostringstream csv;
        divider::printCsv(csv, {result}, true);
        const std::string csvText = csv.str();
        expectTrue(csvText.rfind("rank,high_series,low_series,high_tolerance_pct,low_tolerance_pct,r_high_ohm,r_low_ohm", 0) == 0, "Expected CSV header.");
        expectTrue(csvText.find("power_high_max_w") != std::string::npos, "Expected worst-case power column in voltage CSV.");
        expectTrue(csvText.find("1,E96,E24,1,5,3000,1000,4000,750,0.25,") != std::string::npos, "Expected CSV row in base units.");

        std::ostringstream ratioCsv;
        divider::printCsv(ratioCsv, {result}, false);
        expectTrue(ratioCsv.str().find("vout_min_v") == std::string::npos, "Ratio CSV must omit voltage columns.");

        std::ostringstream json;
        divider::printJson(json, {result}, true);
        expectTrue(json.str().find("\"high_series\": \"E96\"") != std::string::npos, "Expected JSON series.");
        expectTrue(json.str().find("\"r_high_ohm\": 3000") != std::string::npos, "Expected JSON values in base units.");

        divider::SeriesResult empty = result;
        empty.candidates.clear();
        std::ostringstream emptyJson;
        divider::printJson(emptyJson, {empty}, false);
        expectTrue(emptyJson.str().find("\"candidates\": []") != std::string::npos, "Expected empty JSON candidate list.");

        std::ostringstream table;
        divider::printPairTable(table, result, 3, true, false, true);
        expectTrue(table.str().find("top E96 (+/-1%) / bottom E24 (+/-5%)") != std::string::npos, "Expected mixed-series title.");
        expectTrue(table.str().find("top max power (W)") != std::string::npos, "Expected worst-case power columns.");
    }

} // namespace

int main()
{
    const std::vector<TestCase> tests{
        {"loaded divider and limits", testLoadedDividerAndLimits},
        {"optimized search matches exhaustive", testOptimizedSearchMatchesExhaustive},
        {"new options and numeric validation", testNewOptionsAndNumericValidation},
        {"explicit digits and new columns", testExplicitDigitsAndNewColumns},
        {"inclusive error and power limits", testInclusiveErrorAndPowerLimits},
        {"SI parsing", testSiParsing},
        {"SI parsing rejects invalid input", testSiParsingRejectsInvalidInput},
        {"SI formatting", testSiFormatting},
        {"series generation", testSeriesGeneration},
        {"series definitions and bounds", testSeriesDefinitionsAndBounds},
        {"range contains", testRangeContains},
        {"half-ratio search", testHalfRatioSearch},
        {"top limit and tie breakers", testTopLimitAndTieBreakers},
        {"search filters", testSearchFilters},
        {"power filtering and top zero", testPowerFilteringAndTopZero},
        {"voltage columns and power", testVoltageColumnsAndPower},
        {"ratio mode omits voltage columns", testRatioModeOmitsVoltageColumns},
        {"E3 worst-case errors", testE3WorstCaseErrors},
        {"average-error ranking", testAverageErrorRanking},
        {"factor ranking", testFactorRanking},
        {"nominal-only ranking", testNominalOnlyRanking},
        {"low-series column digits", testLowSeriesColumnDigits},
        {"voltage columns use independent table units", testVoltageColumnsUseIndependentTableUnits},
        {"series and parallel use minimum column format", testSeriesAndParallelUseMinimumColumnFormat},
        {"series and parallel use column units", testSeriesAndParallelUseColumnUnits},
        {"zero voltage ignored for scale", testZeroVoltageIgnoredForScale},
        {"near-zero voltage does not force femto scale", testNearZeroVoltageDoesNotForceFemtoScale},
        {"high-series column digits", testHighSeriesColumnDigits},
        {"high and low may use different units", testHighAndLowMayUseDifferentUnits},
        {"empty table output", testEmptyTableOutput},
        {"usage documents important options", testUsageDocumentsImportantOptions},
        {"CLI parsing", testCliParsing},
        {"CLI defaults and help", testCliDefaultsAndHelp},
        {"CLI series selection", testCliSeriesSelection},
        {"CLI series selection accepts names and deduplicates", testCliSeriesSelectionAcceptsNamesAndDeduplicates},
        {"CLI validation errors", testCliValidationErrors},
        {"independent tolerances and value lists", testIndependentToleranceAndValueLists},
        {"mixed search matches exhaustive", testMixedSearchMatchesExhaustive},
        {"worst-case power matches grid", testWorstCasePowerMatchesGrid},
        {"rated power filter", testRatedPowerFilter},
        {"CLI mixed series, power and format", testCliMixedSeriesPowerAndFormat},
        {"machine-readable output", testMachineReadableOutput},
    };

    std::string_view currentTest;
    try
    {
        for (const auto& test : tests)
        {
            currentTest = test.name;
            test.run();
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Test failed";
        if (!currentTest.empty())
        {
            std::cerr << " [" << currentTest << "]";
        }
        std::cerr << ": " << error.what() << '\n';
        return 1;
    }
}
