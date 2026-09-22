#pragma once

#include "topos/analytics.hpp"
#include "topos/data.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace topos {

class Dashboard {
public:
    explicit Dashboard(MarketDataConfig config);
    int run(std::size_t maximum_updates = 0);
    static void request_stop();
private:
    std::string render_header(const AnalysisResult& result) const;
    std::string render_manifold(const AnalysisResult& result) const;
    std::string render_topology(const AnalysisResult& result) const;
    std::string render_information(const AnalysisResult& result) const;
    std::string render_prediction(const AnalysisResult& result) const;
    std::string render_singularity(const AnalysisResult& result) const;
    std::string render_rg(const AnalysisResult& result) const;
    std::string render_footer(const AnalysisResult& result) const;
    std::string panel(const std::string& title, const std::string& content, const std::string& color) const;
    void emit_structured(const AnalysisResult& result) const;
    void clear_screen() const;
    MarketDataConfig config_;
    MarketDataManager data_manager_;
    ToposEngine engine_;
    AnalysisResult current_;
    std::size_t updates_ = 0;
    std::size_t errors_ = 0;
    double average_latency_ = 0.0;
    double last_latency_ = 0.0;
    std::vector<Direction> direction_history_;
};

}
