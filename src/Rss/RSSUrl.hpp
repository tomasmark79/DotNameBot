#pragma once
#include <cstdint>
#include <string>

namespace dotnamecpp::rss {

  /**
   * @brief Represents an URL for a RSS feed along with its metadata.
   *
   */
  struct RSSUrl {
    std::string url;
    bool embedded;
    uint64_t discordChannelId;
    RSSUrl() : embedded(false), discordChannelId(0) {}
    RSSUrl(std::string u, bool e = false, uint64_t dChId = 0)
        : url(std::move(u)), embedded(e), discordChannelId(dChId) {}
  };

} // namespace dotnamecpp::rss