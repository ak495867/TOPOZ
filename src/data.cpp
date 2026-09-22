#include "topos/data.hpp"
#include "topos/util.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <regex>
#include <sstream>
#include <nlohmann/json.hpp>

#ifdef TOPOS_WITH_CURL
#include <curl/curl.h>
#endif

namespace topos {

using Json = nlohmann::json;

namespace {
std::optional<double> json_number(const Json& value) {
    try {
        if (value.is_number()) return value.get<double>();
        if (value.is_string()) return std::stod(value.get<std::string>());
    } catch (...) {}
    return std::nullopt;
}

std::optional<double> json_field_number(const Json& object, const char* key) {
    if (!object.is_object() || !object.contains(key) || object.at(key).is_null()) return std::nullopt;
    return json_number(object.at(key));
}

bool valid_tick(const MarketTick& tick) {
    return std::isfinite(tick.price) && tick.price > 0.0 && std::isfinite(tick.volume) && tick.volume >= 0.0 && std::isfinite(tick.timestamp);
}
}

DataFeedProvider::DataFeedProvider(MarketDataConfig config) : config_(std::move(config)) {}

bool DataFeedProvider::connected() const { return connected_; }
const std::string& DataFeedProvider::last_error() const { return last_error_; }

SimulationProvider::SimulationProvider(MarketDataConfig config)
    : DataFeedProvider(std::move(config)), rng_(42) {}

bool SimulationProvider::connect() {
    connected_ = true;
    return true;
}

void SimulationProvider::disconnect() { connected_ = false; }

void SimulationProvider::update_regime() {
    ++regime_timer_;
    if (regime_timer_ < regime_duration_) return;
    const std::array<std::string, 5> regimes{"bull_trend", "bear_trend", "high_vol", "low_vol", "mean_reverting"};
    std::uniform_int_distribution<int> choice(0, static_cast<int>(regimes.size() - 1));
    regime_ = regimes[choice(rng_)];
    regime_timer_ = 0;
    std::uniform_int_distribution<int> duration(50, 199);
    regime_duration_ = static_cast<std::size_t>(duration(rng_));
}

std::optional<MarketTick> SimulationProvider::current() {
    update_regime();
    if (regime_ == "bull_trend") {
        std::uniform_real_distribution<double> drift(30.0, 120.0);
        std::uniform_real_distribution<double> volatility(150.0, 300.0);
        mu_ = drift(rng_);
        sigma_ = volatility(rng_);
    } else if (regime_ == "bear_trend") {
        std::uniform_real_distribution<double> drift(-120.0, -30.0);
        std::uniform_real_distribution<double> volatility(200.0, 400.0);
        mu_ = drift(rng_);
        sigma_ = volatility(rng_);
    } else if (regime_ == "high_vol") {
        std::uniform_real_distribution<double> drift(-40.0, 40.0);
        std::uniform_real_distribution<double> volatility(400.0, 700.0);
        mu_ = drift(rng_);
        sigma_ = volatility(rng_);
    } else if (regime_ == "low_vol") {
        std::uniform_real_distribution<double> drift(-10.0, 20.0);
        std::uniform_real_distribution<double> volatility(50.0, 150.0);
        mu_ = drift(rng_);
        sigma_ = volatility(rng_);
    } else {
        mu_ = 0.0;
        std::uniform_real_distribution<double> volatility(100.0, 300.0);
        sigma_ = volatility(rng_);
    }
    std::normal_distribution<double> shock(0.0, sigma_);
    std::gamma_distribution<double> volume_distribution(3.0, 30.0);
    price_ = std::max(500.0, price_ + mu_ + shock(rng_));
    volume_ = std::max(0.1, volume_distribution(rng_) * sigma_ / 200.0);
    return MarketTick{price_, volume_, now_seconds(), price_, price_, price_};
}

std::vector<MarketTick> SimulationProvider::historical(std::size_t limit) {
    std::vector<MarketTick> result;
    result.reserve(limit);
    std::mt19937 history_rng(42);
    std::normal_distribution<double> noise(0.0, 180.0);
    std::gamma_distribution<double> volume_distribution(3.0, 30.0);
    double price = 48000.0;
    const double start = now_seconds() - static_cast<double>(limit) * 60.0;
    for (std::size_t i = 0; i < limit; ++i) {
        const double drift = i < limit / 4 ? 40.0 : i < limit / 2 ? -20.0 : i < 3 * limit / 4 ? 0.0 : 25.0;
        price = std::max(100.0, price + drift + noise(history_rng));
        const double volume = volume_distribution(history_rng) + std::abs(noise(history_rng)) * 0.2;
        result.push_back(MarketTick{price, volume, start + static_cast<double>(i) * 60.0, price, price, price});
    }
    if (!result.empty()) price_ = result.back().price;
    return result;
}

CurlProviderBase::CurlProviderBase(MarketDataConfig config) : DataFeedProvider(std::move(config)) {}

#ifdef TOPOS_WITH_CURL
namespace {
size_t curl_write(void* contents, size_t size, size_t count, void* user_data) {
    const auto total = size * count;
    static_cast<std::string*>(user_data)->append(static_cast<char*>(contents), total);
    return total;
}
}
#endif

std::optional<std::string> CurlProviderBase::get(const std::string& url) {
#ifdef TOPOS_WITH_CURL
    CURL* curl = curl_easy_init();
    if (!curl) {
        last_error_ = "Unable to initialize libcurl";
        return std::nullopt;
    }
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(std::max(1.0, config_.request_timeout) * 1000.0));
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Topos/1.0");
    const CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    if (code != CURLE_OK) {
        last_error_ = curl_easy_strerror(code);
        return std::nullopt;
    }
    if (status < 200 || status >= 300) {
        last_error_ = "HTTP status " + std::to_string(status);
        return std::nullopt;
    }
    if (body.size() > 4 * 1024 * 1024) {
        last_error_ = "provider response exceeded 4 MiB limit";
        return std::nullopt;
    }
    if (body.find("API call frequency") != std::string::npos || body.find("rate limit") != std::string::npos || body.find("Rate limit") != std::string::npos) {
        last_error_ = "provider rate limit response";
        return std::nullopt;
    }
    return body;
#else
    (void)url;
    last_error_ = "This build does not include libcurl";
    return std::nullopt;
#endif
}

std::vector<double> CurlProviderBase::numbers_after_key(const std::string& body, const std::string& key) {
    const auto position = body.find(key);
    if (position == std::string::npos) return {};
    const auto start = body.find('[', position);
    const auto end = body.find(']', start);
    if (start == std::string::npos || end == std::string::npos) return {};
    const std::string segment = body.substr(start + 1, end - start - 1);
    std::vector<double> values;
    std::regex pattern(R"((-?[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?))");
    for (std::sregex_iterator iterator(segment.begin(), segment.end(), pattern), finish; iterator != finish; ++iterator) {
        try { values.push_back(std::stod((*iterator)[1].str())); } catch (...) {}
    }
    return values;
}

std::optional<double> CurlProviderBase::number_after_key(const std::string& body, const std::string& key) {
    const auto position = body.find(key);
    if (position == std::string::npos) return std::nullopt;
    const std::string segment = body.substr(position + key.size());
    std::regex pattern(R"((-?[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?))");
    std::smatch match;
    if (!std::regex_search(segment, match, pattern)) return std::nullopt;
    try { return std::stod(match[1].str()); } catch (...) { return std::nullopt; }
}

std::string CurlProviderBase::url_encode(const std::string& value) {
    std::ostringstream output;
    output << std::hex;
    for (unsigned char character : value) {
        if (std::isalnum(character) || character == '-' || character == '_' || character == '.' || character == '~') output << character;
        else output << '%' << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(character) << std::nouppercase << std::setfill(' ');
    }
    return output.str();
}

YahooFinanceProvider::YahooFinanceProvider(MarketDataConfig config) : CurlProviderBase(std::move(config)) {}

std::string YahooFinanceProvider::chart_url() const {
    return "https://query1.finance.yahoo.com/v8/finance/chart/" + url_encode(config_.symbol) + "?range=5d&interval=1m";
}

std::optional<std::string> YahooFinanceProvider::chart_body() { return get(chart_url()); }

bool YahooFinanceProvider::connect() {
    const auto body = chart_body();
    connected_ = body.has_value() && body->find("chart") != std::string::npos;
    if (!connected_ && last_error_.empty()) last_error_ = "Yahoo Finance returned no chart data";
    return connected_;
}

void YahooFinanceProvider::disconnect() { connected_ = false; }

std::optional<MarketTick> YahooFinanceProvider::current() {
    const auto body = chart_body();
    if (!body) return std::nullopt;
    try {
        const auto chart = Json::parse(*body).at("chart").at("result").at(0);
        const auto& quote = chart.at("indicators").at("quote").at(0);
        const auto& closes = quote.at("close");
        for (std::size_t i = closes.size(); i-- > 0;) {
            const auto close = json_number(closes.at(i));
            if (!close || !std::isfinite(*close) || *close <= 0.0) continue;
            const auto volume = i < quote.at("volume").size() ? json_number(quote.at("volume").at(i)) : std::optional<double>{};
            return MarketTick{*close, std::max(0.0, volume.value_or(0.0)), now_seconds(), close.value_or(0.0), close.value_or(0.0), close.value_or(0.0)};
        }
    } catch (const std::exception& error) { last_error_ = std::string("Invalid Yahoo response: ") + error.what(); }
    return std::nullopt;
}

std::vector<MarketTick> YahooFinanceProvider::historical(std::size_t limit) {
    const auto body = chart_body();
    if (!body) return {};
    std::vector<MarketTick> result;
    try {
        const auto chart = Json::parse(*body).at("chart").at("result").at(0);
        const auto& timestamps = chart.at("timestamp");
        const auto& quote = chart.at("indicators").at("quote").at(0);
        const auto& closes = quote.at("close");
        const std::size_t start = closes.size() > limit ? closes.size() - limit : 0;
        for (std::size_t i = start; i < closes.size(); ++i) {
            const auto open = json_number(quote.at("open").at(i));
            const auto high = json_number(quote.at("high").at(i));
            const auto low = json_number(quote.at("low").at(i));
            const auto close = json_number(closes.at(i));
            if (!close || !open || !high || !low || !std::isfinite(*close) || *close <= 0.0) continue;
            const auto volume = i < quote.at("volume").size() ? json_number(quote.at("volume").at(i)) : std::optional<double>{};
            const double timestamp = i < timestamps.size() && timestamps.at(i).is_number() ? timestamps.at(i).get<double>() : now_seconds();
            result.push_back(MarketTick{*close, std::max(0.0, volume.value_or(0.0)), timestamp, *high, *low, *open});
        }
    } catch (const std::exception& error) { last_error_ = std::string("Invalid Yahoo response: ") + error.what(); }
    return result;
}

CoinGeckoProvider::CoinGeckoProvider(MarketDataConfig config) : CurlProviderBase(std::move(config)) {}

std::string CoinGeckoProvider::coin_id() const {
    const auto separator = config_.symbol.find('-');
    const std::string code = config_.symbol.substr(0, separator);
    if (code == "BTC") return "bitcoin";
    if (code == "ETH") return "ethereum";
    if (code == "XRP") return "ripple";
    if (code == "LTC") return "litecoin";
    if (code == "DOGE") return "dogecoin";
    if (code == "SOL") return "solana";
    if (code == "ADA") return "cardano";
    return code;
}

std::string CoinGeckoProvider::currency() const {
    const auto separator = config_.symbol.find('-');
    return separator == std::string::npos ? "usd" : config_.symbol.substr(separator + 1);
}

bool CoinGeckoProvider::connect() {
    const auto body = get("https://api.coingecko.com/api/v3/simple/price?ids=" + url_encode(coin_id()) + "&vs_currencies=" + url_encode(currency()));
    connected_ = body.has_value() && body->find(coin_id()) != std::string::npos;
    return connected_;
}

void CoinGeckoProvider::disconnect() { connected_ = false; }

std::optional<MarketTick> CoinGeckoProvider::current() {
    const auto body = get("https://api.coingecko.com/api/v3/simple/price?ids=" + url_encode(coin_id()) + "&vs_currencies=" + url_encode(currency()) + "&include_24hr_vol=true");
    if (!body) return std::nullopt;
    try {
        const auto object = Json::parse(*body).at(coin_id());
        const auto price = json_field_number(object, currency().c_str());
        if (!price || !std::isfinite(*price) || *price <= 0.0) return std::nullopt;
        const auto volume = json_field_number(object, (currency() + "_24h_vol").c_str());
        return MarketTick{*price, std::max(0.0, volume.value_or(0.0)), now_seconds(), *price, *price, *price};
    } catch (const std::exception& error) { last_error_ = std::string("Invalid CoinGecko response: ") + error.what(); }
    return std::nullopt;
}

std::vector<MarketTick> CoinGeckoProvider::historical(std::size_t limit) {
    const auto body = get("https://api.coingecko.com/api/v3/coins/" + url_encode(coin_id()) + "/market_chart?vs_currency=" + url_encode(currency()) + "&days=1");
    if (!body) return {};
    std::vector<MarketTick> result;
    try {
        const auto prices = Json::parse(*body).at("prices");
        const std::size_t start = prices.size() > limit ? prices.size() - limit : 0;
        for (std::size_t i = start; i < prices.size(); ++i) {
            if (!prices.at(i).is_array() || prices.at(i).size() < 2) continue;
            const auto timestamp = json_number(prices.at(i).at(0));
            const auto price = json_number(prices.at(i).at(1));
            if (!timestamp || !price || !std::isfinite(*price) || *price <= 0.0) continue;
            const double seconds = *timestamp / 1000.0;
            result.push_back(MarketTick{*price, 0.0, seconds, *price, *price, *price});
        }
    } catch (const std::exception& error) { last_error_ = std::string("Invalid CoinGecko response: ") + error.what(); }
    return result;
}

AlphaVantageProvider::AlphaVantageProvider(MarketDataConfig config) : CurlProviderBase(std::move(config)) {}

bool AlphaVantageProvider::connect() {
    connected_ = !config_.api_key.empty();
    if (!connected_) last_error_ = "Alpha Vantage API key is required";
    return connected_;
}

void AlphaVantageProvider::disconnect() { connected_ = false; }

std::optional<MarketTick> AlphaVantageProvider::current() {
    const auto body = get("https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=" + url_encode(config_.symbol) + "&apikey=" + url_encode(config_.api_key));
    if (!body) return std::nullopt;
    try {
        const auto quote = Json::parse(*body).at("Global Quote");
        const auto price = json_field_number(quote, "05. price");
        if (!price || !std::isfinite(*price) || *price <= 0.0) return std::nullopt;
        const auto volume = json_field_number(quote, "06. volume");
        return MarketTick{*price, std::max(0.0, volume.value_or(0.0)), now_seconds(), *price, *price, *price};
    } catch (const std::exception& error) { last_error_ = std::string("Invalid Alpha Vantage response: ") + error.what(); }
    return std::nullopt;
}

std::vector<MarketTick> AlphaVantageProvider::historical(std::size_t limit) {
    const auto body = get("https://www.alphavantage.co/query?function=TIME_SERIES_INTRADAY&symbol=" + url_encode(config_.symbol) + "&interval=5min&outputsize=full&apikey=" + url_encode(config_.api_key));
    if (!body) return {};
    std::vector<MarketTick> result;
    try {
        const auto series = Json::parse(*body).at("Time Series (5min)");
        std::vector<std::pair<std::string, Json>> rows;
        for (const auto& item : series.items()) rows.emplace_back(item.key(), item.value());
        std::sort(rows.begin(), rows.end(), [](const auto& left, const auto& right) { return left.first < right.first; });
        const std::size_t start = rows.size() > limit ? rows.size() - limit : 0;
        for (std::size_t i = start; i < rows.size(); ++i) {
            const auto open = json_field_number(rows[i].second, "1. open");
            const auto high = json_field_number(rows[i].second, "2. high");
            const auto low = json_field_number(rows[i].second, "3. low");
            const auto close = json_field_number(rows[i].second, "4. close");
            const auto volume = json_field_number(rows[i].second, "5. volume");
            if (!open || !high || !low || !close || !std::isfinite(*close) || *close <= 0.0) continue;
            result.push_back(MarketTick{*close, std::max(0.0, volume.value_or(0.0)), now_seconds() - static_cast<double>(rows.size() - i) * 300.0, *high, *low, *open});
        }
    } catch (const std::exception& error) { last_error_ = std::string("Invalid Alpha Vantage response: ") + error.what(); }
    return result;
}

TwelveDataProvider::TwelveDataProvider(MarketDataConfig config) : CurlProviderBase(std::move(config)) {}

bool TwelveDataProvider::connect() {
    connected_ = !config_.api_key.empty();
    if (!connected_) last_error_ = "Twelve Data API key is required";
    return connected_;
}

void TwelveDataProvider::disconnect() { connected_ = false; }

std::optional<MarketTick> TwelveDataProvider::current() {
    const auto body = get("https://api.twelvedata.com/price?symbol=" + url_encode(config_.symbol) + "&apikey=" + url_encode(config_.api_key));
    if (!body) return std::nullopt;
    try {
        const auto price = json_field_number(Json::parse(*body), "price");
        if (!price || !std::isfinite(*price) || *price <= 0.0) return std::nullopt;
        return MarketTick{*price, 0.0, now_seconds(), *price, *price, *price};
    } catch (const std::exception& error) { last_error_ = std::string("Invalid Twelve Data response: ") + error.what(); }
    return std::nullopt;
}

std::vector<MarketTick> TwelveDataProvider::historical(std::size_t limit) {
    const auto body = get("https://api.twelvedata.com/time_series?symbol=" + url_encode(config_.symbol) + "&interval=1min&outputsize=" + std::to_string(limit) + "&apikey=" + url_encode(config_.api_key));
    if (!body) return {};
    std::vector<MarketTick> result;
    try {
        const auto values = Json::parse(*body).at("values");
        const std::size_t start = values.size() > limit ? values.size() - limit : 0;
        for (std::size_t i = start; i < values.size(); ++i) {
            const auto open = json_field_number(values.at(i), "open");
            const auto high = json_field_number(values.at(i), "high");
            const auto low = json_field_number(values.at(i), "low");
            const auto close = json_field_number(values.at(i), "close");
            const auto volume = json_field_number(values.at(i), "volume");
            if (!close || !std::isfinite(*close) || *close <= 0.0) continue;
            result.push_back(MarketTick{*close, std::max(0.0, volume.value_or(0.0)), now_seconds() - static_cast<double>(values.size() - i) * 60.0, high.value_or(*close), low.value_or(*close), open.value_or(*close)});
        }
    } catch (const std::exception& error) { last_error_ = std::string("Invalid Twelve Data response: ") + error.what(); }
    return result;
}

MarketDataManager::MarketDataManager(MarketDataConfig config) : config_(std::move(config)) { initialize(); }
MarketDataManager::~MarketDataManager() { cleanup(); }

void MarketDataManager::initialize() {
    if (config_.data_source == DataSource::Simulation) provider_ = std::make_unique<SimulationProvider>(config_);
    else if (config_.data_source == DataSource::YahooFinance) provider_ = std::make_unique<YahooFinanceProvider>(config_);
    else if (config_.data_source == DataSource::CoinGecko) provider_ = std::make_unique<CoinGeckoProvider>(config_);
    else if (config_.data_source == DataSource::AlphaVantage) provider_ = std::make_unique<AlphaVantageProvider>(config_);
    else provider_ = std::make_unique<TwelveDataProvider>(config_);
    if (provider_->connect()) {
        provider_status_ = "connected";
        const auto history = provider_->historical(200);
        seed(history);
        if (history.empty() && config_.data_source != DataSource::Simulation) degraded_mode_ = true;
        simulation_mode_ = config_.data_source == DataSource::Simulation;
        return;
    }
    provider_error_ = provider_->last_error();
    if (!config_.fallback_to_simulation) {
        provider_status_ = "connection failed";
        return;
    }
    provider_ = std::make_unique<SimulationProvider>(config_);
    provider_->connect();
    simulation_mode_ = true;
    degraded_mode_ = true;
    provider_status_ = "simulation fallback";
    seed(provider_->historical(200));
}

void MarketDataManager::seed(const std::vector<MarketTick>& history) {
    for (const auto& tick : history) {
        if (!valid_tick(tick)) continue;
        prices_.push_back(tick.price);
        volumes_.push_back(tick.volume);
        timestamps_.push_back(tick.timestamp);
        if (prices_.size() > 1000) prices_.pop_front();
        if (volumes_.size() > 1000) volumes_.pop_front();
        if (timestamps_.size() > 1000) timestamps_.pop_front();
    }
    if (!prices_.empty()) last_price_ = prices_.back();
}

std::optional<MarketTick> MarketDataManager::current() {
    if (!provider_) return std::nullopt;
    if (!provider_->connected()) {
        provider_error_ = provider_->last_error().empty() ? "provider is not connected" : provider_->last_error();
        return std::nullopt;
    }
    const auto tick = provider_->current();
    if (tick) {
        if (!valid_tick(*tick)) {
            provider_error_ = "provider returned an invalid market tick";
            return std::nullopt;
        }
        prices_.push_back(tick->price);
        volumes_.push_back(tick->volume);
        timestamps_.push_back(tick->timestamp);
        if (prices_.size() > 1000) prices_.pop_front();
        if (volumes_.size() > 1000) volumes_.pop_front();
        if (timestamps_.size() > 1000) timestamps_.pop_front();
        last_price_ = tick->price;
        return tick;
    }
    provider_error_ = provider_->last_error();
    if (config_.fallback_to_simulation && !simulation_mode_) {
        simulation_mode_ = true;
        degraded_mode_ = true;
        provider_status_ = "degraded simulation";
        provider_ = std::make_unique<SimulationProvider>(config_);
        provider_->connect();
        return current();
    }
    return std::nullopt;
}

const std::deque<double>& MarketDataManager::prices() const { return prices_; }
const std::deque<double>& MarketDataManager::volumes() const { return volumes_; }
bool MarketDataManager::simulation_mode() const { return simulation_mode_; }
bool MarketDataManager::degraded_mode() const { return degraded_mode_; }
const std::string& MarketDataManager::provider_status() const { return provider_status_; }
const std::string& MarketDataManager::provider_error() const { return provider_error_; }
void MarketDataManager::cleanup() { if (provider_) provider_->disconnect(); }

}
