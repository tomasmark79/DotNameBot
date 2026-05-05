
#include "RssManager.hpp"

#include <algorithm>
#include <curl/curl.h>
#include <fstream>
#include <iconv.h>
#include <random>
#include <regex>
#include <string>
#include <utility>

namespace dotnamebot::rss {

  RssManager::RssManager(std::shared_ptr<dotnamebot::logging::ILogger> logger,
                         std::shared_ptr<dotnamebot::assets::IAssetManager> assetManager)
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
          {{{"url", "https://blog.digitalspace.name/feed/atom"}, {"embeddedType", 0}}});

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
      int items = fetchUrlSource(rssUrl.url, rssUrl.embeddedType, rssUrl.discordChannelId);
      if (items > 0) {
        totalItems += items;
      }
    }
    logger_->infoStream() << "Total fetched items: " << totalItems
                          << " (total in buffer: " << feed_.items.size() << ")";
    return totalItems;
  }

  bool RssManager::addUrl(const std::string &url, long embedded, uint64_t discordChannelId) {
    for (const auto &existingUrl : urls_) {
      if (existingUrl.url == url) {
        logger_->warningStream() << "URL already exists: " << url;
        return false;
      }
    }
    urls_.emplace_back(url, embedded, discordChannelId);
    return saveUrls();
  }

  bool RssManager::modUrl(const std::string &url, long embeddedType, uint64_t discordChannelId) {
    for (auto &existingUrl : urls_) {
      if (existingUrl.url == url) {
        existingUrl.embeddedType = embeddedType;
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
                          {"embeddedType", url.embeddedType},
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
      sourcesList += "- " + url.url + " with embeddedType " + std::to_string(url.embeddedType);
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
        sourcesList +=
            "- " + url.url + " with embeddedType " + std::to_string(url.embeddedType) + "\n";
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
        long embedded = item.contains("embeddedType") ? item["embeddedType"].get<long>() : 0;
        uint64_t discordChannelId = 0;
        if (item.contains("discordChannelId") && !item["discordChannelId"].is_null()) {
          discordChannelId = item["discordChannelId"].get<uint64_t>();
        }
        urls_.emplace_back(url, embedded, discordChannelId);
      } else if (item.is_string()) {
        // Backwards compatibility - treat strings as non-embedded
        urls_.emplace_back(item.get<std::string>(), 0);
      }
    }

    logger_->infoStream() << "Loaded " << urls_.size() << " RSS URLs.";
    return true;
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

  // TODO: Improve parsing robustness and support more RSS/Atom variants
  RSSFeed RssManager::parseRSS(const std::string &xmlData, long embeddedType,
                               uint64_t discordChannelId, int &totalDuplicateItems) {
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
      // rssItem.embedded = embedded;
      rssItem.embeddedType = static_cast<EmbeddedType>(embeddedType);
      rssItem.discordChannelId = discordChannelId;

      if (isAtom) {
        // Atom
        if (auto *titleEl = item->FirstChildElement("title")) {
          const char *text = titleEl->GetText();
          if (text != nullptr) {
            rssItem.title = text;
          } else {
            // Fallback: get text from child node (handles CDATA)
            auto *textNode = titleEl->FirstChild();
            if ((textNode != nullptr) && (textNode->ToText() != nullptr)) {
              rssItem.title = (textNode->Value() != nullptr) ? textNode->Value() : "";
            }
          }
          // Strip any residual HTML tags from title (e.g. type="html")
          std::regex htmlTagRegexTitle("<[^>]*>");
          rssItem.title = std::regex_replace(rssItem.title, htmlTagRegexTitle, "");
          rssItem.title = decodeHtmlEntities(rssItem.title);
        }
        // Prefer link with rel="alternate"; fall back to first link with href
        {
          const tinyxml2::XMLElement *chosenLinkEl = nullptr;
          for (auto *linkEl = item->FirstChildElement("link"); linkEl != nullptr;
               linkEl = linkEl->NextSiblingElement("link")) {
            const char *rel = linkEl->Attribute("rel");
            if (rel != nullptr && std::string(rel) == "alternate") {
              chosenLinkEl = linkEl;
              break;
            }
            if (chosenLinkEl == nullptr && linkEl->Attribute("href") != nullptr) {
              chosenLinkEl = linkEl; // first link with href as fallback
            }
          }
          if (chosenLinkEl != nullptr) {
            const char *href = chosenLinkEl->Attribute("href");
            rssItem.url = (href != nullptr) ? href : "";
          }
        }
        // Parse summary or content, strip HTML tags and extract image
        // Helper lambda to get raw text of an XML element (handles CDATA)
        auto getRawText = [](const tinyxml2::XMLElement *el) -> std::string {
          if (el == nullptr) {
            return {};
          }
          const char *text = el->GetText();
          if (text != nullptr) {
            return text;
          }
          const auto *textNode = el->FirstChild();
          if ((textNode != nullptr) && (textNode->ToText() != nullptr) &&
              (textNode->Value() != nullptr)) {
            return textNode->Value();
          }
          return {};
        };

        {
          // Use <summary> for description text; fall back to <content> if missing
          const tinyxml2::XMLElement *descEl = item->FirstChildElement("summary");
          const tinyxml2::XMLElement *contentEl = item->FirstChildElement("content");
          if (descEl == nullptr) {
            descEl = contentEl;
          }

          if (descEl != nullptr) {
            std::string descValue = getRawText(descEl);

            if (!descValue.empty()) {
              // Decode HTML entities first
              descValue = decodeHtmlEntities(descValue);

              // Extract image if present
              std::smatch imgMatch;
              std::regex imgRegex(R"(<img[^>]+src=["']([^"']+)["'][^>]*>)");
              if (std::regex_search(descValue, imgMatch, imgRegex) && imgMatch.size() > 1) {
                rssItem.rssMedia.url = imgMatch[1].str();
                rssItem.rssMedia.type = "image/";
              }

              // Remove all HTML tags
              std::regex htmlTagRegex("<[^>]*>");
              descValue = std::regex_replace(descValue, htmlTagRegex, "");

              // Trim whitespace
              descValue.erase(0, descValue.find_first_not_of(" \t\n\r"));
              if (!descValue.empty()) {
                descValue.erase(descValue.find_last_not_of(" \t\n\r") + 1);
              }

              rssItem.description = descValue;
            }
          }

          // If no image found yet, also scan <content> (e.g. when description came from <summary>)
          if (rssItem.rssMedia.url.empty() && contentEl != nullptr && contentEl != descEl) {
            std::string contentValue = getRawText(contentEl);
            if (!contentValue.empty()) {
              contentValue = decodeHtmlEntities(contentValue);
              std::smatch imgMatch;
              std::regex imgRegex(R"(<img[^>]+src=["']([^"']+)["'][^>]*>)");
              if (std::regex_search(contentValue, imgMatch, imgRegex) && imgMatch.size() > 1) {
                rssItem.rssMedia.url = imgMatch[1].str();
                rssItem.rssMedia.type = "image/";
              }
            }
          }
        }

        // <image>
        // <title>iSport.cz</title>
        // <url>https://picture.png</url>
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

        if (auto *descEl = item->FirstChildElement("description")) {
          auto *textNode = descEl->FirstChild();
          if (textNode != nullptr && textNode->Value() != nullptr) {
            std::string descValue = textNode->Value();

            // Decode HTML entities first
            descValue = decodeHtmlEntities(descValue);

            // Now extract CDATA content if present
            std::regex cdataRegex(R"(<!\[CDATA\[(.*?)\]\]>)");
            std::smatch cdataMatch;
            if (std::regex_search(descValue, cdataMatch, cdataRegex) && cdataMatch.size() > 1) {
              descValue = cdataMatch[1].str();
            }

            // Extract image if present
            std::smatch imgMatch;
            std::regex imgRegex(R"(<img[^>]+src=["']([^"']+)["'][^>]*>)");
            if (std::regex_search(descValue, imgMatch, imgRegex) && imgMatch.size() > 1) {
              rssItem.rssMedia.url = imgMatch[1].str();
              rssItem.rssMedia.type = "image/";
            }

            // Remove all HTML tags
            std::regex htmlTagRegex("<[^>]*>");
            std::string cleanDesc = std::regex_replace(descValue, htmlTagRegex, "");

            // Trim whitespace
            cleanDesc.erase(0, cleanDesc.find_first_not_of(" \t\n\r"));
            cleanDesc.erase(cleanDesc.find_last_not_of(" \t\n\r") + 1);

            rssItem.description = cleanDesc;
          }
        }

        // <media:content
        // url="https://picture.jpg"
        // type="image/jpeg"/>

        // <media:content
        // url="https://picture.png"
        // medium="image"/>

        if (auto *mediaContentEl = item->FirstChildElement("media:content")) {
          const char *mediaUrl = mediaContentEl->Attribute("url");
          rssItem.rssMedia.url = (mediaUrl != nullptr) ? mediaUrl : "";

          const char *mediaType = mediaContentEl->Attribute("type");
          rssItem.rssMedia.type = (mediaType != nullptr) ? mediaType : "";
          if (rssItem.rssMedia.type.empty()) {
            const char *mediaMedium = mediaContentEl->Attribute("medium");
            if (mediaMedium != nullptr && std::string(mediaMedium) == "image") {
              rssItem.rssMedia.type = "image/";
            }
          }
        }

        // <enclosure url="https://picture.jpg" length="78026"
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

        // <szn:image>
        // <szn:url>https://picture.jpg</szn:url>
        // </szn:image>
        if (auto *sznImageEl = item->FirstChildElement("szn:image")) {
          if (auto *sznUrlEl = sznImageEl->FirstChildElement("szn:url")) {
            rssItem.rssMedia.url = (sznUrlEl->GetText() != nullptr) ? sznUrlEl->GetText() : "";
          }
          rssItem.rssMedia.type = "image/"; // Type is not usually provided, set as image
        }
      } // End RSS vs Atom parsing

      if (rssItem.title.empty() || rssItem.url.empty()) {
        continue;
      };

      rssItem.generateHash(); // Generate hash from original, unprocessed data

      // Skip if already seen
      if (seenHashes_.contains(rssItem.hash)) {
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

  int RssManager::fetchUrlSource(const std::string &url, long embeddedType,
                                 uint64_t discordChannelId) {
    std::string xmlData = downloadFeed(url);
    if (xmlData.empty()) {
      return -1;
    }

    xmlData = convertToUtf8(xmlData);

    int totalDuplicateItems = 0;
    RSSFeed newFeed = parseRSS(xmlData, embeddedType, discordChannelId, totalDuplicateItems);

    int addedItems = 0;
    for (const auto &item : newFeed.items) {
      feed_.addItem(item);
      addedItems++;
    }

    logger_->infoStream() << "New " << addedItems << " items added to the feed buffer."
                          << " Found " << totalDuplicateItems << " seen items."
                          << " url: " << url << " (embeddedType: " << embeddedType << ")"
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

  std::string RssManager::convertToUtf8(const std::string &xmlData) const {
    // Extract encoding from XML declaration, e.g. <?xml version="1.0" encoding="windows-1250" ?>
    std::regex encodingRegex(R"(encoding\s*=\s*["']([^"']+)["'])", std::regex::icase);
    std::smatch match;
    if (!std::regex_search(xmlData, match, encodingRegex)) {
      return xmlData; // No encoding declaration — assume UTF-8
    }

    std::string declaredEncoding = match[1].str();

    // Normalise to upper-case for comparison
    std::string upperEncoding = declaredEncoding;
    std::transform(upperEncoding.begin(), upperEncoding.end(), upperEncoding.begin(), ::toupper);

    if (upperEncoding == "UTF-8" || upperEncoding == "UTF8" || upperEncoding == "US-ASCII" ||
        upperEncoding == "ASCII") {
      return xmlData; // Already UTF-8 compatible, nothing to do
    }

    // Use iconv to transcode to UTF-8
    iconv_t cd = iconv_open("UTF-8", declaredEncoding.c_str());
    if (cd == (iconv_t)-1) {
      logger_->warningStream() << "convertToUtf8: iconv_open failed for encoding '"
                               << declaredEncoding << "' — returning raw data";
      return xmlData;
    }

    // Allocate output buffer (UTF-8 is at most 4 bytes per character)
    std::string output(xmlData.size() * 4, '\0');
    const char *inPtr = xmlData.data();
    size_t inLeft = xmlData.size();
    char *outPtr = output.data();
    size_t outLeft = output.size();

    size_t rc = iconv(cd, const_cast<char **>(&inPtr), &inLeft, &outPtr, &outLeft);
    iconv_close(cd);

    if (rc == (size_t)-1) {
      logger_->warningStream() << "convertToUtf8: iconv conversion failed for encoding '"
                               << declaredEncoding << "' — returning raw data";
      return xmlData;
    }

    output.resize(output.size() - outLeft);

    // Update the encoding declaration so tinyxml2 treats the data as UTF-8
    std::regex replaceEncoding(R"(encoding\s*=\s*["'][^"']+["'])", std::regex::icase);
    output = std::regex_replace(output, replaceEncoding, "encoding=\"UTF-8\"");

    logger_->infoStream() << "convertToUtf8: transcoded from '" << declaredEncoding << "' to UTF-8";
    return output;
  }

  // Helper function to convert Unicode code point to UTF-8 bytes
  static std::string codePointToUtf8(uint32_t codePoint) {
    std::string utf8;
    if (codePoint <= 0x7F) {
      // 1 byte (ASCII)
      utf8 += static_cast<char>(codePoint);
    } else if (codePoint <= 0x7FF) {
      // 2 bytes
      utf8 += static_cast<char>(0xC0 | (codePoint >> 6));
      utf8 += static_cast<char>(0x80 | (codePoint & 0x3F));
    } else if (codePoint <= 0xFFFF) {
      // 3 bytes
      utf8 += static_cast<char>(0xE0 | (codePoint >> 12));
      utf8 += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
      utf8 += static_cast<char>(0x80 | (codePoint & 0x3F));
    } else if (codePoint <= 0x10FFFF) {
      // 4 bytes
      utf8 += static_cast<char>(0xF0 | (codePoint >> 18));
      utf8 += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
      utf8 += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
      utf8 += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
    return utf8;
  }

  std::string RssManager::decodeHtmlEntities(const std::string &str) {
    std::string result = str;

    // Decode numeric character references: &#353; (decimal) and &#x0161; (hexadecimal)
    std::regex numericEntityRegex(R"(&#(?:([0-9]+)|x([0-9a-fA-F]+));)");
    std::smatch match;
    std::string::const_iterator searchStart(result.cbegin());
    std::string decodedResult;

    while (std::regex_search(searchStart, result.cend(), match, numericEntityRegex)) {
      // Append text before the match
      decodedResult.append(match.prefix().first, match.prefix().second);

      uint32_t codePoint = 0;
      if (match[1].matched) {
        // Decimal entity: &#353;
        codePoint = static_cast<uint32_t>(std::stoul(match[1].str()));
      } else if (match[2].matched) {
        // Hexadecimal entity: &#x0161;
        codePoint = static_cast<uint32_t>(std::stoul(match[2].str(), nullptr, 16));
      }

      // Convert to UTF-8 and append
      if (codePoint <= 0x10FFFF) {
        decodedResult += codePointToUtf8(codePoint);
      }

      searchStart = match.suffix().first;
    }
    // Append remaining text after last match
    decodedResult.append(searchStart, result.cend());
    result = decodedResult;

    // Decode named HTML entities
    size_t pos = 0;
    while ((pos = result.find("&lt;", pos)) != std::string::npos) {
      result.replace(pos, 4, "<");
      pos += 1;
    }
    pos = 0;
    while ((pos = result.find("&gt;", pos)) != std::string::npos) {
      result.replace(pos, 4, ">");
      pos += 1;
    }
    pos = 0;
    while ((pos = result.find("&amp;", pos)) != std::string::npos) {
      result.replace(pos, 5, "&");
      pos += 1;
    }
    pos = 0;
    while ((pos = result.find("&quot;", pos)) != std::string::npos) {
      result.replace(pos, 6, "\"");
      pos += 1;
    }
    pos = 0;
    while ((pos = result.find("&apos;", pos)) != std::string::npos) {
      result.replace(pos, 6, "'");
      pos += 1;
    }

    return result;
  }

} // namespace dotnamebot::rss