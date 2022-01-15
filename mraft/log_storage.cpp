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
#include "mraft/log_storage.h"
#include <folly/io/IOBuf.h>

#include "mraft/logger.h"

namespace mraft {

static void* raftLogInit(void* raft, void* arg) {
  LogStorage* storage = reinterpret_cast<LogStorage*>(arg);
  // storage->Init();
  return storage;
}
static void raftLogFree(void* log) {
  // LogStorage* storage = reinterpret_cast<LogStorage*>(log);
  // delete storage;
}
static void raftLogReset(void* log, raft_index_t first_idx, raft_term_t term) {
  LogStorage* storage = reinterpret_cast<LogStorage*>(log);
  storage->Reset(first_idx, term);
}
static int raftLogAppend(void* log, raft_entry_t* entry) {
  LogStorage* storage = reinterpret_cast<LogStorage*>(log);
  entry->id = storage->LastLogIndex() + 1;
  LogEntry log_entry = LogEntry::Wrap(entry, false);
  return storage->AppendEntry(log_entry);
}
static int raftLogPoll(void* log, raft_index_t first_idx) {
  LogStorage* storage = reinterpret_cast<LogStorage*>(log);
  return storage->TruncatePrefix(first_idx);
}
static int raftLogPop(void* log, raft_index_t from_idx, func_entry_notify_f cb, void* cb_arg) {
  LogStorage* storage = reinterpret_cast<LogStorage*>(log);
  return storage->TruncateSuffix(from_idx - 1);
}
static raft_entry_t* raftLogGet(void* log, raft_index_t idx) {
  LogStorage* storage = reinterpret_cast<LogStorage*>(log);
  std::unique_ptr<LogEntry> entry = storage->GetEntry(idx);
  if (!entry) {
    return nullptr;
  }
  return entry->TakeRawEntry();
}
static int raftLogGetBatch(void* log, raft_index_t idx, int entries_n, raft_entry_t** entries) {
  LogStorage* storage = reinterpret_cast<LogStorage*>(log);
  std::vector<std::unique_ptr<LogEntry>> all_entries = storage->GetBatchEntries(idx, entries_n);
  for (size_t i = 0; i < all_entries.size(); i++) {
    entries[i] = all_entries[i]->TakeRawEntry();
  }
  return all_entries.size();
}
static raft_index_t raftLogFirstIdx(void* log) {
  LogStorage* storage = reinterpret_cast<LogStorage*>(log);
  return storage->FirstLogIndex();
}
static raft_index_t raftLogCurrentIdx(void* log) {
  LogStorage* storage = reinterpret_cast<LogStorage*>(log);
  return storage->LastLogIndex();
}
static raft_index_t raftLogCount(void* log) {
  LogStorage* storage = reinterpret_cast<LogStorage*>(log);
  return storage->LastLogIndex() - storage->FirstLogIndex() + 1;
}

void init_raft_log_impl(raft_log_impl_t* log) {
  log->init = raftLogInit;
  log->free = raftLogFree;
  log->reset = raftLogReset;
  log->append = raftLogAppend;
  log->poll = raftLogPoll;
  log->pop = raftLogPop;
  log->get = raftLogGet;
  log->get_batch = raftLogGetBatch;
  log->first_idx = raftLogFirstIdx;
  log->current_idx = raftLogCurrentIdx;
  log->count = raftLogCount;
}
}  // namespace mraft