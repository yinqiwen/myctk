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
#include <vector>

#include "mraft/c_raft.h"
#include "mraft/log_entry.h"

namespace mraft {
class LogStorage {
 public:
  virtual ~LogStorage() {}

  // init logstorage, check consistency and integrity
  virtual int Init() = 0;

  // first log index in log
  virtual int64_t FirstLogIndex() = 0;

  // last log index in log
  virtual int64_t LastLogIndex() = 0;

  // get logentry by index
  virtual std::unique_ptr<LogEntry> GetEntry(const int64_t index) = 0;

  virtual std::vector<std::unique_ptr<LogEntry>> GetBatchEntries(const int64_t index, uint32_t n) = 0;

  // get logentry's term by index
  virtual int64_t GetTerm(const int64_t index) = 0;

  // append entries to log
  virtual int AppendEntry(LogEntry& entry) = 0;

  // // append entries to log and update IOMetric, return append success number
  // virtual int AppendEntries(const std::vector<LogEntry*>& entries) = 0;

  // delete logs from storage's head, [first_log_index, first_index_kept) will be discarded
  virtual int TruncatePrefix(const int64_t first_index_kept) = 0;

  // delete uncommitted logs from storage's tail, (last_index_kept, last_log_index] will be discarded
  virtual int TruncateSuffix(const int64_t last_index_kept) = 0;

  // Drop all the existing logs and reset next log index to |next_log_index|.
  // This function is called after installing snapshot from leader
  virtual int Reset(const int64_t next_log_index, int64_t term) = 0;

  virtual int ClearCacheEntry(const int64_t index) = 0;

  //   // Create an instance of this kind of LogStorage with the parameters encoded
  //   // in |uri|
  //   // Return the address referenced to the instance on success, NULL otherwise.
  //   virtual LogStorage* new_instance(const std::string& uri) const = 0;
};

void init_raft_log_impl(raft_log_impl_t* log);

}  // namespace mraft