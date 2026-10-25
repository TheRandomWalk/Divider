#include "divider/si.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cerrno>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace divider
{
    namespace
    {

        std::string trim(std::string_view text)
        {
            constexpr std::string_view whitespace = " \t\n\r";
            const std::size_t begin = text.find_first_not_of(whitespace);
            if (begin == std::string_view::npos)
            {
                return {};
            }
            const std::size_t end = text.find_last_not_of(whitespace);
            return std::string(text.substr(begin, end - begin + 1));
        }

        double prefixMultiplier(const std::string& suffix)
        {
            struct Prefix
            {
                std::string_view symbols[4];
                double multiplier;
            };
            static constexpr Prefix prefixes[]{
                {{"u", "U", "\xC2\xB5", "\xCE\xBC"}, 1.0e-6},
                {{"m"}, 1.0e-3},
                {{"k", "K"}, 1.0e3},
                {{"M"}, 1.0e6},
                {{"g", "G"}, 1.0e9},
                {{"t", "T"}, 1.0e12},
            };

            if (suffix.empty())
            {
                return 1.0;
            }
            for (const auto& prefix : prefixes)
            {
                if (std::find(std::begin(prefix.symbols), std::end(prefix.symbols), suffix) != std::end(prefix.symbols))
                {
                    return prefix.multiplier;
                }
            }
            throw std::invalid_argument("Unknown SI prefix '" + suffix + "'.");
        }

        const std::array<SiScale, 10>& outputPrefixes()
        {
            static const std::array<SiScale, 10> prefixes
            {{
                {1.0e-15, "f"},
                {1.0e-12, "p"},
                {1.0e-9, "n"},
                {1.0e-6, "u"},
                {1.0e-3, "m"},
                {1.0, ""},
                {1.0e3, "k"},
                {1.0e6, "M"},
                {1.0e9, "G"},
                {1.0e12, "T"},
            }};
            return prefixes;
        }

        std::string addThousandsSeparators(const std::string& text)
        {
            const std::size_t decimal = text.find('.');
            const std::size_t integerEnd = decimal == std::string::npos ? text.size() : decimal;
            std::size_t integerBegin = 0;

            if (!text.empty() && (text[0] == '-' || text[0] == '+'))
            {
                integerBegin = 1;
            }

            std::string output;
            output.reserve(text.size() + text.size() / 3);
            output.append(text.substr(0, integerBegin));

            const std::size_t integerLength = integerEnd - integerBegin;

            for (std::size_t index = 0; index < integerLength; ++index)
            {
                if (index > 0 && (integerLength - index) % 3 == 0)
                {
                    output.push_back(',');
                }
                output.push_back(text[integerBegin + index]);
            }

            if (decimal != std::string::npos)
            {
                output.append(text.substr(decimal));
            }

            return output;
        }

    } // namespace

    double parseSiValue(std::string_view text)
    {
        const std::string value = trim(text);

        if (value.empty())
        {
            throw std::invalid_argument("Empty SI value.");
        }

        char* end = nullptr;
        errno = 0;
        const double number = std::strtod(value.c_str(), &end);

        if (end == value.c_str() || !std::isfinite(number) || errno == ERANGE)
        {
            throw std::invalid_argument("Invalid numeric value '" + value + "'.");
        }

        const std::string suffix = trim(std::string_view(end, value.c_str() + value.size() - end));
        const double result = number * prefixMultiplier(suffix);
        if (!std::isfinite(result) || (number != 0.0 && result == 0.0))
        {
            throw std::invalid_argument("SI value out of range '" + value + "'.");
        }
        return result;
    }

    bool isZeroNumber(std::string_view formatted)
    {
        bool sawDigit = false;
        for (const unsigned char ch : formatted)
        {
            if (!std::isdigit(ch))
            {
                continue;
            }
            if (ch != '0')
            {
                return false;
            }
            sawDigit = true;
        }
        return sawDigit;
    }

    std::string formatDecimal(double value, int digits)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(digits) << value;
        return addThousandsSeparators(out.str());
    }

    std::string formatSignedDecimal(double value, int digits)
    {
        std::string output = formatDecimal(std::abs(value), digits);
        if (isZeroNumber(output))
        {
            return output;
        }
        if (value < 0.0)
        {
            return "-" + output;
        }
        return "+" + output;
    }

    std::string formatCompactDecimal(double value)
    {
        std::ostringstream out;
        out << std::defaultfloat << value;
        return out.str();
    }

    SiScale chooseSiScale(double value)
    {
        const auto& prefixes = outputPrefixes();
        if (!std::isfinite(value) || value == 0.0)
        {
            return SiScale{1.0, ""};
        }

        const double absolute = std::abs(value);
        const SiScale* selected = &prefixes.front();
        for (const auto& prefix : prefixes)
        {
            if (absolute >= prefix.multiplier)
            {
                selected = &prefix;
            }
        }
        return *selected;
    }

    std::string formatSiWithScale(double value, const SiScale& scale, std::string_view unit, int digits)
    {
        if (!std::isfinite(value))
        {
            return "N/A";
        }

        const double scaledValue = scale.multiplier == 0.0 ? value : value / scale.multiplier;
        std::ostringstream out;
        out << std::fixed << std::setprecision(digits) << scaledValue;
        std::string output = addThousandsSeparators(out.str());

        if (unit.empty())
        {
            if (scale.label.empty())
            {
                output += "  ";
            }
            else
            {
                output += " ";
                output += scale.label;
            }
        }
        else
        {
            output += " ";
            output += scale.label;
            output += unit;
        }
        return output;
    }

    std::string formatSi(double value, std::string_view unit, int digits)
    {
        return formatSiWithScale(value, chooseSiScale(value), unit, digits);
    }

} // namespace divider
