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
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "folly/FBString.h"
#include "folly/SharedMutex.h"
#include "folly/Synchronized.h"
#include "folly/concurrency/AtomicSharedPtr.h"
#include "folly/container/EvictingCacheMap.h"
#include "folly/container/F14Map.h"
#include "robims_db.h"
#include "robims_query.h"
#include "robims_table.h"

namespace robims {
typedef std::shared_ptr<RobimsQuery> RobimsQueryPtr;
typedef folly::atomic_shared_ptr<RobimsTable> RobimsTablePtr;
struct RobimsDBData {
  IDMapping* id_mapping;
  folly::F14NodeMap<std::string_view, RobimsTablePtr> tables;
  folly::EvictingCacheMap<folly::fbstring, RobimsQueryPtr> query_cache;
  std::mutex query_cache_mutex;
  RobimsDBData();
  ~RobimsDBData();
};
class RobimsDBImpl {
 private:
  RobimsDBImpl(const RobimsDBImpl&) = delete;
  RobimsDBImpl& operator=(const RobimsDBImpl&) = delete;
  typedef folly::atomic_shared_ptr<RobimsDBData> RobimsDBDataPtr;
  // IDMapping* id_mapping_;
  // folly::F14FastMap<std::string_view, std::unique_ptr<RobimsTable>> tables_;
  // folly::EvictingCacheMap<folly::fbstring, RobimsQuery> query_table_;
  RobimsDBDataPtr db_data_;
  folly::SharedMutex* shared_mutex_;

  void GetRealIDs(const std::vector<uint32_t>& local_ids, std::vector<std::string>& ids);
  std::shared_ptr<RobimsTable> CreateTableInstance(const TableSchema& schema);

 public:
  RobimsDBImpl();
  void DisableThreadSafe();
  void EnableThreadSafe();
  int Load(const std::string& file);
  int Save(const std::string& file, bool readonly);
  int SaveTable(const std::string& file, const std::string& table, bool readonly);
  int CreateTable(const std::string& schema);
  int CreateTable(const TableSchema& schema);
  int Put(const std::string& table, const std::string& json);
  int Remove(const std::string& table, const std::string& json);
  int Select(const std::string& query, int64_t offset, int64_t limit, SelectResult& result);
  RobimsTable* GetTable(const std::string& name);
  ~RobimsDBImpl();
};

}  // namespace robims