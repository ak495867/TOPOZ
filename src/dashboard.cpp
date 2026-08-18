#include "topos/dashboard.hpp"
#include "topos/util.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace topos {
namespace {
const std::string cyan = "\033[36m";
const std::string green = "\033[32m";
const std::string yellow = "\033[33m";
const std::string red = "\033[31m";
const std::string magenta = "\033[35m";
const std::string blue = "\033[34m";
const std::string reset = "\033[0m";
const std::string dim = "\033[2m";

std::string status_color(Direction direction) {
    return direction == Direction::Bullish ? green : direction == Direction::Bearish ? red : yellow;
}

std::string line(const std::string& value) { return "  " + value + "\n"; }
}

Dashboard::Dashboard(MarketDataConfig config)
    : config_(std::move(config)), data_manager_(config_), engine_(5, 200) {
    std::vector<double> historical_prices(data_manager_.prices().begin(), data_manager_.prices().end());
    std::vector<double> historical_volumes(data_manager_.volumes().begin(), data_manager_.volumes().end());
    engine_.seed(historical_prices, historical_volumes);
    const auto tick = data_manager_.current();
    if (tick) current_ = engine_.update(tick->price, tick->volume, tick->timestamp);
}

std::string Dashboard::panel(const std::string& title, const std::string& content, const std::string& color) const {
    std::ostringstream output;
    output << color << "+------------------------------------------------------------+\n" << reset;
    output << color << "| " << std::left << std::setw(58) << title << "|\n" << reset;
    output << color << "+------------------------------------------------------------+\n" << reset;
    output << content;
    output << color << "+------------------------------------------------------------+\n" << reset;
    return output.str();
}

std::string Dashboard::render_header(const AnalysisResult& result) const {
    const auto& prediction = result.prediction;
    const auto& singularity = result.singularity;
    std::ostringstream content;
    content << line(config_.symbol + "  |  $" + format_number(result.price, 2) + "  |  " + (data_manager_.simulation_mode() ? yellow + "SIMULATION" + reset : green + "LIVE" + reset));
    content << line("Regime: " + uppercase(to_string(prediction.regime)) + "  Direction: " + status_color(prediction.direction) + to_string(prediction.direction) + reset + "  Singularity: " + (singularity.active ? red : yellow) + to_string(singularity.type) + " [" + format_number(singularity.score, 2) + "]" + reset);
    content << line("Provider: " + data_manager_.provider_status());
    return panel("TOPOΣ | TOPOLOGICAL ORDER PARAMETER & SINGULARITY ENGINE", content.str(), cyan);
}

std::string Dashboard::render_manifold(const AnalysisResult& result) const {
    const auto& metric = result.manifold;
    std::ostringstream content;
    content << line("Curvature Z-Score:    " + format_number(metric.scalar_curvature, 3));
    content << line("Directional Curv:     " + format_number(metric.directional_curvature, 4));
    content << line("Geometry:             " + (metric.directional_curvature > 0.1 ? green + "CONVEX" : metric.directional_curvature < -0.1 ? red + "CONCAVE" : yellow + "FLAT") + reset);
    content << line("Metric Determinant:   " + format_number(metric.metric_determinant, 6));
    content << line("Manifold Energy:      " + format_number(metric.energy, 4));
    content << line("Active Points:        " + std::to_string(metric.active_points));
    return panel("MANIFOLD GEOMETRY", content.str(), magenta);
}

std::string Dashboard::render_topology(const AnalysisResult& result) const {
    const auto& metric = result.topology;
    std::ostringstream content;
    content << line("Betti 0 Components:   " + std::to_string(metric.betti_0));
    content << line("Betti 1 Heuristic:    " + std::to_string(metric.betti_1));
    content << line("Betti 2 Heuristic:    " + std::to_string(metric.betti_2));
    content << line("Fragmentation:        " + bar(metric.fragmentation, 16) + " " + format_number(metric.fragmentation, 2));
    content << line("Complexity:           " + format_number(metric.complexity, 2));
    content << line("Entropy:              " + format_number(metric.entropy, 3));
    return panel("TOPOLOGY", content.str(), blue);
}

std::string Dashboard::render_information(const AnalysisResult& result) const {
    const auto& metric = result.information;
    std::ostringstream content;
    content << line("Distribution Distance: " + bar(metric.distribution_distance / 5.0, 16) + " " + format_number(metric.distribution_distance, 3));
    content << line("State:                " + (metric.distribution_distance > 2.5 ? red + "REGIME SHIFT" : metric.distribution_distance > 1.0 ? yellow + "EVOLVING" : green + "STABLE") + reset);
    content << line("Entropy:              " + format_number(metric.entropy, 4));
    return panel("INFORMATION GEOMETRY", content.str(), green);
}

std::string Dashboard::render_prediction(const AnalysisResult& result) const {
    const auto& prediction = result.prediction;
    std::ostringstream content;
    content << line("Direction:            " + status_color(prediction.direction) + to_string(prediction.direction) + reset);
    content << line("Direction Score:      " + format_number(prediction.direction_score, 4));
    content << line("Strength:             " + bar(prediction.strength) + " " + format_number(prediction.strength, 2));
    content << line("Confidence:           " + bar(prediction.confidence) + " " + format_number(prediction.confidence, 2));
    content << line("Expected Move:        " + format_number(prediction.expected_move_pct, 2) + "%");
    content << line("Regime:               " + uppercase(to_string(prediction.regime)));
    content << line("Hurst Exponent:       " + format_number(prediction.hurst_exponent, 3));
    return panel("PREDICTION", content.str(), cyan);
}

std::string Dashboard::render_singularity(const AnalysisResult& result) const {
    const auto& metric = result.singularity;
    std::ostringstream content;
    content << line("Score:                " + bar(metric.score) + " " + format_number(metric.score, 2));
    content << line("Status:               " + (metric.active ? red + "ACTIVE" : metric.score > 0.3 ? yellow + "MONITORING" : green + "CLEAR") + reset);
    content << line("Signals:");
    if (metric.signals.empty()) content << line("  none detected");
    else for (const auto& signal : metric.signals) content << line("  " + signal);
    return panel("SINGULARITY DETECTION", content.str(), red);
}

std::string Dashboard::render_rg(const AnalysisResult& result) const {
    const auto& metric = result.renormalization;
    std::ostringstream content;
    content << line("Hurst Exponent:       " + format_number(metric.hurst, 4));
    std::ostringstream scales;
    for (double value : metric.hurst_values) scales << "[" << format_number(value, 3) << "] ";
    content << line("Scale Flow:           " + scales.str());
    content << line("Fixed Points:         " + std::to_string(metric.fixed_points));
    content << line("Flow Stability:       " + uppercase(to_string(metric.flow_stability)));
    content << line("Regime:               " + uppercase(to_string(metric.regime)));
    return panel("RENORMALIZATION GROUP FLOW", content.str(), yellow);
}

std::string Dashboard::render_footer(const AnalysisResult&) const {
    std::size_t changes = 0;
    for (std::size_t i = 1; i < direction_history_.size(); ++i) if (direction_history_[i] != direction_history_[i - 1]) ++changes;
    std::ostringstream content;
    content << dim << "Updates: " << std::setw(6) << std::setfill('0') << updates_ << std::setfill(' ') << "  |  Latency: " << std::fixed << std::setprecision(0) << last_latency_ * 1000.0 << "ms  |  Avg: " << average_latency_ * 1000.0 << "ms  |  Direction Changes: " << changes << "  |  Errors: " << errors_ << reset << "\n";
    return content.str();
}

void Dashboard::clear_screen() const { std::cout << "\033[2J\033[H"; }

int Dashboard::run(std::size_t maximum_updates) {
    while (maximum_updates == 0 || updates_ < maximum_updates) {
        const auto start = std::chrono::steady_clock::now();
        const auto tick = data_manager_.current();
        if (!tick) {
            ++errors_;
            std::this_thread::sleep_for(std::chrono::duration<double>(config_.update_interval));
            continue;
        }
        current_ = engine_.update(tick->price, tick->volume, tick->timestamp);
        direction_history_.push_back(current_.prediction.direction);
        if (direction_history_.size() > 20) direction_history_.erase(direction_history_.begin());
        ++updates_;
        const auto end = std::chrono::steady_clock::now();
        last_latency_ = std::chrono::duration<double>(end - start).count();
        average_latency_ = average_latency_ * 0.99 + last_latency_ * 0.01;
        clear_screen();
        std::cout << render_header(current_);
        std::cout << render_manifold(current_);
        std::cout << render_topology(current_);
        std::cout << render_information(current_);
        std::cout << render_prediction(current_);
        std::cout << render_singularity(current_);
        std::cout << render_rg(current_);
        std::cout << render_footer(current_) << std::flush;
        std::this_thread::sleep_for(std::chrono::duration<double>(config_.update_interval));
    }
    data_manager_.cleanup();
    return errors_ == 0 ? 0 : 1;
}

}
