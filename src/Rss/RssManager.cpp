
#include "RssManager.hpp"

#include <curl/curl.h>
#include <fstream>
#include <random>
#include <regex>
#include <string>
#include <utility>

namespace dotnamecpp::rss {

  RssManager::RssManager(std::shared_ptr<dotnamecpp::logging::ILogger> logger,
                         std::shared_ptr<dotnamecpp::assets::IAssetManager> assetManager)
      : logger_(std::move(logger)), assetManager_(std::move(assetManager)) {

    rng_.seed(std::random_device{}());
    urlsPath_ = assetManager_->getAssetsPath() / "rssUrls.json";
    hashesPath_ = assetManager_->getAssetsPath() / "seenHashes.json";

    if (!isInitialized_) {
      isInitialized_ = this->Initialize();
    }
  }

  RssManager::~RssManager() {
    // Save seen hashes on destruction
    if (!saveAllSeenHashes()) {
      logger_->error("Failed to save seen hashes on RssManager destruction");
    }
    isInitialized_ = false;
  }

  bool RssManager::Initialize() {
    // Create default files if they do not exist
    // Avoid trailing slash in url paths is best practice
    if (!std::filesystem::exists(urlsPath_)) {
      nlohmann::json defaultUrls = nlohmann::json::array(
          {{{"url", "https://blog.digitalspace.name/feed/atom"}, {"embedded", true}}});

      std::ofstream file(urlsPath_);
      if (!file.is_open()) {
        return false;
      }
      file << defaultUrls.dump(4);
      logger_->infoStream() << "Created default RSS URLs file at: " << urlsPath_;
    }

    if (!std::filesystem::exists(hashesPath_)) {
      nlohmann::json defaultHashes = nlohmann::json::array();
      std::ofstream file(hashesPath_);
      if (!file.is_open()) {
        return false;
      }
      file << defaultHashes.dump(4);
      logger_->infoStream() << "Created default seen hashes file at: " << hashesPath_;
    }

    if (std::filesystem::exists(urlsPath_)) {
      urlsLastModified_ = std::filesystem::last_write_time(urlsPath_);
    }

    if (std::filesystem::exists(hashesPath_)) {
      hashesLastModified_ = std::filesystem::last_write_time(hashesPath_);
    }

    return loadUrls() && loadSeenHashes();
  }

  int RssManager::refetchRssFeeds() {
    if (!hasFilesChanged()) {
      logger_->infoStream() << "Files changed, reloading URLs and seen hashes.";
    }

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

  bool RssManager::addUrl(const std::string &url, bool embedded, uint64_t discordChannelId) {
    for (const auto &existingUrl : urls_) {
      if (existingUrl.url == url) {
        logger_->warningStream() << "URL already exists: " << url;
        return false;
      }
    }
    urls_.emplace_back(url, embedded, discordChannelId);
    return saveUrls();
  }

  bool RssManager::modUrl(const std::string &url, bool embedded, uint64_t discordChannelId) {
    for (auto &existingUrl : urls_) {
      if (existingUrl.url == url) {
        existingUrl.embedded = embedded;
        existingUrl.discordChannelId = discordChannelId;
        return saveUrls();
      }
    }
    logger_->warningStream() << "URL: " << url << " not found for modification";
    return false;
  }

  bool RssManager::remUrl(const std::string &url) {
    auto it = std::remove_if(urls_.begin(), urls_.end(),
                             [&url](const RSSUrl &rssUrl) { return rssUrl.url == url; });
    if (it != urls_.end()) {
      urls_.erase(it, urls_.end());
      return saveUrls();
    }
    logger_->warningStream() << "URL: " << url << " not found for removal";
    return false;
  }

  bool RssManager::saveUrls() {
    nlohmann::json jsonData = nlohmann::json::array();
    for (const auto &url : urls_) {
      jsonData.push_back({{"url", url.url},
                          {"embedded", url.embedded},
                          {"discordChannelId", url.discordChannelId}});
    }
    std::ofstream file(urlsPath_);
    if (!file.is_open()) {
      return false;
    }
    file << jsonData.dump(4);
    return true;
  }

  std::string RssManager::listUrlsAsString() {
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

  std::string RssManager::listChannelUrlsAsString(uint64_t discordChannelId) {
    std::string sourcesList;
    for (const auto &url : urls_) {
      if (url.discordChannelId == discordChannelId) {
        sourcesList += "- " + url.url + (url.embedded ? " (embedded)" : " (non-embedded)") + "\n";
      }
    }
    return sourcesList.empty() ? "No RSS sources available for this channel." : sourcesList;
  }

  bool RssManager::loadUrls() {
    std::ifstream file(urlsPath_);
    if (!file.is_open()) {
      return false;
    }

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
    return true;
  }

  size_t RssManager::getItemCountMatchingEmbedded(bool embedded) const {
    size_t count = 0;
    for (const auto &item : feed_.items) {
      if (item.embedded == embedded) {
        count++;
      }
    }
    return count;
  }

  bool RssManager::loadSeenHashes() {
    std::ifstream file(hashesPath_);
    if (!file.is_open()) {
      return false;
    }

    nlohmann::json jsonData;
    try {
      file >> jsonData;
    } catch (const std::exception &e) {
      logger_->errorStream() << "Hashes file corrupted: " << e.what() << ". Creating new file.";
      // std::endl;
      seenHashes_.clear();
      std::ofstream outFile(hashesPath_);
      if (!outFile.is_open()) {
        return false;
      }
      outFile << nlohmann::json::array().dump(4);
      return true;
    }

    seenHashes_.clear();
    for (const auto &hash : jsonData) {
      if (hash.is_string()) {
        seenHashes_.insert(hash.get<std::string>());
      }
    }

    logger_->infoStream() << "Loaded " << seenHashes_.size() << " seen hashes.";
    return true;
  }

  bool RssManager::saveSeenHash(const std::string &hash) {
    seenHashes_.insert(hash);

    nlohmann::json jsonData = nlohmann::json::array();
    for (const auto &h : seenHashes_) {
      jsonData.push_back(h);
    }

    std::ofstream file(hashesPath_);
    if (!file.is_open()) {
      return false;
    }
    file << jsonData.dump(4);
    return true;
  }

  std::string RssManager::downloadFeed(const std::string &url) {
    std::string buffer;
    try {
      CURL *curl = curl_easy_init();
      if (curl == nullptr) {
        return "";
      }

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
          "DotNameBot RSS Reader by DotName: https://github.com/tomasmark79/DotNameBot");

      curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
      curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
      curl_easy_setopt(curl, CURLOPT_ENCODING, "");

      CURLcode res = curl_easy_perform(curl);

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
    if (auto *rssElement = doc.FirstChildElement("rss")) {
      channel = rssElement->FirstChildElement("channel");
      if (channel != nullptr) {
        firstItem = channel->FirstChildElement("item");
      }
    }
    // Try RSS 1.0 format
    else if (auto *rdfElement = doc.FirstChildElement("rdf:RDF")) {
      channel = rdfElement->FirstChildElement("channel");
      firstItem = rdfElement->FirstChildElement("item");
    }
    // Try Atom format
    else if (auto *feedElement = doc.FirstChildElement("feed")) {
      channel = feedElement;
      firstItem = feedElement->FirstChildElement("entry");
      isAtom = true;
    }

    if (channel == nullptr) {
      logger_->errorStream() << "No valid RSS/Atom channel found.";
      return feed;
    }

    // Parse Feed Header
    if (isAtom) {
      // Atom
      if (auto *titleEl = channel->FirstChildElement("title")) {
        feed.headTitle = (titleEl->GetText() != nullptr) ? titleEl->GetText() : "";
      }
      if (auto *subtitleEl = channel->FirstChildElement("subtitle")) {
        feed.headDescription = (subtitleEl->GetText() != nullptr) ? subtitleEl->GetText() : "";
      }
      if (auto *linkEl = channel->FirstChildElement("link")) {
        const char *href = linkEl->Attribute("href");
        feed.headLink = (href != nullptr) ? href : "";
      }
    } else {
      // Rss
      if (auto *titleEl = channel->FirstChildElement("title")) {
        feed.headTitle = (titleEl->GetText() != nullptr) ? titleEl->GetText() : "";
      }
      if (auto *descEl = channel->FirstChildElement("description")) {
        feed.headDescription = (descEl->GetText() != nullptr) ? descEl->GetText() : "";
      }
      if (auto *linkEl = channel->FirstChildElement("link")) {
        feed.headLink = (linkEl->GetText() != nullptr) ? linkEl->GetText() : "";
      }
    }

    // Parse Feed Items
    [[maybe_unused]]
    int newItems = 0;
    const char *itemTag = isAtom ? "entry" : "item";

    for (auto *item = firstItem; item != nullptr; item = item->NextSiblingElement(itemTag)) {
      RSSItem rssItem;
      rssItem.embedded = embedded;
      rssItem.discordChannelId = discordChannelId;

      if (isAtom) {
        // Atom
        if (auto *titleEl = item->FirstChildElement("title")) {
          rssItem.title = (titleEl->GetText() != nullptr) ? titleEl->GetText() : "";
        }
        if (auto *linkEl = item->FirstChildElement("link")) {
          const char *href = linkEl->Attribute("href");
          rssItem.url = (href != nullptr) ? href : "";
        }
        if (auto *summaryEl = item->FirstChildElement("summary")) {
          rssItem.description = (summaryEl->GetText() != nullptr) ? summaryEl->GetText() : "";
        } else if (auto *contentEl = item->FirstChildElement("content")) {
          rssItem.description = (contentEl->GetText() != nullptr) ? contentEl->GetText() : "";
        }

        // <image>
        // <title>iSport.cz</title>
        // <url>https://1958898586.rsc.cdn77.org/images/isportcz/dist/svg_fallback/logo-isport.png</url>
        // <link>https://isport.blesk.cz</link>
        // </image>
        if (auto *imageEl = item->FirstChildElement("image")) {
          if (auto *imgUrlEl = imageEl->FirstChildElement("url")) {
            rssItem.rssMedia.url = (imgUrlEl->GetText() != nullptr) ? imgUrlEl->GetText() : "";
          }
          // Type is not usually provided in Atom <image>, set as empty
          rssItem.rssMedia.type = "";
        }

        if (auto *updatedEl = item->FirstChildElement("updated")) {
          rssItem.pubDate = (updatedEl->GetText() != nullptr) ? updatedEl->GetText() : "";
        } else if (auto *publishedEl = item->FirstChildElement("published")) {
          rssItem.pubDate = (publishedEl->GetText() != nullptr) ? publishedEl->GetText() : "";
        }

      } else {
        // Rss
        if (auto *titleEl = item->FirstChildElement("title")) {
          // Handle CDATA sections properly by getting all text content
          const char *text = titleEl->GetText();
          if (text != nullptr) {
            rssItem.title = text;
          } else {
            // If GetText() returns null, try to get text from child nodes (including CDATA)
            auto *textNode = titleEl->FirstChild();
            if ((textNode != nullptr) && (textNode->ToText() != nullptr)) {
              rssItem.title = (textNode->Value() != nullptr) ? textNode->Value() : "";
            }
          }
        }
        if (auto *linkEl = item->FirstChildElement("link")) {
          rssItem.url = (linkEl->GetText() != nullptr) ? linkEl->GetText() : "";
        }

        // TODO: Previous simpler implementation, does not handle CDATA with HTML properly
        // for sure
        
        // if (auto *descEl = item->FirstChildElement("description")) {
        //   // Handle CDATA sections properly by getting all text content
        //   const char *text = descEl->GetText();
        //   if (text != nullptr) {
        //     rssItem.description = text;
        //   } else {
        //     // If GetText() returns null, try to get text from child nodes (including CDATA)
        //     auto *textNode = descEl->FirstChild();
        //     if ((textNode != nullptr) && (textNode->ToText() != nullptr)) {
        //       rssItem.description = (textNode->Value() != nullptr) ? textNode->Value() : "";
        //     }
        //   }
        // }

        // <description><![CDATA[<img
        // src="https://1884403144.rsc.cdn77.org/foto/sport-hokej-spengler-cup-2025-fribourg-sparta-eberle/MzMweDE4MC9jZW50ZXIvdG9wL3NtYXJ0L2ZpbHRlcnM6cXVhbGl0eSg4NSk6Zm9jYWwoMXgyNjoxMzk1eDgxMCkvaW1n/9704364.jpg?v=0&st=4S47btowDVyAyC_uF06gGsgo3PEIczgQwry2TVlqATo&ts=1600812000&e=0"
        // /> Extraligové boje mění na konci roku za nejstarší klubový turnaj na světě. Hokejisté
        // Sparty po přesunu do Švýcarska vstupují do Spengler Cupu. Prestižní akci v Davosu
        // rozehrávají už od 15.10 proti Fribourgu, který před rokem vyhrál. Za Pražany poprvé hraje
        // i nová posila v podobě finského útočníka Kristiana Vesalainena. ONLINE přenos z utkání
        // sledujte na iSportu.]]></description>

        if (auto *descEl = item->FirstChildElement("description")) {
          auto *textNode = descEl->FirstChild();
          if ((textNode != nullptr) && (textNode->ToText() != nullptr)) {
            std::string descValue = textNode->Value() != nullptr ? textNode->Value() : "";

            // Extract image from description if possible
            std::smatch match;
            std::regex imgRegex(R"(<img[^>]+src=["']([^"']+)["'][^>]*>)");
            if (std::regex_search(descValue, match, imgRegex) && match.size() > 1) {
              rssItem.rssMedia.url = match[1].str();
              rssItem.rssMedia.type = "hybrid_type";
            }

            // Remove img tag from description
            std::string cleanDesc = std::regex_replace(descValue, imgRegex, "");

            // Optional: trim whitespace
            cleanDesc.erase(0, cleanDesc.find_first_not_of(" \t\n\r"));
            cleanDesc.erase(cleanDesc.find_last_not_of(" \t\n\r") + 1);

            rssItem.description = cleanDesc;
          }
        }

        // <media:content
        // url="https://1gr.cz/fotky/idnes/24/063/cl6/IHA6058a4a762_shutterstock_1259282710.jpg"
        // type="image/jpeg"/>
        if (auto *mediaContentEl = item->FirstChildElement("media:content")) {
          const char *mediaUrl = mediaContentEl->Attribute("url");
          rssItem.rssMedia.url = (mediaUrl != nullptr) ? mediaUrl : "";
          const char *mediaType = mediaContentEl->Attribute("type");
          rssItem.rssMedia.type = (mediaType != nullptr) ? mediaType : "";
        }

        // <enclosure url="https://i.iinfo.cz/images/668/disky-1.jpg" length="78026"
        // type="image/jpeg"/>
        if (auto *enclosureEl = item->FirstChildElement("enclosure")) {
          const char *encUrl = enclosureEl->Attribute("url");
          rssItem.rssMedia.url = (encUrl != nullptr) ? encUrl : "";
          const char *encType = enclosureEl->Attribute("type");
          rssItem.rssMedia.type = (encType != nullptr) ? encType : "";
        }

        if (auto *dateEl = item->FirstChildElement("pubDate")) {
          rssItem.pubDate = (dateEl->GetText() != nullptr) ? dateEl->GetText() : "";
        }
      }

      if (rssItem.title.empty() || rssItem.url.empty()) {
        continue;
      };

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

  bool RssManager::saveAllSeenHashes() {
    nlohmann::json jsonData = nlohmann::json::array();
    for (const auto &h : seenHashes_) {
      jsonData.push_back(h);
    }

    std::ofstream file(hashesPath_);
    if (!file.is_open()) {
      return false;
    }
    file << jsonData.dump(4);
    return true;
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

  bool RssManager::hasFilesChanged() {
    bool urlsChanged = hasFileChanged(urlsPath_, urlsLastModified_);
    bool hashesChanged = hasFileChanged(hashesPath_, hashesLastModified_);

    if (urlsChanged) {
      logger_->infoStream() << "URLs file changed, reloading...";
      loadUrls();
    }

    if (hashesChanged) {
      logger_->infoStream() << "Hashes file changed, reloading...";
      loadSeenHashes();
    }
    return (urlsChanged || hashesChanged);
  }

  size_t RssManager::WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
  }

  std::string RssManager::getItemAsMarkdown(const RSSItem &item) { return item.toMarkdownLink(); }
  void RssManager::clearFeedBuffer() { feed_.clear(); }

} // namespace dotnamecpp::rss