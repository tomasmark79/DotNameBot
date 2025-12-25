#include <Rss/RSSItem.hpp>

#include <cstdint>
#include <string>

namespace dotnamecpp::rss {

  RSSItem::RSSItem(std::string &t, std::string &u, std::string &d, std::string &date, bool e,
                   uint64_t dChId)
      : title(std::move(t)), url(std::move(u)), description(std::move(d)), pubDate(std::move(date)),
        embedded(e), discordChannelId(dChId) {
    generateHash();
  }

} // namespace dotnamecpp::rss