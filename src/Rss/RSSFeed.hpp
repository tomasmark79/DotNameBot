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
    void addItem(const RSSItem &item);
    [[nodiscard]] size_t size() const;
    void clear();
  };

} // namespace dotnamecpp::rss