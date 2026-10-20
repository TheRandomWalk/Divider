# Divider

A C++20 command-line tool for finding resistor pairs from E series that approximate a target voltage ratio or output voltage. It evaluates nominal error and tolerance extremes, then lists the best candidates for each series.

## Features

- E3, E6, E12, E24, E48, E96, and E192 selection with configurable tolerances.
- Search by voltage ratio or input/output voltages, with an optional resistive load.
- Filters for individual resistors, total resistance, parallel equivalent, power, and maximum error across tolerance extremes.
- Output voltage limits, errors, and nominal power dissipation for each resistor.
- Configurable precision with `--digits` and interval pruning to accelerate filtered searches.

## Get the project

Repository: [TheRandomWalk/Divider](https://github.com/TheRandomWalk/Divider).

```sh
git clone https://github.com/TheRandomWalk/Divider.git
cd Divider
```

The repository contains the source code. Build the executable using the instructions below; local binaries and build directories are excluded from Git.

## Circuit and calculations

```text
          VIN
           │
        ┌──┴──┐
        │     │
        │     │ R_high
        │     │
        └──┬──┘
           │
           ├────────── VOUT
           │
        ┌──┴──┐
        │     │
        │     │ R_low
        │     │
        └──┬──┘
           │
          GND
```

By default, calculations assume an unloaded divider:

```text
VOUT / VIN = R_low / (R_high + R_low)
R_series   = R_high + R_low
R_parallel = (R_high * R_low) / (R_high + R_low)
P_total    = VIN² / R_series
```

With `--load OHMS`, a fixed resistive load is connected between VOUT and ground. The voltage-ratio calculation replaces `R_low` with its parallel equivalent with the load. The load remains fixed when evaluating resistor tolerance extremes. The `series` and `parallel` columns continue to describe the resistor pair alone.

Nominal resistor power is calculated as `(VIN - VOUT)² / R_high` and `VOUT² / R_low`, using the actual nominal output voltage. With a load, `P_total` is the sum of these two powers and excludes load dissipation. These are nominal powers, not worst-case values under tolerance.

Both resistors belong to the same E series. The search covers values from `10 µΩ` to `10 TΩ`; use filters to restrict candidates to practical ranges for your circuit.

## Quick start

In PowerShell, with the compiled executable in the project directory:

```powershell
.\divider.exe --help
.\divider.exe --ratio 0.275 --top 5 --E 96 192 --series 1k 1M
.\divider.exe --vin-vout 12 3.3 --top 3 --parallel 1k 100k --power 1m 100m
```

The second command searches for a ratio of `0.275` using E96 and E192, with total resistance between `1 kΩ` and `1 MΩ`. The third targets `3.3 V` from `12 V`, with parallel equivalent resistance between `1 kΩ` and `100 kΩ` and total resistor power between `1 mW` and `100 mW`.

## Options

Specify exactly one target mode:

| Option | Description |
| --- | --- |
| `--ratio RATIO` | Target `VOUT / VIN`, strictly between 0 and 1. |
| `--vin-vout VIN VOUT` | Voltages in volts; requires `VIN > VOUT > 0`. |

Search and display options:

| Option | Description | Default |
| --- | --- | --- |
| `--top N` | Maximum number of candidates per series; a positive integer. | `10` |
| `--E LIST` | Space-separated series, such as `--E 24 96`. Also accepts `E24` and `E96`. | All series |
| `--factor F` | Weight of nominal error in the ranking, from 0 to 1. | `0.5` |
| `--digits N` | Sets 0 to 9 decimal places in all numeric columns after SI scaling. | Automatic |
| `--load OHMS` | Fixed load resistance between VOUT and ground; must be positive. Available in both target modes. | No load |
| `--help`, `-h` | Displays help. | — |

Filters use inclusive limits:

| Option | Condition |
| --- | --- |
| `--high MIN MAX` | Nominal upper resistor value in ohms; both limits must be positive. |
| `--low MIN MAX` | Nominal lower resistor value in ohms; both limits must be positive. |
| `--max-error PCT` | Maximum absolute percentage error at both tolerance extremes; zero is allowed. |
| `--series MIN MAX` | Total resistance `R_high + R_low` in ohms; both limits must be positive. |
| `--parallel MIN MAX` | Parallel equivalent of `R_high` and `R_low` in ohms; both limits must be positive. |
| `--power MIN MAX` | Total nominal power in both resistors in watts, excluding the load. Requires `--vin-vout`; a zero minimum is allowed. |

For every interval filter, the minimum must not exceed the maximum.

Example with a load, individual resistor limits, and a maximum error of ±1%:

```powershell
.\divider.exe --vin-vout 12 3.3 --E 96 192 --load 100k --high 10k 100k --low 1k 50k --max-error 1 --digits 4 --top 3
```

The `--max-error` filter requires the entire tolerance interval to stay within the specified percentage of the target. A narrow interval far from the target does not pass. Resistance and power limits apply to nominal values.

## E series and tolerances

The program uses these default tolerances:

| Series | Tolerance |
| --- | --- |
| E3 | ±40% |
| E6 | ±20% |
| E12 | ±10% |
| E24 | ±5% |
| E48 | ±2% |
| E96 | ±1% |
| E192 | ±0.5% |

Override a series tolerance with `--E3 PCT`, `--E6 PCT`, `--E12 PCT`, `--E24 PCT`, `--E48 PCT`, `--E96 PCT`, or `--E192 PCT`. The percentage must be at least zero and less than 100.

```powershell
.\divider.exe --vin-vout 12 3.3 --E 96 --E96 0.1 --series 10k 100k
```

`--E 96` selects the series; `--E96 0.1` assigns ±0.1% tolerance to both resistors. Overriding a tolerance does not select that series by itself.

## Candidate ranking

Percentage errors are relative to the target. Tolerance extremes use opposite resistor variations: maximum upper resistance with minimum lower resistance, and minimum upper resistance with maximum lower resistance.

Candidates are ranked by increasing score:

```text
score = abs(nominal_error) * F
      + (abs(high_error) + abs(low_error)) * (1 - F)
```

`--factor 1` ranks only by nominal error; `--factor 0` ranks by tolerance-extreme errors. Ties favor smaller absolute nominal error, then lower total resistance.

The search sorts resistor values and uses binary searches to narrow lower-resistor intervals according to individual resistance, total resistance, parallel equivalent, unloaded power, and maximum-error filters. It evaluates the remaining candidates and keeps the best results in a priority queue; pruning does not approximate the ranking.

## Reading the output

A table is printed for each selected series:

| Column | Meaning |
| --- | --- |
| `top (Ω)` | Upper resistor, `R_high`. |
| `bottom (Ω)` | Lower resistor, `R_low`. |
| `series (Ω)` | Sum of both resistor values. |
| `parallel (Ω)` | Parallel equivalent of the resistor pair. |
| `nom error (%)` | Signed nominal percentage error. |
| `error range (%)` | Upper error minus lower error, in percentage points. |
| `VOUT min (V)` | Minimum output voltage under resistor tolerances; shown with `--vin-vout`. |
| `VOUT max (V)` | Maximum output voltage under resistor tolerances; shown with `--vin-vout`. |
| `top power (W)` | Nominal upper-resistor power; shown with `--vin-vout`. |
| `bottom power (W)` | Nominal lower-resistor power; shown with `--vin-vout`. |
| `power (W)` | Total nominal resistor power, excluding the load; shown with `--vin-vout`. |
| `nom error (V)` | Nominal output voltage minus the target voltage; shown with `--vin-vout`. |
| `error range (V)` | Maximum output voltage minus minimum output voltage; shown with `--vin-vout`. |

Values use SI prefixes. Without `--digits`, formatting is automatic for each column; with `--digits`, the requested precision applies throughout the table. If no pair in a series satisfies the filters, the program prints `No candidates found.`.

## Input units

Use a decimal point and omit unit suffixes: `3.3`, `10k`, `1M`, or `100m`. Resistance is in ohms, voltage in volts, and power in watts.

| Prefix | Multiplier |
| --- | --- |
| `u`, `U`, `µ`, `μ` | 10⁻⁶ |
| `m` | 10⁻³ |
| `k`, `K` | 10³ |
| `M` | 10⁶ |
| `g`, `G` | 10⁹ |
| `t`, `T` | 10¹² |

Non-finite values (`nan`, `inf`) and numeric input overflow or underflow are rejected.

For example, `1m` with `--power` means `0.001 W`. Lowercase `m` and uppercase `M` have different meanings.

## Build and test

Requirements: CMake 3.20 or newer and a C++20-compatible compiler. No external libraries are required.

On Windows, with Visual Studio and its C++ tools installed, run from a developer shell:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\divider.exe --help
```

Alternatively, use NMake from the Visual Studio developer shell with a separate build directory:

```powershell
cmake -S . -B build-nmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-nmake
ctest --test-dir build-nmake --output-on-failure
.\build-nmake\divider.exe --help
```

With a single-configuration generator, such as Unix Makefiles on Linux:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/divider --help
```

The suite includes 36 test cases covering loaded and unloaded calculations, tolerance extremes, inclusive filters, numeric validation, CLI options, and output formatting. It also compares optimized searches against exhaustive evaluation across a matrix of configurations.

## Model assumptions

Both resistors use the same tolerance, and the load is treated as fixed. Displayed powers and the power filter use nominal values rather than worst-case dissipation under tolerances.

## Project structure

- `src/`: CLI, E series, search, and table implementations.
- `include/divider/`: library interfaces.
- `tests/unit_tests.cpp`: calculation, option, and output tests.
- `CMakeLists.txt`: build and test configuration.
