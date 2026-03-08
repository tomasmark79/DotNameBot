/**
 * @file RssFeedLiveTest.cpp
 * @brief Live (network) tests for RSS/Atom feed parsing against real URLs from rssUrls.json.
 *
 * These tests require internet access.
 * Each test fetches a single feed, parses it, and verifies that at least one item
 * with a non-empty title and URL was produced.
 *
 * Feed format coverage:
 *   RSS 2.0 (Czech)  – root.cz, lupa.cz, novinky.cz, pctuning.cz
 *   RSS 2.0 (EN)     – bbc, phoronix, itsfoss
 *   Atom (Czech)     – sampler.cz, hitzone.cz, muzikantiakapely.cz
 *   Atom (EN)        – 9to5linux, fedoramagazine, kernel.org, theregister
 *   RDF / RSS 1.0    – dw.com (rdf/rss-en-all)
 */

#include "../src/lib/Rss/RssManager.hpp"
#include "../src/lib/Utils/Logger/ConsoleLogger.hpp"
#include "MockAssetManager.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <string>meson test --suite live

using namespace dotnamebot::rss;
using namespace dotnamebot::logging;
using namespace dotnamebot::assets;

// ---------------------------------------------------------------------------
// Parameter type
// ---------------------------------------------------------------------------

struct FeedTestParam {
  std::string description; ///< used as test display name
  std::string url;
};

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class RssFeedLiveTest : public ::testing::TestWithParam<FeedTestParam> {
protected:
  void SetUp() override {
    // Unique temp dir per test run
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                  std::chrono::high_resolution_clock::now().time_since_epoch())
                  .count();
    testDir_ =
        std::filesystem::temp_directory_path() / ("rss_live_test_" + std::to_string(ns));
    std::filesystem::create_directories(testDir_);

    // Pre-create empty JSON files so RssManager does not inject its default URL
    std::ofstream(testDir_ / "rssUrls.json") << "[]";
    std::ofstream(testDir_ / "seenHashes.json") << "[]";

    auto logger = std::make_shared<ConsoleLogger>();
    logger->setLevel(Level::LOG_WARNING); // suppress noise in test output

    auto assetManager = std::make_shared<MockAssetManager>(testDir_);
    manager_ = std::make_shared<RssManager>(logger, assetManager);
  }

  void TearDown() override {
    manager_.reset();
    if (std::filesystem::exists(testDir_)) {
      std::filesystem::remove_all(testDir_);
    }
  }

  std::filesystem::path testDir_;
  std::shared_ptr<RssManager> manager_;
};

// ---------------------------------------------------------------------------
// Test body
// ---------------------------------------------------------------------------

TEST_P(RssFeedLiveTest, ParsesAtLeastOneItem) {
  const auto &param = GetParam();

  ASSERT_TRUE(manager_->addUrl(param.url, 2 /*EMBEDDED_AS_ADVANCED*/, 0))
      << "addUrl() failed for: " << param.url;

  int fetched = manager_->refetchRssFeeds();
  EXPECT_GT(fetched, 0) << "No items fetched from: " << param.url;
  EXPECT_GT(manager_->getItemCount(), 0u) << "Feed buffer empty for: " << param.url;

  if (manager_->getItemCount() > 0) {
    RSSItem item = manager_->getRandomItem();
    EXPECT_FALSE(item.title.empty()) << "Item title is empty for: " << param.url;
    EXPECT_FALSE(item.url.empty()) << "Item URL is empty for: " << param.url;
  }
}

// ---------------------------------------------------------------------------
// Parameter list
// ---------------------------------------------------------------------------

// clang-format off
INSTANTIATE_TEST_SUITE_P(
    LiveFeeds,
    RssFeedLiveTest,
    ::testing::Values(
        // ── RSS 2.0 Czech ──────────────────────────────────────────────────
        FeedTestParam{"root_cz_clanky",           "https://www.root.cz/rss/clanky"},
        FeedTestParam{"lupa_cz_clanky",           "https://www.lupa.cz/rss/clanky"},
        FeedTestParam{"novinky_cz",               "https://www.novinky.cz/rss"},
        FeedTestParam{"pctuning_cz",              "https://pctuning.cz/rss/all"},

        // ── RSS 2.0 English ────────────────────────────────────────────────
        FeedTestParam{"bbc_news",                 "https://feeds.bbci.co.uk/news/rss.xml"},
        FeedTestParam{"phoronix",                 "https://www.phoronix.com/rss.php"},
        FeedTestParam{"itsfoss",                  "https://itsfoss.com/rss"},

        // ── Atom Czech ────────────────────────────────────────────────────
        FeedTestParam{"sampler_cz",               "https://sampler.cz/feed/atom"},
        FeedTestParam{"hitzone_cz",               "https://hitzone.cz/feed/atom"},
        FeedTestParam{"muzikantiakapely_cz",       "https://muzikantiakapely.cz/feed/atom"},

        // ── Atom English ──────────────────────────────────────────────────
        FeedTestParam{"9to5linux",                "https://9to5linux.com/feed/atom"},
        FeedTestParam{"fedoramagazine",           "https://fedoramagazine.org/feed/atom"},
        FeedTestParam{"kernel_org_all_atom",      "https://www.kernel.org/feeds/all.atom.xml"},
        FeedTestParam{"theregister_atom",         "https://www.theregister.com/headlines.atom"},

        // ── RDF / RSS 1.0 ─────────────────────────────────────────────────
        FeedTestParam{"dw_rdf",                   "https://rss.dw.com/rdf/rss-en-all"}
    ),
    [](const ::testing::TestParamInfo<FeedTestParam> &info) {
        return info.param.description;
    }
);
// clang-format on
