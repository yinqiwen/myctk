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

#pragma once
#include <folly/String.h>
#include <exception>
#include <memory>
#include <string>
#include <string_view>

#include "cachelib/allocator/CacheAllocator.h"
#include "cachelib/allocator/memory/Slab.h"
#include "cachelib/datatype/Map.h"
#include "cachelib/datatype/RangeMap.h"
#include "folly/Format.h"

#include "ecache.h"
#include "ecache_common.h"
#include "ecache_log.h"

namespace ecache {
template <std::size_t N>
struct MapFieldImpl {
  char part[N];
  MapFieldImpl() {}
  bool operator!=(const MapFieldImpl& other) const {
    int rc = memcmp(part, other.part, N);
    return 0 != rc;
  }
  bool operator==(const MapFieldImpl& other) const {
    int rc = memcmp(part, other.part, N);
    return 0 == rc;
  }
  bool operator<(const MapFieldImpl& other) const {
    int rc = memcmp(part, other.part, N);
    return rc < 0;
  }
};
}  // namespace ecache

namespace folly {
template <std::size_t N>
class FormatValue<ecache::MapFieldImpl<N>> {
 public:
  explicit FormatValue(const ecache::MapFieldImpl<N>& val) : val_(val) {}

  template <class FormatCallback>
  void format(FormatArg& arg, FormatCallback& cb) const {
    std::string_view s(val_.part, N);
    FormatValue<std::string_view>(s).format(arg, cb);
  }

 private:
  const ecache::MapFieldImpl<N>& val_;
};
}  // namespace folly

namespace ecache {

#define DO_MAP_CASE_OP(FUNC, N, ...) \
  case N: {                          \
    return FUNC<N>(__VA_ARGS__);     \
  }

#define DO_MAP_OP(FUNC, SIZE, ...)                        \
  switch (SIZE) {                                         \
    DO_MAP_CASE_OP(FUNC, 8, __VA_ARGS__)                  \
    DO_MAP_CASE_OP(FUNC, 16, __VA_ARGS__)                 \
    DO_MAP_CASE_OP(FUNC, 24, __VA_ARGS__)                 \
    DO_MAP_CASE_OP(FUNC, 32, __VA_ARGS__)                 \
    DO_MAP_CASE_OP(FUNC, 40, __VA_ARGS__)                 \
    DO_MAP_CASE_OP(FUNC, 48, __VA_ARGS__)                 \
    DO_MAP_CASE_OP(FUNC, 56, __VA_ARGS__)                 \
    DO_MAP_CASE_OP(FUNC, 64, __VA_ARGS__)                 \
    DO_MAP_CASE_OP(FUNC, 72, __VA_ARGS__)                 \
    DO_MAP_CASE_OP(FUNC, 80, __VA_ARGS__)                 \
    DO_MAP_CASE_OP(FUNC, 88, __VA_ARGS__)                 \
    DO_MAP_CASE_OP(FUNC, 96, __VA_ARGS__)                 \
    DO_MAP_CASE_OP(FUNC, 104, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 112, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 120, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 128, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 136, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 144, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 152, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 160, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 168, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 176, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 184, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 192, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 200, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 208, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 216, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 224, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 232, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 240, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 248, __VA_ARGS__)                \
    DO_MAP_CASE_OP(FUNC, 256, __VA_ARGS__)                \
    default: {                                            \
      throw std::invalid_argument("Invalid field size."); \
    }                                                     \
  }

class ECacheManager;
template <typename Cache>
class ECacheImpl : public ECache {
 private:
  Cache* cache_;
  ECacheConfig config_;
  facebook::cachelib::PoolId pool_;

  Cache* GetCache() { return cache_; }
  CacheValue toCacheValue(typename Cache::ItemHandle& handle) {
    CacheValue val;
    if (handle) {
      val.value_view = folly::StringPiece((const char*)handle->getMemory(), handle->getSize());
    } else {
      // ECACHE_INFO("empty handle");
    }
    std::shared_ptr<typename Cache::ItemHandle> handle_ptr(
        new typename Cache::ItemHandle(std::move(handle)));
    val._handle = handle_ptr;

    return val;
  }
  template <std::size_t N>
  InsertOrReplaceResult DoHashMapSet(std::string_view key, std::string_view field,
                                     std::string_view val, int expire_secs) {
    using HashMap = facebook::cachelib::Map<MapFieldImpl<N>, MapValue, Cache>;
    MapFieldImpl<N> field_key;
    memcpy(field_key.part, field.data(), N);
    auto field_value = MapValue::fromString(val);
    auto item_handle = GetCache()->find(key);
    InsertOrReplaceResult result;
    HashMap map;
    if (!item_handle) {
      map = HashMap::create(*(GetCache()), pool_, key);
      result = (InsertOrReplaceResult)map.insertOrReplace(field_key, *field_value);
      auto map_handle = std::move(map).resetToItemHandle();
      GetCache()->insertOrReplace(map_handle);
      if (expire_secs > 0) {
        uint32_t expiry_time = gettimeofday_s() + expire_secs;
        if (!map_handle->updateExpiryTime(expiry_time)) {
          ECACHE_ERROR("Failed to set expiry time:{}, now:{}", expiry_time, gettimeofday_s());
        }
      }
    } else {
      if (expire_secs > 0) {
        uint32_t expiry_time = gettimeofday_s() + expire_secs;
        if (!item_handle->updateExpiryTime(expiry_time)) {
          ECACHE_ERROR("Failed to set expiry time:{}, now:{}", expiry_time, gettimeofday_s());
        }
      }
      map = HashMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      result = (InsertOrReplaceResult)map.insertOrReplace(field_key, *field_value);
    }
    return result;
  }
  template <std::size_t N>
  CacheValue DoHashMapGet(std::string_view key, std::string_view field) {
    CacheValue result;
    using HashMap = facebook::cachelib::Map<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      // ECACHE_ERROR("No map found:{}", key);
      return result;
    } else {
      MapFieldImpl<N> field_key;
      memcpy(field_key.part, field.data(), N);
      auto map = HashMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      auto entry_val = map.find(field_key);
      if (nullptr == entry_val) {
        // ECACHE_ERROR("No map field found");
        return result;
      }
      result.value_view = folly::StringPiece((const char*)entry_val->data(), entry_val->length);
      std::shared_ptr<typename Cache::ItemHandle> handle_ptr(
          new typename Cache::ItemHandle(std::move(map).resetToItemHandle()));
      result._handle = handle_ptr;
      return result;
    }
  }
  template <std::size_t N>
  int DoHashMapCompact(std::string_view key) {
    using HashMap = facebook::cachelib::Map<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      return -1;
    } else {
      auto map = HashMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      map.compact();
      return 0;
    }
  }
  template <std::size_t N>
  int DoHashMapSizeInBytes(std::string_view key) {
    using HashMap = facebook::cachelib::Map<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      return -1;
    } else {
      auto map = HashMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      return map.sizeInBytes();
    }
  }
  template <std::size_t N>
  int DoHashMapSize(std::string_view key) {
    using HashMap = facebook::cachelib::Map<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      return -1;
    } else {
      auto map = HashMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      return map.size();
    }
  }
  template <std::size_t N>
  bool DoHashMapDel(std::string_view key, std::string_view field) {
    using HashMap = facebook::cachelib::Map<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      return false;
    } else {
      auto map = HashMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      MapFieldImpl<N> field_key;
      memcpy(field_key.part, field.data(), N);
      return map.erase(field_key);
    }
  }
  template <std::size_t N>
  int DoHashMapGetAll(std::string_view key, size_t field_size, std::vector<CacheValue>& vals) {
    using HashMap = facebook::cachelib::Map<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      return -1;
    } else {
      auto map = HashMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      auto iter = map.begin();
      while (iter != map.end()) {
        CacheValue c;
        c.field_view = folly::StringPiece(iter->key.part, N);
        c.value_view = folly::StringPiece((const char*)iter->value.data(), iter->value.length);
        vals.push_back(std::move(c));
        iter++;
      }
      if (vals.size() > 0) {
        std::shared_ptr<typename Cache::ItemHandle> handle_ptr(
            new typename Cache::ItemHandle(std::move(map).resetToItemHandle()));
        vals[0]._handle = handle_ptr;
      }
      return 0;
    }
  }

  template <std::size_t N>
  int DoRangeMapGetAll(std::string_view key, size_t field_size, std::vector<CacheValue>& vals) {
    using RangeMap = facebook::cachelib::RangeMap<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      return -1;
    } else {
      auto map = RangeMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      auto iter = map.begin();
      while (iter != map.end()) {
        CacheValue c;
        c.field_view = folly::StringPiece(iter->key.part, N);
        c.value_view = folly::StringPiece((const char*)iter->value.data(), iter->value.length);
        vals.push_back(std::move(c));
        iter++;
      }
      if (vals.size() > 0) {
        std::shared_ptr<typename Cache::ItemHandle> handle_ptr(
            new typename Cache::ItemHandle(std::move(map).resetToItemHandle()));
        vals[0]._handle = handle_ptr;
      }
      return 0;
    }
  }
  template <std::size_t N>
  InsertOrReplaceResult DoRangeMapSet(std::string_view key, std::string_view field,
                                      std::string_view val, int expire_secs) {
    using RangeMap = facebook::cachelib::RangeMap<MapFieldImpl<N>, MapValue, Cache>;
    MapFieldImpl<N> field_key;
    memcpy(field_key.part, field.data(), N);
    auto field_value = MapValue::fromString(val);
    auto item_handle = GetCache()->find(key);
    InsertOrReplaceResult result;
    RangeMap map;

    if (!item_handle) {
      map = RangeMap::create(*(GetCache()), pool_, key);
      result = (InsertOrReplaceResult)map.insertOrReplace(field_key, *field_value);
      auto map_handle = std::move(map).resetToItemHandle();
      GetCache()->insertOrReplace(map_handle);
      if (expire_secs > 0) {
        uint32_t expiry_time = gettimeofday_s() + expire_secs;
        if (!map_handle->updateExpiryTime(expiry_time)) {
          ECACHE_ERROR("Failed to set expiry time:{}, now:{}", expiry_time, gettimeofday_s());
        }
      }
    } else {
      if (expire_secs > 0) {
        uint32_t expiry_time = gettimeofday_s() + expire_secs;
        if (!item_handle->updateExpiryTime(expiry_time)) {
          ECACHE_ERROR("Failed to set expiry time:{}, now:{}", expiry_time, gettimeofday_s());
        }
      }
      map = RangeMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      result = (InsertOrReplaceResult)map.insertOrReplace(field_key, *field_value);
    }

    return result;
  }
  template <std::size_t N>
  CacheValue DoRangeMapGet(std::string_view key, std::string_view field) {
    CacheValue result;
    using RangeMap = facebook::cachelib::RangeMap<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      ECACHE_ERROR("No map found:{}", key);
      return result;
    } else {
      MapFieldImpl<N> field_key;
      memcpy(field_key.part, field.data(), N);
      auto map = RangeMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      auto iter = map.lookup(field_key);
      if (iter == map.end()) {
        ECACHE_ERROR("No map field found");
        return result;
      }
      result.value_view = folly::StringPiece((const char*)iter->value.data(), iter->value.length);
      std::shared_ptr<typename Cache::ItemHandle> handle_ptr(
          new typename Cache::ItemHandle(std::move(map).resetToItemHandle()));
      result._handle = handle_ptr;
      return result;
    }
  }
  template <std::size_t N>
  int DoRangeMapRangeGet(std::string_view key, std::string_view min, std::string_view max,
                         std::vector<CacheValue>& vals) {
    using RangeMap = facebook::cachelib::RangeMap<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      // ECACHE_ERROR("No map found:{}", key);
      return -1;
    } else {
      MapFieldImpl<N> field_min_key, field_max_key;
      memcpy(field_min_key.part, min.data(), N);
      memcpy(field_max_key.part, max.data(), N);
      auto map = RangeMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      auto range = map.rangeLookupApproximate(field_min_key, field_max_key);
      for (auto& kv : range) {
        CacheValue c;
        c.field_view = folly::StringPiece(kv.key.part, N);
        c.value_view = folly::StringPiece((const char*)kv.value.data(), kv.value.length);
        vals.emplace_back(std::move(c));
      }
      if (vals.size() > 0) {
        std::shared_ptr<typename Cache::ItemHandle> handle_ptr(
            new typename Cache::ItemHandle(std::move(map).resetToItemHandle()));
        vals[0]._handle = handle_ptr;
      }
      return 0;
    }
  }
  template <std::size_t N>
  CacheValue DoRangeMapMin(std::string_view key) {
    CacheValue result;
    using RangeMap = facebook::cachelib::RangeMap<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      ECACHE_ERROR("No map found:{}", key);
      return result;
    } else {
      auto map = RangeMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      auto iter = map.begin();
      if (iter == map.end()) {
        return result;
      }
      result.field_view = folly::StringPiece(iter->key.part, N);
      result.value_view = folly::StringPiece((const char*)iter->value.data(), iter->value.length);
      std::shared_ptr<typename Cache::ItemHandle> handle_ptr(
          new typename Cache::ItemHandle(std::move(map).resetToItemHandle()));
      result._handle = handle_ptr;
      return result;
    }
  }
  template <std::size_t N>
  bool DoRangeMapDel(std::string_view key, std::string_view field) {
    using RangeMap = facebook::cachelib::RangeMap<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      return false;
    } else {
      auto map = RangeMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      MapFieldImpl<N> field_key;
      memcpy(field_key.part, field.data(), N);
      return map.remove(field_key);
    }
  }

  template <std::size_t N>
  bool DoRangeMapPop(std::string_view key) {
    using RangeMap = facebook::cachelib::RangeMap<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      return false;
    } else {
      auto map = RangeMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      auto iter = map.begin();
      return map.remove(iter->key);
    }
  }
  template <std::size_t N>
  int DoRangeMapSize(std::string_view key) {
    using RangeMap = facebook::cachelib::RangeMap<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      return -1;
    } else {
      auto map = RangeMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      return map.size();
    }
  }
  template <std::size_t N>
  int DoRangeMapSizeInBytes(std::string_view key) {
    using RangeMap = facebook::cachelib::RangeMap<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      return -1;
    } else {
      auto map = RangeMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      return map.sizeInBytes();
    }
  }
  template <std::size_t N>
  int DoRangeMapRemainingBytes(std::string_view key) {
    using RangeMap = facebook::cachelib::RangeMap<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      return -1;
    } else {
      auto map = RangeMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      return map.remainingBytes();
    }
  }
  template <std::size_t N>
  int DoRangeMapWastedBytes(std::string_view key) {
    using RangeMap = facebook::cachelib::RangeMap<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      return -1;
    } else {
      auto map = RangeMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      return map.wastedBytes();
    }
  }
  template <std::size_t N>
  int DoRangeMapCapacity(std::string_view key) {
    using RangeMap = facebook::cachelib::RangeMap<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      return -1;
    } else {
      auto map = RangeMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      return map.capacity();
    }
  }
  template <std::size_t N>
  int DoRangeMapCompact(std::string_view key) {
    using RangeMap = facebook::cachelib::RangeMap<MapFieldImpl<N>, MapValue, Cache>;
    auto item_handle = GetCache()->find(key);
    if (!item_handle) {
      return -1;
    } else {
      auto map = RangeMap::fromItemHandle(*(GetCache()), std::move(item_handle));
      map.compact();
      return 0;
    }
  }
  template <std::size_t N>
  int DoLoadHashMap(FILE* fp, std::string_view key, uint32_t expiry_time) {
    using HashMap = facebook::cachelib::Map<MapFieldImpl<N>, MapValue, Cache>;
    auto map = HashMap::create(*(GetCache()), pool_, key);
    uint32_t map_len = 0;
    if (0 != file_read_uint32(fp, map_len)) {
      return -1;
    }
    for (uint32_t i = 0; i < map_len; i++) {
      MapFieldImpl<N> field_key;
      int rc = fread(field_key.part, N, 1, fp);
      if (rc != 1) {
        return -1;
      }
      std::string field_val_str;
      if (0 != file_read_string(fp, field_val_str)) {
        return -1;
      }
      auto field_value = MapValue::fromString(field_val_str);
      map.insert(field_key, *field_value);
    }

    auto map_handle = std::move(map).resetToItemHandle();
    GetCache()->insertOrReplace(map_handle);
    if (expiry_time > 0) {
      if (!map_handle->updateExpiryTime(expiry_time)) {
        ECACHE_ERROR("Failed to update expiry time:{}, now:{}", expiry_time, gettimeofday_s());
      }
    }
    return 0;
  }
  template <std::size_t N>
  int DoSaveHashMap(FILE* fp, typename Cache::ItemHandle& handle) {
    using HashMap = facebook::cachelib::Map<MapFieldImpl<N>, MapValue, Cache>;
    auto map = HashMap::fromItemHandle(*(GetCache()), std::move(handle));
    uint32_t n = map.size();
    if (0 != file_write_uint32(fp, n)) {
      return -1;
    }
    auto iter = map.begin();
    while (iter != map.end()) {
      int rc = fwrite(iter->key.part, N, 1, fp);
      if (rc != 1) {
        return -1;
      }
      std::string_view value_view((const char*)iter->value.data(), iter->value.length);
      if (0 != file_write_string(fp, value_view)) {
        return -1;
      }
      iter++;
    }
    return 0;
  }
  template <std::size_t N>
  int DoLoadRangeMap(FILE* fp, std::string_view key, uint32_t expiry_time) {
    using RangeMap = facebook::cachelib::RangeMap<MapFieldImpl<N>, MapValue, Cache>;
    auto map = RangeMap::create(*(GetCache()), pool_, key);
    uint32_t map_len = 0;
    if (0 != file_read_uint32(fp, map_len)) {
      return -1;
    }
    for (uint32_t i = 0; i < map_len; i++) {
      MapFieldImpl<N> field_key;
      int rc = fread(field_key.part, N, 1, fp);
      if (rc != 1) {
        return -1;
      }
      std::string field_val_str;
      if (0 != file_read_string(fp, field_val_str)) {
        return -1;
      }
      auto field_value = MapValue::fromString(field_val_str);
      map.insert(field_key, *field_value);
    }

    auto map_handle = std::move(map).resetToItemHandle();
    GetCache()->insertOrReplace(map_handle);
    if (expiry_time > 0) {
      if (!map_handle->updateExpiryTime(expiry_time)) {
        ECACHE_ERROR("Failed to update expiry time:{}, now:{}", expiry_time, gettimeofday_s());
      }
    }
    return 0;
  }
  template <std::size_t N>
  int DoSaveRangeMap(FILE* fp, typename Cache::ItemHandle& handle) {
    using RangeMap = facebook::cachelib::RangeMap<MapFieldImpl<N>, MapValue, Cache>;
    auto map = RangeMap::fromItemHandle(*(GetCache()), std::move(handle));
    uint32_t n = map.size();
    if (0 != file_write_uint32(fp, n)) {
      return -1;
    }
    auto iter = map.begin();
    while (iter != map.end()) {
      int rc = fwrite(iter->key.part, N, 1, fp);
      if (rc != 1) {
        return -1;
      }
      std::string_view value_view((const char*)iter->value.data(), iter->value.length);
      if (0 != file_write_string(fp, value_view)) {
        return -1;
      }
      iter++;
    }
    return 0;
  }

  friend class ECacheManager;

 public:
  ECacheImpl(void* cache, uint8_t pool = 0) : cache_(nullptr) {
    cache_ = (Cache*)cache;
    pool_ = pool;
  }
  uint8_t GetPoolId() override { return pool_; }
  int Init(const ECacheConfig& config) override {
    try {
      pool_ = GetCache()->addPool(config.name(), config.size());
      config_ = config;
      ECACHE_INFO("Init cache:{} with pool id:{}", config.name(), pool_);
      return 0;
    } catch (std::exception& e) {
      ECACHE_ERROR("Failed to init cache with config:{}, got exception:{}", config.DebugString(),
                   e.what());
      return -1;
    }
  }
  int SaveRangeMap(FILE* fp, typename Cache::ItemHandle& handle, size_t field_size) {
    DO_MAP_OP(DoSaveRangeMap, field_size, fp, handle);
  }
  int LoadRangeMap(FILE* fp, std::string_view key, size_t field_size, uint32_t expiry_time) {
    DO_MAP_OP(DoLoadRangeMap, field_size, fp, key, expiry_time);
  }
  int SaveHashMap(FILE* fp, typename Cache::ItemHandle& handle, size_t field_size) {
    DO_MAP_OP(DoSaveHashMap, field_size, fp, handle);
  }
  int LoadHashMap(FILE* fp, std::string_view key, size_t field_size, uint32_t expiry_time) {
    DO_MAP_OP(DoLoadHashMap, field_size, fp, key, expiry_time);
  }
  CacheValue Set(std::string_view key, std::string_view value, int expire_secs) override {
    uint32_t ttlSecs = 0;  // expires in 10 mins
    if (expire_secs > 0) {
      ttlSecs = expire_secs;
    }
    auto item = GetCache()->allocate(pool_, getActualKey(pool_, CACHE_KEY_STRING, key),
                                     value.size(), ttlSecs);
    std::memcpy(item->getMemory(), value.data(), value.size());
    auto item_handle = GetCache()->insertOrReplace(item);
    // ECACHE_INFO("Set here:{}/{} {}", key, value, ttlSecs);
    return toCacheValue(item_handle);
  }

  CacheValue Get(std::string_view key) override {
    auto item_handle = GetCache()->find(getActualKey(pool_, CACHE_KEY_STRING, key));
    // ECACHE_INFO("Get here:{}", key);
    return toCacheValue(item_handle);
  }
  RemoveRes Del(std::string_view key, CacheKeyType type, size_t field_size) override {
    return (RemoveRes)GetCache()->remove(getActualKey(pool_, type, key, field_size));
  }
  bool Exists(std::string_view key, CacheKeyType type, size_t field_size) override {
    auto item_handle = GetCache()->find(getActualKey(pool_, type, key, field_size));
    if (item_handle) {
      return true;
    }
    return false;
  }
  bool Expire(std::string_view key, uint32_t secs, CacheKeyType type, size_t field_size) {
    auto item_handle = GetCache()->find(getActualKey(pool_, type, key, field_size));
    if (!item_handle) {
      return false;
    }
    return item_handle->extendTTL(std::chrono::seconds(secs));
  }
  bool ExpireAt(std::string_view key, uint32_t secs, CacheKeyType type, size_t field_size) {
    auto item_handle = GetCache()->find(getActualKey(pool_, type, key, field_size));
    if (!item_handle) {
      return false;
    }
    return item_handle->updateExpiryTime(secs);
  }

  // int ListAdd(std::string_view key, std::string_view value) override {
  //   auto lkey = getActualKey(pool_, CACHE_KEY_LIST, key);
  //   auto item_handle = GetCache()->find(lkey);
  //   bool create_list = false;
  //   if (!item_handle) {
  //     item_handle = GetCache()->allocate(pool_, lkey, 0);
  //     create_list = true;
  //   }
  //   auto child = GetCache()->allocateChainedItem(item_handle, value.size());
  //   std::memcpy(child->getMemory(), value.data(), value.size());
  //   GetCache()->addChainedItem(item_handle, std::move(child));
  //   if (create_list) {
  //     GetCache()->insert(item_handle);
  //   }
  //   return 0;
  // }
  // int ListBatchGet(std::string_view key, int count, std::vector<CacheValue>& vals) {
  //   vals.clear();
  //   auto item_handle = GetCache()->find(getActualKey(pool_, CACHE_KEY_LIST, key));
  //   if (!item_handle) {
  //     return 0;
  //   }
  //   auto chainedAllocs = GetCache()->viewAsChainedAllocs(item_handle);
  //   for (const auto& item : chainedAllocs.getChain()) {
  //     if (count >= 0 && vals.size() >= (size_t)count) {
  //       break;
  //     }
  //     CacheValue c;
  //     c.value = item.getMemory();
  //     c.value_size = item.getSize();
  //     c.value_view = folly::StringPiece((const char*)c.value, c.value_size);
  //     vals.push_back(std::move(c));
  //   }
  //   if (vals.size() > 0) {
  //     std::shared_ptr<typename Cache::ChainedAllocs> handle_ptr(
  //         new typename Cache::ChainedAllocs(std::move(chainedAllocs)));
  //     vals[0]._handle = handle_ptr;
  //   }
  //   return 0;
  // }
  // CacheValue ListGet(std::string_view key, size_t index) override {
  //   CacheValue result;
  //   auto item_handle = GetCache()->find(getActualKey(pool_, CACHE_KEY_LIST, key));
  //   if (!item_handle) {
  //     return result;
  //   }
  //   auto chainedAllocs = GetCache()->viewAsChainedAllocs(item_handle);
  //   auto item = chainedAllocs.getNthInChain(index);
  //   if (nullptr == item) {
  //     return result;
  //   }
  //   result.value = item->getMemory();
  //   result.value_size = item->getSize();
  //   result.value_view = folly::StringPiece((const char*)result.value, result.value_size);
  //   std::shared_ptr<typename Cache::ChainedAllocs> handle_ptr(
  //       new typename Cache::ChainedAllocs(std::move(chainedAllocs)));
  //   result._handle = handle_ptr;
  //   return result;
  // }
  // CacheValue ListUpdate(std::string_view key, size_t index, std::string_view value) override {
  //   CacheValue old;
  //   auto item_handle = GetCache()->find(getActualKey(pool_, CACHE_KEY_LIST, key));
  //   if (!item_handle) {
  //     return old;
  //   }
  //   auto item = GetCache()->viewAsChainedAllocs(item_handle).getNthInChain(index);
  //   if (nullptr == item) {
  //     return old;
  //   }
  //   auto child = GetCache()->allocateChainedItem(item_handle, value.size());
  //   std::memcpy(child->getMemory(), value.data(), value.size());
  //   auto old_item_handler = GetCache()->replaceChainedItem(*item, std::move(child),
  //   *item_handle); return toCacheValue(old_item_handler);
  // }
  // int ListPop(std::string_view key, size_t count) override {
  //   auto item_handle = GetCache()->find(getActualKey(pool_, CACHE_KEY_LIST, key));
  //   if (!item_handle) {
  //     return -1;
  //   }
  //   int total = 0;
  //   while ((size_t)total < count) {
  //     auto element = GetCache()->popChainedItem(item_handle);
  //     if (!element) {
  //       break;
  //     }
  //     total++;
  //   }
  //   return total;
  // }

  // int ListSize(std::string_view key) override {
  //   auto item_handle = GetCache()->find(getActualKey(pool_, CACHE_KEY_LIST, key));
  //   if (!item_handle) {
  //     return -1;
  //   }
  //   auto chainedAllocs = GetCache()->viewAsChainedAllocs(item_handle);
  //   return chainedAllocs.computeChainLength();
  // }

  CacheValue RangeMapGet(std::string_view key, std::string_view field) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_RANGE_MAP, key, field.size());
    DO_MAP_OP(DoRangeMapGet, field.size(), mkey, field);
  }
  int RangeMapRangeGet(std::string_view key, std::string_view min, std::string_view max,
                       std::vector<CacheValue>& vals) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_RANGE_MAP, key, min.size());
    DO_MAP_OP(DoRangeMapRangeGet, min.size(), mkey, min, max, vals);
  }
  CacheValue RangeMapMin(std::string_view key, size_t field_size) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_RANGE_MAP, key, field_size);
    DO_MAP_OP(DoRangeMapMin, field_size, mkey);
  }
  InsertOrReplaceResult RangeMapSet(std::string_view key, std::string_view field,
                                    std::string_view val, int expire_secs) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_RANGE_MAP, key, field.size());
    DO_MAP_OP(DoRangeMapSet, field.size(), mkey, field, val, expire_secs);
  }
  bool RangeMapPop(std::string_view key, size_t field_size) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_RANGE_MAP, key, field_size);
    DO_MAP_OP(DoRangeMapPop, field_size, mkey);
  }
  bool RangeMapDel(std::string_view key, std::string_view field) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_RANGE_MAP, key, field.size());
    DO_MAP_OP(DoRangeMapDel, field.size(), mkey, field);
  }
  int RangeMapSize(std::string_view key, size_t field_size) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_RANGE_MAP, key, field_size);
    DO_MAP_OP(DoRangeMapSize, field_size, mkey);
  }
  int RangeMapCapacity(std::string_view key, size_t field_size) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_RANGE_MAP, key, field_size);
    DO_MAP_OP(DoRangeMapCapacity, field_size, mkey);
  }
  int RangeMapSizeInBytes(std::string_view key, size_t field_size) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_RANGE_MAP, key, field_size);
    DO_MAP_OP(DoRangeMapSizeInBytes, field_size, mkey);
  }
  int RangeMapRemainingBytes(std::string_view key, size_t field_size) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_RANGE_MAP, key, field_size);
    DO_MAP_OP(DoRangeMapRemainingBytes, field_size, mkey);
  }
  int RangeMapWastedBytes(std::string_view key, size_t field_size) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_RANGE_MAP, key, field_size);
    DO_MAP_OP(DoRangeMapWastedBytes, field_size, mkey);
  }
  int RangeMapCompact(std::string_view key, size_t field_size) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_RANGE_MAP, key, field_size);
    DO_MAP_OP(DoRangeMapCompact, field_size, mkey);
  }
  int RangeMapGetAll(std::string_view key, size_t field_size,
                     std::vector<CacheValue>& vals) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_RANGE_MAP, key, field_size);
    DO_MAP_OP(DoRangeMapGetAll, field_size, mkey, field_size, vals);
  }

  int HashMapCompact(std::string_view key, size_t field_size) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_HASH_MAP, key, field_size);
    DO_MAP_OP(DoHashMapCompact, field_size, mkey);
  }
  int HashMapSizeInBytes(std::string_view key, size_t field_size) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_HASH_MAP, key, field_size);
    DO_MAP_OP(DoHashMapSizeInBytes, field_size, mkey);
  }
  InsertOrReplaceResult HashMapSet(std::string_view key, std::string_view field,
                                   std::string_view val, int expire_secs) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_HASH_MAP, key, field.size());
    DO_MAP_OP(DoHashMapSet, field.size(), mkey, field, val, expire_secs);
  }
  CacheValue HashMapGet(std::string_view key, std::string_view field) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_HASH_MAP, key, field.size());
    DO_MAP_OP(DoHashMapGet, field.size(), mkey, field);
  }
  int HashMapSize(std::string_view key, size_t field_size) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_HASH_MAP, key, field_size);
    DO_MAP_OP(DoHashMapSize, field_size, mkey);
  }
  bool HashMapDel(std::string_view key, std::string_view field) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_HASH_MAP, key, field.size());
    DO_MAP_OP(DoHashMapDel, field.size(), mkey, field);
  }
  int HashMapGetAll(std::string_view key, size_t field_size,
                    std::vector<CacheValue>& vals) override {
    auto mkey = getActualKey(pool_, CACHE_KEY_HASH_MAP, key, field_size);
    DO_MAP_OP(DoHashMapGetAll, field_size, mkey, field_size, vals);
  }
};
}  // namespace ecache