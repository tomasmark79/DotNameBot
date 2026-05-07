#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

#include "../src/lib/Utils/Logger/ConsoleLogger.hpp"
#include "MockAssetManager.hpp"

#define private public
#include "../src/lib/Rss/RssManager.hpp"
#undef private

using dotnamebot::rss::RssManager;

namespace {

  class RssManagerParsingTest : public ::testing::Test {
  protected:
    void SetUp() override {
      testDir_ = std::filesystem::temp_directory_path() / "dotnamebot-rss-manager-test";
      std::filesystem::create_directories(testDir_);

      std::ofstream(testDir_ / "rssUrls.json") << "[]";
      std::ofstream(testDir_ / "seenHashes.json") << "[]";

      logger_ = std::make_shared<ConsoleLogger>();
      assetManager_ = std::make_shared<MockAssetManager>(testDir_);
    }

    void TearDown() override { std::filesystem::remove_all(testDir_); }

    std::filesystem::path testDir_;
    std::shared_ptr<ConsoleLogger> logger_;
    std::shared_ptr<MockAssetManager> assetManager_;
  };

} // namespace

TEST(RssManagerTest, DecodeHtmlEntitiesDecodesDecimalNumericEntities) {
  EXPECT_EQ(RssManager::decodeHtmlEntities("zve\u0159ejnili v&#353;em"), "zve\u0159ejnili všem");
}

TEST(RssManagerTest, DecodeHtmlEntitiesDecodesNestedNumericEntities) {
  EXPECT_EQ(RssManager::decodeHtmlEntities("zve\u0159ejnili v&amp;#353;em"),
            "zve\u0159ejnili všem");
}

TEST(RssManagerTest, DecodeHtmlEntitiesDecodesHexNumericEntities) {
  EXPECT_EQ(RssManager::decodeHtmlEntities(
                "zp\u0159\u00edstup\u0148uj\u00ed &#x161;ir\u0161\u00ed publikum"),
            "zp\u0159\u00edstup\u0148uj\u00ed širší publikum");
}

TEST_F(RssManagerParsingTest, ParseRssDecodesRootZpravickyDescriptionEntities) {
  auto rssManager = RssManager(logger_, assetManager_);
  int totalDuplicateItems = 0;

  const std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0">
  <channel>
    <title>Root.cz - zprávičky</title>
    <link>https://www.root.cz/zpravicky/</link>
    <description>Root.cz - informace nejen ze světa Linuxu</description>
    <item>
      <title>Dell a Lenovo jsou novými sponzory Linux Vendor Firmware Service</title>
      <link>https://www.root.cz/zpravicky/dell-a-lenovo-jsou-novymi-sponzory-linux-vendor-firmware-service/</link>
      <description>Linux Vendor Firmware Service, LVFS, má dva nové velké sponzory. Stávají se jimi společnosti Dell a Lenovo, které jistě netřeba představovat blíže. Dell koketuje s Linuxem už po léta, Lenovo také, navíc víme, že Linux pro ně je a bude vět&#353;í prioritou, v neprospěch Windows. Takže vlastně nic &#353;okujícího: služba LVFS a klient Fwupd jsou zavedené projekty zaji&#353;ťující infrastrukturu pro instalace firmwarů, a to je přesně to, co Lenovo i Dell potřebují.</description>
    </item>
  </channel>
</rss>)";

  const auto feed = rssManager.parseRSS(xml, 2, 0, totalDuplicateItems);

  ASSERT_EQ(feed.items.size(), 1);
  EXPECT_EQ(feed.items[0].title,
            "Dell a Lenovo jsou novými sponzory Linux Vendor Firmware Service");
  EXPECT_EQ(feed.items[0].description,
            "Linux Vendor Firmware Service, LVFS, má dva nové velké sponzory. Stávají se jimi "
            "společnosti Dell a Lenovo, které jistě netřeba představovat blíže. Dell koketuje s "
            "Linuxem už po léta, Lenovo také, navíc víme, že Linux pro ně je a bude větší "
            "prioritou, v neprospěch Windows. Takže vlastně nic šokujícího: služba LVFS a klient "
            "Fwupd jsou zavedené projekty zajišťující infrastrukturu pro instalace firmwarů, a to "
            "je přesně to, co Lenovo i Dell potřebují.");
}