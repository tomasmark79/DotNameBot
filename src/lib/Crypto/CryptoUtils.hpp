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

  private:
    static size_t writeCallback(void *contents, size_t size, size_t nmemb, std::string *output);
  };

} // namespace dotnamebot::crypto
