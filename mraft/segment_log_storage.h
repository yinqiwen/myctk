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
#include <folly/lang/Bits.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/btree_map.h"
#include "folly/File.h"
#include "folly/FileUtil.h"
#include "folly/Synchronized.h"
#include "folly/container/F14Map.h"
#include "folly/io/IOBuf.h"
#include "mraft/log_storage.h"

namespace mraft {

struct EntryHeader {
  int64_t term = 0;
  uint8_t type = 0;
  uint32_t data_len = 0;
  uint32_t data_checksum = 0;
  uint32_t header_checksum = 0;
  int Decode(const char* data);
  std::unique_ptr<folly::IOBuf> Encode() const;
  std::string ToString() const;
};
struct LogMeta {
  off_t offset = 0;
  size_t length = 0;
  int64_t term = 0;
};
class Segment {
 public:
  Segment(const std::string& path, const int64_t first_index);
  Segment(const std::string& path, const int64_t first_index, const int64_t last_index);
  int64_t GetTerm(const int64_t index) const;
  std::unique_ptr<LogEntry> Get(const int64_t index) const;
  int64_t FirstIndex() const { return first_index_; }
  int64_t LastIndex() const { return last_index_.load(); }
  int Create();
  int Load();
  int Sync(bool will_sync);
  int Close(bool will_sync);
  int Append(LogEntry& entry);
  int Unlink();
  int64_t Bytes() const { return bytes_; }
  int Truncate(const int64_t last_index_kept);
  bool IsOpen() const { return is_open_; }
  ~Segment() = default;

 private:
  std::string GetPath(bool is_open);
  int LoadEntry(int64_t index, off_t offset, EntryHeader& head, LogEntry* data) const;
  int GetMeta(int64_t index, LogMeta& meta) const;

  std::unique_ptr<folly::File> file_;
  std::string path_;
  int64_t bytes_;
  int fd_;
  mutable std::mutex mutex_;
  bool is_open_;
  const int64_t first_index_;
  std::atomic<int64_t> last_index_;
  std::vector<std::pair<int64_t /*offset*/, int64_t /*term*/>> offset_and_term_;
};

class SegmentLogStorage : public LogStorage {
 public:
  typedef std::shared_ptr<Segment> SegmentPtr;
  SegmentLogStorage(const std::string& path);

 private:
  // init logstorage, check consistency and integrity
  int Init() override;

  // first log index in log
  int64_t FirstLogIndex() override;

  // last log index in log
  int64_t LastLogIndex() override;

  // get logentry by index
  std::unique_ptr<LogEntry> GetEntry(const int64_t index) override;

  std::vector<std::unique_ptr<LogEntry>> GetBatchEntries(const int64_t index, uint32_t n) override;

  // get logentry's term by index
  int64_t GetTerm(const int64_t index) override;

  // append entries to log
  int AppendEntry(LogEntry& entry) override;

  // delete logs from storage's head, [first_log_index, first_index_kept) will be discarded
  int TruncatePrefix(const int64_t first_index_kept) override;

  // delete uncommitted logs from storage's tail, (last_index_kept, last_log_index] will be discarded
  int TruncateSuffix(const int64_t last_index_kept) override;

  // Drop all the existing logs and reset next log index to |next_log_index|.
  // This function is called after installing snapshot from leader
  int Reset(const int64_t next_log_index, int64_t term) override;

  int ClearCacheEntry(const int64_t index) override;

  int LoadMeta(bool& is_empty);
  int SaveMeta(int64_t log_index);
  int ListSegments(bool is_empty);
  int LoadSegments();
  SegmentPtr GetOpenSegment();
  SegmentPtr GetSegment(const int64_t index);
  void PopSegments(int64_t first_index_kept, std::vector<SegmentPtr>& poped);
  void PopSegmentsFromBack(const int64_t last_index_kept, std::vector<SegmentPtr>& popped, SegmentPtr& last_segment);

  std::string path_;

  typedef absl::btree_map<int64_t, SegmentPtr> SegmentMap;
  SegmentMap segments_;
  SegmentPtr open_segment_;
  folly::Synchronized<uint32_t> segments_lock_;

  std::atomic<int64_t> last_log_index_{1};
  std::atomic<int64_t> first_log_index_{0};
  std::deque<LogEntry> latest_entries_cache_;
  std::unique_ptr<LogEntry> latest_entry_;
};
}  // namespace mraft
