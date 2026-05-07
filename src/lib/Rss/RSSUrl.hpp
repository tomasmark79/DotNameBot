#pragma once
#include <cstdint>
#include <string>

namespace dotnamebot::rss {

  /**
   * @brief Represents an URL for a RSS feed along with its metadata.
   *
   */
  struct RSSUrl {
    std::string url;
    std::string label;
    long embeddedType;
    uint64_t discordChannelId;
    RSSUrl() : embeddedType(0), discordChannelId(0) {}
    RSSUrl(std::string u, long e = 0, uint64_t dChId = 0, std::string lbl = "")
        : url(std::move(u)), label(std::move(lbl)), embeddedType(e), discordChannelId(dChId) {}
  };

} // namespace dotnamebot::rss