#include "HtmlFeedWriter.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

namespace dotnamebot::rss {

  static constexpr const char *PALETTE[] = {
      "#5865f2", "#eb459e", "#57f287", "#faa61a", "#ed4245",
      "#00a8fc", "#ff7f7f", "#f47fff", "#43b581", "#fee75c",
  };
  static constexpr size_t PALETTE_SIZE = 10;

  std::string HtmlFeedWriter::labelToColor(const std::string &label) {
    size_t hash = 0;
    for (unsigned char c : label) {
      hash = hash * 31 + c;
    }
    return PALETTE[hash % PALETTE_SIZE];
  }

  std::string HtmlFeedWriter::labelToInitials(const std::string &label) {
    if (label.empty()) {
      return "?";
    }
    std::string result;
    bool next = true;
    for (unsigned char c : label) {
      if (std::isspace(c) || c == '.') {
        next = true;
      } else if (next && std::isalnum(c)) {
        result += static_cast<char>(std::toupper(c));
        next = false;
        if (result.size() >= 2) {
          break;
        }
      }
    }
    return result.empty() ? label.substr(0, 1) : result;
  }

  static std::string labelToAnchor(const std::string &label) {
    std::string id;
    for (unsigned char c : label) {
      id += std::isalnum(c) ? static_cast<char>(std::tolower(c)) : '-';
    }
    std::string out;
    bool prevDash = false;
    for (char c : id) {
      if (c == '-') {
        if (!prevDash && !out.empty()) {
          out += c;
        }
        prevDash = true;
      } else {
        out += c;
        prevDash = false;
      }
    }
    while (!out.empty() && out.back() == '-') {
      out.pop_back();
    }
    return out.empty() ? "feed" : out;
  }

  // Tries RFC 822 ("Mon, 07 May 2026 10:00:00 +0000") then ISO 8601 ("2026-05-07T10:00:00Z").
  // Returns 0 when unparseable — those items sort to the end.
  static time_t parsePubDate(const std::string &s) {
    if (s.empty()) {
      return 0;
    }
    std::tm tm{};
    if (strptime(s.c_str(), "%a, %d %b %Y %H:%M:%S", &tm) != nullptr) {
      return mktime(&tm);
    }
    if (strptime(s.c_str(), "%Y-%m-%dT%H:%M:%S", &tm) != nullptr) {
      return mktime(&tm);
    }
    if (strptime(s.c_str(), "%Y-%m-%d", &tm) != nullptr) {
      return mktime(&tm);
    }
    return 0;
  }

  std::string HtmlFeedWriter::escapeHtml(const std::string &str) {
    std::string out;
    out.reserve(str.size());
    for (unsigned char c : str) {
      switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += static_cast<char>(c); break;
      }
    }
    return out;
  }

  std::string HtmlFeedWriter::buildHtml(const std::vector<RSSItem> &items) {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
    localtime_r(&now, &tm);
    std::ostringstream ts;
    ts << (tm.tm_mday < 10 ? "0" : "") << tm.tm_mday << "." << (tm.tm_mon + 1 < 10 ? "0" : "")
       << (tm.tm_mon + 1) << "." << (tm.tm_year + 1900) << " " << (tm.tm_hour < 10 ? "0" : "")
       << tm.tm_hour << ":" << (tm.tm_min < 10 ? "0" : "") << tm.tm_min;
    std::string timestamp = ts.str();

    // Group items by feedLabel, preserving first-seen order of labels
    std::vector<std::string> labelOrder;
    std::map<std::string, std::vector<const RSSItem *>> groups;
    for (const auto &item : items) {
      const std::string &lbl = item.feedLabel.empty() ? "feed" : item.feedLabel;
      if (groups.find(lbl) == groups.end()) {
        labelOrder.push_back(lbl);
      }
      groups[lbl].push_back(&item);
    }

    // Sort each section newest-first; unparseable dates go to the end
    for (auto &[lbl, sect] : groups) {
      std::stable_sort(sect.begin(), sect.end(), [](const RSSItem *a, const RSSItem *b) {
        time_t ta = parsePubDate(a->pubDate);
        time_t tb = parsePubDate(b->pubDate);
        if (ta == 0 && tb == 0) {
          return false;
        }
        if (ta == 0) {
          return false;
        }
        if (tb == 0) {
          return true;
        }
        return ta > tb;
      });
    }

    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html lang="cs">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>DotNameBot Feed</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',system-ui,sans-serif;background:#f2f3f5;color:#2e3338;display:flex;min-height:100vh}

/* ── sidebar ── */
#sidebar{width:210px;flex-shrink:0;position:sticky;top:0;height:100vh;overflow-y:auto;background:#e3e5e8;border-right:1px solid #d0d1d3;display:flex;flex-direction:column}
.sb-head{padding:.85rem .9rem .6rem;border-bottom:1px solid #d0d1d3}
.sb-head h1{font-size:.95rem;font-weight:700;color:#060607}
.sb-head .meta{font-size:.68rem;color:#5c5f66;margin-top:.2rem;line-height:1.4}
.sb-nav{padding:.5rem .4rem;flex:1}
.sb-label{display:flex;align-items:center;gap:.5rem;padding:.35rem .5rem;border-radius:4px;text-decoration:none;color:#5c5f66;font-size:.82rem;cursor:pointer;transition:background .12s,color .12s;border:none;background:none;width:100%;text-align:left}
.sb-label:hover{background:#d0d1d3;color:#060607}
.sb-label.active{background:#c7c9cd;color:#060607;font-weight:600}
.sb-label .dot{width:8px;height:8px;border-radius:50%;flex-shrink:0}
.sb-label .name{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.sb-label .cnt{font-size:.7rem;color:#80848e;flex-shrink:0}
.sb-label.active .cnt{color:#4e5058}
.sb-all{border-bottom:1px solid #d0d1d3;margin-bottom:.35rem;padding-bottom:.35rem}
.sb-total{padding:.6rem .9rem;border-top:1px solid #d0d1d3;font-size:.68rem;color:#80848e}

/* ── main content ── */
#content{flex:1;min-width:0;max-width:720px;padding:1.5rem 1.25rem 3rem}

/* ── section ── */
section{scroll-margin-top:1rem;margin-bottom:2rem}
.sec-header{display:flex;align-items:center;gap:.6rem;padding-bottom:.5rem;margin-bottom:.5rem;border-bottom:2px solid #d0d1d3}
.sec-avatar{width:32px;height:32px;border-radius:50%;flex-shrink:0;display:flex;align-items:center;justify-content:center;font-size:.7rem;font-weight:700;color:#fff}
.sec-title{font-size:.95rem;font-weight:600;color:#060607}
.sec-cnt{font-size:.75rem;color:#80848e;margin-left:.2rem}

/* ── article card ── */
.card{display:flex;gap:.65rem;padding:.4rem .4rem;border-radius:4px;margin-bottom:1px}
.card:hover{background:#e8e9eb}
.c-avatar{width:36px;height:36px;border-radius:50%;flex-shrink:0;display:flex;align-items:center;justify-content:center;font-weight:700;color:#fff;font-size:.68rem}
.c-body{flex:1;min-width:0}
.c-meta{font-size:.72rem;color:#5c5f66;margin-bottom:.15rem}
.c-meta .name{font-weight:600;margin-right:.35rem}
.embed{border-left:4px solid #5865f2;background:#fff;border-radius:0 4px 4px 0;padding:.5rem .7rem;overflow:hidden;box-shadow:0 1px 2px rgba(0,0,0,.06)}
.embed-title a{font-size:.88rem;font-weight:600;color:#0068e0;text-decoration:none}
.embed-title a:hover{text-decoration:underline}
.embed-desc{font-size:.78rem;color:#4e5058;margin-top:.2rem;display:-webkit-box;-webkit-line-clamp:3;-webkit-box-orient:vertical;overflow:hidden}
.embed-thumb{float:right;max-width:72px;max-height:72px;border-radius:4px;margin-left:.65rem;object-fit:cover}

/* ── mobile: sidebar becomes a top bar ── */
@media(max-width:640px){
  body{flex-direction:column}
  #sidebar{width:100%;height:auto;position:sticky;top:0;flex-direction:row;flex-wrap:nowrap;overflow-x:auto;border-right:none;border-bottom:1px solid #d0d1d3;z-index:100}
  .sb-head{padding:.5rem .75rem;border-bottom:none;border-right:1px solid #d0d1d3;white-space:nowrap}
  .sb-head .meta{display:none}
  .sb-nav{display:flex;flex-direction:row;padding:.4rem .5rem;gap:.25rem;overflow-x:auto}
  .sb-all{border-bottom:none;margin-bottom:0;padding-bottom:0;border-right:1px solid #d0d1d3}
  .sb-total{display:none}
  .sb-label{white-space:nowrap}
  #content{padding:1rem .75rem 2rem}
}
</style>
</head>
<body>
)";

    // ── Sidebar ──────────────────────────────────────────────────────────────
    html << "<aside id=\"sidebar\">\n"
         << "  <div class=\"sb-head\">\n"
         << "    <h1>#feeder</h1>\n"
         << "    <div class=\"meta\">" << escapeHtml(timestamp) << "</div>\n"
         << "  </div>\n"
         << "  <nav class=\"sb-nav\">\n"
         << "    <a href=\"#top\" class=\"sb-label sb-all\" data-section=\"*\">"
         << "<span class=\"name\">Vše</span>"
         << "<span class=\"cnt\">" << items.size() << "</span></a>\n";

    for (const auto &lbl : labelOrder) {
      std::string anchor = labelToAnchor(lbl);
      std::string color = labelToColor(lbl);
      std::string initials = labelToInitials(lbl);
      html << "    <a href=\"#" << anchor << "\" class=\"sb-label\" data-section=\""
           << escapeHtml(anchor) << "\">"
           << "<span class=\"dot\" style=\"background:" << color << "\"></span>"
           << "<span class=\"name\">" << escapeHtml(lbl) << "</span>"
           << "<span class=\"cnt\">" << groups[lbl].size() << "</span></a>\n";
    }

    html << "  </nav>\n"
         << "  <div class=\"sb-total\">" << items.size() << " položek &mdash; " << labelOrder.size()
         << " zdrojů</div>\n"
         << "</aside>\n";

    // ── Content ──────────────────────────────────────────────────────────────
    html << "<main id=\"content\">\n<div id=\"top\"></div>\n";

    for (const auto &lbl : labelOrder) {
      std::string anchor = labelToAnchor(lbl);
      std::string color = labelToColor(lbl);
      std::string initials = labelToInitials(lbl);
      const auto &sect = groups[lbl];

      html << "<section id=\"" << anchor << "\">\n"
           << "  <div class=\"sec-header\">\n"
           << "    <div class=\"sec-avatar\" style=\"background:" << color << "\">"
           << escapeHtml(initials) << "</div>\n"
           << "    <span class=\"sec-title\">" << escapeHtml(lbl) << "</span>\n"
           << "    <span class=\"sec-cnt\">" << sect.size() << " článků</span>\n"
           << "  </div>\n";

      for (const RSSItem *item : sect) {
        html << "  <div class=\"card\">\n"
             << "    <div class=\"c-avatar\" style=\"background:" << color << "\">"
             << escapeHtml(initials) << "</div>\n"
             << "    <div class=\"c-body\">\n"
             << "      <div class=\"c-meta\"><span class=\"name\" style=\"color:" << color << "\">"
             << escapeHtml(lbl) << "</span>";
        if (!item->pubDate.empty()) {
          html << "<span>" << escapeHtml(item->pubDate) << "</span>";
        }
        html << "</div>\n"
             << "      <div class=\"embed\" style=\"border-left-color:" << color << "\">\n";
        if (!item->rssMedia.url.empty() && item->rssMedia.type.find("image") != std::string::npos) {
          html << "        <img class=\"embed-thumb\" src=\"" << escapeHtml(item->rssMedia.url)
               << "\" alt=\"\" loading=\"lazy\">\n";
        }
        html << "        <div class=\"embed-title\"><a href=\"" << escapeHtml(item->url)
             << "\" target=\"_blank\" rel=\"noopener\">" << escapeHtml(item->title)
             << "</a></div>\n";
        if (!item->description.empty()) {
          html << "        <div class=\"embed-desc\">" << escapeHtml(item->description)
               << "</div>\n";
        }
        html << "      </div>\n    </div>\n  </div>\n";
      }

      html << "</section>\n";
    }

    html << "</main>\n";

    // IntersectionObserver — highlights the sidebar entry for the visible section
    html << R"(<script>
(function(){
  var links = document.querySelectorAll('#sidebar .sb-label[data-section]');
  var sections = document.querySelectorAll('#content section[id]');
  if(!sections.length) return;
  function activate(id){
    links.forEach(function(a){
      var match = id ? a.getAttribute('data-section')===id : a.getAttribute('data-section')==='*';
      a.classList.toggle('active', match);
    });
  }
  var current = '';
  var obs = new IntersectionObserver(function(entries){
    entries.forEach(function(e){
      if(e.isIntersecting) current = e.target.id;
    });
    activate(current);
  },{rootMargin:'-10% 0px -80% 0px'});
  sections.forEach(function(s){ obs.observe(s); });
  activate('');
})();
</script>
</body>
</html>
)";
    return html.str();
  }

  bool HtmlFeedWriter::write(const std::vector<RSSItem> &items,
                             const std::filesystem::path &outputPath) {
    if (std::filesystem::exists(outputPath)) {
      auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
      std::tm tm{};
      localtime_r(&now, &tm);
      std::ostringstream ts;
      ts << (tm.tm_year + 1900) << (tm.tm_mon + 1 < 10 ? "0" : "") << (tm.tm_mon + 1)
         << (tm.tm_mday < 10 ? "0" : "") << tm.tm_mday << "_" << (tm.tm_hour < 10 ? "0" : "")
         << tm.tm_hour << (tm.tm_min < 10 ? "0" : "") << tm.tm_min << (tm.tm_sec < 10 ? "0" : "")
         << tm.tm_sec;

      auto stem = outputPath.stem().string();
      auto ext = outputPath.extension().string();
      auto backup = outputPath.parent_path() / (stem + "_" + ts.str() + ext);
      std::filesystem::rename(outputPath, backup);
    }

    std::ofstream file(outputPath);
    if (!file.is_open()) {
      return false;
    }
    file << buildHtml(items);
    return file.good();
  }

} // namespace dotnamebot::rss
