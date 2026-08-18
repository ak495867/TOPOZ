#include "topos/analytics.hpp"
#include "topos/data.hpp"

#include <cmath>
#include <iostream>
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

    if (failures == 0) std::cout << "All TOPOΣ tests passed\n";
    return failures == 0 ? 0 : 1;
}
