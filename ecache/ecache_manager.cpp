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
#include "ecache_manager.h"
#include <bits/stdint-uintn.h>
#include <cachelib/allocator/Cache.h>
#include <stdio.h>
#include <string_view>
#include "ecache_common.h"
#include "ecache_impl.hpp"
#include "ecache_log.h"
namespace ecache {

template <typename Cache>
std::unique_ptr<Cache> new_cache(const ECacheManagerConfig& mconfig) {
  typename Cache::Config config;
  if (mconfig.enable_tail_hits_racking()) {
    config.enableTailHitsTracking();
  }
  if (mconfig.background_reaper_interval_ms() > 0) {
    config.enableItemReaperInBackground(
        std::chrono::milliseconds(mconfig.background_reaper_interval_ms()));
  }
  if (mconfig.memory_monitor_interval_ms() > 0) {
    using MMConfig = facebook::cachelib::MemoryMonitor::Config;
    MMConfig mm_config;
    mm_config.mode = facebook::cachelib::MemoryMonitor::Mode::ResidentMemory;
    config.enableMemoryMonitor(std::chrono::milliseconds(mconfig.memory_monitor_interval_ms()),
                               mm_config);
  }
  config.setTrackRecentItemsForDump(true)
      .setCacheSize(mconfig.size())
      .setCacheName(mconfig.name())
      .setAccessConfig({mconfig.buckets_power(), mconfig.locks_power()})
      .validate();
  return std::make_unique<Cache>(config);
}

template <typename Cache>
static int save_cache(FILE* fp, void* c) {
  Cache* cache = (Cache*)c;
  using ItemHandle = typename Cache::ItemHandle;
  size_t count = 0;
  int64_t start = gettimeofday_ms();
  auto itr = cache->begin();
  while (itr != cache->end()) {
    const auto& handle = itr.asHandle();
    auto key = handle->getKey();
    if (0 != file_write_string(fp, key)) {
      return -1;
    }
    uint32_t expiry_time = handle->getExpiryTime();
    bool has_expiry_time = expiry_time > 0;
    if (fwrite(&has_expiry_time, 1, 1, fp) != 1) {
      return -1;
    }
    if (has_expiry_time) {
      if (fwrite(&expiry_time, 4, 1, fp) != 1) {
        return -1;
      }
    }
    switch (key[1]) {
      case CACHE_KEY_STRING: {
        folly::StringPiece val((const char*)handle->getMemory(), handle->getSize());
        if (0 != file_write_string(fp, val)) {
          return -1;
        }
        break;
      }
      case CACHE_KEY_RANGE_MAP: {
        uint16_t field_size = 0;
        memcpy(&field_size, key.data() + 2, 2);
        ECacheImpl<Cache> tmp(c);
        if (0 != tmp.SaveRangeMap(fp, const_cast<ItemHandle&>(handle), field_size)) {
          return -1;
        }
        break;
      }
      case CACHE_KEY_HASH_MAP: {
        uint16_t field_size = 0;
        memcpy(&field_size, key.data() + 2, 2);
        ECacheImpl<Cache> tmp(c);
        if (0 != tmp.SaveHashMap(fp, const_cast<ItemHandle&>(handle), field_size)) {
          return -1;
        }
        break;
      }
      default: {
        ECACHE_ERROR("Invalid key type:{}", key[1]);
        return -1;
      }
    }
    count++;
    ++itr;
  }
  int64_t end = gettimeofday_ms();
  ECACHE_INFO("Cost {}ms to save {} keys.", end - start, count);
  return 0;
}

template <typename Cache>
static int load_cache(FILE* fp, void* c) {
  Cache* cache = (Cache*)c;
  using ItemHandle = typename Cache::ItemHandle;
  size_t count = 0;
  int64_t start = gettimeofday_ms();
  while (true) {
    std::string key;
    if (0 != file_read_string(fp, key)) {
      if (feof(fp)) {
        break;
      }
      return -1;
    }
    count++;
    uint8_t pool_id = key[0];
    bool has_expiry_time = false;
    int rc = fread(&has_expiry_time, 1, 1, fp);
    if (rc != 1) {
      return -1;
    }
    uint32_t expiry_time = 0;
    if (has_expiry_time) {
      rc = fread(&expiry_time, sizeof(expiry_time), 1, fp);
      if (rc != 1) {
        return -1;
      }
    }
    switch (key[1]) {
      case CACHE_KEY_STRING: {
        std::string val;
        if (0 != file_read_string(fp, val)) {
          return -1;
        }
        auto item = cache->allocate(pool_id, key, val.size());
        std::memcpy(item->getMemory(), val.data(), val.size());
        cache->insert(item);
        if (expiry_time > 0) {
          if (!item->updateExpiryTime(expiry_time)) {
            ECACHE_ERROR("Failed to update expiry time:{}, now:{}", expiry_time, gettimeofday_s());
          }
        }
        break;
      }
      case CACHE_KEY_RANGE_MAP: {
        uint16_t field_size = 0;
        memcpy(&field_size, key.data() + 2, 2);
        ECacheImpl<Cache> tmp(c, pool_id);
        if (0 != tmp.LoadRangeMap(fp, key, field_size, expiry_time)) {
          return -1;
        }
        break;
      }
      case CACHE_KEY_HASH_MAP: {
        uint16_t field_size = 0;
        memcpy(&field_size, key.data() + 2, 2);
        ECacheImpl<Cache> tmp(c, pool_id);
        if (0 != tmp.LoadHashMap(fp, key, field_size, expiry_time)) {
          return -1;
        }
        break;
      }
      default: {
        ECACHE_ERROR("Invalid key type:{}", key[1]);
        return -1;
      }
    }
  }

  int64_t end = gettimeofday_ms();
  ECACHE_INFO("Cost {}ms to load {} keys.", end - start, count);
  return 0;
}

ECacheManager::~ECacheManager() {
  switch (config_.type()) {
    case CACHE_LRU: {
      // facebook::cachelib::CacheBase;
      delete (facebook::cachelib::LruAllocator*)cache_;
      break;
    }
    case CACHE_LRU2Q: {
      // rc = ECacheImpl<facebook::cachelib::Lru2QAllocator>::Destroy();
      delete (facebook::cachelib::Lru2QAllocator*)cache_;
      break;
    }
    case CACHE_TINYLFU: {
      // rc = ECacheImpl<facebook::cachelib::TinyLFUAllocator>::Destroy();
      delete (facebook::cachelib::TinyLFUAllocator*)cache_;
      break;
    }
    case CACHE_LRU_SPIN_BUCKET: {
      // rc = ECacheImpl<facebook::cachelib::LruAllocatorSpinBuckets>::Destroy();
      delete (facebook::cachelib::LruAllocatorSpinBuckets*)cache_;
      break;
    }
    default: {
      break;
    }
  }
}
int ECacheManager::Load(const std::string& file) {
  FILE* fp = fopen(file.c_str(), "r");
  if (nullptr == fp) {
    ECACHE_ERROR("Failed to open file:{} to load robims db", file);
    return -1;
  }
  ECacheBackupHeader backup_header;
  std::string config_bin;
  if (0 != file_read_string(fp, config_bin)) {
    fclose(fp);
    return -1;
  }
  if (!backup_header.ParseFromString(config_bin)) {
    ECACHE_ERROR("Failed to parse ECacheBackupHeader!");
    fclose(fp);
    return -1;
  }
  if (0 != Init(backup_header.config())) {
    fclose(fp);
    return -1;
  }
  pool_id_mapping_.clear();
  for (const auto& pool_config : backup_header.pools()) {
    NewCache(pool_config);
  }
  int rc = -1;
  switch (config_.type()) {
    case CACHE_LRU: {
      rc = load_cache<facebook::cachelib::LruAllocator>(fp, cache_);
      break;
    }
    case CACHE_LRU2Q: {
      rc = load_cache<facebook::cachelib::Lru2QAllocator>(fp, cache_);
      break;
    }
    case CACHE_TINYLFU: {
      rc = load_cache<facebook::cachelib::TinyLFUAllocator>(fp, cache_);
      break;
    }
    case CACHE_LRU_SPIN_BUCKET: {
      rc = load_cache<facebook::cachelib::LruAllocatorSpinBuckets>(fp, cache_);
      break;
    }
    default: {
      break;
    }
  }
  fclose(fp);
  return rc;
}
int ECacheManager::Save(const std::string& file) {
  FILE* fp = fopen(file.c_str(), "w+");
  if (nullptr == fp) {
    ECACHE_ERROR("Failed to create file:{} to save robims db", file);
    return -1;
  }
  ECacheBackupHeader backup_header;
  backup_header.set_version(1);
  backup_header.mutable_config()->CopyFrom(config_);
  for (size_t i = 0; i < pool_configs_.size(); i++) {
    backup_header.add_pools()->CopyFrom(pool_configs_[i]);
  }

  int rc = -1;
  std::string config_bin = backup_header.SerializeAsString();
  if (0 != file_write_string(fp, config_bin)) {
    fclose(fp);
    return -1;
  }

  switch (config_.type()) {
    case CACHE_LRU: {
      rc = save_cache<facebook::cachelib::LruAllocator>(fp, cache_);
      break;
    }
    case CACHE_LRU2Q: {
      rc = save_cache<facebook::cachelib::Lru2QAllocator>(fp, cache_);
      break;
    }
    case CACHE_TINYLFU: {
      rc = save_cache<facebook::cachelib::TinyLFUAllocator>(fp, cache_);
      break;
    }
    case CACHE_LRU_SPIN_BUCKET: {
      rc = save_cache<facebook::cachelib::LruAllocatorSpinBuckets>(fp, cache_);
      break;
    }
    default: {
      break;
    }
  }
  fclose(fp);
  return rc;
}

int ECacheManager::Init(const ECacheManagerConfig& config) {
  ECacheManagerConfig tmp_config = config;
  if (tmp_config.size() <= 0) {
    tmp_config.set_size(1 * 1024 * 1024 * 1024);
  }
  tmp_config.set_size(tmp_config.size() + 4 * 1024 * 1024);
  if (tmp_config.buckets_power() <= 0) {
    tmp_config.set_buckets_power(25);
  }
  if (tmp_config.locks_power() <= 0) {
    tmp_config.set_locks_power(10);
  }
  if (tmp_config.name().empty()) {
    tmp_config.set_name("ECache");
  }
  switch (tmp_config.type()) {
    case CACHE_LRU: {
      cache_ = new_cache<facebook::cachelib::LruAllocator>(tmp_config).release();
      break;
    }
    case CACHE_LRU2Q: {
      cache_ = new_cache<facebook::cachelib::Lru2QAllocator>(tmp_config).release();
      break;
    }
    case CACHE_TINYLFU: {
      cache_ = new_cache<facebook::cachelib::TinyLFUAllocator>(tmp_config).release();
      break;
    }
    case CACHE_LRU_SPIN_BUCKET: {
      cache_ = new_cache<facebook::cachelib::LruAllocatorSpinBuckets>(tmp_config).release();
      break;
    }
    default: {
      ECACHE_ERROR("Failed to init cache with invalid type:{} and size:{}", tmp_config.type(),
                   tmp_config.size());
      return -1;
    }
  }
  if (nullptr != cache_) {
    config_ = tmp_config;
    ECACHE_INFO("Success to init cache with  type:{} and size:{}", config.type(), config.size());
    return 0;
  } else {
    ECACHE_ERROR("Failed to init cache with  type:{} and size:{}", config.type(), config.size());
    return -1;
  }
}

std::unique_ptr<ECache> ECacheManager::NewCache(const ECacheConfig& config) {
  std::unique_ptr<ECache> cache;
  switch (config_.type()) {
    case CACHE_LRU: {
      cache.reset(new ECacheImpl<facebook::cachelib::LruAllocator>(cache_));
      break;
    }
    case CACHE_LRU2Q: {
      cache.reset(new ECacheImpl<facebook::cachelib::Lru2QAllocator>(cache_));
      break;
    }
    case CACHE_TINYLFU: {
      cache.reset(new ECacheImpl<facebook::cachelib::TinyLFUAllocator>(cache_));
      break;
    }
    case CACHE_LRU_SPIN_BUCKET: {
      cache.reset(new ECacheImpl<facebook::cachelib::LruAllocatorSpinBuckets>(cache_));
      break;
    }
    default: {
      ECACHE_ERROR("Failed to create cache since ECacheManager is NOT init success.");
      return nullptr;
    }
  }
  if (0 != cache->Init(config)) {
    return nullptr;
  }
  pool_id_mapping_[config.name()] = cache->GetPoolId();
  pool_configs_.push_back(config);
  ECACHE_INFO("Success to init cache:{}.", config.DebugString());
  return cache;
}
std::unique_ptr<ECache> ECacheManager::GetCache(const std::string& name) {
  auto found = pool_id_mapping_.find(name);
  if (found == pool_id_mapping_.end()) {
    return nullptr;
  }
  uint8_t pool_id = found->second;
  std::unique_ptr<ECache> cache;
  switch (config_.type()) {
    case CACHE_LRU: {
      cache.reset(new ECacheImpl<facebook::cachelib::LruAllocator>(cache_, pool_id));
      break;
    }
    case CACHE_LRU2Q: {
      cache.reset(new ECacheImpl<facebook::cachelib::Lru2QAllocator>(cache_, pool_id));
      break;
    }
    case CACHE_TINYLFU: {
      cache.reset(new ECacheImpl<facebook::cachelib::TinyLFUAllocator>(cache_, pool_id));
      break;
    }
    case CACHE_LRU_SPIN_BUCKET: {
      cache.reset(new ECacheImpl<facebook::cachelib::LruAllocatorSpinBuckets>(cache_, pool_id));
      break;
    }
    default: {
      ECACHE_ERROR("Failed to create cache since ECacheManager is NOT init success.");
      return nullptr;
    }
  }
  return cache;
}

facebook::cachelib::GlobalCacheStats ECacheManager::getGlobalCacheStats() const {
  facebook::cachelib::CacheBase* cache = (facebook::cachelib::CacheBase*)cache_;
  return cache->getGlobalCacheStats();
}

facebook::cachelib::CacheMemoryStats ECacheManager::getCacheMemoryStats() const {
  facebook::cachelib::CacheBase* cache = (facebook::cachelib::CacheBase*)cache_;
  return cache->getCacheMemoryStats();
}

facebook::cachelib::PoolStats ECacheManager::getPoolStats(const std::string& name) const {
  facebook::cachelib::CacheBase* cache = (facebook::cachelib::CacheBase*)cache_;
  auto found = pool_id_mapping_.find(name);
  if (found == pool_id_mapping_.end()) {
    static facebook::cachelib::PoolStats empty;
    return empty;
  }
  uint8_t pool_id = found->second;
  return cache->getPoolStats(pool_id);
}

}  // namespace ecache