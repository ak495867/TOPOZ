#pragma once

#include "topos/types.hpp"

#include <cstddef>
#include <deque>
#include <string>
#include <vector>

namespace topos {

using Matrix = std::vector<std::vector<double>>;

class ManifoldGeometry {
public:
    static Matrix metric_tensor(const Matrix& points);
    static double scalar_curvature(const Matrix& metric);
    static double curvature_direction(const std::vector<double>& prices);
    static double metric_determinant(const Matrix& metric);
};

class InformationGeometry {
public:
    static double distribution_distance(const Matrix& past, const Matrix& recent);
    static double entropy(const Matrix& samples);
};

class PersistentHomology {
public:
    static TopologyMetrics compute(const Matrix& points);
};

class RenormalizationGroup {
public:
    static double hurst_exponent(const std::vector<double>& series);
    static RenormalizationMetrics compute_flow(const std::vector<double>& prices, std::size_t scales = 4);
};

class ToposEngine {
public:
    explicit ToposEngine(std::size_t embedding_dimension = 5, std::size_t history_size = 200);
    void seed(const std::vector<double>& prices, const std::vector<double>& volumes, const std::vector<double>& timestamps = {});
    AnalysisResult update(double price, double volume, double timestamp);
    std::size_t size() const;
private:
    Matrix extract_features() const;
    Matrix embed(const Matrix& features) const;
    ManifoldMetrics analyze_manifold(const Matrix& embedded, double price, double volume, const std::vector<double>& prices);
    TopologyMetrics analyze_topology(const Matrix& embedded) const;
    InformationMetrics analyze_information(const Matrix& embedded) const;
    RenormalizationMetrics analyze_rg(const std::vector<double>& prices) const;
    SingularityMetrics detect_singularity(const ManifoldMetrics& manifold, const TopologyMetrics& topology, const InformationMetrics& information, const RenormalizationMetrics& rg) const;
    PredictionMetrics synthesize(const ManifoldMetrics& manifold, const TopologyMetrics& topology, const RenormalizationMetrics& rg, const SingularityMetrics& singularity, const std::vector<double>& prices) const;
    AnalysisResult empty_result(double timestamp, double price, double volume) const;
    std::size_t embedding_dimension_;
    std::size_t history_size_;
    std::deque<double> prices_;
    std::deque<double> volumes_;
    std::deque<double> timestamps_;
    Matrix projection_;
    std::deque<double> curvature_history_;
    std::deque<double> directional_curvature_history_;
    double curvature_mean_ = 0.0;
    double curvature_std_ = 1.0;
};

}
