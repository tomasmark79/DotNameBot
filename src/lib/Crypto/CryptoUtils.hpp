#pragma once

#include <string>

namespace dotnamebot::crypto {

  class CryptoUtils {
  public:
    CryptoUtils() = default;
    ~CryptoUtils() = default;

    /**
     * @brief Fetches the current BTC/USD price from the Binance public API.
     * @return Price as a string (e.g. "67000.50"), or empty string on failure.
     */
    static std::string getCurrentBtcUsdPrice();

    /**
     * @brief Fetches the current ETH/USD price from the Binance public API.
     * @return Price as a string (e.g. "3000.50"), or empty string on failure.
     */
    static std::string getCurrentEthUsdPrice();

    /**
     * @brief Returns trend direction from the last two completed hourly Klines candles.
     * @param symbol   Binance symbol, e.g. "BTCUSDT"
     * @param interval Kline interval, e.g. "1h"
     * @return  1 if price is rising, -1 if falling, 0 on flat or error.
     */
    static int getKlinesTrend(const char *symbol, const char *interval = "1h");

  private:
    static std::string httpGet(const char *url);
    static std::string fetchUsdPrice(const char *url);
    static size_t writeCallback(void *contents, size_t size, size_t nmemb, std::string *output);
  };

} // namespace dotnamebot::crypto
