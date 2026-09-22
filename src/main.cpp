#include "topos/dashboard.hpp"
#include "topos/util.hpp"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <string>

namespace {

void print_help() {
    std::cout << "TOPOΣ market structure analyzer\n\n";
    std::cout << "Usage: topos [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --symbol VALUE       Instrument symbol\n";
    std::cout << "  --source VALUE       simulation, yahoo_finance, coingecko, alpha_vantage, twelve_data\n";
    std::cout << "  --asset-type VALUE   stock, crypto, forex, index, commodity\n";
    std::cout << "  --api-key VALUE      Provider API key\n";
    std::cout << "  --interval VALUE     Update interval in seconds\n";
    std::cout << "  --updates VALUE      Stop after a fixed number of updates\n";
    std::cout << "  --max-errors VALUE   Maximum consecutive provider errors\n";
    std::cout << "  --output VALUE       dashboard, json, or csv\n";
    std::cout << "  --no-fallback        Disable simulation fallback\n";
    std::cout << "  --help               Show this message\n";
}

std::optional<std::string> next_value(int& index, int argc, char** argv) {
    if (index + 1 >= argc) return std::nullopt;
    ++index;
    return std::string(argv[index]);
}

std::optional<double> parse_double(const std::string& value) {
    try {
        std::size_t consumed = 0;
        const double parsed = std::stod(value, &consumed);
        if (consumed != value.size()) return std::nullopt;
        return parsed;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::size_t> parse_size(const std::string& value) {
    try {
        std::size_t consumed = 0;
        const auto parsed = std::stoull(value, &consumed);
        if (consumed != value.size() || parsed > std::numeric_limits<std::size_t>::max()) return std::nullopt;
        return static_cast<std::size_t>(parsed);
    } catch (...) {
        return std::nullopt;
    }
}

bool takes_value(const std::string& argument) {
    return argument == "--symbol" || argument == "--source" || argument == "--asset-type" || argument == "--api-key" || argument == "--interval" || argument == "--updates" || argument == "--max-errors" || argument == "--output";
}

}

int main(int argc, char** argv) {
    topos::MarketDataConfig config;
    std::size_t updates = 0;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help") {
            print_help();
            return 0;
        }
        if (argument == "--no-fallback") {
            config.fallback_to_simulation = false;
            continue;
        }
        if (!takes_value(argument)) {
            std::cerr << "Unknown option: " << argument << "\n";
            return 2;
        }
        const auto value = next_value(index, argc, argv);
        if (!value) {
            std::cerr << "Missing value for " << argument << "\n";
            return 2;
        }
        if (argument == "--symbol") {
            config.symbol = *value;
        } else if (argument == "--source") {
            const auto source = topos::data_source_from_string(*value);
            if (!source) { std::cerr << "Invalid data source\n"; return 2; }
            config.data_source = *source;
        } else if (argument == "--asset-type") {
            const auto asset_type = topos::asset_type_from_string(*value);
            if (!asset_type) { std::cerr << "Invalid asset type\n"; return 2; }
            config.asset_type = *asset_type;
        } else if (argument == "--api-key") {
            config.api_key = *value;
        } else if (argument == "--interval") {
            const auto parsed = parse_double(*value);
            if (!parsed) { std::cerr << "Invalid interval: " << *value << "\n"; return 2; }
            config.update_interval = *parsed;
        } else if (argument == "--updates") {
            const auto parsed = parse_size(*value);
            if (!parsed) { std::cerr << "Invalid update count: " << *value << "\n"; return 2; }
            updates = *parsed;
        } else if (argument == "--max-errors") {
            const auto parsed = parse_size(*value);
            if (!parsed) { std::cerr << "Invalid max errors: " << *value << "\n"; return 2; }
            config.max_consecutive_errors = *parsed;
        } else if (argument == "--output") {
            if (*value != "dashboard" && *value != "json" && *value != "csv") { std::cerr << "Invalid output mode: " << *value << "\n"; return 2; }
            config.output_mode = *value;
        }
    }
    if (config.symbol.empty()) { std::cerr << "Symbol must not be empty\n"; return 2; }
    if (!topos::is_finite_positive(config.update_interval)) { std::cerr << "Interval must be finite and greater than zero\n"; return 2; }
    if (config.max_consecutive_errors == 0) { std::cerr << "Max errors must be greater than zero\n"; return 2; }
    if (config.api_key.empty()) {
        if (const char* value = std::getenv("ALPHA_VANTAGE_API_KEY"); value && config.data_source == topos::DataSource::AlphaVantage) config.api_key = value;
        if (const char* value = std::getenv("TWELVE_DATA_API_KEY"); value && config.data_source == topos::DataSource::TwelveData) config.api_key = value;
    }
    try {
        std::signal(SIGINT, [](int) { topos::Dashboard::request_stop(); });
        std::signal(SIGTERM, [](int) { topos::Dashboard::request_stop(); });
        topos::Dashboard dashboard(config);
        return dashboard.run(updates);
    } catch (const std::exception& error) {
        std::cerr << "Startup failure: " << error.what() << "\n";
        return 1;
    }
}
