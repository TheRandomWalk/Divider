#pragma once

#include <string>
#include <string_view>

namespace divider
{

    struct SiScale
    {
        double multiplier = 1.0;
        std::string_view label;
    };

    double parseSiValue(std::string_view text);
    SiScale chooseSiScale(double value);
    std::string formatSi(double value, std::string_view unit, int digits);
    std::string formatSiWithScale(double value, const SiScale& scale, std::string_view unit, int digits);
    std::string formatDecimal(double value, int digits);
    std::string formatSignedDecimal(double value, int digits);
    std::string formatCompactDecimal(double value);

} // namespace divider
