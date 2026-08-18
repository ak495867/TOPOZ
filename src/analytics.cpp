#include "topos/analytics.hpp"
#include "topos/util.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <random>

namespace topos {
namespace {

std::vector<double> column(const Matrix& matrix, std::size_t index) {
    std::vector<double> values;
    values.reserve(matrix.size());
    for (const auto& row : matrix) if (index < row.size()) values.push_back(row[index]);
    return values;
}

double distance(const std::vector<double>& left, const std::vector<double>& right) {
    const std::size_t count = std::min(left.size(), right.size());
    double sum = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double difference = left[i] - right[i];
        sum += difference * difference;
    }
    return std::sqrt(sum);
}

std::vector<double> solve_3x3(double matrix[3][4]) {
    for (int pivot = 0; pivot < 3; ++pivot) {
        int best = pivot;
        for (int row = pivot + 1; row < 3; ++row) if (std::abs(matrix[row][pivot]) > std::abs(matrix[best][pivot])) best = row;
        for (int col = pivot; col < 4; ++col) std::swap(matrix[pivot][col], matrix[best][col]);
        if (std::abs(matrix[pivot][pivot]) < 1e-12) return {0.0, 0.0, 0.0};
        const double divisor = matrix[pivot][pivot];
        for (int col = pivot; col < 4; ++col) matrix[pivot][col] /= divisor;
        for (int row = 0; row < 3; ++row) {
            if (row == pivot) continue;
            const double factor = matrix[row][pivot];
            for (int col = pivot; col < 4; ++col) matrix[row][col] -= factor * matrix[pivot][col];
        }
    }
    return {matrix[0][3], matrix[1][3], matrix[2][3]};
}

}

Matrix ManifoldGeometry::metric_tensor(const Matrix& points) {
    if (points.empty()) return Matrix(1, std::vector<double>(1, 0.1));
    const std::size_t dimension = points.front().size();
    Matrix metric(dimension, std::vector<double>(dimension, 0.0));
    if (points.size() < 3) {
        for (std::size_t i = 0; i < dimension; ++i) metric[i][i] = 0.1;
        return metric;
    }
    for (std::size_t dimension_index = 0; dimension_index < dimension; ++dimension_index) {
        std::vector<double> differences;
        for (std::size_t row = 1; row < points.size(); ++row) differences.push_back(points[row][dimension_index] - points[row - 1][dimension_index]);
        metric[dimension_index][dimension_index] = standard_deviation(differences) * standard_deviation(differences) + 0.05;
    }
    return metric;
}

double ManifoldGeometry::scalar_curvature(const Matrix& metric) {
    if (metric.empty()) return 0.0;
    double total = 0.0;
    for (std::size_t i = 0; i < metric.size(); ++i) total += std::log(std::max(std::abs(metric[i][i]), 1e-12));
    return total / static_cast<double>(metric.size());
}

double ManifoldGeometry::curvature_direction(const std::vector<double>& prices) {
    if (prices.size() < 10) return 0.0;
    const std::size_t start = prices.size() > 50 ? prices.size() - 50 : 0;
    const std::size_t count = prices.size() - start;
    double normalizer = 0.0;
    for (std::size_t i = 0; i < count; ++i) normalizer += prices[start + i];
    const double center_price = normalizer / static_cast<double>(count);
    double variance = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double difference = prices[start + i] - center_price;
        variance += difference * difference;
    }
    const double price_scale = std::sqrt(variance / static_cast<double>(count)) + 1e-10;
    double normal_x = 0.0;
    for (std::size_t i = 0; i < count; ++i) normal_x += static_cast<double>(i);
    normal_x /= static_cast<double>(count);
    double x_variance = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double difference = static_cast<double>(i) - normal_x;
        x_variance += difference * difference;
    }
    const double x_scale = std::sqrt(x_variance / static_cast<double>(count)) + 1e-10;
    double normal[3][4]{};
    for (std::size_t i = 0; i < count; ++i) {
        const double x = (static_cast<double>(i) - normal_x) / x_scale;
        const double y = (prices[start + i] - center_price) / price_scale;
        const double basis[3]{x * x, x, 1.0};
        for (int row = 0; row < 3; ++row) for (int col = 0; col < 3; ++col) normal[row][col] += basis[row] * basis[col];
        for (int row = 0; row < 3; ++row) normal[row][3] += basis[row] * y;
    }
    return 2.0 * solve_3x3(normal)[0];
}

double ManifoldGeometry::metric_determinant(const Matrix& metric) {
    if (metric.empty()) return 1.0;
    double result = 1.0;
    for (std::size_t i = 0; i < metric.size(); ++i) result *= metric[i][i];
    return result;
}

double InformationGeometry::distribution_distance(const Matrix& past, const Matrix& recent) {
    if (past.size() < 5 || recent.size() < 5 || past.empty() || recent.empty()) return 0.0;
    const std::size_t dimension = std::min(past.front().size(), recent.front().size());
    double mean_distance = 0.0;
    double scale_distance = 0.0;
    double skew_distance = 0.0;
    for (std::size_t d = 0; d < dimension; ++d) {
        const auto left = column(past, d);
        const auto right = column(recent, d);
        const double left_mean = mean(left);
        const double right_mean = mean(right);
        const double left_std = standard_deviation(left) + 1e-8;
        const double right_std = standard_deviation(right) + 1e-8;
        double left_skew = 0.0;
        double right_skew = 0.0;
        for (double value : left) left_skew += std::pow((value - left_mean) / left_std, 3.0);
        for (double value : right) right_skew += std::pow((value - right_mean) / right_std, 3.0);
        left_skew /= static_cast<double>(left.size());
        right_skew /= static_cast<double>(right.size());
        mean_distance += (left_mean - right_mean) * (left_mean - right_mean);
        scale_distance += std::pow(std::log(left_std) - std::log(right_std), 2.0);
        skew_distance += (left_skew - right_skew) * (left_skew - right_skew);
    }
    return std::sqrt(mean_distance) + 0.5 * std::sqrt(scale_distance) + 0.25 * std::sqrt(skew_distance);
}

double InformationGeometry::entropy(const Matrix& samples) {
    if (samples.size() < 5 || samples.empty()) return 0.0;
    double result = 0.0;
    for (std::size_t d = 0; d < samples.front().size(); ++d) {
        const auto values = column(samples, d);
        const double deviation = standard_deviation(values) + 1e-8;
        result += 0.5 * (1.0 + std::log(2.0 * std::acos(-1.0) * deviation * deviation));
    }
    return clamp(result, -5.0, 5.0);
}

TopologyMetrics PersistentHomology::compute(const Matrix& points) {
    TopologyMetrics result;
    const std::size_t n = points.size();
    if (n < 5) return result;
    struct Edge { double distance; std::size_t left; std::size_t right; };
    std::vector<Edge> edges;
    edges.reserve(n * (n - 1) / 2);
    for (std::size_t i = 0; i < n; ++i) for (std::size_t j = i + 1; j < n; ++j) edges.push_back({distance(points[i], points[j]), i, j});
    std::sort(edges.begin(), edges.end(), [](const Edge& left, const Edge& right) { return left.distance < right.distance; });
    std::vector<std::size_t> parent(n), size(n, 1);
    std::iota(parent.begin(), parent.end(), 0);
    const auto find = [&parent](std::size_t value) {
        std::size_t root = value;
        while (parent[root] != root) root = parent[root];
        while (parent[value] != value) {
            const auto next = parent[value];
            parent[value] = root;
            value = next;
        }
        return root;
    };
    std::vector<double> birth(n, 0.0);
    std::vector<double> lifetimes;
    for (const auto& edge : edges) {
        std::size_t left = find(edge.left);
        std::size_t right = find(edge.right);
        if (left == right) continue;
        const std::size_t elder = size[left] >= size[right] ? left : right;
        const std::size_t younger = elder == left ? right : left;
        lifetimes.push_back(edge.distance - birth[younger]);
        parent[younger] = elder;
        size[elder] += size[younger];
        birth[elder] = std::min(birth[elder], birth[younger]);
    }
    std::vector<double> finite;
    for (double lifetime : lifetimes) if (lifetime > 1e-10) finite.push_back(lifetime);
    if (finite.empty()) return result;
    const double first_threshold = percentile(finite, 0.25);
    const double second_threshold = percentile(finite, 0.75);
    result.betti_0 = std::max(1, static_cast<int>(std::count_if(lifetimes.begin(), lifetimes.end(), [first_threshold](double lifetime) { return lifetime > first_threshold; })));
    result.betti_1 = std::max(0, static_cast<int>(std::count_if(lifetimes.begin(), lifetimes.end(), [first_threshold, second_threshold](double lifetime) { return lifetime > first_threshold && lifetime <= second_threshold; })));
    result.betti_2 = std::max(0, static_cast<int>(finite.size()) - result.betti_0 - result.betti_1);
    const double total = std::accumulate(finite.begin(), finite.end(), 0.0);
    for (double lifetime : finite) {
        const double probability = std::max(lifetime / total, 1e-15);
        result.entropy -= probability * std::log(probability);
    }
    result.complexity = (result.betti_0 + 2.0 * result.betti_1 + 3.0 * result.betti_2) * (1.0 + result.entropy);
    result.fragmentation = clamp(static_cast<double>(result.betti_0) / std::max(static_cast<double>(n) * 0.5, 1.0), 0.0, 1.0);
    return result;
}

double RenormalizationGroup::hurst_exponent(const std::vector<double>& series) {
    if (series.size() < 20) return 0.5;
    const std::array<int, 10> candidates{4, 5, 6, 8, 10, 12, 16, 20, 25, 32};
    std::vector<double> x;
    std::vector<double> y;
    for (const int lag : candidates) {
        if (static_cast<std::size_t>(lag) >= series.size() / 3) continue;
        const std::size_t windows = series.size() / static_cast<std::size_t>(lag);
        if (windows < 3) continue;
        double rs_total = 0.0;
        std::size_t valid = 0;
        for (std::size_t window_index = 0; window_index < windows; ++window_index) {
            std::vector<double> window(series.begin() + static_cast<std::ptrdiff_t>(window_index * lag), series.begin() + static_cast<std::ptrdiff_t>((window_index + 1) * lag));
            const double window_mean = mean(window);
            double cumulative = 0.0;
            double minimum = 0.0;
            double maximum = 0.0;
            for (double value : window) {
                cumulative += value - window_mean;
                minimum = std::min(minimum, cumulative);
                maximum = std::max(maximum, cumulative);
            }
            const double deviation = standard_deviation(window);
            if (deviation > 1e-12) {
                rs_total += (maximum - minimum) / deviation;
                ++valid;
            }
        }
        if (valid > 0) {
            x.push_back(std::log(static_cast<double>(lag)));
            y.push_back(std::log(std::max(rs_total / static_cast<double>(valid), 1e-15)));
        }
    }
    if (x.size() < 4) return 0.5;
    const double x_mean = mean(x);
    const double y_mean = mean(y);
    double numerator = 0.0;
    double denominator = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        numerator += (x[i] - x_mean) * (y[i] - y_mean);
        denominator += (x[i] - x_mean) * (x[i] - x_mean);
    }
    return clamp(denominator < 1e-12 ? 0.5 : numerator / denominator, 0.1, 0.95);
}

RenormalizationMetrics RenormalizationGroup::compute_flow(const std::vector<double>& prices, std::size_t scales) {
    RenormalizationMetrics result;
    if (prices.size() < 50) return result;
    std::vector<double> returns;
    for (std::size_t i = 1; i < prices.size(); ++i) returns.push_back(std::log(std::max(prices[i], 1e-10) / std::max(prices[i - 1], 1e-10)));
    for (std::size_t scale = 0; scale < scales && returns.size() >= 20; ++scale) {
        result.hurst_values.push_back(hurst_exponent(returns));
        const std::size_t next_size = returns.size() / 2;
        if (next_size < 10) break;
        std::vector<double> next;
        next.reserve(next_size);
        for (std::size_t i = 0; i < next_size; ++i) next.push_back((returns[2 * i] + returns[2 * i + 1]) / 2.0);
        returns = std::move(next);
    }
    if (result.hurst_values.size() <= 1) return result;
    result.hurst = result.hurst_values[1];
    if (result.hurst > 0.58) result.regime = MarketRegime::Trending;
    else if (result.hurst < 0.42) result.regime = MarketRegime::MeanReverting;
    else result.regime = MarketRegime::RandomWalk;
    for (std::size_t i = 2; i < result.hurst_values.size(); ++i) if (std::abs(result.hurst_values[i] - result.hurst_values[i - 1]) < 0.02) ++result.fixed_points;
    result.flow_stability = result.fixed_points >= 2 ? FlowStability::Stable : result.fixed_points == 1 ? FlowStability::Critical : FlowStability::Transient;
    return result;
}

ToposEngine::ToposEngine(std::size_t embedding_dimension, std::size_t history_size)
    : embedding_dimension_(embedding_dimension), history_size_(history_size), projection_(embedding_dimension, std::vector<double>(4, 0.0)) {
    for (std::size_t row = 0; row < embedding_dimension_; ++row) for (std::size_t column = 0; column < 4; ++column) projection_[row][column] = std::sin(static_cast<double>((row + 1) * (column + 2))) * 0.3;
}

void ToposEngine::seed(const std::vector<double>& prices, const std::vector<double>& volumes, const std::vector<double>& timestamps) {
    const std::size_t count = std::min(prices.size(), volumes.size());
    for (std::size_t i = count > history_size_ ? count - history_size_ : 0; i < count; ++i) {
        prices_.push_back(prices[i]);
        volumes_.push_back(volumes[i]);
        timestamps_.push_back(i < timestamps.size() ? timestamps[i] : now_seconds());
    }
}

Matrix ToposEngine::extract_features() const {
    const std::size_t n = prices_.size();
    Matrix features(n, std::vector<double>(4, 0.0));
    if (n < 2) return features;
    std::vector<double> returns(n, 0.0);
    for (std::size_t i = 1; i < n; ++i) returns[i] = std::log(std::max(prices_[i], 1e-10) / std::max(prices_[i - 1], 1e-10));
    const std::size_t recent_start = n > 50 ? n - 50 : 1;
    std::vector<double> recent_returns(returns.begin() + static_cast<std::ptrdiff_t>(recent_start), returns.end());
    const double return_scale = standard_deviation(recent_returns) + 1e-10;
    for (std::size_t i = 1; i < n; ++i) features[i][0] = clamp(returns[i] / return_scale, -3.0, 3.0);
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t start = i > 19 ? i - 19 : 0;
        std::vector<double> window;
        for (std::size_t j = start; j <= i; ++j) window.push_back(volumes_[j]);
        features[i][1] = clamp(volumes_[i] / (mean(window) + 1e-10), 0.1, 5.0);
        const std::size_t return_start = i > 29 ? i - 29 : 1;
        std::vector<double> volatility_window;
        if (i >= 1) volatility_window.assign(returns.begin() + static_cast<std::ptrdiff_t>(return_start), returns.begin() + static_cast<std::ptrdiff_t>(i + 1));
        features[i][2] = clamp(standard_deviation(volatility_window) / return_scale, 0.0, 5.0);
        const std::size_t price_start = i > 29 ? i - 29 : 0;
        double minimum = prices_[price_start];
        double maximum = prices_[price_start];
        for (std::size_t j = price_start; j <= i; ++j) {
            minimum = std::min(minimum, prices_[j]);
            maximum = std::max(maximum, prices_[j]);
        }
        features[i][3] = maximum > minimum + 1e-10 ? clamp((prices_[i] - minimum) / (maximum - minimum), 0.0, 1.0) : 0.5;
    }
    return features;
}

Matrix ToposEngine::embed(const Matrix& features) const {
    Matrix result(features.size(), std::vector<double>(embedding_dimension_, 0.0));
    for (std::size_t i = 0; i < features.size(); ++i) {
        double norm = 0.0;
        for (std::size_t row = 0; row < embedding_dimension_; ++row) for (std::size_t column = 0; column < 4; ++column) result[i][row] += features[i][column] * projection_[row][column];
        for (double value : result[i]) norm += value * value;
        norm = std::sqrt(norm);
        for (double& value : result[i]) value = value * std::sqrt(static_cast<double>(embedding_dimension_)) / std::max(norm, 1e-10);
    }
    return result;
}

ManifoldMetrics ToposEngine::analyze_manifold(const Matrix& embedded, double, double, const std::vector<double>& prices) {
    const std::size_t start = embedded.size() > 50 ? embedded.size() - 50 : 0;
    Matrix recent(embedded.begin() + static_cast<std::ptrdiff_t>(start), embedded.end());
    const Matrix metric = ManifoldGeometry::metric_tensor(recent);
    const double curvature = ManifoldGeometry::scalar_curvature(metric);
    const double directional = ManifoldGeometry::curvature_direction(prices);
    curvature_history_.push_back(curvature);
    directional_curvature_history_.push_back(directional);
    if (curvature_history_.size() > 100) curvature_history_.pop_front();
    if (directional_curvature_history_.size() > 100) directional_curvature_history_.pop_front();
    if (curvature_history_.size() > 20) {
        std::vector<double> history(curvature_history_.begin(), curvature_history_.end());
        curvature_mean_ = mean(history);
        curvature_std_ = standard_deviation(history) + 1e-10;
    }
    const double normalized_curvature = curvature_history_.size() > 20 ? (curvature - curvature_mean_) / curvature_std_ : curvature;
    double energy = 0.0;
    for (std::size_t i = 1; i < recent.size(); ++i) energy += distance(recent[i], recent[i - 1]) * distance(recent[i], recent[i - 1]);
    if (recent.size() > 1) energy /= static_cast<double>(recent.size() - 1);
    return ManifoldMetrics{normalized_curvature, curvature, directional, ManifoldGeometry::metric_determinant(metric), energy, embedding_dimension_, curvature_history_.size()};
}

TopologyMetrics ToposEngine::analyze_topology(const Matrix& embedded) const {
    const std::size_t start = embedded.size() > 80 ? embedded.size() - 80 : 0;
    Matrix recent(embedded.begin() + static_cast<std::ptrdiff_t>(start), embedded.end());
    return recent.size() < 20 ? TopologyMetrics{} : PersistentHomology::compute(recent);
}

InformationMetrics ToposEngine::analyze_information(const Matrix& embedded) const {
    if (embedded.size() < 30) return InformationMetrics{};
    const std::size_t middle = embedded.size() / 2;
    Matrix past(embedded.begin(), embedded.begin() + static_cast<std::ptrdiff_t>(middle));
    Matrix recent(embedded.begin() + static_cast<std::ptrdiff_t>(middle), embedded.end());
    return InformationMetrics{clamp(InformationGeometry::distribution_distance(past, recent), 0.0, 5.0), InformationGeometry::entropy(recent)};
}

RenormalizationMetrics ToposEngine::analyze_rg(const std::vector<double>& prices) const {
    return RenormalizationGroup::compute_flow(prices);
}

SingularityMetrics ToposEngine::detect_singularity(const ManifoldMetrics& manifold, const TopologyMetrics& topology, const InformationMetrics& information, const RenormalizationMetrics& rg) const {
    double score = 0.0;
    std::vector<std::string> signals;
    const double curvature = std::abs(manifold.scalar_curvature);
    if (curvature > 3.0) { score += 0.30; signals.push_back("extreme_curvature"); }
    else if (curvature > 2.0) { score += 0.15; signals.push_back("elevated_curvature"); }
    if (topology.fragmentation > 0.7) { score += 0.25; signals.push_back("high_fragmentation"); }
    else if (topology.fragmentation > 0.5) { score += 0.10; signals.push_back("moderate_fragmentation"); }
    if (information.distribution_distance > 3.0) { score += 0.25; signals.push_back("distributional_shift"); }
    else if (information.distribution_distance > 1.5) { score += 0.10; signals.push_back("distributional_drift"); }
    if (rg.flow_stability == FlowStability::Critical) { score += 0.15; signals.push_back("rg_critical"); }
    else if (rg.flow_stability == FlowStability::Transient) score += 0.05;
    if (topology.fragmentation > 0.5 && rg.regime == MarketRegime::Trending) { score += 0.10; signals.push_back("trend_instability"); }
    score = std::min(score, 1.0);
    const SingularityType type = score > 0.6 ? SingularityType::Active : score > 0.35 ? SingularityType::Building : score > 0.15 ? SingularityType::Dormant : SingularityType::None;
    return SingularityMetrics{score, type, signals, score > 0.5};
}

PredictionMetrics ToposEngine::synthesize(const ManifoldMetrics& manifold, const TopologyMetrics& topology, const RenormalizationMetrics& rg, const SingularityMetrics& singularity, const std::vector<double>& prices) const {
    const double curvature = manifold.directional_curvature;
    const std::size_t start = prices.size() > 20 ? prices.size() - 20 : 0;
    const double momentum = prices.size() >= 20 ? (prices.back() - prices[start]) / (prices[start] + 1e-10) : 0.0;
    double score = clamp(curvature, -1.0, 1.0) * 0.3 + clamp(momentum * 100.0, -1.0, 1.0) * 0.35;
    if (rg.regime == MarketRegime::Trending && rg.hurst > 0.55) score += (momentum > 0.0 ? 0.2 : momentum < 0.0 ? -0.2 : 0.0);
    else if (rg.regime == MarketRegime::MeanReverting) score -= (momentum > 0.0 ? 0.15 : momentum < 0.0 ? -0.15 : 0.0);
    if (topology.fragmentation > 0.6) score *= 0.5;
    Direction direction = score > 0.15 ? Direction::Bullish : score < -0.15 ? Direction::Bearish : Direction::Neutral;
    double strength = direction == Direction::Neutral ? 0.1 : std::min(std::abs(score) * 1.5, 1.0);
    if (singularity.active && topology.fragmentation > 0.7) strength = std::max(strength, 0.7);
    double confidence = 0.4;
    if ((curvature > 0.0 && momentum > 0.001) || (curvature < 0.0 && momentum < -0.001)) confidence += 0.2;
    if (rg.flow_stability == FlowStability::Stable) confidence += 0.15;
    confidence = clamp(confidence - topology.fragmentation * 0.2 - singularity.score * 0.2, 0.1, 0.95);
    double expected_move = rg.regime == MarketRegime::Trending ? rg.hurst * strength * 4.0 : rg.regime == MarketRegime::MeanReverting ? (1.0 - rg.hurst) * strength * 2.0 : strength;
    return PredictionMetrics{direction, strength, confidence, expected_move, rg.regime, rg.hurst, score};
}

AnalysisResult ToposEngine::empty_result(double timestamp, double price, double volume) const {
    AnalysisResult result;
    result.timestamp = timestamp;
    result.price = price;
    result.volume = volume;
    result.manifold.manifold_dimension = embedding_dimension_;
    return result;
}

AnalysisResult ToposEngine::update(double price, double volume, double timestamp) {
    prices_.push_back(price);
    volumes_.push_back(volume);
    timestamps_.push_back(timestamp);
    if (prices_.size() > history_size_) prices_.pop_front();
    if (volumes_.size() > history_size_) volumes_.pop_front();
    if (timestamps_.size() > history_size_) timestamps_.pop_front();
    if (prices_.size() < 50) return empty_result(timestamp, price, volume);
    const std::vector<double> prices(prices_.begin(), prices_.end());
    const Matrix embedded = embed(extract_features());
    const ManifoldMetrics manifold = analyze_manifold(embedded, price, volume, prices);
    const TopologyMetrics topology = analyze_topology(embedded);
    const InformationMetrics information = analyze_information(embedded);
    const RenormalizationMetrics rg = analyze_rg(prices);
    const SingularityMetrics singularity = detect_singularity(manifold, topology, information, rg);
    const PredictionMetrics prediction = synthesize(manifold, topology, rg, singularity, prices);
    return AnalysisResult{timestamp, price, volume, manifold, topology, information, rg, singularity, prediction};
}

std::size_t ToposEngine::size() const { return prices_.size(); }

}
