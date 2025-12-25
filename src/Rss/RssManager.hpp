#pragma once

#include <Rss/IRssService.hpp>
#include <Rss/RSSFeed.hpp>
#include <Rss/RSSItem.hpp>
#include <Rss/RSSUrl.hpp>

#include <Utils/UtilsFactory.hpp>

#include <filesystem>
#include <nlohmann/json.hpp>
#include <random>
#include <string>
#include <tinyxml2.h>
#include <unordered_set>
#include <vector>

namespace dotnamecpp::rss {

  class RssManager : public IRssService {

  public:
    RssManager(std::shared_ptr<dotnamecpp::logging::ILogger> logger,
               std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager);

    ~RssManager() override;

    // Public interface implementations
    bool Initialize() override;
    int refetchRssFeeds() override;
    bool addUrl(const std::string &url, bool embedded, uint64_t discordChannelId = 0) override;
    [[nodiscard]] std::string listUrlsAsString() override;
    [[nodiscard]] RSSItem getRandomItem() override;
    [[nodiscard]] RSSItem getRandomItemMatchingEmbedded(bool embedded) override;
    [[nodiscard]] size_t getItemCount() const override { return feed_.items.size(); }
    [[nodiscard]] size_t getItemCountMatchingEmbedded(bool embedded) const override;

  private:
    // Private helpers
    /**
     * @brief Fetches the source of a URL
     *
     * @param url The URL to fetch
     * @param embedded Whether the items should be marked as embedded
     * @param discordChannelId The Discord channel ID associated with the feed
     * @return int Returns added items count on success, -1 on failure
     */
    int fetchUrlSource(const std::string &url, bool embedded = false,
                       uint64_t discordChannelId = 0);

    /**
     * @brief Get the Item As Markdown object
     *
     * @param item The RSS item to convert to Markdown
     * @return std::string
     */
    static std::string getItemAsMarkdown(const RSSItem &item);

    /**
     * @brief Clears the feed buffer by removing all items.
     *
     */
    void clearFeedBuffer();

    /**
     * @brief CURL write callback function
     *
     * @param contents Pointer to the delivered data
     * @param size Size of each data element
     * @param nmemb Number of data elements
     * @param userp Pointer to the user data (string buffer)
     * @return size_t
     */
    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

    /**
     * @brief Checks if a file has changed since the last check.
     *
     * @param path
     * @param lastModified
     * @return true if the file has changed, otherwise false
     */
    static bool hasFileChanged(const std::filesystem::path &path,
                               std::filesystem::file_time_type &lastModified);

    /**
     * @brief Checks if the RSS URLs or seen hashes files have changed and reloads them if
     * necessary.
     *
     * @return 1 if either URLs or hashes files have changed, otherwise 0
     */
    bool hasFilesChanged();

    /**
     * @brief Load RSS URLs from the JSON file
     *
     * @return true on success, false on failure
     */
    bool loadUrls();

    /**
     * @brief Load seen hashes from the JSON file
     *
     * @return true on success, false on failure
     */
    bool loadSeenHashes();

    /**
     * @brief Save RSS URLs to the JSON file
     *
     * @return true on success, false on failure
     */
    bool saveUrls();

    /**
     * @brief
     *
     * @param hash
     * @return bool
     */
    bool saveSeenHash(const std::string &hash);

    /**
     * @brief Save all seen hashes to the JSON file
     *
     * @return bool
     */
    bool saveAllSeenHashes(); // Save all hashes at once

    /**
     * @brief Parses RSS feed XML data into an RSSFeed object
     *
     * @param xmlData The raw XML data of the RSS feed
     * @param embedded Whether the items should be marked as embedded
     * @param discordChannelId The Discord channel ID associated with the feed
     * @param totalDuplicateItems Reference to an integer to count duplicate items
     * @return RSSFeed The parsed RSS feed
     */
    RSSFeed parseRSS(const std::string &xmlData, bool embedded, uint64_t discordChannelId,
                     int &totalDuplicateItems);

    /**
     * @brief Downloads the RSS feed data from the given URL
     *
     * @param url The URL of the RSS feed to download
     * @return std::string The raw XML data of the RSS feed
     */
    std::string downloadFeed(const std::string &url);

    // Data members
    bool isInitialized_{false};

    std::filesystem::path urlsPath_;
    std::filesystem::file_time_type urlsLastModified_;

    std::filesystem::path hashesPath_;
    std::filesystem::file_time_type hashesLastModified_;

    std::mt19937 rng_;
    std::shared_ptr<dotnamecpp::logging::ILogger> logger_;
    std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager_;
    RSSFeed feed_;
    std::vector<RSSUrl> urls_;
    std::unordered_set<std::string> seenHashes_;
  };
} // namespace dotnamecpp::rss