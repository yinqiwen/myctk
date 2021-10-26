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
#include <memory>
#include <string>
#include <string_view>
#include "ecache_types.h"
namespace ecache {

class ECache {
 protected:
  virtual InsertOrReplaceResult RangeMapSet(std::string_view key, std::string_view field,
                                            std::string_view value, int expire_secs) = 0;
  virtual CacheValue RangeMapGet(std::string_view key, std::string_view field) = 0;
  virtual CacheValue RangeMapMin(std::string_view key, size_t field_size) = 0;
  virtual bool RangeMapPop(std::string_view key, size_t field_size) = 0;
  virtual bool RangeMapDel(std::string_view key, std::string_view field) = 0;
  virtual int RangeMapSize(std::string_view key, size_t field_size) = 0;
  virtual int RangeMapCapacity(std::string_view key, size_t field_size) = 0;
  virtual int RangeMapSizeInBytes(std::string_view key, size_t field_size) = 0;
  virtual int RangeMapRemainingBytes(std::string_view key, size_t field_size) = 0;
  virtual int RangeMapWastedBytes(std::string_view key, size_t field_size) = 0;
  virtual int RangeMapCompact(std::string_view key, size_t field_size) = 0;
  virtual int RangeMapGetAll(std::string_view key, size_t field_size,
                             std::vector<CacheValue>& vals) = 0;
  virtual int RangeMapRangeGet(std::string_view key, std::string_view min, std::string_view max,
                               std::vector<CacheValue>& vals) = 0;

  virtual InsertOrReplaceResult HashMapSet(std::string_view key, std::string_view field,
                                           std::string_view value, int expire_secs) = 0;
  virtual CacheValue HashMapGet(std::string_view key, std::string_view field) = 0;
  virtual int HashMapSize(std::string_view key, size_t field_size) = 0;
  virtual bool HashMapDel(std::string_view key, std::string_view field) = 0;
  virtual int HashMapGetAll(std::string_view key, size_t field_size,
                            std::vector<CacheValue>& vals) = 0;
  virtual int HashMapCompact(std::string_view key, size_t field_size) = 0;
  virtual int HashMapSizeInBytes(std::string_view key, size_t field_size) = 0;

 public:
  virtual int Init(const ECacheConfig& config) = 0;
  virtual uint8_t GetPoolId() = 0;
  virtual CacheValue Set(std::string_view key, std::string_view value, int expire_secs = -1) = 0;
  virtual CacheValue Get(std::string_view key) = 0;
  virtual RemoveRes Del(std::string_view key, CacheKeyType type = CACHE_KEY_STRING,
                        size_t field_size = 0) = 0;
  virtual bool Expire(std::string_view key, uint32_t secs, CacheKeyType type = CACHE_KEY_STRING,
                      size_t field_size = 0) = 0;
  virtual bool ExpireAt(std::string_view key, uint32_t secs, CacheKeyType type = CACHE_KEY_STRING,
                        size_t field_size = 0) = 0;
  virtual bool Exists(std::string_view key, CacheKeyType type = CACHE_KEY_STRING,
                      size_t field_size = 0) = 0;

  template <typename T>
  RemoveRes DelOrderedMap(std::string_view key) {
    return Del(key, CACHE_KEY_RANGE_MAP, T::field_size);
  }
  template <typename T>
  bool ExistsOrderedMap(std::string_view key) {
    return Exists(key, CACHE_KEY_RANGE_MAP, T::field_size);
  }
  template <typename T>
  InsertOrReplaceResult OrderedMapSet(std::string_view key, const T& field, std::string_view value,
                                      int expire_secs = -1) {
    auto field_bin = field.Encode();
    return RangeMapSet(key, field_bin, value, expire_secs);
  }
  template <typename T>
  CacheValue OrderedMapGet(std::string_view key, const T& field) {
    auto field_bin = field.Encode();
    return RangeMapGet(key, field_bin);
  }
  template <typename T>
  int OrderedMapRangeGet(std::string_view key, const T& min, const T& max,
                         std::vector<CacheValue>& vals) {
    auto field_min_bin = min.Encode();
    auto field_max_bin = max.Encode();
    return RangeMapRangeGet(key, field_min_bin, field_max_bin, vals);
  }
  template <typename T>
  CacheValue OrderedMapMin(std::string_view key) {
    return RangeMapMin(key, T::field_size);
  }
  template <typename T>
  bool OrderedMapDel(std::string_view key, const T& field) {
    auto field_bin = field.Encode();
    return RangeMapDel(key, field_bin);
  }
  template <typename T>
  int OrderedMapSize(std::string_view key) {
    return RangeMapSize(key, T::field_size);
  }
  template <typename T>
  int OrderedMapCapacity(std::string_view key) {
    return RangeMapCapacity(key, T::field_size);
  }
  template <typename T>
  int OrderedMapSizeInBytes(std::string_view key) {
    return RangeMapSizeInBytes(key, T::field_size);
  }
  template <typename T>
  int OrderedMapRemainingBytes(std::string_view key) {
    return RangeMapRemainingBytes(key, T::field_size);
  }
  template <typename T>
  int OrderedMapWastedBytes(std::string_view key) {
    return RangeMapWastedBytes(key, T::field_size);
  }
  template <typename T>
  int OrderedMapCompact(std::string_view key) {
    return RangeMapCompact(key, T::field_size);
  }
  template <typename T>
  bool OrderedMapPop(std::string_view key) {
    return RangeMapPop(key, T::field_size);
  }
  template <typename T>
  int OrderedMapGetAll(std::string_view key, std::vector<CacheValue>& vals) {
    return RangeMapGetAll(key, T::field_size, vals);
  }

  template <typename T>
  RemoveRes DelUnorderedMap(std::string_view key) {
    return Del(key, CACHE_KEY_HASH_MAP, T::field_size);
  }
  template <typename T>
  bool ExistsUnorderedMap(std::string_view key) {
    return Exists(key, CACHE_KEY_HASH_MAP, T::field_size);
  }
  template <typename T>
  InsertOrReplaceResult UnorderedMapSet(std::string_view key, const T& field,
                                        std::string_view value, int expire_secs = -1) {
    auto field_bin = field.Encode();
    return HashMapSet(key, field_bin, value, expire_secs);
  }
  template <typename T>
  CacheValue UnorderedMapGet(std::string_view key, const T& field) {
    auto field_bin = field.Encode();
    return HashMapGet(key, field_bin);
  }
  template <typename T>
  int UnorderedMapSize(std::string_view key) {
    return HashMapSize(key, T::field_size);
  }
  template <typename T>
  bool UnorderedMapDel(std::string_view key, const T& field) {
    auto field_bin = field.Encode();
    return HashMapDel(key, field_bin);
  }

  template <typename T>
  int UnorderedMapGetAll(std::string_view key, std::vector<CacheValue>& vals) {
    return HashMapGetAll(key, T::field_size, vals);
  }
  template <typename T>
  int UnorderedMapSizeInBytes(std::string_view key) {
    return HashMapSizeInBytes(key, T::field_size);
  }
  template <typename T>
  int UnorderedMapCompact(std::string_view key) {
    return HashMapCompact(key, T::field_size);
  }

  virtual ~ECache() = default;
};
}  // namespace ecache