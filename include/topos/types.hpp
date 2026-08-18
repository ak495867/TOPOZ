#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace topos {

enum class DataSource { Simulation, YahooFinance, CoinGecko, AlphaVantage, TwelveData };
enum class AssetType { Stock, Crypto, Forex, Index, Commodity };

enum class SingularityType { None, Dormant, Building, Active };

enum class Direction { Neutral, Bullish, Bearish };

enum class MarketRegime { Initializing, Trending, MeanReverting, RandomWalk };

enum class FlowStability { Unknown, Stable, Critical, Transient };

struct MarketDataConfig {
    std::string symbol = "BTC-USD";
    AssetType asset_type = AssetType::Crypto;
    DataSource data_source = DataSource::Simulation;
    double update_interval = 0.5;
    std::string api_key;
    bool use_websocket = false;
    bool fallback_to_simulation = true;
};

struct MarketTick {
    double price = 0.0;
    double volume = 0.0;
    double timestamp = 0.0;
    double high = 0.0;
    double low = 0.0;
    double open = 0.0;
};

struct ManifoldMetrics {
    double scalar_curvature = 0.0;
    double raw_curvature = 0.0;
    double directional_curvature = 0.0;
    double metric_determinant = 0.0;
    double energy = 0.0;
    std::size_t manifold_dimension = 5;
    std::size_t active_points = 0;
};

struct TopologyMetrics {
    int betti_0 = 0;
    int betti_1 = 0;
    int betti_2 = 0;
    double entropy = 0.0;
    double complexity = 0.0;
    double fragmentation = 0.0;
};

struct InformationMetrics {
    double distribution_distance = 0.0;
    double entropy = 0.0;
};

struct RenormalizationMetrics {
    MarketRegime regime = MarketRegime::Initializing;
    double hurst = 0.5;
    int fixed_points = 0;
    FlowStability flow_stability = FlowStability::Unknown;
    std::vector<double> hurst_values{0.5};
};

struct SingularityMetrics {
    double score = 0.0;
    SingularityType type = SingularityType::None;
    std::vector<std::string> signals;
    bool active = false;
};

struct PredictionMetrics {
    Direction direction = Direction::Neutral;
    double strength = 0.0;
    double confidence = 0.0;
    double expected_move_pct = 0.0;
    MarketRegime regime = MarketRegime::Initializing;
    double hurst_exponent = 0.5;
    double direction_score = 0.0;
};

struct AnalysisResult {
    double timestamp = 0.0;
    double price = 0.0;
    double volume = 0.0;
    ManifoldMetrics manifold;
    TopologyMetrics topology;
    InformationMetrics information;
    RenormalizationMetrics renormalization;
    SingularityMetrics singularity;
    PredictionMetrics prediction;
};

std::string to_string(DataSource source);
std::string to_string(AssetType type);
std::string to_string(SingularityType type);
std::string to_string(Direction direction);
std::string to_string(MarketRegime regime);
std::string to_string(FlowStability stability);
std::optional<DataSource> data_source_from_string(const std::string& value);
std::optional<AssetType> asset_type_from_string(const std::string& value);

}
