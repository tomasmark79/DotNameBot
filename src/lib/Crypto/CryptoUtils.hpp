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

  private:
    static std::string fetchUsdPrice(const char *url);
    static size_t writeCallback(void *contents, size_t size, size_t nmemb, std::string *output);
  };

} // namespace dotnamebot::crypto
