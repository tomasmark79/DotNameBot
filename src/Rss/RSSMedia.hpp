#pragma once
#include <string>

namespace dotnamecpp::rss {

  /**
   * @brief Represents an media content from a RSS feed item.
   *
   */
  struct RSSMedia {
    std::string url;
    std::string type;

    RSSMedia(std::string u, std::string t) : url(std::move(u)), type(std::move(t)) {}
  };

} // namespace dotnamecpp::rss