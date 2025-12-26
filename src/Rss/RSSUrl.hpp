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
    long embeddedType;
    uint64_t discordChannelId;
    RSSUrl() : embeddedType(0), discordChannelId(0) {}
    RSSUrl(std::string u, long e = 0, uint64_t dChId = 0)
        : url(std::move(u)), embeddedType(e), discordChannelId(dChId) {}
  };

} // namespace dotnamecpp::rss