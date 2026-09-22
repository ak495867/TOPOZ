#include "topos/analytics.hpp"
#include "topos/data.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace {

int failures = 0;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        ++failures;
    }
}

}

int main() {
    topos::SimulationProvider provider(topos::MarketDataConfig{});
    require(provider.connect(), "simulation provider connects");
    const auto history = provider.historical(200);
    require(history.size() == 200, "simulation history size");
    require(history.front().price > 0.0, "simulation prices are positive");
    require(provider.current().has_value(), "simulation current tick");

    topos::ToposEngine engine;
    std::vector<double> prices;
    std::vector<double> volumes;
    for (const auto& tick : history) {
        prices.push_back(tick.price);
        volumes.push_back(tick.volume);
    }
    engine.seed(prices, volumes);
    const auto result = engine.update(history.back().price, history.back().volume, history.back().timestamp);
    require(engine.size() == 200, "engine history bound");
    require(std::isfinite(result.manifold.raw_curvature), "finite manifold curvature");
    require(std::isfinite(result.topology.fragmentation), "finite topology fragmentation");
    require(result.prediction.confidence >= 0.0 && result.prediction.confidence <= 1.0, "bounded prediction confidence");
    require(result.singularity.score >= 0.0 && result.singularity.score <= 1.0, "bounded singularity score");

    topos::MarketDataConfig manager_config;
    manager_config.data_source = topos::DataSource::Simulation;
    topos::MarketDataManager manager(manager_config);
    require(manager.simulation_mode(), "manager simulation mode");
    require(manager.prices().size() >= 50, "manager seeds history");
    require(manager.current().has_value(), "manager current tick");

    topos::ToposEngine bounded_engine(5, 10);
    std::vector<double> long_prices(25, 100.0);
    std::vector<double> long_volumes(25, 10.0);
    bounded_engine.seed(long_prices, long_volumes);
    require(bounded_engine.size() == 10, "engine honors configured history size");
    const auto invalid = bounded_engine.update(std::numeric_limits<double>::quiet_NaN(), 10.0, 1.0);
    require(bounded_engine.size() == 10, "invalid update does not mutate history");
    require(!std::isfinite(invalid.price) || invalid.price == 0.0, "invalid update returns empty result");

    topos::ToposEngine constant_engine;
    constant_engine.seed(std::vector<double>(100, 100.0), std::vector<double>(100, 1.0));
    const auto constant = constant_engine.update(100.0, 1.0, 1.0);
    require(std::isfinite(constant.prediction.confidence), "constant series confidence is finite");
    require(std::isfinite(constant.manifold.raw_curvature), "constant series curvature is finite");

    if (failures == 0) std::cout << "All TOPOΣ tests passed\n";
    return failures == 0 ? 0 : 1;
}
