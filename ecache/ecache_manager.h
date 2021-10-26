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
#include <string_view>
#include "cachelib/allocator/CacheStats.h"
#include "ecache.h"
#include "ecache.pb.h"
#include "folly/container/F14Map.h"
namespace ecache {

class ECacheManager {
 private:
  ECacheManagerConfig config_;
  std::vector<ECacheConfig> pool_configs_;
  void* cache_ = nullptr;
  folly::F14FastMap<std::string, uint8_t> pool_id_mapping_;

 public:
  int Init(const ECacheManagerConfig& config);
  int Load(const std::string& file);
  int Save(const std::string& file);
  std::unique_ptr<ECache> NewCache(const ECacheConfig& config);
  std::unique_ptr<ECache> GetCache(const std::string& name);

  // return the overall cache stats
  facebook::cachelib::GlobalCacheStats getGlobalCacheStats() const;

  // return cache's memory usage stats
  facebook::cachelib::CacheMemoryStats getCacheMemoryStats() const;

  // pool stats by pool name
  facebook::cachelib::PoolStats getPoolStats(const std::string& name) const;
  ~ECacheManager();
};
}  // namespace ecache