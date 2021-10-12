#include <gtest/gtest.h>
#include "robims_bsi.h"
#include "robims_common.h"

using namespace robims;
TEST(BSITest, PutGet) {
  BitSliceIntIndex index;
  FieldMeta opt;

  index.Init(opt);

  index.Put(0, 3);
  index.Put(1, 3);
  index.Put(2, 3);
  index.Put(3, 4);
  index.Put(5, 6);

  int64_t val;
  EXPECT_EQ(true, index.Get(0, val));
  EXPECT_EQ(3, val);
  EXPECT_EQ(true, index.Get(3, val));
  EXPECT_EQ(4, val);
  EXPECT_EQ(true, index.Get(5, val));
  EXPECT_EQ(6, val);

  int64_t min, max, count;
  index.GetMin(nullptr, min, count);
  EXPECT_EQ(3, min);
  EXPECT_EQ(3, count);
  index.GetMax(nullptr, max, count);
  EXPECT_EQ(6, max);
  EXPECT_EQ(1, count);

  index.RemoveMin();
  index.GetMin(nullptr, min, count);
  EXPECT_EQ(3, min);
  EXPECT_EQ(2, count);
}

TEST(BSITest, LT) {
  BitSliceIntIndex index;
  FieldMeta opt;
  index.Init(opt);

  for (int i = 0; i < 100; i++) {
    index.Put(i, i);
  }
  std::vector<uint32_t> ids;
  bimap_get_ids([&](roaring_bitmap_t* out) { index.RangeLT(10, false, out); }, ids);
  EXPECT_EQ(10, ids.size());
  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(i, ids[i]);
  }
}

TEST(BSITest, GT) {
  BitSliceIntIndex index;
  FieldMeta opt;
  index.Init(opt);

  for (int i = 0; i < 100; i++) {
    index.Put(i, i);
  }
  std::vector<uint32_t> ids;
  bimap_get_ids([&](roaring_bitmap_t* out) { index.RangeGT(90, true, out); }, ids);
  EXPECT_EQ(10, ids.size());
  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(i + 90, ids[i]);
  }
}

TEST(BSITest, Between) {
  BitSliceIntIndex index;
  FieldMeta opt;
  index.Init(opt);

  for (int i = 0; i < 100; i++) {
    index.Put(i, i);
  }
  std::vector<uint32_t> ids;
  bimap_get_ids([&](roaring_bitmap_t* out) { index.RangeBetween(50, 60, out); }, ids);
  EXPECT_EQ(11, ids.size());
  for (int i = 0; i < 11; i++) {
    EXPECT_EQ(i + 50, ids[i]);
  }
}

TEST(BSITest, TopK) {
  BitSliceIntIndex index;
  FieldMeta opt;
  index.Init(opt);

  for (int i = 0; i < 100; i++) {
    index.Put(i, i);
  }
  std::vector<uint32_t> ids;
  bimap_get_ids([&](roaring_bitmap_t* out) { index.TopK(10, out); }, ids);
  EXPECT_EQ(10, ids.size());
  // for (int i : ids) {
  //   printf("%d\n", i);
  // }
  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(i + 90, ids[i]);
  }
}