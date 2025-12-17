#pragma once

#include <Rss/IRssService.hpp>
#include <Utils/UtilsFactory.hpp>

#include <filesystem>
#include <nlohmann/json.hpp>
#include <random>
#include <string>
#include <tinyxml2.h>
#include <unordered_set>
#include <vector>

namespace dotnamecpp::rss {

  /**
   * @brief Represents an URL for a RSS feed along with its metadata.
   *
   */
  struct RSSUrl {
    std::string url;
    bool embedded; // Whether this item should use embedded format
    uint64_t discordChannelId;
    RSSUrl() : embedded(false), discordChannelId(0) {}
    RSSUrl(const std::string &u, bool e = false, uint64_t dChId = 0)
        : url(u), embedded(e), discordChannelId(dChId) {}
  };

  /**
   * @brief Represents an item from a RSS feed along with its metadata.
   *
   */
  struct RSSItem {
    std::string title;
    std::string link;
    std::string description;
    std::string pubDate;
    std::string hash;
    bool embedded; // Whether this item should use embedded format
    uint64_t discordChannelId;

    RSSItem() : embedded(false), discordChannelId(0) {}
    RSSItem(const std::string &t, const std::string &l, const std::string &d,
            const std::string &date, bool e, uint64_t dChId);

    void generateHash();
    [[nodiscard]]
    std::string toMarkdownLink() const;
  };

  /**
   * @brief Represents a RSS feed containing multiple RSS items.
   *
   */
  struct RSSFeed {
    std::string title;
    std::string description;
    std::string link;
    std::vector<RSSItem> items;
    void addItem(const RSSItem &item);

    [[nodiscard]]
    size_t size() const;

    void clear();
  };

  class RssManager : public IRssService {

  public:
    RssManager(std::shared_ptr<dotnamecpp::logging::ILogger> logger,
               std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager);
    ~RssManager() override = default;

    int Initialize() override;

    int refetchRssFeeds() override;

    [[nodiscard]]
    std::string listUrls() override;

    [[nodiscard]]
    RSSItem getRandomItem() override;

    [[nodiscard]]
    RSSItem getRandomItemMatchingEmbedded(bool embedded) override;

    [[nodiscard]]
    size_t getItemCount() const override {
      return feed_.items.size();
    }

    [[nodiscard]]
    size_t getItemCountMatchingEmbedded(bool embedded) const override {
      size_t count = 0;
      for (const auto &item : feed_.items) {
        if (item.embedded == embedded) {
          count++;
        }
      }
      return count;
    }

    int addUrl(const std::string &url, bool embedded, uint64_t discordChannelId = 0) override;

  private:
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

    int fetchUrlSource(const std::string &url, bool embedded = false,
                       uint64_t discordChannelId = 0);
    std::string getItemAsMarkdown(const RSSItem &item) const { return item.toMarkdownLink(); }
    void clearFeedBuffer() { feed_.clear(); } // Clear all items from buffer

    RSSFeed feed_;
    std::vector<RSSUrl> urls_;
    std::unordered_set<std::string> seenHashes_;
    std::mt19937 rng_;

    // File operations
    int saveUrls();
    int loadUrls();
    int loadSeenHashes();
    int saveSeenHash(const std::string &hash);
    int saveAllSeenHashes(); // Save all hashes at once
    bool hasFileChanged(const std::filesystem::path &path,
                        std::filesystem::file_time_type &lastModified);
    void checkAndReloadFiles();

    RSSFeed parseRSS(const std::string &xmlData, bool embedded, uint64_t discordChannelId,
                     int &totalDuplicateItems);

    std::string downloadFeed(const std::string &url);

    std::filesystem::path getUrlsPath() const {
      return assetManager_->getAssetsPath() / "rssUrls.json";
    }

    std::filesystem::path getHashesPath() const {
      return assetManager_->getAssetsPath() / "seenHashes.json";
    }

    std::filesystem::file_time_type urlsLastModified_;
    std::filesystem::file_time_type hashesLastModified_;

    std::shared_ptr<dotnamecpp::logging::ILogger> logger_;
    std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager_;
  };
} // namespace dotnamecpp::rss