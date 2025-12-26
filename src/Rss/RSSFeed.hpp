#pragma once
#include <Rss/RSSItem.hpp>
#include <string>
#include <vector>

namespace dotnamecpp::rss {

  /**
   * @brief Represents a RSS feed containing multiple RSS items.
   *
   */
  struct RSSFeed {
    std::string headTitle;
    std::string headDescription;
    std::string headLink;
    std::vector<RSSItem> items;
    void addItem(const RSSItem &item) { items.push_back(item); };
    [[nodiscard]] size_t size() const { return items.size(); }
    void clear() { items.clear(); };
  };

} // namespace dotnamecpp::rss