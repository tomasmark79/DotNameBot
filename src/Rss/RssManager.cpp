
#include "RssManager.hpp"
// #include <Logger/Logger.hpp>

#include <curl/curl.h>
#include <fstream>
#include <random>
#include <regex>

namespace dotnamecpp::rss {

  // RSSItem Struct Implementation
  RSSItem::RSSItem(const std::string &t, const std::string &l, const std::string &d,
                   const std::string &date = "", bool e = false, uint64_t dChId = 0)
      : title(t), link(l), description(d), pubDate(date), embedded(e), discordChannelId(dChId) {
    generateHash();
  }
  void RSSItem::generateHash() {
    std::hash<std::string> hasher;
    hash = std::to_string(hasher(title + link + description));
  }

  std::string RSSItem::toMarkdownLink() const { return "[" + title + "](" + link + ")"; }

  // RSSFeed Struct Implementation
  void RSSFeed::addItem(const RSSItem &item) { items.push_back(item); }

  size_t RSSFeed::size() const { return items.size(); }
  void RSSFeed::clear() { items.clear(); }

  // RssManager Class Implementation
  RssManager::RssManager(std::shared_ptr<dotnamecpp::logging::ILogger> logger,
                         std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager)
      : rng_(std::random_device{}()), logger_(std::move(logger)),
        assetManager_(std::move(assetManager)) {

    // Create default files if they don't exist
    if (!std::filesystem::exists(getUrlsPath())) {
      nlohmann::json defaultUrls = nlohmann::json::array(
          {{{"url", "https://blog.digitalspace.name/feed/atom/"}, {"embedded", true}}});

      std::ofstream file(getUrlsPath());
      if (!file.is_open()) {
        return;
      }
      file << defaultUrls.dump(4);
      logger_->infoStream() << "Created default RSS URLs file at: " << getUrlsPath();
    }

    if (!std::filesystem::exists(getHashesPath())) {
      nlohmann::json defaultHashes = nlohmann::json::array();
      std::ofstream file(getHashesPath());
      if (!file.is_open()) return;
      file << defaultHashes.dump(4);
      logger_->infoStream() << "Created default seen hashes file at: " << getHashesPath();
    }

    // Initialize timestamps after loading
    if (std::filesystem::exists(getUrlsPath())) {
      urlsLastModified_ = std::filesystem::last_write_time(getUrlsPath());
    }

    if (std::filesystem::exists(getHashesPath())) {
      hashesLastModified_ = std::filesystem::last_write_time(getHashesPath());
    }
  }

  int RssManager::Initialize() { return loadUrls() == 0 && loadSeenHashes() == 0 ? 0 : -1; }

  size_t RssManager::WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
  }

  int RssManager::addUrl(const std::string &url, bool embedded, uint64_t discordChannelId) {
    for (const auto &existingUrl : urls_) {
      if (existingUrl.url == url) {
        logger_->warningStream() << "URL already exists: " << url;
        return -1;
      }
    }
    urls_.emplace_back(url, embedded, discordChannelId);
    return saveUrls();
  }

  int RssManager::saveUrls() {
    nlohmann::json jsonData = nlohmann::json::array();
    for (const auto &url : urls_) {
      jsonData.push_back({{"url", url.url},
                          {"embedded", url.embedded},
                          {"discordChannelId", url.discordChannelId}});
    }
    std::ofstream file(getUrlsPath());
    if (!file.is_open()) return -1;
    file << jsonData.dump(4);
    return 0;
  }

  std::string RssManager::listUrls() {
    std::string sourcesList;
    sourcesList = "";
    for (const auto &url : urls_) {
      sourcesList += "- " + url.url + (url.embedded ? " (embedded)" : " (non-embedded)");
      if (url.discordChannelId != 0) {
        sourcesList += " [Channel: " + std::to_string(url.discordChannelId) + "]";
      }
      sourcesList += "\n";
    }
    return sourcesList.empty() ? "No RSS sources available." : sourcesList;
  }

  int RssManager::loadUrls() {
    std::ifstream file(getUrlsPath());
    if (!file.is_open()) return -1;

    nlohmann::json jsonData;
    file >> jsonData;

    urls_.clear();
    for (const auto &item : jsonData) {
      if (item.is_object() && item.contains("url")) {
        std::string url = item["url"].get<std::string>();
        bool embedded = item.contains("embedded") ? item["embedded"].get<bool>() : false;
        uint64_t discordChannelId = 0;
        if (item.contains("discordChannelId") && !item["discordChannelId"].is_null()) {
          discordChannelId = item["discordChannelId"].get<uint64_t>();
        }
        urls_.emplace_back(url, embedded, discordChannelId);
      } else if (item.is_string()) {
        // Backwards compatibility - treat strings as non-embedded
        urls_.emplace_back(item.get<std::string>(), false);
      }
    }

    logger_->infoStream() << "Loaded " << urls_.size() << " RSS URLs.";
    return 0;
  }

  int RssManager::loadSeenHashes() {
    std::ifstream file(getHashesPath());
    if (!file.is_open()) return -1;

    nlohmann::json jsonData;
    try {
      file >> jsonData;
    } catch (const std::exception &e) {
      logger_->errorStream() << "Hashes file corrupted: " << e.what() << ". Creating new file.";
      // std::endl;
      seenHashes_.clear();
      std::ofstream outFile(getHashesPath());
      if (!outFile.is_open()) return -1;
      outFile << nlohmann::json::array().dump(4);
      return 0;
    }

    seenHashes_.clear();
    for (const auto &hash : jsonData) {
      if (hash.is_string()) {
        seenHashes_.insert(hash.get<std::string>());
      }
    }

    logger_->infoStream() << "Loaded " << seenHashes_.size() << " seen hashes.";
    return 0;
  }

  int RssManager::saveSeenHash(const std::string &hash) {
    seenHashes_.insert(hash);

    nlohmann::json jsonData = nlohmann::json::array();
    for (const auto &h : seenHashes_) {
      jsonData.push_back(h);
    }

    std::ofstream file(getHashesPath());
    if (!file.is_open()) return -1;
    file << jsonData.dump(4);
    return 0;
  }

  std::string RssManager::downloadFeed(const std::string &url) {
    std::string buffer = "";
    try {
      CURL *curl = curl_easy_init();
      if (!curl) return "";

      // Set HTTP headers
      struct curl_slist *headers = nullptr;
      headers =
          curl_slist_append(headers, "Accept: application/rss+xml, application/xml, text/xml");
      headers = curl_slist_append(headers, "Cache-Control: no-cache");
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, RssManager::WriteCallback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

      curl_easy_setopt(
          curl, CURLOPT_USERAGENT,
          "DiTwi RSS Reader by DotName (Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36)");

      // Set timeout options
      curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
      curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

      // Accept any encoding
      curl_easy_setopt(curl, CURLOPT_ENCODING, "");

      CURLcode res = curl_easy_perform(curl);

      // Clean up headers
      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);

      if (res != CURLE_OK) {
        logger_->errorStream() << "CURL error for URL '" << url << "': " << curl_easy_strerror(res);
        return "";
      }
    } catch (const std::exception &e) {
      logger_->errorStream() << "Exception during CURL operation: " << e.what();
      return "";
    }

    return buffer;
  }

  RSSFeed RssManager::parseRSS(const std::string &xmlData, bool embedded, uint64_t discordChannelId,
                               int &totalDuplicateItems) {
    RSSFeed feed;
    tinyxml2::XMLDocument doc;
    doc.Parse(xmlData.c_str());

    tinyxml2::XMLElement *channel = nullptr;
    tinyxml2::XMLElement *firstItem = nullptr;
    bool isAtom = false;

    // Try RSS 2.0 format
    if (auto rssElement = doc.FirstChildElement("rss")) {
      channel = rssElement->FirstChildElement("channel");
      if (channel) {
        firstItem = channel->FirstChildElement("item");
      }
    }
    // Try RSS 1.0 format
    else if (auto rdfElement = doc.FirstChildElement("rdf:RDF")) {
      channel = rdfElement->FirstChildElement("channel");
      firstItem = rdfElement->FirstChildElement("item");
    }
    // Try Atom format
    else if (auto feedElement = doc.FirstChildElement("feed")) {
      channel = feedElement;
      firstItem = feedElement->FirstChildElement("entry");
      isAtom = true;
    }

    if (!channel) {
      logger_->errorStream() << "No valid RSS/Atom channel found.";
      return feed;
    }

    // Parse channel info
    if (isAtom) {
      // Atom feed info
      if (auto titleEl = channel->FirstChildElement("title")) {
        feed.title = titleEl->GetText() ? titleEl->GetText() : "";
      }
      if (auto subtitleEl = channel->FirstChildElement("subtitle")) {
        feed.description = subtitleEl->GetText() ? subtitleEl->GetText() : "";
      }
      if (auto linkEl = channel->FirstChildElement("link")) {
        const char *href = linkEl->Attribute("href");
        feed.link = href ? href : "";
      }
    } else {
      // RSS feed info
      if (auto titleEl = channel->FirstChildElement("title")) {
        feed.title = titleEl->GetText() ? titleEl->GetText() : "";
      }
      if (auto descEl = channel->FirstChildElement("description")) {
        feed.description = descEl->GetText() ? descEl->GetText() : "";
      }
      if (auto linkEl = channel->FirstChildElement("link")) {
        feed.link = linkEl->GetText() ? linkEl->GetText() : "";
      }
    }

    // Parse items
    int newItems = 0;
    const char *itemTag = isAtom ? "entry" : "item";

    for (auto item = firstItem; item; item = item->NextSiblingElement(itemTag)) {
      RSSItem rssItem;
      rssItem.embedded = embedded;
      rssItem.discordChannelId = discordChannelId;

      if (isAtom) {
        // Parse Atom entry
        if (auto titleEl = item->FirstChildElement("title")) {
          rssItem.title = titleEl->GetText() ? titleEl->GetText() : "";
        }
        if (auto linkEl = item->FirstChildElement("link")) {
          const char *href = linkEl->Attribute("href");
          rssItem.link = href ? href : "";
        }
        if (auto summaryEl = item->FirstChildElement("summary")) {
          rssItem.description = summaryEl->GetText() ? summaryEl->GetText() : "";
        } else if (auto contentEl = item->FirstChildElement("content")) {
          rssItem.description = contentEl->GetText() ? contentEl->GetText() : "";
        }

        if (auto updatedEl = item->FirstChildElement("updated")) {
          rssItem.pubDate = updatedEl->GetText() ? updatedEl->GetText() : "";
        } else if (auto publishedEl = item->FirstChildElement("published")) {
          rssItem.pubDate = publishedEl->GetText() ? publishedEl->GetText() : "";
        }
      } else {
        // Parse RSS item (existing code)
        if (auto titleEl = item->FirstChildElement("title")) {
          // Handle CDATA sections properly by getting all text content
          const char *text = titleEl->GetText();
          if (text) {
            rssItem.title = text;
          } else {
            // If GetText() returns null, try to get text from child nodes (including CDATA)
            auto textNode = titleEl->FirstChild();
            if (textNode && textNode->ToText()) {
              rssItem.title = textNode->Value() ? textNode->Value() : "";
            }
          }
        }
        if (auto linkEl = item->FirstChildElement("link")) {
          rssItem.link = linkEl->GetText() ? linkEl->GetText() : "";
        }
        if (auto descEl = item->FirstChildElement("description")) {
          // Handle CDATA sections properly by getting all text content
          const char *text = descEl->GetText();
          if (text) {
            rssItem.description = text;
          } else {
            // If GetText() returns null, try to get text from child nodes (including CDATA)
            auto textNode = descEl->FirstChild();
            if (textNode && textNode->ToText()) {
              rssItem.description = textNode->Value() ? textNode->Value() : "";
            }
          }
        }
        if (auto dateEl = item->FirstChildElement("pubDate")) {
          rssItem.pubDate = dateEl->GetText() ? dateEl->GetText() : "";
        }
      }

      if (rssItem.title.empty() || rssItem.link.empty()) continue;

      rssItem.generateHash(); // Generate hash from original, unprocessed data

      // Skip if already seen
      if (seenHashes_.find(rssItem.hash) != seenHashes_.end()) {
        totalDuplicateItems++;
        continue;
      }

      // Clean up description for display AFTER hash generation (both RSS and Atom)
      if (!rssItem.description.empty()) {
        std::string &desc = rssItem.description;

        // Replace multiple whitespace characters with single space
        std::regex ws_re("\\s+");
        desc = std::regex_replace(desc, ws_re, " ");

        // Trim leading/trailing whitespace
        desc = std::regex_replace(desc, std::regex("^\\s+|\\s+$"), "");
      }

      feed.addItem(rssItem);
      newItems++;
    }

    return feed;
  }

  int RssManager::fetchUrlSource(const std::string &url, bool embedded, uint64_t discordChannelId) {
    std::string xmlData = downloadFeed(url);
    if (xmlData.empty()) {
      return -1;
    }

    int totalDuplicateItems = 0;
    RSSFeed newFeed = parseRSS(xmlData, embedded, discordChannelId, totalDuplicateItems);

    int addedItems = 0;
    for (const auto &item : newFeed.items) {
      feed_.addItem(item);
      addedItems++;
    }

    logger_->infoStream() << "New " << addedItems << " items added to the feed buffer."
                          << " Found " << totalDuplicateItems << " seen items."
                          << " url: " << url << " (embedded: " << (embedded ? "true" : "false")
                          << ")"
                          << " (Buffer size: " << feed_.items.size() << ")";
    return addedItems;
  }

  int RssManager::refetchRssFeeds() {

    checkAndReloadFiles();

    // Clear _feed buffer
    feed_.clear();

    int totalItems = 0;

    for (const auto &rssUrl : urls_) {
      int items = fetchUrlSource(rssUrl.url, rssUrl.embedded, rssUrl.discordChannelId);
      if (items > 0) {
        totalItems += items;
      }
    }
    logger_->infoStream() << "Total fetched items: " << totalItems
                          << " (total in buffer: " << feed_.items.size() << ")";
    return totalItems;
  }

  RSSItem RssManager::getRandomItem() {
    if (feed_.items.empty()) {
      return RSSItem{};
    }

    std::uniform_int_distribution<size_t> dist(0, feed_.items.size() - 1);
    size_t index = dist(rng_);

    RSSItem item = feed_.items[index];

    // Save hash immediately to prevent re-processing
    saveSeenHash(item.hash);

    // Remove item from feed
    feed_.items.erase(feed_.items.begin() + index);

    return item;
  }

  RSSItem RssManager::getRandomItemMatchingEmbedded(bool embedded = false) {
    // Find items with matching embedded flag
    std::vector<size_t> matchingIndices;
    for (size_t i = 0; i < feed_.items.size(); ++i) {
      if (feed_.items[i].embedded == embedded) {
        matchingIndices.push_back(i);
      }
    }

    if (matchingIndices.empty()) {
      return RSSItem{};
    }

    // Pick random item from matching ones
    std::uniform_int_distribution<size_t> dist(0, matchingIndices.size() - 1);
    size_t randomIndex = dist(rng_);
    size_t actualIndex = matchingIndices[randomIndex];

    RSSItem item = feed_.items[actualIndex];

    // Save hash immediately to prevent re-processing
    saveSeenHash(item.hash);

    // Remove item from feed
    feed_.items.erase(feed_.items.begin() + actualIndex);

    return item;
  }

  // Add method to save all hashes at once (call this periodically or at shutdown)
  int RssManager::saveAllSeenHashes() {
    nlohmann::json jsonData = nlohmann::json::array();
    for (const auto &h : seenHashes_) {
      jsonData.push_back(h);
    }

    std::ofstream file(getHashesPath());
    if (!file.is_open()) return -1;
    file << jsonData.dump(4);
    return 0;
  }

  bool RssManager::hasFileChanged(const std::filesystem::path &path,
                                  std::filesystem::file_time_type &lastModified) {
    if (!std::filesystem::exists(path)) {
      return false;
    }

    auto currentModified = std::filesystem::last_write_time(path);
    if (currentModified != lastModified) {
      lastModified = currentModified;
      return true;
    }
    return false;
  }

  void RssManager::checkAndReloadFiles() {
    bool urlsChanged = hasFileChanged(getUrlsPath(), urlsLastModified_);
    bool hashesChanged = hasFileChanged(getHashesPath(), hashesLastModified_);

    if (urlsChanged) {
      logger_->infoStream() << "URLs file changed, reloading...";
      loadUrls();
    }

    if (hashesChanged) {
      logger_->infoStream() << "Hashes file changed, reloading...";
      loadSeenHashes();
    }
  }
} // namespace dotnamecpp::rss