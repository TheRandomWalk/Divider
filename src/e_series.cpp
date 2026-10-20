#include "divider/e_series.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace divider
{
    namespace
    {

        constexpr double minimumResistance = 1.0e-5;
        constexpr double maximumResistance = 1.0e13;

        const std::vector<double>& manualBaseValues(int valueCount)
        {
            static const std::vector<double> e3{1.0, 2.2, 4.7};
            static const std::vector<double> e6{1.0, 1.5, 2.2, 3.3, 4.7, 6.8};
            static const std::vector<double> e12{1.0, 1.2, 1.5, 1.8, 2.2, 2.7, 3.3, 3.9, 4.7, 5.6, 6.8, 8.2};
            static const std::vector<double> e24{1.0, 1.1, 1.2, 1.3, 1.5, 1.6, 1.8, 2.0, 2.2, 2.4, 2.7, 3.0, 3.3, 3.6, 3.9, 4.3, 4.7, 5.1, 5.6, 6.2, 6.8, 7.5, 8.2, 9.1};

            switch (valueCount)
            {
            case 3:
                return e3;
            case 6:
                return e6;
            case 12:
                return e12;
            case 24:
                return e24;
            default:
                throw std::invalid_argument("No manual base table for this E series.");
            }
        }

        double roundToSignificantDigits(double value, int digits)
        {
            if (value == 0.0)
            {
                return 0.0;
            }
            const double exponent = std::floor(std::log10(std::abs(value)));
            const double scale = std::pow(10.0, digits - 1 - exponent);
            return std::round(value * scale) / scale;
        }

        bool isSupportedSeries(int valueCount)
        {
            return valueCount == 3 || valueCount == 6 || valueCount == 12 || valueCount == 24 || valueCount == 48 || valueCount == 96 || valueCount == 192;
        }

    } // namespace

    const std::vector<SeriesDefinition>& allSeries()
    {
        static const std::vector<SeriesDefinition> series{
            {3, "E3", 40.0}, {6, "E6", 20.0}, {12, "E12", 10.0}, {24, "E24", 5.0}, {48, "E48", 2.0}, {96, "E96", 1.0}, {192, "E192", 0.5},
        };
        return series;
    }

    std::vector<double> generateBaseValues(int valueCount)
    {
        if (!isSupportedSeries(valueCount))
        {
            throw std::invalid_argument("Unsupported E series.");
        }

        if (valueCount <= 24)
        {
            return manualBaseValues(valueCount);
        }

        std::vector<double> values;
        values.reserve(static_cast<std::size_t>(valueCount));

        for (int index = 0; index < valueCount; ++index)
        {
            values.push_back(roundToSignificantDigits(std::pow(10.0, static_cast<double>(index) / valueCount), 3));
        }
        return values;
    }

    std::vector<double> generateSeriesValues(int valueCount)
    {
        const auto baseValues = generateBaseValues(valueCount);
        std::vector<double> values;
        values.reserve(baseValues.size() * 20);

        for (int decade = -5; decade <= 13; ++decade)
        {
            const double scale = std::pow(10.0, decade);

            for (double base : baseValues)
            {
                const double value = base * scale;

                if (value >= minimumResistance * (1.0 - 1.0e-12) && value <= maximumResistance * (1.0 + 1.0e-12))
                {
                    values.push_back(value);
                }
            }
        }

        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end(), [](double left, double right) {return std::abs(left - right) <= std::max(std::abs(left), std::abs(right)) * 1.0e-12;}), values.end());

        return values;
    }

} // namespace divider
