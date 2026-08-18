#include "topos/dashboard.hpp"
#include "topos/util.hpp"

#include <cstdlib>
#include <iostream>
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
    std::cout << "  --no-fallback        Disable simulation fallback\n";
    std::cout << "  --help               Show this message\n";
}

std::optional<std::string> next_value(int& index, int argc, char** argv) {
    if (index + 1 >= argc) return std::nullopt;
    ++index;
    return std::string(argv[index]);
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
        const auto value = next_value(index, argc, argv);
        if (!value) {
            std::cerr << "Missing value for " << argument << "\n";
            return 2;
        }
        if (argument == "--symbol") config.symbol = *value;
        else if (argument == "--source") {
            const auto source = topos::data_source_from_string(*value);
            if (!source) { std::cerr << "Invalid data source\n"; return 2; }
            config.data_source = *source;
        } else if (argument == "--asset-type") {
            const auto asset_type = topos::asset_type_from_string(*value);
            if (!asset_type) { std::cerr << "Invalid asset type\n"; return 2; }
            config.asset_type = *asset_type;
        } else if (argument == "--api-key") config.api_key = *value;
        else if (argument == "--interval") config.update_interval = std::stod(*value);
        else if (argument == "--updates") updates = static_cast<std::size_t>(std::stoull(*value));
        else {
            std::cerr << "Unknown option: " << argument << "\n";
            return 2;
        }
    }
    if (config.api_key.empty()) {
        if (const char* value = std::getenv("ALPHA_VANTAGE_API_KEY"); value && config.data_source == topos::DataSource::AlphaVantage) config.api_key = value;
        if (const char* value = std::getenv("TWELVE_DATA_API_KEY"); value && config.data_source == topos::DataSource::TwelveData) config.api_key = value;
    }
    try {
        topos::Dashboard dashboard(config);
        return dashboard.run(updates);
    } catch (const std::exception& error) {
        std::cerr << "Startup failure: " << error.what() << "\n";
        return 1;
    }
}
