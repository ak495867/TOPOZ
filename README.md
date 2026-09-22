# TOPOΣ

## Topological Order Parameter and Singularity Engine

TOPOΣ is a C++17 market-structure analysis terminal application inspired by geometric, information-theoretic, topological, and scaling-based descriptions of financial time series. It ingests market observations, constructs a causal rolling feature representation, computes exploratory structure metrics, detects regime-transition conditions, and presents the result through a live terminal dashboard.

The project is designed for research, education, prototyping, and systems experimentation. It is not investment advice, a brokerage system, or a guarantee of market performance.

## Highlights

| Capability | Description |
|---|---|
| Market-data abstraction | Common provider interface for simulation, Yahoo Finance, CoinGecko, Alpha Vantage, and Twelve Data. |
| Deterministic simulation | Seeded historical data and evolving synthetic regimes for offline development. |
| Feature pipeline | Causal rolling returns, volume pressure, volatility, and price-position features. |
| Geometric analysis | Covariance-based metric, curvature proxy, directional path curvature, determinant, and energy. |
| Information analysis | Distribution shift, scale, skewness, and entropy proxies. |
| Topological analysis | Connectivity filtration and fragmentation metrics inspired by persistent homology. |
| Scaling analysis | Rescaled-range Hurst estimation and multiscale flow labels. |
| Signal synthesis | Rule-based direction, strength, confidence score, expected move score, and singularity status. |
| Terminal dashboard | ANSI terminal interface with live metrics, status, latency, and provider state. |
| Structured output | Dashboard, JSON, and CSV output modes for scripts and monitoring. |
| Portable build | CMake project using C++17, nlohmann-json, and optional libcurl integration for HTTP providers. |

## Repository layout

```text
TOPOΣ/
├── .github/workflows/ci.yml
├── include/topos/
│   ├── analytics.hpp
│   ├── dashboard.hpp
│   ├── data.hpp
│   ├── types.hpp
│   └── util.hpp
├── src/
│   ├── analytics.cpp
│   ├── data.cpp
│   ├── dashboard.cpp
│   ├── main.cpp
│   └── util.cpp
├── tests/test_topos.cpp
├── CMakeLists.txt
├── LICENSE
├── README.md
└── .gitignore
```

## Requirements

The minimum build requirement is a compiler with C++17 support and CMake 3.20 or newer. GCC 9 or newer, Clang 10 or newer, and recent MSVC toolchains should be suitable. Live HTTP providers require libcurl at build time. The application remains fully usable in simulation mode without libcurl, API keys, or network connectivity.

### Ubuntu and Debian

```bash
sudo apt update
sudo apt install -y build-essential cmake libcurl4-openssl-dev nlohmann-json3-dev
```

### macOS

```bash
brew install cmake curl
```

### Windows

Install Visual Studio 2022 with the C++ desktop workload and CMake support. Install libcurl separately only when live HTTP providers are required.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

To build without libcurl detection:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DTOPOΣ_ENABLE_CURL=OFF
cmake --build build --parallel
```

Equivalent preset builds are available:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

The tests cover simulation connectivity, historical seeding, engine bounds, invalid-input rejection, finite analytical outputs, score ranges, and manager operation. CI additionally exercises GCC, Clang, curl-enabled and no-curl builds, structured-output smoke tests, and AddressSanitizer/UndefinedBehaviorSanitizer.

## Run

Run a finite smoke session:

```bash
./build/topos --source simulation --symbol SIM-BTC --interval 0.05 --updates 20
```

Run continuously in simulation mode:

```bash
./build/topos --source simulation --symbol SIM-BTC --interval 0.5
```

For automation, use structured output:

```bash
./build/topos --source simulation --interval 0.05 --updates 20 --output json
./build/topos --source simulation --interval 0.05 --updates 20 --output csv
```

The terminal dashboard uses ANSI escape sequences. Run it in a terminal emulator with ANSI support for the intended display.

The CLI validates finite positive intervals and bounded retry settings. Provider failures terminate cleanly when `--no-fallback` is selected; otherwise the manager falls back to simulation while retaining the original provider error. Provider responses are parsed with `nlohmann::json`, and invalid market ticks are rejected before entering the analytical engine.

## Live data providers

The following command examples select the supported providers:

```bash
./build/topos --symbol BTC-USD --source yahoo_finance --interval 5
./build/topos --symbol BTC-USD --source coingecko --interval 10
./build/topos --symbol AAPL --source alpha_vantage --api-key "$ALPHA_VANTAGE_API_KEY" --interval 60
./build/topos --symbol AAPL --source twelve_data --api-key "$TWELVE_DATA_API_KEY" --interval 10
```

The program falls back to simulation by default when a provider cannot connect or stops returning data. Disable that behavior with `--no-fallback` when a failed live connection should terminate the session through the normal error path.

API credentials should be supplied through environment variables or a protected shell session. Do not commit credentials to the repository.

## Analytical model

TOPOΣ maintains bounded rolling price and volume histories. Once at least 50 observations are available, it computes four causal features for each observation:

1. Standardized log return.
2. Volume relative to a trailing moving average.
3. Trailing return volatility relative to recent volatility.
4. Current price position within its trailing 30-observation range.

The features are projected into a five-dimensional normalized representation. The engine then calculates exploratory geometric, information, connectivity, and scaling metrics. A rule-based synthesis layer combines directional curvature, momentum, Hurst regime, fragmentation, flow stability, and singularity score.

The names used by the application are intentionally descriptive of the research inspiration. Several values are operational proxies rather than complete implementations of formal differential geometry, information geometry, persistent homology, or renormalization-group theory. Users should validate the metrics against clearly defined targets and historical data before drawing conclusions.

## Design principles

The implementation keeps the core analytical engine independent from the dashboard. Data providers implement a common interface. The simulation provider enables deterministic testing. Histories are bounded to control memory use. Provider health is surfaced to the terminal. Optional libcurl support keeps the baseline build portable while allowing live HTTP integrations when the dependency is available.


## License

This project is distributed under the MIT License. See [LICENSE](LICENSE).

## Disclaimer

TOPOΣ is experimental software for market-data analysis. It does not provide financial, investment, legal, tax, or trading advice. The output is not a forecast guarantee. Users are responsible for validating data quality, model behavior, risk assumptions, and regulatory obligations before using the software in any operational context.
