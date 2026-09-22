#include "topos/types.hpp"
#include "topos/util.hpp"

#include <algorithm>
#include <cctype>
#include <numeric>

namespace topos {

std::string to_string(DataSource source) {
    switch (source) {
        case DataSource::Simulation: return "simulation";
        case DataSource::YahooFinance: return "yahoo_finance";
        case DataSource::CoinGecko: return "coingecko";
        case DataSource::AlphaVantage: return "alpha_vantage";
        case DataSource::TwelveData: return "twelve_data";
    }
    return "simulation";
}

std::string to_string(AssetType type) {
    switch (type) {
        case AssetType::Stock: return "stock";
        case AssetType::Crypto: return "crypto";
        case AssetType::Forex: return "forex";
        case AssetType::Index: return "index";
        case AssetType::Commodity: return "commodity";
    }
    return "crypto";
}

std::string to_string(SingularityType type) {
    switch (type) {
        case SingularityType::None: return "NONE";
        case SingularityType::Dormant: return "DORMANT";
        case SingularityType::Building: return "BUILDING";
        case SingularityType::Active: return "ACTIVE";
    }
    return "NONE";
}

std::string to_string(Direction direction) {
    switch (direction) {
        case Direction::Neutral: return "NEUTRAL";
        case Direction::Bullish: return "BULLISH";
        case Direction::Bearish: return "BEARISH";
    }
    return "NEUTRAL";
}

std::string to_string(MarketRegime regime) {
    switch (regime) {
        case MarketRegime::Initializing: return "initializing";
        case MarketRegime::Trending: return "trending";
        case MarketRegime::MeanReverting: return "mean_reverting";
        case MarketRegime::RandomWalk: return "random_walk";
    }
    return "initializing";
}

std::string to_string(FlowStability stability) {
    switch (stability) {
        case FlowStability::Unknown: return "unknown";
        case FlowStability::Stable: return "stable";
        case FlowStability::Critical: return "critical";
        case FlowStability::Transient: return "transient";
    }
    return "unknown";
}

std::optional<DataSource> data_source_from_string(const std::string& value) {
    if (value == "simulation") return DataSource::Simulation;
    if (value == "yahoo_finance") return DataSource::YahooFinance;
    if (value == "coingecko") return DataSource::CoinGecko;
    if (value == "alpha_vantage") return DataSource::AlphaVantage;
    if (value == "twelve_data") return DataSource::TwelveData;
    return std::nullopt;
}

std::optional<AssetType> asset_type_from_string(const std::string& value) {
    if (value == "stock") return AssetType::Stock;
    if (value == "crypto") return AssetType::Crypto;
    if (value == "forex") return AssetType::Forex;
    if (value == "index") return AssetType::Index;
    if (value == "commodity") return AssetType::Commodity;
    return std::nullopt;
}

double now_seconds() {
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration<double>(now.time_since_epoch()).count();
}

double clamp(double value, double low, double high) {
    return std::max(low, std::min(value, high));
}

bool is_finite_positive(double value) {
    return std::isfinite(value) && value > 0.0;
}

double mean(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    return std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
}

double standard_deviation(const std::vector<double>& values) {
    if (values.size() < 2) return 0.0;
    const double m = mean(values);
    double sum = 0.0;
    for (double value : values) sum += (value - m) * (value - m);
    return std::sqrt(sum / static_cast<double>(values.size()));
}

double percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const double position = clamp(p, 0.0, 1.0) * static_cast<double>(values.size() - 1);
    const auto lower = static_cast<std::size_t>(position);
    const auto upper = std::min(lower + 1, values.size() - 1);
    const double fraction = position - static_cast<double>(lower);
    return values[lower] + fraction * (values[upper] - values[lower]);
}

std::string uppercase(std::string value) {
    for (char& character : value) character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    return value;
}

std::string format_number(double value, int precision) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(precision) << value;
    return output.str();
}

std::string bar(double value, std::size_t width, double maximum) {
    const double normalized = clamp(value / maximum, 0.0, 1.0);
    const std::size_t filled = static_cast<std::size_t>(normalized * static_cast<double>(width));
    return std::string(filled, '#') + std::string(width - filled, '-');
}

}
