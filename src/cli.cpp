#include "divider/cli.h"

#include "divider/e_series.h"
#include "divider/si.h"

#include <charconv>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace divider
{
    namespace
    {

        bool isOption(std::string_view value, std::string_view option)
        {
            return value == option;
        }

        std::string requireArgument(int argc, char** argv, int& index, std::string_view option)
        {
            if (index + 1 >= argc)
            {
                throw std::invalid_argument("Missing value after " + std::string(option) + ".");
            }
            ++index;
            return argv[index];
        }

        double requireSiArgument(int argc, char** argv, int& index, std::string_view option)
        {
            return parseSiValue(requireArgument(argc, argv, index, option));
        }

        std::size_t parsePositiveSize(std::string_view text, std::string_view option)
        {
            std::size_t value = 0;
            const char* begin = text.data();
            const char* end = text.data() + text.size();
            auto [ptr, error] = std::from_chars(begin, end, value);
            if (error != std::errc{} || ptr != end || value == 0)
            {
                throw std::invalid_argument("Invalid positive integer for " + std::string(option) + ".");
            }
            return value;
        }

        int parseDigits(std::string_view text)
        {
            int value = -1;
            const char* begin = text.data();
            const char* end = text.data() + text.size();
            auto [ptr, error] = std::from_chars(begin, end, value);
            if (error != std::errc{} || ptr != end || value < 0 || value > 9)
            {
                throw std::invalid_argument("Invalid --digits value. Expected an integer from 0 to 9.");
            }
            return value;
        }

        double parsePercent(std::string_view text, std::string_view option)
        {
            std::string owned(text);
            char* end = nullptr;
            errno = 0;
            const double value = std::strtod(owned.c_str(), &end);
            if (end == owned.c_str() || *end != '\0' || errno == ERANGE || !std::isfinite(value) || value < 0.0 || value >= 100.0)
            {
                throw std::invalid_argument("Invalid tolerance for " + std::string(option) + ". Expected 0 <= value < 100 percent.");
            }
            return value;
        }

        double parseFactor(std::string_view text)
        {
            std::string owned(text);
            char* end = nullptr;
            errno = 0;
            const double value = std::strtod(owned.c_str(), &end);
            if (end == owned.c_str() || *end != '\0' || errno == ERANGE || !std::isfinite(value) || value < 0.0 || value > 1.0)
            {
                throw std::invalid_argument("Invalid --factor value. Expected 0 <= factor <= 1.");
            }
            return value;
        }

        Range makeRange(double min, double max, std::string_view option)
        {
            if (min <= 0.0 || max <= 0.0)
            {
                throw std::invalid_argument("Invalid " + std::string(option) + " range. Values must be positive.");
            }
            if (min > max)
            {
                throw std::invalid_argument("Invalid " + std::string(option) + " range. Minimum must not exceed maximum.");
            }
            return Range{true, min, max};
        }

        Range makeNonNegativeRange(double min, double max, std::string_view option)
        {
            if (min < 0.0 || max < 0.0)
            {
                throw std::invalid_argument("Invalid " + std::string(option) + " range. Values must be non-negative.");
            }
            if (min > max)
            {
                throw std::invalid_argument("Invalid " + std::string(option) + " range. Minimum must not exceed maximum.");
            }
            return Range{true, min, max};
        }

        int parseSeriesOption(std::string_view option)
        {
            if (option.size() < 4 || option[0] != '-' || option[1] != '-' || option[2] != 'E')
            {
                return 0;
            }

            int value = 0;
            const char* begin = option.data() + 3;
            const char* end = option.data() + option.size();
            auto [ptr, error] = std::from_chars(begin, end, value);
            if (error != std::errc{} || ptr != end)
            {
                return 0;
            }

            for (const auto& series : allSeries())
            {
                if (series.valueCount == value)
                {
                    return value;
                }
            }
            return 0;
        }

        bool isSupportedSeriesNumber(int value)
        {
            for (const auto& series : allSeries())
            {
                if (series.valueCount == value)
                {
                    return true;
                }
            }
            return false;
        }

        int parseSeriesSelector(std::string_view text)
        {
            if (!text.empty() && (text[0] == 'E' || text[0] == 'e'))
            {
                text.remove_prefix(1);
            }

            int value = 0;
            const char* begin = text.data();
            const char* end = text.data() + text.size();
            auto [ptr, error] = std::from_chars(begin, end, value);
            if (error != std::errc{} || ptr != end || !isSupportedSeriesNumber(value))
            {
                throw std::invalid_argument("Invalid --E series '" + std::string(text) + "'. Expected one of 3, 6, 12, 24, 48, 96, 192.");
            }
            return value;
        }

        bool containsSeries(const std::vector<int>& selectedSeries, int value)
        {
            for (const int selected : selectedSeries)
            {
                if (selected == value)
                {
                    return true;
                }
            }
            return false;
        }

        void parseSeriesList(int argc, char** argv, int& index, Options& options)
        {
            bool foundSeries = false;
            while (index + 1 < argc)
            {
                const std::string_view next(argv[index + 1]);
                if (next.size() >= 2 && next[0] == '-' && next[1] == '-')
                {
                    break;
                }

                ++index;
                const int seriesValue = parseSeriesSelector(next);
                if (!containsSeries(options.selectedSeries, seriesValue))
                {
                    options.selectedSeries.push_back(seriesValue);
                }
                foundSeries = true;
            }

            if (!foundSeries)
            {
                throw std::invalid_argument("Missing series list after --E.");
            }
        }

        void validateOptions(const Options& options)
        {
            if (options.help)
            {
                return;
            }
            if (options.useVoltage)
            {
                if (options.vin <= 0.0 || options.vout <= 0.0 || options.vout >= options.vin)
                {
                    throw std::invalid_argument("Invalid voltage pair. Expected VIN > VOUT > 0.");
                }
            }
            if (!std::isfinite(options.ratio) || options.ratio <= 0.0 || options.ratio >= 1.0)
            {
                throw std::invalid_argument("Invalid ratio. Expected 0 < ratio < 1.");
            }
            if (!options.useVoltage && options.powerRange.enabled)
            {
                throw std::invalid_argument("--power is only valid with --vin-vout.");
            }
        }

    } // namespace

    Options parseCommandLine(int argc, char** argv)
    {
        Options options;
        bool ratioProvided = false;
        bool voltageProvided = false;

        for (int index = 1; index < argc; ++index)
        {
            const std::string_view arg(argv[index]);

            if (isOption(arg, "--help") || isOption(arg, "-h"))
            {
                options.help = true;
                continue;
            }

            if (isOption(arg, "--ratio"))
            {
                if (voltageProvided || ratioProvided)
                {
                    throw std::invalid_argument("Use exactly one target mode: --ratio or --vin-vout.");
                }
                options.ratio = requireSiArgument(argc, argv, index, arg);
                ratioProvided = true;
                continue;
            }

            if (isOption(arg, "--vin-vout"))
            {
                if (voltageProvided || ratioProvided)
                {
                    throw std::invalid_argument("Use exactly one target mode: --ratio or --vin-vout.");
                }

                options.vin = requireSiArgument(argc, argv, index, arg);
                options.vout = requireSiArgument(argc, argv, index, arg);
                options.ratio = options.vout / options.vin;
                options.useVoltage = true;
                voltageProvided = true;
                continue;
            }

            if (isOption(arg, "--top"))
            {
                options.top = parsePositiveSize(requireArgument(argc, argv, index, arg), arg);
                continue;
            }

            if (isOption(arg, "--digits"))
            {
                options.digitsProvided = true;
                options.digits = parseDigits(requireArgument(argc, argv, index, arg));
                continue;
            }

            if (isOption(arg, "--factor"))
            {
                options.factor = parseFactor(requireArgument(argc, argv, index, arg));
                continue;
            }

            if (isOption(arg, "--E"))
            {
                parseSeriesList(argc, argv, index, options);
                continue;
            }

            if (isOption(arg, "--series"))
            {
                const double min = requireSiArgument(argc, argv, index, arg);
                const double max = requireSiArgument(argc, argv, index, arg);
                options.seriesRange = makeRange(min, max, arg);
                continue;
            }

            if (isOption(arg, "--parallel"))
            {
                const double min = requireSiArgument(argc, argv, index, arg);
                const double max = requireSiArgument(argc, argv, index, arg);
                options.parallelRange = makeRange(min, max, arg);
                continue;
            }

            if (isOption(arg, "--high") || isOption(arg, "--low"))
            {
                const double min = requireSiArgument(argc, argv, index, arg);
                const double max = requireSiArgument(argc, argv, index, arg);
                (arg == "--high" ? options.highRange : options.lowRange) = makeRange(min, max, arg);
                continue;
            }
            if (isOption(arg, "--load"))
            {
                const double value = requireSiArgument(argc, argv, index, arg);
                if (value <= 0.0) throw std::invalid_argument("--load must be positive.");
                options.loadResistance = value;
                continue;
            }
            if (isOption(arg, "--max-error"))
            {
                const double value = requireSiArgument(argc, argv, index, arg);
                if (value < 0.0) throw std::invalid_argument("--max-error must be non-negative.");
                options.maxErrorPercent = value;
                continue;
            }

            if (isOption(arg, "--power"))
            {
                const double min = requireSiArgument(argc, argv, index, arg);
                const double max = requireSiArgument(argc, argv, index, arg);
                options.powerRange = makeNonNegativeRange(min, max, arg);
                continue;
            }

            const int seriesValue = parseSeriesOption(arg);

            if (seriesValue != 0)
            {
                options.toleranceOverridesPercent[seriesValue] = parsePercent(requireArgument(argc, argv, index, arg), arg);
                continue;
            }

            throw std::invalid_argument("Unknown option: " + std::string(arg) + ".");
        }

        if (!options.help && !ratioProvided && !voltageProvided)
        {
            throw std::invalid_argument("Missing target. Use --ratio or --vin-vout.");
        }

        validateOptions(options);
        return options;
    }

    std::string usage(std::string_view executableName)
    {
        std::ostringstream out;
        out << "Usage:\n"
            << "  " << executableName << " --ratio RATIO [--top N] [--digits N] [filters] [tolerances]\n"
            << "  " << executableName << " --vin-vout VIN VOUT [--top N] [--digits N] [filters] [tolerances]\n"
            << "\n"
            << "Options:\n"
            << "  --top N              Print the best N candidates per E series. Default: 10.\n"
            << "  --digits N           Decimal places (0-9) for all columns; default: automatic.\n"
            << "  --factor F           Ranking weight for nominal error versus tolerance error. Default: 0.5.\n"
            << "                       score = abs(nominal error) * F + (abs(high error) + abs(low error)) * (1 - F).\n"
            << "                       F=1 ranks only by nominal error; F=0 ranks only by tolerance error.\n"
            << "  --E LIST             Evaluate only selected E series, e.g. --E 96 192.\n"
            << "  --load OHMS          Fixed load resistance from VOUT to ground (default: no load).\n"
            << "                       Load remains fixed when evaluating resistor tolerances.\n"
            << "\n"
            << "Filters:\n"
            << "  --high MIN MAX       Keep individual upper resistor values in range.\n"
            << "  --low MIN MAX        Keep individual lower resistor values in range.\n"
            << "  --max-error PCT      Maximum absolute error including tolerance extremes.\n"
            << "  --series MIN MAX     Keep dividers with R_high + R_low in range.\n"
            << "  --parallel MIN MAX   Keep dividers with R_high || R_low in range.\n"
            << "  --power MIN MAX      Keep total resistor power in range (excludes load); only with --vin-vout.\n"
            << "\n"
            << "Tolerance overrides, in percent:\n"
            << "  --E3 PCT --E6 PCT --E12 PCT --E24 PCT --E48 PCT --E96 PCT --E192 PCT\n"
            << "\n"
            << "SI input prefixes:\n"
            << "  u/U, m, k/K, M, g/G, t/T. Micro sign is also accepted.\n"
            << "\n"
            << "Voltage output:\n"
            << "  Includes VOUT min/max under tolerance and nominal power in each resistor.\n"
            << "  Total resistor power excludes the load; power values are nominal.\n"
            << "\n"
            << "Examples:\n"
            << "  " << executableName << " --ratio 0.275 --top 5 --E 96 192 --series 1k 1M\n"
            << "  " << executableName << " --vin-vout 12 3.3 --top 3 --parallel 1k 100k --power 1m 100m\n"
            << "  " << executableName << " --vin-vout 12 3.3 --E 192 --load 100k --high 10k 100k --low 1k 50k --max-error 1\n";
        return out.str();
    }

} // namespace divider
