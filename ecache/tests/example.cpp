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
#include <string>
#include <vector>
#include "ecache_log.h"
#include "ecache_manager.h"
#include "folly/String.h"

using namespace ecache;

struct CustomMapField {
  static constexpr int field_size = 16;
  int64_t score = 0;
  int64_t id = 0;
  folly::fbstring Encode() const {
    folly::fbstring s;
    field_encode_int(s, score);
    field_encode_int(s, id);
    return s;
  }
  template <typename T>
  void Decode(T& view) {
    field_decode_int(view, 0, score);
    field_decode_int(view, 8, id);
  }
};

static void print_stats(ECacheManager& m) {
  auto gstats = m.getGlobalCacheStats();
  auto pool_stats = m.getPoolStats(0);
  auto mstats = m.getCacheMemoryStats();

  ECACHE_INFO("[GlobalCacheStats]numCacheGets:{}, numCacheGetMiss:{}, ", gstats.numCacheGets,
              gstats.numCacheGetMiss);
}

static void save_db() {
  ECacheManager m;
  ECacheManagerConfig manager_config;
  manager_config.set_type(CACHE_LRU2Q);
  manager_config.set_size(2 * 1024 * 1024 * 1024LL);
  m.Init(manager_config);
  ECacheConfig config1;
  config1.set_name("test1");
  config1.set_size(512 * 1024 * 1024);
  auto cache1 = m.NewCache(config1);
  ECacheConfig config2;
  config2.set_name("test2");
  config2.set_size(512 * 1024 * 1024);
  auto cache2 = m.NewCache(config2);
  ECacheConfig config3;
  config3.set_name("test3");
  config3.set_size(512 * 1024 * 1024);
  auto cache3 = m.NewCache(config3);

  for (int i = 0; i < 10000; i++) {
    std::string key = "string" + std::to_string(i);
    std::string val = "value" + std::to_string(i);
    cache1->Set(key, val, 10000);
  }
  for (int i = 0; i < 100; i++) {
    std::string key = "hash" + std::to_string(i);
    CustomMapField f;
    f.score = 1000;
    f.id = 0;
    for (int j = 0; j < 1000; j++) {
      std::string val = "value" + std::to_string(i) + "_" + std::to_string(j);
      cache2->UnorderedMapSet(key, f, val, 10000);
      f.score++;
      f.id++;
    }
  }
  for (int i = 0; i < 100; i++) {
    std::string key = "range" + std::to_string(i);
    CustomMapField f;
    f.score = 1000;
    for (int j = 0; j < 1000; j++) {
      std::string val = "value" + std::to_string(i) + "_" + std::to_string(j);
      cache2->OrderedMapSet(key, f, val, 10000);
      f.score++;
      f.id++;
    }
  }
  int rc = m.Save("./ecache.save");
  ECACHE_INFO("Save return {}", rc);
}

static void load_db() {
  ECacheManager m;
  int rc = m.Load("./ecache.save");
  ECACHE_INFO("Load return {}", rc);
  auto cache1 = m.GetCache("test1");
  auto cache2 = m.GetCache("test2");
  auto cache3 = m.GetCache("test3");

  for (int i = 0; i < 10000; i++) {
    std::string key = "string" + std::to_string(i);
    auto val = cache1->Get(key);
    ECACHE_INFO("key:{}, val:{}", key, val.value_view);
  }

  for (int i = 0; i < 100; i++) {
    std::string key = "hash" + std::to_string(i);
    CustomMapField f;
    f.score = 1000;
    f.id = 0;
    for (int j = 0; j < 1000; j++) {
      std::string val = "value" + std::to_string(i) + "_" + std::to_string(j);
      auto r = cache2->UnorderedMapGet(key, f);
      ECACHE_INFO("key:{},field:{}, val:{}", key, f.id, r.value_view);
      f.id++;
      f.score++;
    }
  }
  for (int i = 0; i < 100; i++) {
    std::string key = "range" + std::to_string(i);
    CustomMapField f;
    f.score = 1000;
    f.id = 0;
    for (int j = 0; j < 1000; j++) {
      std::string val = "value" + std::to_string(i) + "_" + std::to_string(j);
      auto r = cache2->OrderedMapGet(key, f);
      ECACHE_INFO("key:{},field:{}, val:{}", key, f.id, r.value_view);
      f.id++;
      f.score++;
    }
  }
}

int main() {
  save_db();
  load_db();
  return 0;
}