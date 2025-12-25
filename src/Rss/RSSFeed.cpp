#pragma once
#include <Rss/RSSFeed.hpp>

namespace dotnamecpp::rss {

  void RSSFeed::addItem(const RSSItem &item) { items.push_back(item); }

  size_t RSSFeed::size() const { return items.size(); }
  void RSSFeed::clear() { items.clear(); }

} // namespace dotnamecpp::rss