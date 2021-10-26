/*
 *Copyright (c) 2021, qiyingwang <qiyingwang@tencent.com>
 *All rights reserved.
 *
 *Redistribution and use in source and binary forms, with or without
 *modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of rimos nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 *THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <gtest/gtest.h>
#include <vector>
#include "ecache_log.h"
#include "ecache_manager.h"
#include "folly/String.h"

using namespace ecache;

class ECacheTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ECacheManagerConfig manager_config;
    manager_config.set_type(CACHE_LRU2Q);
    manager_config.set_size(1 * 1024 * 1024 * 1024);
    cache_manager.Init(manager_config);
    ECacheConfig config;
    config.set_name("test");
    config.set_size(1024 * 1024 * 1024);
    cache = cache_manager.NewCache(config);
  }
  virtual void TearDown() {}

  std::unique_ptr<ECache> cache;
  ECacheManager cache_manager;
};

// TEST_F(ECacheTest, list) {
//   cache->ListAdd("list", "value0");
//   cache->ListAdd("list", "value1");
//   cache->ListAdd("list", "value2");
//   cache->ListAdd("list", "value3");
//   EXPECT_EQ(4, cache->ListSize("list"));

//   std::vector<CacheValue> vals;
//   cache->ListBatchGet("list", 2, vals);
//   EXPECT_EQ(2, vals.size());
//   EXPECT_EQ("value3", vals[0].value_view);
//   EXPECT_EQ("value2", vals[1].value_view);

//   cache->ListGetAll("list", vals);
//   EXPECT_EQ(4, vals.size());
//   EXPECT_EQ("value3", vals[0].value_view);
//   EXPECT_EQ("value2", vals[1].value_view);
//   EXPECT_EQ("value1", vals[2].value_view);
//   EXPECT_EQ("value0", vals[3].value_view);

//   vals.clear();
//   EXPECT_EQ(2, cache->ListPop("list", 2));
//   cache->ListGetAll("list", vals);
//   EXPECT_EQ(2, vals.size());
//   EXPECT_EQ("value1", vals[0].value_view);
//   EXPECT_EQ("value0", vals[1].value_view);
//   vals.clear();

//   auto r = cache->ListUpdate("list", 1, "new_value");
//   EXPECT_EQ("value0", r.value_view);
//   r = cache->ListGet("list", 1);
//   EXPECT_EQ("new_value", r.value_view);
// }

struct CustomMapField {
  static constexpr int field_size = 8;
  int64_t id = 0;
  folly::fbstring Encode() const {
    folly::fbstring s;
    field_encode_int(s, id);
    return s;
  }
  template <typename T>
  void Decode(T& view) {
    field_decode_int(view, 0, id);
  }
};

TEST_F(ECacheTest, unordered_map) {
  CustomMapField f;
  f.id = 101;
  cache->UnorderedMapSet("hash", f, "value0");
  EXPECT_EQ(1, cache->UnorderedMapSize<CustomMapField>("hash"));
  auto r = cache->UnorderedMapGet("hash", f);
  EXPECT_EQ("value0", r.value_view);
  EXPECT_EQ(true, cache->UnorderedMapDel("hash", f));
  EXPECT_EQ(false, cache->UnorderedMapDel("hash", f));
  EXPECT_EQ(0, cache->UnorderedMapSize<CustomMapField>("hash"));

  f.id = 1000;
  for (int i = 0; i < 100; i++) {
    std::string val = "value" + std::to_string(i);
    cache->UnorderedMapSet("hash", f, val);
    f.id++;
  }
  std::vector<CacheValue> all;
  cache->UnorderedMapGetAll<CustomMapField>("hash", all);
  EXPECT_EQ(100, all.size());
  int expect_id = 1000;
  for (int i = 0; i < 100; i++) {
    EXPECT_EQ(expect_id, all[i].GetField<CustomMapField>().id);
    std::string val = "value" + std::to_string(i);
    EXPECT_EQ(val, all[i].value_view);
    expect_id++;
  }
  f.id = 1000;
  for (int i = 0; i < 20; i++) {
    cache->UnorderedMapDel("hash", f);
    f.id++;
  }
  ECACHE_INFO("before compact hashmap size:{},  size_in_bytes:{}",
              cache->UnorderedMapSize<CustomMapField>("hash"),
              cache->UnorderedMapSizeInBytes<CustomMapField>("hash"));
  EXPECT_EQ(0, cache->UnorderedMapCompact<CustomMapField>("hash"));
  ECACHE_INFO("after compact hashmap size:{},  size_in_bytes:{}",
              cache->UnorderedMapSize<CustomMapField>("hash"),
              cache->UnorderedMapSizeInBytes<CustomMapField>("hash"));
}

TEST_F(ECacheTest, ordered_map) {
  CustomMapField f;
  f.id = 100;
  for (int i = 0; i < 10; i++) {
    std::string val = "value" + std::to_string(i);
    cache->OrderedMapSet("range", f, val);
    f.id++;
  }
  EXPECT_EQ(10, cache->OrderedMapSize<CustomMapField>("range"));
  f.id = 100;
  auto r = cache->OrderedMapGet("range", f);
  EXPECT_EQ("value0", r.value_view);

  std::vector<CacheValue> range;
  CustomMapField min, max;
  min.id = 100;
  max.id = 105;
  EXPECT_EQ(0, cache->OrderedMapRangeGet<CustomMapField>("range", min, max, range));
  EXPECT_EQ(6, range.size());
  for (int i = 0; i < range.size(); i++) {
    std::string expect_val = "value" + std::to_string(i);
    EXPECT_EQ(expect_val, range[i].value_view);
    EXPECT_EQ(100 + i, range[i].GetField<CustomMapField>().id);
  }

  EXPECT_EQ(true, cache->OrderedMapPop<CustomMapField>("range"));
  r = cache->OrderedMapMin<CustomMapField>("range");
  EXPECT_EQ("value1", r.value_view);
  EXPECT_EQ(9, cache->OrderedMapSize<CustomMapField>("range"));
}

TEST_F(ECacheTest, expire) {
  cache->Set("expire_k1", "value0", 2);
  auto r = cache->Get("expire_k1");
  EXPECT_EQ("value0", r.value_view);
  sleep(3);
  EXPECT_EQ(false, cache->Exists("expire_k1"));

  CustomMapField f;
  f.id = 100;
  for (int i = 0; i < 10; i++) {
    std::string val = "value" + std::to_string(i);
    cache->OrderedMapSet("expire_range", f, val, 2);
    f.id++;
  }
  EXPECT_EQ(10, cache->OrderedMapSize<CustomMapField>("expire_range"));
  sleep(3);
  EXPECT_EQ(false, cache->ExistsOrderedMap<CustomMapField>("expire_range"));

  f.id = 100;
  for (int i = 0; i < 10; i++) {
    std::string val = "value" + std::to_string(i);
    cache->UnorderedMapSet("expire_hash", f, val, 2);
    f.id++;
  }
  EXPECT_EQ(10, cache->UnorderedMapSize<CustomMapField>("expire_hash"));
  sleep(3);
  EXPECT_EQ(false, cache->ExistsUnorderedMap<CustomMapField>("expire_hash"));
}