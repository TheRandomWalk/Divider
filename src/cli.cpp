#include "divider/cli.h"

#include "divider/e_series.h"
#include "divider/si.h"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace divider
{
    namespace
    {

        [[noreturn]] void fail(const std::string& message)
        {
            throw std::invalid_argument(message);
        }

        // Parses the whole text as an integer; returns false on trailing or missing characters.
        template <typename Integer>
        bool parseWhole(std::string_view text, Integer& value)
        {
            const char* end = text.data() + text.size();
            const auto [ptr, error] = std::from_chars(text.data(), end, value);
            return error == std::errc{} && ptr == end;
        }

        // Parses the whole text as a finite double; returns false on trailing characters, overflow, NaN or infinity.
        bool parseFiniteDouble(std::string_view text, double& value)
        {
            const std::string owned(text);
            char* end = nullptr;
            errno = 0;
            value = std::strtod(owned.c_str(), &end);
            return end != owned.c_str() && *end == '\0' && errno != ERANGE && std::isfinite(value);
        }

        std::size_t parsePositiveSize(std::string_view text, std::string_view option)
        {
            std::size_t value = 0;
            if (!parseWhole(text, value) || value == 0)
            {
                fail("Invalid positive integer for " + std::string(option) + ".");
            }
            return value;
        }

        int parseDigits(std::string_view text)
        {
            int value = -1;
            if (!parseWhole(text, value) || value < 0 || value > 9)
            {
                fail("Invalid --digits value. Expected an integer from 0 to 9.");
            }
            return value;
        }

        double parsePercent(std::string_view text, std::string_view option, std::string_view what = "tolerance")
        {
            double value = 0.0;
            if (!parseFiniteDouble(text, value) || value < 0.0 || value >= 100.0)
            {
                fail("Invalid " + std::string(what) + " for " + std::string(option) + ". Expected 0 <= value < 100 percent.");
            }
            return value;
        }

        double parseFactor(std::string_view text)
        {
            double value = 0.0;
            if (!parseFiniteDouble(text, value) || value < 0.0 || value > 1.0)
            {
                fail("Invalid --factor value. Expected 0 <= factor <= 1.");
            }
            return value;
        }

        enum class RangeBound
        {
            Positive,
            NonNegative,
        };

        Range makeRange(double min, double max, std::string_view option, RangeBound bound)
        {
            const bool positive = bound == RangeBound::Positive;
            if (positive ? (min <= 0.0 || max <= 0.0) : (min < 0.0 || max < 0.0))
            {
                fail("Invalid " + std::string(option) + " range. Values must be " + (positive ? "positive." : "non-negative."));
            }
            if (min > max)
            {
                fail("Invalid " + std::string(option) + " range. Minimum must not exceed maximum.");
            }
            return Range{true, min, max};
        }

        // Recognizes the per-series tolerance override "--E<N>"; returns 0 when the option is not one.
        int parseSeriesToleranceOption(std::string_view option)
        {
            if (!option.starts_with("--E"))
            {
                return 0;
            }
            int value = 0;
            return parseWhole(option.substr(3), value) && findSeries(value) ? value : 0;
        }

        int parseSeriesSelector(std::string_view text)
        {
            if (!text.empty() && (text[0] == 'E' || text[0] == 'e'))
            {
                text.remove_prefix(1);
            }

            int value = 0;
            if (!parseWhole(text, value) || !findSeries(value))
            {
                fail("Invalid --E series '" + std::string(text) + "'. Expected one of 3, 6, 12, 24, 48, 96, 192.");
            }
            return value;
        }

        OutputFormat parseFormat(std::string_view text)
        {
            if (text == "table")
            {
                return OutputFormat::Table;
            }
            if (text == "csv")
            {
                return OutputFormat::Csv;
            }
            if (text == "json")
            {
                return OutputFormat::Json;
            }
            fail("Invalid --format value. Expected table, csv or json.");
        }

        bool isTargetRatio(double ratio)
        {
            return std::isfinite(ratio) && ratio > 0.0 && ratio < 1.0;
        }

        // Walks argv once; each option handler pulls its own values from it.
        class CommandLineParser
        {
        public:
            CommandLineParser(int argc, char** argv) : m_argc(argc), m_argv(argv)
            {
            }

            Options parse()
            {
                for (m_index = 1; m_index < m_argc; ++m_index)
                {
                    parseOption(m_argv[m_index]);
                }
                finish();
                return m_options;
            }

        private:
            using Handler = void (CommandLineParser::*)(std::string_view);

            void parseOption(std::string_view option)
            {
                static const std::unordered_map<std::string_view, Handler> handlers{
                    {"--help", &CommandLineParser::help},
                    {"-h", &CommandLineParser::help},
                    {"--ratio", &CommandLineParser::ratio},
                    {"--vin-vout", &CommandLineParser::vinVout},
                    {"--vin", &CommandLineParser::vin},
                    {"--top", &CommandLineParser::top},
                    {"--digits", &CommandLineParser::digits},
                    {"--factor", &CommandLineParser::factor},
                    {"--E", &CommandLineParser::seriesList},
                    {"--E-high", &CommandLineParser::seriesList},
                    {"--E-low", &CommandLineParser::seriesList},
                    {"--rating", &CommandLineParser::rating},
                    {"--derate", &CommandLineParser::derate},
                    {"--format", &CommandLineParser::format},
                    {"--series", &CommandLineParser::seriesRange},
                    {"--parallel", &CommandLineParser::parallelRange},
                    {"--high", &CommandLineParser::highRange},
                    {"--low", &CommandLineParser::lowRange},
                    {"--power", &CommandLineParser::powerRange},
                    {"--load", &CommandLineParser::load},
                    {"--max-error", &CommandLineParser::maxError},
                };

                if (const auto handler = handlers.find(option); handler != handlers.end())
                {
                    (this->*handler->second)(option);
                    return;
                }
                if (const int series = parseSeriesToleranceOption(option))
                {
                    m_options.toleranceOverridesPercent[series] = parsePercent(value(option), option);
                    return;
                }
                fail("Unknown option: " + std::string(option) + ".");
            }

            std::string_view value(std::string_view option)
            {
                if (m_index + 1 >= m_argc)
                {
                    fail("Missing value after " + std::string(option) + ".");
                }
                return m_argv[++m_index];
            }

            double siValue(std::string_view option)
            {
                return parseSiValue(value(option));
            }

            double positiveSiValue(std::string_view option)
            {
                const double result = siValue(option);
                if (result <= 0.0)
                {
                    fail(std::string(option) + " must be positive.");
                }
                return result;
            }

            // Reads the "MIN MAX" pair shared by every interval option.
            Range readRange(std::string_view option, RangeBound bound = RangeBound::Positive)
            {
                const double min = siValue(option);
                const double max = siValue(option);
                return makeRange(min, max, option, bound);
            }

            // Consumes series names up to the next "--" option.
            void readSeriesList(std::string_view option, std::vector<int>& target)
            {
                bool found = false;
                while (m_index + 1 < m_argc && !std::string_view(m_argv[m_index + 1]).starts_with("--"))
                {
                    const int series = parseSeriesSelector(m_argv[++m_index]);
                    if (std::find(target.begin(), target.end(), series) == target.end())
                    {
                        target.push_back(series);
                    }
                    found = true;
                }
                if (!found)
                {
                    fail("Missing series list after " + std::string(option) + ".");
                }
            }

            void requireSingleTarget() const
            {
                if (m_ratioProvided || m_voltageProvided)
                {
                    fail("Use exactly one target mode: --ratio or --vin-vout.");
                }
            }

            void help(std::string_view)
            {
                m_options.help = true;
            }

            void ratio(std::string_view option)
            {
                requireSingleTarget();
                m_options.ratio = siValue(option);
                m_ratioProvided = true;
            }

            void vinVout(std::string_view option)
            {
                requireSingleTarget();
                m_options.vin = siValue(option);
                m_options.vout = siValue(option);
                m_options.ratio = m_options.vout / m_options.vin;
                m_options.useVoltage = true;
                m_voltageProvided = true;
            }

            void vin(std::string_view option)
            {
                m_inputVoltage = positiveSiValue(option);
            }

            void top(std::string_view option)
            {
                m_options.top = parsePositiveSize(value(option), option);
            }

            void digits(std::string_view option)
            {
                m_options.digitsProvided = true;
                m_options.digits = parseDigits(value(option));
            }

            void factor(std::string_view option)
            {
                m_options.factor = parseFactor(value(option));
            }

            void seriesList(std::string_view option)
            {
                std::vector<int>& target = option == "--E-high" ? m_options.highSeries : option == "--E-low" ? m_options.lowSeries : m_options.selectedSeries;
                readSeriesList(option, target);
            }

            void rating(std::string_view option)
            {
                m_options.ratedPower = positiveSiValue(option);
            }

            void derate(std::string_view option)
            {
                m_options.deratePercent = parsePercent(value(option), option, "derating");
            }

            void format(std::string_view option)
            {
                m_options.format = parseFormat(value(option));
            }

            void seriesRange(std::string_view option)
            {
                m_options.seriesRange = readRange(option);
            }

            void parallelRange(std::string_view option)
            {
                m_options.parallelRange = readRange(option);
            }

            void highRange(std::string_view option)
            {
                m_options.highRange = readRange(option);
            }

            void lowRange(std::string_view option)
            {
                m_options.lowRange = readRange(option);
            }

            void powerRange(std::string_view option)
            {
                m_options.powerRange = readRange(option, RangeBound::NonNegative);
            }

            void load(std::string_view option)
            {
                m_options.loadResistance = positiveSiValue(option);
            }

            void maxError(std::string_view option)
            {
                const double percent = siValue(option);
                if (percent < 0.0)
                {
                    fail("--max-error must be non-negative.");
                }
                m_options.maxErrorPercent = percent;
            }

            // Checks that need the whole command line.
            void finish()
            {
                if (!m_options.help && !m_ratioProvided && !m_voltageProvided)
                {
                    fail("Missing target. Use --ratio or --vin-vout.");
                }

                if (m_inputVoltage)
                {
                    if (m_voltageProvided)
                    {
                        fail("--vin cannot be combined with --vin-vout.");
                    }
                    if (!m_ratioProvided)
                    {
                        fail("--vin requires --ratio.");
                    }
                    if (!isTargetRatio(m_options.ratio))
                    {
                        fail("Invalid ratio. Expected 0 < ratio < 1.");
                    }
                    m_options.vin = *m_inputVoltage;
                    m_options.vout = m_options.ratio * m_options.vin;
                    m_options.useVoltage = true;
                }

                validate();
            }

            void validate() const
            {
                if (m_options.help)
                {
                    return;
                }
                if (m_options.useVoltage && (m_options.vin <= 0.0 || m_options.vout <= 0.0 || m_options.vout >= m_options.vin))
                {
                    fail("Invalid voltage pair. Expected VIN > VOUT > 0.");
                }
                if (!isTargetRatio(m_options.ratio))
                {
                    fail("Invalid ratio. Expected 0 < ratio < 1.");
                }
                if (!m_options.useVoltage && m_options.powerRange.enabled)
                {
                    fail("--power is only valid with --vin-vout or --vin.");
                }
                if (!m_options.useVoltage && m_options.ratedPower)
                {
                    fail("--rating is only valid with --vin-vout or --vin.");
                }
                if (m_options.deratePercent && !m_options.ratedPower)
                {
                    fail("--derate requires --rating.");
                }
            }

            int m_argc;
            char** m_argv;
            int m_index = 0;
            Options m_options;
            bool m_ratioProvided = false;
            bool m_voltageProvided = false;
            std::optional<double> m_inputVoltage;
        };

    } // namespace

    Options parseCommandLine(int argc, char** argv)
    {
        return CommandLineParser(argc, argv).parse();
    }

    std::string usage(std::string_view executableName)
    {
        std::ostringstream out;
        out << "Usage:\n"
            << "  " << executableName << " --ratio RATIO [--top N] [--digits N] [filters] [tolerances]\n"
            << "  " << executableName << " --vin-vout VIN VOUT [--top N] [--digits N] [filters] [tolerances]\n"
            << "  " << executableName << " --ratio RATIO --vin VIN [--top N] [--digits N] [filters] [tolerances]\n"
            << "\n"
            << "Options:\n"
            << "  --top N              Print the best N candidates per E series. Default: 10.\n"
            << "  --digits N           Decimal places (0-9) for all columns; default: automatic.\n"
            << "  --factor F           Ranking weight for nominal error versus tolerance error. Default: 0.5.\n"
            << "                       score = abs(nominal error) * F + (abs(high error) + abs(low error)) * (1 - F).\n"
            << "                       F=1 ranks only by nominal error; F=0 ranks only by tolerance error.\n"
            << "  --E LIST             Evaluate only selected E series, e.g. --E 96 192.\n"
            << "  --E-high LIST        E series for the upper resistor; with --E-low, every combination is evaluated.\n"
            << "  --E-low LIST         E series for the lower resistor. A missing list falls back to --E, then all series.\n"
            << "  --vin VIN            Input voltage for --ratio mode; enables voltage, power and --rating output.\n"
            << "  --format FORMAT      Output as table (default), csv or json. csv and json use base units.\n"
            << "  --load OHMS          Fixed load resistance from VOUT to ground (default: no load).\n"
            << "                       Load remains fixed when evaluating resistor tolerances.\n"
            << "\n"
            << "Filters:\n"
            << "  --high MIN MAX       Keep individual upper resistor values in range.\n"
            << "  --low MIN MAX        Keep individual lower resistor values in range.\n"
            << "  --max-error PCT      Maximum absolute error including tolerance extremes.\n"
            << "  --series MIN MAX     Keep dividers with R_high + R_low in range.\n"
            << "  --parallel MIN MAX   Keep dividers with R_high || R_low in range.\n"
            << "  --power MIN MAX      Keep total resistor power in range (excludes load); needs --vin-vout or --vin.\n"
            << "  --rating WATTS       Keep dividers whose worst-case power (over tolerances) in each resistor\n"
            << "                       does not exceed WATTS; needs --vin-vout or --vin. Adds max power columns.\n"
            << "  --derate PCT         With --rating, use WATTS * (1 - PCT/100) as the limit. Default: 0.\n"
            << "\n"
            << "Tolerance overrides, in percent:\n"
            << "  --E3 PCT --E6 PCT --E12 PCT --E24 PCT --E48 PCT --E96 PCT --E192 PCT\n"
            << "\n"
            << "SI input prefixes:\n"
            << "  u/U, m, k/K, M, g/G, t/T. Micro sign is also accepted.\n"
            << "\n"
            << "Voltage output:\n"
            << "  Includes VOUT min/max under tolerance and nominal power in each resistor.\n"
            << "  Total resistor power excludes the load; power values are nominal unless marked max.\n"
            << "  Tolerances are per series: --E-high 96 --E-low 24 uses the E96 tolerance above and E24 below.\n"
            << "\n"
            << "Examples:\n"
            << "  " << executableName << " --ratio 0.275 --top 5 --E 96 192 --series 1k 1M\n"
            << "  " << executableName << " --vin-vout 12 3.3 --top 3 --parallel 1k 100k --power 1m 100m\n"
            << "  " << executableName << " --vin-vout 12 3.3 --E 192 --load 100k --high 10k 100k --low 1k 50k --max-error 1\n"
            << "  " << executableName << " --ratio 0.275 --vin 24 --E-high 96 --E-low 24 --rating 0.125 --derate 50 --format csv\n";
        return out.str();
    }

} // namespace divider
