#include "../src/lib/Rss/RssManager.hpp"
#include <gtest/gtest.h>

using dotnamebot::rss::RssManager;

TEST(RssManagerTest, DecodeHtmlEntitiesDecodesDecimalNumericEntities) {
  EXPECT_EQ(RssManager::decodeHtmlEntities("zve\u0159ejnili v&#353;em"), "zve\u0159ejnili všem");
}

TEST(RssManagerTest, DecodeHtmlEntitiesDecodesNestedNumericEntities) {
  EXPECT_EQ(RssManager::decodeHtmlEntities("zve\u0159ejnili v&amp;#353;em"), "zve\u0159ejnili všem");
}

TEST(RssManagerTest, DecodeHtmlEntitiesDecodesHexNumericEntities) {
  EXPECT_EQ(RssManager::decodeHtmlEntities("zp\u0159\u00edstup\u0148uj\u00ed &#x161;ir\u0161\u00ed publikum"),
            "zp\u0159\u00edstup\u0148uj\u00ed širší publikum");
}