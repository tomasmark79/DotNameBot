#pragma once
#include <Rss/RSSItem.hpp>
#include <filesystem>
#include <vector>

namespace dotnamebot::rss {

  class HtmlFeedWriter {
  public:
    static bool write(const std::vector<RSSItem> &items, const std::filesystem::path &outputPath);

  private:
    static std::string escapeHtml(const std::string &str);
    static std::string buildHtml(const std::vector<RSSItem> &items);
    static std::string labelToInitials(const std::string &label);
    static std::string labelToColor(const std::string &label);
  };

} // namespace dotnamebot::rss
