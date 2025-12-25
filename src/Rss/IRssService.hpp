#pragma once

#include <cstdint>
#include <string>

namespace dotnamecpp::rss {

  // Forward declaration
  struct RSSItem;

  /**
   * @brief Interface for RSS Service
   *
   * Provides methods for fetching and managing RSS feeds.
   */

  class IRssService {
  public:
    /**
     * @brief Destroy the IRssService object
     *
     */
    virtual ~IRssService() = default;

    /**
     * @brief Initialize the RSS service
     *
     * @return bool Returns true on success, false on failure
     */
    virtual bool Initialize() = 0;

    /**
     * @brief Refetch all RSS feeds from the stored URLs
     *
     * @return int Returns the number of new items fetched
     */
    [[nodiscard]] virtual int refetchRssFeeds() = 0;

    /**
     * @brief List all stored RSS URLs
     *
     * @return std::string A formatted string listing all RSS URLs
     */
    [[nodiscard]] virtual std::string listUrlsAsString() = 0;

    /**
     * @brief Get a random RSS item from the feed buffer
     *
     * @return RSSItem
     */
    [[nodiscard]] virtual RSSItem getRandomItem() = 0;

    /**
     * @brief Get a random RSS item matching the embedded flag
     *
     * @param embedded Whether to match embedded items
     * @return RSSItem
     */
    [[nodiscard]] virtual RSSItem getRandomItemMatchingEmbedded(bool embedded) = 0;

    /**
     * @brief Get the total number of items in the feed buffer
     *
     * @return size_t
     */
    [[nodiscard]] virtual size_t getItemCount() const = 0;

    /**
     * @brief Get the total number of items matching the embedded flag
     *
     * @param embedded Whether to match embedded items
     * @return size_t
     */
    [[nodiscard]] virtual size_t getItemCountMatchingEmbedded(bool embedded) const = 0;

    /**
     * @brief Add a new RSS URL to the list
     *
     * @param url The RSS feed URL
     * @param embedded Whether items from this feed should be marked as embedded
     * @param discordChannelId Optional Discord channel ID associated with this feed
     * @return true on success, otherwise false
     */
    virtual bool addUrl(const std::string &url, bool embedded, uint64_t discordChannelId) = 0;

    /**
     * @brief Modify an existing RSS URL in the list
     *
     * @param url The RSS feed URL
     * @param embedded Whether items from this feed should be marked as embedded
     * @param discordChannelId Optional Discord channel ID associated with this feed
     * @return true on success, otherwise false
     */

    virtual bool modUrl(const std::string &url, bool embedded, uint64_t discordChannelId) = 0;
    /**
     * @brief Remove an existing RSS URL from the list
     *
     * @param url The RSS feed URL
     * @return true on success, otherwise false
     */
    virtual bool remUrl(const std::string &url) = 0;
  };

} // namespace dotnamecpp::rss