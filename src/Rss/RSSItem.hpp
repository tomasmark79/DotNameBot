#pragma once
#include <Rss/RSSMedia.hpp>
#include <cstdint>
#include <dpp/dpp.h>
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
    RSSMedia rssMedia;
    bool embedded;
    uint64_t discordChannelId;

    RSSItem() : rssMedia(std::string(), std::string()), embedded(false), discordChannelId(0) {
      // Default constructor - std:string members are default-initialized
    }
    RSSItem(std::string &title, std::string &url, std::string &description, RSSMedia &rssMedia,
            std::string &pubDate, bool embedded, uint64_t discordChannelId)
        : title(std::move(title)), url(std::move(url)), description(std::move(description)),
          pubDate(std::move(pubDate)), rssMedia(std::move(rssMedia)), embedded(embedded),
          discordChannelId(discordChannelId) {

      // Generate the hash for the RSS item
      generateHash();
    }

    void generateHash() {
      std::hash<std::string> hasher;
      hash = std::to_string(hasher(title + url + description));
    }

    [[nodiscard]] std::string toMarkdownLink() const { return "[" + title + "](" + url + ")"; }

    [[nodiscard]] std::string toDebug() const {
      return "Title: " + title + "\nURL: " + url + "\nDescription: " + description +
             "\nPublication Date: " + pubDate + "\nEmbedded: " + (embedded ? "true" : "false") +
             "\nDiscord Channel ID: " + std::to_string(discordChannelId) + "\nHash: " + hash +
             "\nMedia URL: " + rssMedia.url + "\nMedia Type: " + rssMedia.type;
    }

    [[nodiscard]] dpp::embed toEmbed() const {
      dpp::embed e;
      e.set_title(title);
      e.set_url(url);
      e.set_description(description);
      if (!pubDate.empty()) {
        e.add_field("Published", pubDate, false);
      }
      if (!rssMedia.url.empty()) {
        if (rssMedia.type.find("image/") == 0 || rssMedia.type.find("hybrid_type") == 0) {
          e.set_image(rssMedia.url);
        } else {
          e.add_field("Media", "[" + rssMedia.url + "](" + rssMedia.url + ")", false);
        }
      }
      return e;
    }
  };

} // namespace dotnamecpp::rss