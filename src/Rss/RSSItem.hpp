#pragma once
#include <Rss/RSSMedia.hpp>
#include <cstdint>
#include <dpp/dpp.h>
#include <string>

namespace dotnamecpp::rss {

  /**
   * @brief Enumeration for the types of embedding for RSS items.
   *
   */
  enum class EmbeddedType { EMBEDDED_NONE = 0, EMBEDDED_AS_MARKDOWN = 1, EMBEDDED_AS_ADVANCED = 2 };

  /**
   * @brief Represents a single RSS item.
   *
   */
  struct RSSItem {
    std::string title;
    std::string url;
    std::string description;
    std::string pubDate;
    std::string hash;
    RSSMedia rssMedia;
    EmbeddedType embeddedType;
    uint64_t discordChannelId;

    RSSItem()
        : rssMedia(std::string(), std::string()), embeddedType(EmbeddedType::EMBEDDED_NONE),
          discordChannelId(0) {
      // Default constructor - std:string members are default-initialized
    }
    RSSItem(std::string &title, std::string &url, std::string &description, RSSMedia &rssMedia,
            std::string &pubDate, EmbeddedType embeddedType, uint64_t discordChannelId)
        : title(std::move(title)), url(std::move(url)), description(std::move(description)),
          pubDate(std::move(pubDate)), rssMedia(std::move(rssMedia)), embeddedType(embeddedType),
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
             "\nPublication Date: " + pubDate +
             "\nEmbeddedType: " + std::to_string(static_cast<int>(embeddedType)) +
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
        if (rssMedia.type.find("image/") == 0) {
          e.set_image(rssMedia.url);
        } else {
          e.add_field("Media", "[" + rssMedia.url + "](" + rssMedia.url + ")", false);
        }
      }
      return e;
    }
  };

} // namespace dotnamecpp::rss