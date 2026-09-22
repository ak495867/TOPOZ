#pragma once

#include "topos/types.hpp"

#include <deque>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace topos {

class DataFeedProvider {
public:
    explicit DataFeedProvider(MarketDataConfig config);
    virtual ~DataFeedProvider() = default;
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual std::optional<MarketTick> current() = 0;
    virtual std::vector<MarketTick> historical(std::size_t limit) = 0;
    bool connected() const;
    const std::string& last_error() const;
protected:
    MarketDataConfig config_;
    bool connected_ = false;
    std::string last_error_;
};

class SimulationProvider final : public DataFeedProvider {
public:
    explicit SimulationProvider(MarketDataConfig config);
    bool connect() override;
    void disconnect() override;
    std::optional<MarketTick> current() override;
    std::vector<MarketTick> historical(std::size_t limit) override;
private:
    void update_regime();
    std::mt19937 rng_;
    double price_ = 50000.0;
    double sigma_ = 250.0;
    double mu_ = 0.0;
    double volume_ = 100.0;
    std::size_t regime_timer_ = 0;
    std::size_t regime_duration_ = 100;
    std::string regime_ = "random_walk";
};

class CurlProviderBase : public DataFeedProvider {
public:
    explicit CurlProviderBase(MarketDataConfig config);
protected:
    std::optional<std::string> get(const std::string& url);
    static std::vector<double> numbers_after_key(const std::string& body, const std::string& key);
    static std::optional<double> number_after_key(const std::string& body, const std::string& key);
    static std::string url_encode(const std::string& value);
};

class YahooFinanceProvider final : public CurlProviderBase {
public:
    explicit YahooFinanceProvider(MarketDataConfig config);
    bool connect() override;
    void disconnect() override;
    std::optional<MarketTick> current() override;
    std::vector<MarketTick> historical(std::size_t limit) override;
private:
    std::string chart_url() const;
    std::optional<std::string> chart_body();
};

class CoinGeckoProvider final : public CurlProviderBase {
public:
    explicit CoinGeckoProvider(MarketDataConfig config);
    bool connect() override;
    void disconnect() override;
    std::optional<MarketTick> current() override;
    std::vector<MarketTick> historical(std::size_t limit) override;
private:
    std::string coin_id() const;
    std::string currency() const;
};

class AlphaVantageProvider final : public CurlProviderBase {
public:
    explicit AlphaVantageProvider(MarketDataConfig config);
    bool connect() override;
    void disconnect() override;
    std::optional<MarketTick> current() override;
    std::vector<MarketTick> historical(std::size_t limit) override;
};

class TwelveDataProvider final : public CurlProviderBase {
public:
    explicit TwelveDataProvider(MarketDataConfig config);
    bool connect() override;
    void disconnect() override;
    std::optional<MarketTick> current() override;
    std::vector<MarketTick> historical(std::size_t limit) override;
};

class MarketDataManager {
public:
    explicit MarketDataManager(MarketDataConfig config);
    ~MarketDataManager();
    std::optional<MarketTick> current();
    const std::deque<double>& prices() const;
    const std::deque<double>& volumes() const;
    bool simulation_mode() const;
    bool degraded_mode() const;
    const std::string& provider_status() const;
    const std::string& provider_error() const;
    void cleanup();
private:
    void initialize();
    void seed(const std::vector<MarketTick>& history);
    MarketDataConfig config_;
    std::unique_ptr<DataFeedProvider> provider_;
    std::deque<double> prices_;
    std::deque<double> volumes_;
    std::deque<double> timestamps_;
    bool simulation_mode_ = false;
    bool degraded_mode_ = false;
    std::string provider_status_ = "not initialized";
    std::string provider_error_;
    double last_price_ = 50000.0;
};

}
