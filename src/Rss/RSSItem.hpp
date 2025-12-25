#pragma once
#include <cstdint>
#include <string>

namespace dotnamecpp::rss {

  /**
   * @brief Represents an item from a RSS feed along with its metadata.
   *
   */
  struct RSSItem {
    std::string title;
    std::string url;
    std::string description;
    std::string pubDate;
    std::string hash;
    bool embedded;
    uint64_t discordChannelId;

    RSSItem() : embedded(false), discordChannelId(0) {}
    RSSItem(std::string &t, std::string &u, std::string &d, std::string &date, bool e,
            uint64_t dChId);

    void generateHash() {
      std::hash<std::string> hasher;
      hash = std::to_string(hasher(title + url + description));
    }

    [[nodiscard]]
    std::string toMarkdownLink() const {
      return "[" + title + "](" + url + ")";
    }
  };

} // namespace dotnamecpp::rss