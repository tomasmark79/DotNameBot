#include "CryptoUtils.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>

namespace dotnamebot::crypto {

  size_t CryptoUtils::writeCallback(void *contents, size_t size, size_t nmemb,
                                    std::string *output) {
    output->append(static_cast<char *>(contents), size * nmemb);
    return size * nmemb;
  }

  std::string CryptoUtils::httpGet(const char *url) {
    std::string buffer;
    CURL *curl = curl_easy_init();
    if (curl == nullptr) {
      return {};
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CryptoUtils::writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "DotNameBot/1.0");

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
      return {};
    }
    return buffer;
  }

  std::string CryptoUtils::fetchUsdPrice(const char *url) {
    const std::string body = httpGet(url);
    if (body.empty()) return {};
    try {
      auto jsonResponse = nlohmann::json::parse(body);
      return jsonResponse.at("price").get<std::string>();
    } catch (const nlohmann::json::exception &) {
      return {};
    }
  }

  std::string CryptoUtils::getCurrentBtcUsdPrice() {
    constexpr const char *kUrl = "https://api.binance.com/api/v3/ticker/price?symbol=BTCUSDT";
    return fetchUsdPrice(kUrl);
  }

  std::string CryptoUtils::getCurrentEthUsdPrice() {
    constexpr const char *kUrl = "https://api.binance.com/api/v3/ticker/price?symbol=ETHUSDT";
    return fetchUsdPrice(kUrl);
  }

  int CryptoUtils::getKlinesTrend(const char *symbol, const char *interval) {
    const std::string url = std::string("https://api.binance.com/api/v3/klines?symbol=") + symbol +
                            "&interval=" + interval + "&limit=2";
    const std::string body = httpGet(url.c_str());
    if (body.empty()) return 0;
    try {
      // Each kline: [openTime, open, high, low, close, ...]
      // Index 4 is the close price as a string.
      const auto j = nlohmann::json::parse(body);
      if (j.size() < 2) return 0;
      const double close0 = std::stod(j[0][4].get<std::string>());
      const double close1 = std::stod(j[1][4].get<std::string>());
      if (close1 > close0) return 1;
      if (close1 < close0) return -1;
      return 0;
    } catch (...) {
      return 0;
    }
  }

} // namespace dotnamebot::crypto