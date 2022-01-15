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
#include "mraft/segment_log_storage.h"
#include <folly/io/IOBuf.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "folly/Conv.h"
#include "folly/File.h"
#include "folly/FileUtil.h"
#include "folly/String.h"
#include "folly/hash/Checksum.h"
#include "folly/portability/Dirent.h"
#include "folly/portability/Filesystem.h"
#include "gflags/gflags.h"
#include "mraft.pb.h"
#include "mraft/logger.h"
#include "mraft/utils.h"

namespace mraft {

DEFINE_int32(raft_max_segment_size, 8 * 1024 * 1024 /*8M*/, "Max size of one segment file");

static constexpr std::string_view kSegmentOpenPrefix = "log_inprogress_";
static constexpr std::string_view kSegmentClosePrefix = "log_";
static constexpr std::string_view kSegmentLogMeta = "log_meta";
// Format of Header, all fields are in network order
// | -------------------- term (64bits) -------------------------  |
// | entry-type (8bits) | checksum_type (8bits) | reserved(16bits) |
// | ------------------ data len (32bits) -----------------------  |
// | data_checksum (32bits) | header checksum (32bits)             |
static constexpr ssize_t kEntryHeaderLength = 24;

static inline bool verify_checksum(const char* data, size_t len, uint32_t value) {
  uint32_t cksm = folly::crc32(reinterpret_cast<const uint8_t*>(data), len);
  return cksm == value;
}
inline uint32_t get_checksum(const folly::IOBuf& data) { return folly::crc32(data.data(), data.length()); }

int EntryHeader::Decode(const char* data) {
  term = LocalInt<int64_t>::Read(data);
  data += sizeof(term);
  type = LocalInt<uint8_t>::Read(data);
  data += 4;
  data_len = LocalInt<uint32_t>::Read(data);
  data += sizeof(data_len);
  data_checksum = LocalInt<uint32_t>::Read(data);
  data += sizeof(data_checksum);
  header_checksum = LocalInt<uint32_t>::Read(data);
  return 0;
}
std::unique_ptr<folly::IOBuf> EntryHeader::Encode() const {
  std::unique_ptr<folly::IOBuf> buf = folly::IOBuf::create(kEntryHeaderLength);
  uint8_t* data = buf->writableData();
  LocalInt<int64_t>::Write(data, term);
  data += sizeof(term);
  LocalInt<uint8_t>::Write(data, type);
  data += 4;
  LocalInt<uint32_t>::Write(data, data_len);
  data += sizeof(data_len);
  LocalInt<uint32_t>::Write(data, data_checksum);
  data += sizeof(data_checksum);
  buf->append(kEntryHeaderLength - sizeof(header_checksum));
  uint32_t header_cksm = get_checksum(*buf);
  LocalInt<uint32_t>::Write(data, header_cksm);
  buf->append(sizeof(data_checksum));
  return buf;
}
std::string EntryHeader::ToString() const {
  std::string s;
  s.append("term=")
      .append(std::to_string(term))
      .append(",type=")
      .append(std::to_string(type))
      .append(",data_len=")
      .append(std::to_string(data_len))
      .append(",data_cksm=")
      .append(std::to_string(data_checksum))
      .append(",header_cksm=")
      .append(std::to_string(header_checksum));
  return s;
}

Segment::Segment(const std::string& path, const int64_t first_index)
    : path_(path), bytes_(0), fd_(-1), is_open_(true), first_index_(first_index), last_index_(first_index - 1) {}
Segment::Segment(const std::string& path, const int64_t first_index, const int64_t last_index)
    : path_(path), bytes_(0), fd_(-1), is_open_(false), first_index_(first_index), last_index_(last_index) {}
int Segment::LoadEntry(int64_t index, off_t offset, EntryHeader& head, LogEntry* data) const {
  char buf[kEntryHeaderLength];
  const ssize_t n = folly::preadFull(fd_, buf, kEntryHeaderLength, offset);
  if (n != kEntryHeaderLength) {
    return n < 0 ? -1 : 1;
  }
  head.Decode(buf);
  if (!verify_checksum(buf, kEntryHeaderLength - 4, head.header_checksum)) {
    MRAFT_ERROR("Found corrupted header at offset={}, header={},path:{}", offset, head.ToString(), path_);
    return -1;
  }
  if (nullptr != data) {
    LogEntry entry = LogEntry::Create(index, head.term, head.type, head.data_len);
    if (head.data_len > 0) {
      const ssize_t n = folly::preadFull(fd_, entry.WritableData(), head.data_len, offset + kEntryHeaderLength);
      if (n != static_cast<ssize_t>(head.data_len)) {
        return n < 0 ? -1 : 1;
      }
      if (!verify_checksum(reinterpret_cast<const char*>(entry.Data()), head.data_len, head.data_checksum)) {
        MRAFT_ERROR("Found corrupted data at offset={}, header={},path:{}", offset + kEntryHeaderLength,
                    head.ToString(), path_);
        return -1;
      }
    }
    *data = std::move(entry);
  }
  return 0;
}
int Segment::GetMeta(int64_t index, LogMeta& meta) const {
  std::unique_lock<std::mutex> lck(mutex_);
  if (index > last_index_.load(std::memory_order_relaxed) || index < first_index_) {
    // out of range
    MRAFT_ERROR("last_index={},first_index:{}", last_index_.load(std::memory_order_relaxed), first_index_);
    return -1;
  } else if (last_index_ == first_index_ - 1) {
    MRAFT_ERROR("last_index={},first_index:{}", last_index_.load(std::memory_order_relaxed), first_index_);
    // empty
    return -1;
  }
  int64_t meta_index = index - first_index_;
  int64_t entry_cursor = offset_and_term_[meta_index].first;
  int64_t next_cursor =
      (index < last_index_.load(std::memory_order_relaxed)) ? offset_and_term_[meta_index + 1].first : bytes_;
  // DCHECK_LT(entry_cursor, next_cursor);
  meta.offset = entry_cursor;
  meta.term = offset_and_term_[meta_index].second;
  meta.length = next_cursor - entry_cursor;
  return 0;
}
int Segment::Create() {
  if (!is_open_) {
    MRAFT_ERROR("Create on a closed segment at first_index={} in {}", first_index_, path_);
    return -1;
  }

  std::string file_path = GetPath(is_open_);
  try {
    file_ = std::make_unique<folly::File>(file_path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    fd_ = file_->fd();
    MRAFT_INFO("Create new segment:{} with fd:{}", file_path, fd_);
  } catch (const std::exception& ex) {
    MRAFT_ERROR("Created new segment:{} error:{}", file_path, ex.what());
    return -1;
  }
  return fd_ >= 0 ? 0 : -1;
}
int Segment::Sync(bool will_sync) {
  if (will_sync) {
    fdatasync(fd_);
  }
  return 0;
}
int64_t Segment::GetTerm(const int64_t index) const {
  LogMeta meta;
  if (GetMeta(index, meta) != 0) {
    return 0;
  }
  return meta.term;
}

std::unique_ptr<LogEntry> Segment::Get(const int64_t index) const {
  LogMeta meta;
  if (GetMeta(index, meta) != 0) {
    return nullptr;
  }
  bool ok = true;
  std::unique_ptr<LogEntry> entry = std::make_unique<LogEntry>();
  do {
    EntryHeader header;
    if (LoadEntry(index, meta.offset, header, entry.get()) != 0) {
      ok = false;
      break;
    }
    if (!ok) {
      break;
    }
  } while (0);

  if (!ok) {
    entry = nullptr;
  }
  return entry;
}

int Segment::Append(LogEntry& entry) {
  if (!is_open_) {
    return EINVAL;
  } else if (entry.Index() != last_index_.load(std::memory_order_consume) + 1) {
    MRAFT_ERROR("entry->index={}, last_index={}, first_index={}", entry.Index(), last_index_, first_index_);
    return ERANGE;
  }
  EntryHeader header;
  header.term = entry.Term();
  header.type = entry.Type();
  std::unique_ptr<folly::IOBuf> data_buf;
  if (entry.DataLength() > 0) {
    header.data_len = entry.DataLength();
    data_buf = folly::IOBuf::wrapBuffer(entry.Data(), entry.DataLength());
    header.data_checksum = get_checksum(*data_buf);
  } else {
    header.data_len = 0;
  }

  auto head_buf = header.Encode();
  if (entry.DataLength() > 0) {
    head_buf->appendToChain(std::move(data_buf));
  }
  const size_t to_write = head_buf->length() + header.data_len;
  auto iov = head_buf->getIov();
  ssize_t n = folly::writevFull(fd_, iov.data(), iov.size());
  if (n != to_write) {
    int err = errno;
    MRAFT_ERROR("Fail to write to fd={}, path:{}, error:{}", fd_, path_, folly::errnoStr(err));
    return err;
  }

  std::unique_lock<std::mutex> lck(mutex_);
  offset_and_term_.push_back(std::make_pair(bytes_, entry.Term()));
  last_index_.fetch_add(1, std::memory_order_relaxed);
  bytes_ += to_write;
  return 0;
}

int Segment::Close(bool will_sync) {
  if (!is_open_) {
    return 0;
  }
  std::string old_path = GetPath(true);
  std::string new_path = GetPath(false);
  MRAFT_INFO("Close a full segment. Current first_index:{}, last_index:{},path:{}, sync:{}", first_index_,
             last_index_.load(), new_path, will_sync);
  // TODO: optimize index memory usage by reconstruct vector
  int ret = 0;
  if (last_index_.load() > first_index_) {
    if (will_sync) {
      ret = Sync(will_sync);
    }
  }
  if (ret == 0) {
    is_open_ = false;
    const int rc = ::rename(old_path.c_str(), new_path.c_str());
    if (0 == rc) {
      MRAFT_INFO("Rename '{}' to '{}'", old_path, new_path);
    } else {
      int err = errno;
      MRAFT_ERROR("Fail to rename to '{}' to '{}', error:{}", old_path, new_path, folly::errnoStr(err));
    }
    return rc;
  }
  return ret;
}

int Segment::Unlink() {
  int ret = 0;
  do {
    std::string old_path = GetPath(is_open_);
    std::string tmp_path(old_path);
    tmp_path.append(".tmp");
    ret = ::rename(old_path.c_str(), tmp_path.c_str());
    if (ret != 0) {
      int err = errno;
      MRAFT_ERROR("Failed to rename {} to {} with err:{}", old_path, tmp_path, folly::errnoStr(err));
      break;
    }

    // start bthread to unlink
    // TODO unlink follow control
    MRAFT_INFO("Unlinked segment '{}'", old_path);
    int rc = ::unlink(tmp_path.c_str());
    if (0 != rc) {
      int err = errno;
      MRAFT_ERROR("Failed to unlink {} with err:{}", tmp_path, folly::errnoStr(err));
    }
  } while (0);
  return ret;
}

int Segment::Truncate(const int64_t last_index_kept) {
  int64_t truncate_size = 0;
  int64_t first_truncate_in_offset = 0;
  std::unique_lock<std::mutex> lck(mutex_);
  if (last_index_kept >= last_index_) {
    return 0;
  }
  first_truncate_in_offset = last_index_kept + 1 - first_index_;
  truncate_size = offset_and_term_[first_truncate_in_offset].first;
  MRAFT_INFO("Truncating {} first_index:{} last_index from {} to {}  truncate size to {}", path_, first_index_,
             last_index_, last_index_kept, truncate_size);
  lck.unlock();

  // Truncate on a full segment need to rename back to inprogess segment again,
  // because the node may crash before truncate.
  if (!is_open_) {
    std::string old_path = GetPath(false);
    std::string new_path = GetPath(true);
    int ret = ::rename(old_path.c_str(), new_path.c_str());
    if (0 == ret) {
      MRAFT_INFO("Renamed {}  to {} ", old_path, new_path);
    } else {
      int err = errno;
      MRAFT_ERROR("Fail to rename {}  to {} with error:{}", old_path, new_path, folly::errnoStr(err));
      return ret;
    }
    is_open_ = true;
  }

  // truncate fd
  int ret = folly::ftruncateNoInt(fd_, truncate_size);
  if (ret < 0) {
    int err = errno;
    MRAFT_ERROR("Fail to truncate fd:{} to size:{} with error:{}", fd_, truncate_size, folly::errnoStr(err));
    return ret;
  }

  // seek fd
  off_t ret_off = ::lseek(fd_, truncate_size, SEEK_SET);
  if (ret_off < 0) {
    int err = errno;
    MRAFT_ERROR("Fail to lseek fd:{} to size:{} with error:{}", fd_, truncate_size, folly::errnoStr(err));
    return -1;
  }

  lck.lock();
  // update memory var
  offset_and_term_.resize(first_truncate_in_offset);
  last_index_.store(last_index_kept, std::memory_order_relaxed);
  bytes_ = truncate_size;
  return ret;
}

std::string Segment::GetPath(bool is_open) {
  std::string file_path = path_;
  if (is_open) {
    file_path.append("/").append(kSegmentOpenPrefix).append(std::to_string(first_index_));
  } else {
    file_path.append("/")
        .append(kSegmentClosePrefix)
        .append(std::to_string(first_index_))
        .append("_")
        .append(std::to_string(last_index_.load()));
  }
  return file_path;
}

int Segment::Load() {
  std::string file_path = GetPath(is_open_);
  try {
    file_ = std::make_unique<folly::File>(file_path, O_RDWR);
    fd_ = file_->fd();
  } catch (...) {
    // MRAFT_ERROR("Failed to open file:{} with msg:{}", file_path, ex.what());
    MRAFT_ERROR("Failed to open file:{}", file_path);
    return -1;
  }
  // get file size
  struct stat st_buf;
  if (fstat(fd_, &st_buf) != 0) {
    int save_err = errno;
    MRAFT_ERROR("Failed to get file stat:{} with error:{}", file_path, folly::errnoStr(save_err));
    file_ = nullptr;
    fd_ = -1;
    return -1;
  }
  int64_t file_size = st_buf.st_size;
  int64_t entry_off = 0;
  int64_t actual_last_index = first_index_ - 1;
  int ret = 0;
  for (int64_t i = first_index_; entry_off < file_size; i++) {
    EntryHeader header;
    int rc = LoadEntry(i, entry_off, header, nullptr);
    if (rc > 0) {
      // The last log was not completely written, which should be truncated
      break;
    }
    if (rc < 0) {
      ret = rc;
      break;
    }
    const int64_t skip_len = kEntryHeaderLength + header.data_len;
    if (entry_off + skip_len > file_size) {
      // The last log was not completely written and it should be
      // truncated
      break;
    }
    offset_and_term_.push_back(std::make_pair(entry_off, header.term));
    ++actual_last_index;
    entry_off += skip_len;
  }
  if (ret != 0) {
    return ret;
  }
  if (is_open_) {
    last_index_ = actual_last_index;
  }

  // truncate last uncompleted entry
  if (entry_off != file_size) {
    MRAFT_INFO("truncate last uncompleted write entry, path:{},first_index:{},old_size:{},new_size:{}", path_,
               first_index_, file_size, entry_off);
    ret = folly::ftruncateNoInt(fd_, entry_off);
  }

  // seek to end, for opening segment
  ::lseek(fd_, entry_off, SEEK_SET);

  bytes_ = entry_off;
  return ret;
}

SegmentLogStorage::SegmentLogStorage(const std::string& path) : path_(path) {}

int SegmentLogStorage::ListSegments(bool is_empty) {
  DIR* dir = opendir(path_.c_str());
  if (nullptr == dir) {
    int err = errno;
    MRAFT_ERROR("Failed to open dir:{} with error:{}", path_, folly::errnoStr(err));
    return -1;
  }
  struct dirent* ptr;
  while ((ptr = readdir(dir)) != nullptr) {
    std::string file_name = ptr->d_name;
    if (file_name == kSegmentLogMeta || file_name == "." || file_name == "..") {
      continue;
    }
    if ((is_empty && has_prefix(file_name, "log_")) || has_suffix(file_name, ".tmp")) {
      std::string segment_path(path_);
      segment_path.append("/");
      segment_path.append(ptr->d_name);
      ::unlink(segment_path.c_str());
      MRAFT_WARN("unlink unused segment, path:{}", segment_path);
      continue;
    }
    if (has_prefix(file_name, kSegmentOpenPrefix)) {
      std::string index_str = file_name.substr(kSegmentOpenPrefix.size());
      try {
        int64_t first_index = folly::to<int64_t>(index_str);
        MRAFT_INFO("restore open segment, path:{},first_index:{}", path_, first_index);
        if (open_segment_) {
          MRAFT_ERROR("open segment conflict, path::{},first_index:{}", path_, first_index);
          return -1;
        }
        open_segment_ = std::make_shared<Segment>(path_, first_index);
      } catch (const std::range_error& ex) {
        MRAFT_WARN("Invalid file path name:{} in dir:{} with err:{}", file_name, path_, ex.what());
      }
      continue;
    }
    if (has_prefix(file_name, kSegmentClosePrefix)) {
      std::string index_str = file_name.substr(kSegmentClosePrefix.size());
      std::vector<std::string> parts;
      folly::split('_', index_str, parts);
      if (parts.size() != 2) {
        MRAFT_WARN("Invalid file path name:{} in dir:{}", file_name, path_);
        continue;
      }
      try {
        int64_t first_index = folly::to<int64_t>(parts[0]);
        int64_t last_index = folly::to<int64_t>(parts[1]);
        MRAFT_INFO("restore closed segment, path:{},first_index:{},last_index:{}", path_, first_index, last_index);
        SegmentPtr segment = std::make_shared<Segment>(path_, first_index, last_index);
        segments_[first_index] = segment;
      } catch (const std::range_error& ex) {
        MRAFT_WARN("Invalid file path name:{} in dir:{} with err:{}", file_name, path_, ex.what());
      }
      continue;
    }

    MRAFT_WARN("Unexpected file path name:{} in dir:{}", file_name, path_);
  }
  closedir(dir);
  return 0;
}

int SegmentLogStorage::LoadSegments() {
  MRAFT_INFO("Segment map size:{}", segments_.size());
  for (auto& pair : segments_) {
    SegmentPtr segment = pair.second;
    MRAFT_INFO("load closed segment, path:{}, first_index:{}, last_index:{}", path_, segment->FirstIndex(),
               segment->LastIndex());
    int rc = segment->Load();
    if (rc != 0) {
      return rc;
    }
    last_log_index_.store(segment->LastIndex(), std::memory_order_release);
  }
  // open segment
  if (open_segment_) {
    MRAFT_INFO("load open segment, path:{}, first_index:{}, last_index:{}", path_, open_segment_->FirstIndex(),
               open_segment_->LastIndex());
    int rc = open_segment_->Load();
    if (rc != 0) {
      return rc;
    }
    if (first_log_index_.load() > open_segment_->LastIndex()) {
      MRAFT_WARN("open segment need discard, path:{}, first_log_index:{}, first_index:{}, last_index:{}", path_,
                 first_log_index_.load(), open_segment_->FirstIndex(), open_segment_->LastIndex());
      open_segment_->Unlink();
      open_segment_ = nullptr;
    } else {
      last_log_index_.store(open_segment_->LastIndex(), std::memory_order_release);
    }
  }
  if (last_log_index_ == 0) {
    last_log_index_ = first_log_index_.load() - 1;
  }
  return 0;
}

SegmentLogStorage::SegmentPtr SegmentLogStorage::GetOpenSegment() {
  SegmentPtr prev_open_segment;
  segments_lock_.withWLock([this, &prev_open_segment](auto&) {
    if (open_segment_ && open_segment_->Bytes() > FLAGS_raft_max_segment_size) {
      segments_[open_segment_->FirstIndex()] = open_segment_;
      prev_open_segment.swap(open_segment_);
    }
    if (!open_segment_) {
      open_segment_ = std::make_shared<Segment>(path_, LastLogIndex() + 1);
      if (open_segment_->Create() != 0) {
        open_segment_ = nullptr;
      }
    }
  });
  if (prev_open_segment) {
    if (0 != prev_open_segment->Close(true)) {
      MRAFT_ERROR("Fail to close old open_segment or create new open_segment path:{}", path_);
      // Failed, revert former changes
      segments_lock_.withWLock([this, &prev_open_segment](auto&) {
        segments_.erase(prev_open_segment->FirstIndex());
        open_segment_.swap(prev_open_segment);
      });
      return nullptr;
    }
  }
  return open_segment_;
}

int SegmentLogStorage::SaveMeta(int64_t log_index) {
  std::string meta_file = path_ + "/";
  meta_file.append(kSegmentLogMeta);

  LogPBMeta meta;
  meta.set_first_log_index(log_index);
  int rc = pb_write_file(meta_file, meta);
  if (0 != rc) {
    return rc;
  } else {
    MRAFT_INFO("log save meta:{},first_log_index:{}", meta_file, log_index);
    return 0;
  }
}

int SegmentLogStorage::LoadMeta(bool& is_empty) {
  is_empty = false;
  std::string meta_file = path_ + "/";
  meta_file.append(kSegmentLogMeta);
  if (!folly::fs::is_regular_file(meta_file)) {
    MRAFT_WARN("Meta:{} is not exist.", meta_file);
    is_empty = true;
    return 0;
  }
  LogPBMeta meta;
  int rc = pb_read_file(meta_file, meta);
  if (0 == rc) {
    first_log_index_.store(meta.first_log_index());
    MRAFT_INFO("log load_meta:{}, first_log_index:{}", meta_file, meta.first_log_index());
    return 0;
  } else {
    return rc;
  }
}

void SegmentLogStorage::PopSegments(int64_t first_index_kept, std::vector<SegmentPtr>& poped) {
  poped.clear();
  poped.reserve(32);
  segments_lock_.withWLock([this, first_index_kept, &poped](auto&) {
    first_log_index_.store(first_index_kept, std::memory_order_release);
    for (SegmentMap::iterator it = segments_.begin(); it != segments_.end();) {
      SegmentPtr& segment = it->second;
      if (segment->LastIndex() < first_index_kept) {
        poped.push_back(segment);
        segments_.erase(it++);
      } else {
        return;
      }
    }
    if (open_segment_) {
      if (open_segment_->LastIndex() < first_index_kept) {
        poped.push_back(open_segment_);
        open_segment_ = nullptr;
        // _log_storage is empty
        last_log_index_.store(first_index_kept - 1);
      } else {
        // CHECK(open_segment_->first_index() <= first_index_kept);
      }
    } else {
      // _log_storage is empty
      last_log_index_.store(first_index_kept - 1);
    }
  });
}
void SegmentLogStorage::PopSegmentsFromBack(const int64_t last_index_kept, std::vector<SegmentPtr>& popped,
                                            SegmentPtr& last_segment) {
  popped.clear();
  popped.reserve(32);
  last_segment = nullptr;
  segments_lock_.withWLock([&](auto&) {
    last_log_index_.store(last_index_kept, std::memory_order_release);
    if (open_segment_) {
      if (open_segment_->FirstIndex() <= last_index_kept) {
        last_segment = open_segment_;
        return;
      }
      popped.push_back(open_segment_);
      open_segment_ = nullptr;
    }
    for (SegmentMap::reverse_iterator it = segments_.rbegin(); it != segments_.rend(); ++it) {
      if (it->second->FirstIndex() <= last_index_kept) {
        // Not return as we need to maintain _segments at the end of this
        // routine
        break;
      }
      popped.push_back(it->second);
      // XXX: C++03 not support erase reverse_iterator
    }
    for (size_t i = 0; i < popped.size(); i++) {
      segments_.erase(popped[i]->FirstIndex());
    }
    if (segments_.rbegin() != segments_.rend()) {
      last_segment = segments_.rbegin()->second;
    } else {
      // all the logs have been cleared, the we move _first_log_index to the
      // next index
      first_log_index_.store(last_index_kept + 1, std::memory_order_release);
    }
  });
}

int SegmentLogStorage::Init() {
  if (FLAGS_raft_max_segment_size < 0) {
    MRAFT_ERROR("FLAGS_raft_max_segment_size:{} must be greater than or equal to 0", FLAGS_raft_max_segment_size);
    return -1;
  }
  folly::fs::create_directories(path_);
  bool is_empty = false;
  if (0 != LoadMeta(is_empty)) {
    MRAFT_ERROR("Failed to load log meta.");
    return -1;
  }
  if (0 != ListSegments(is_empty)) {
    return -1;
  }
  if (is_empty) {
    first_log_index_.store(1);
    last_log_index_.store(0);
    return SaveMeta(1);
  }
  if (0 != LoadSegments()) {
    MRAFT_ERROR("Failed to load log segments.");
    return -1;
  }
  return 0;
}
SegmentLogStorage::SegmentPtr SegmentLogStorage::GetSegment(const int64_t index) {
  // BAIDU_SCOPED_LOCK(_mutex);
  SegmentPtr segment;
  segments_lock_.withWLock([this, &segment, index](auto&) {
    int64_t first_index = FirstLogIndex();
    int64_t last_index = LastLogIndex();
    if (first_index == last_index + 1) {
      return;
    }
    if (index < first_index || index > last_index + 1) {
      if (index > last_index) {
        MRAFT_WARN("Attempted to access entry:{} outside of log, first_log_index:{},last_log_index:{}", index,
                   first_index, last_index);
      }
      return;
    } else if (index == last_index + 1) {
      return;
    }

    if (open_segment_ && index >= open_segment_->FirstIndex()) {
      segment = open_segment_;

    } else {
      if (segments_.empty()) {
        return;
      }
      SegmentMap::iterator it = segments_.upper_bound(index);
      SegmentMap::iterator saved_it = it;
      --it;
      // CHECK(it != saved_it);
      segment = it->second;
    }
  });
  return segment;
}

// first log index in log
int64_t SegmentLogStorage::FirstLogIndex() { return first_log_index_.load(std::memory_order_acquire); }

// last log index in log
int64_t SegmentLogStorage::LastLogIndex() { return last_log_index_.load(std::memory_order_acquire); }

// get logentry by index
std::unique_ptr<LogEntry> SegmentLogStorage::GetEntry(const int64_t index) {
  if (latest_entry_ && latest_entry_->Index() == index) {
    return std::make_unique<LogEntry>(std::move(latest_entry_->Clone()));
  }
  if (!latest_entries_cache_.empty()) {
    int64_t first_cache_index = latest_entries_cache_[0].Index();
    int64_t last_cache_index = latest_entries_cache_[latest_entries_cache_.size() - 1].Index();
    if (index >= first_cache_index && index <= last_cache_index) {
      auto cache_entry = latest_entries_cache_[index - first_cache_index].Clone();

      return std::make_unique<LogEntry>(std::move(cache_entry));
    }
  }
  SegmentPtr ptr = GetSegment(index);
  if (!ptr) {
    return nullptr;
  }
  std::unique_ptr<LogEntry> entry = ptr->Get(index);
  return entry;
}

std::vector<std::unique_ptr<LogEntry>> SegmentLogStorage::GetBatchEntries(const int64_t index, uint32_t n) {
  std::vector<std::unique_ptr<LogEntry>> vals;
  for (uint32_t i = 0; i < n; i++) {
    std::unique_ptr<LogEntry> entry = GetEntry(index + i);
    if (!entry) {
      break;
    }
    vals.emplace_back(std::move(entry));
  }
  return vals;
}

// get logentry's term by index
int64_t SegmentLogStorage::GetTerm(const int64_t index) {
  SegmentPtr ptr = GetSegment(index);
  if (!ptr) {
    return 0;
  }
  return ptr->GetTerm(index);
}

// append entries to log
int SegmentLogStorage::AppendEntry(LogEntry& entry) {
  SegmentPtr segment = GetOpenSegment();
  if (!segment) {
    return EIO;
  }
  int ret = segment->Append(entry);
  if (ret != 0) {
    return ret;
  }
  latest_entries_cache_.emplace_back(std::move(entry.Clone()));
  latest_entry_ = std::make_unique<LogEntry>(std::move(entry.Clone()));
  last_log_index_.fetch_add(1, std::memory_order_release);
  return segment->Sync(true);
}

// delete logs from storage's head, [first_log_index, first_index_kept) will be discarded
int SegmentLogStorage::TruncatePrefix(const int64_t first_index_kept) {
  // segment files
  if (first_log_index_.load(std::memory_order_acquire) >= first_index_kept) {
    MRAFT_INFO("Nothing is going to happen since first_log_index:{} >= first_index_kept:{}", first_log_index_,
               first_index_kept);
    return 0;
  }
  // NOTE: truncate_prefix is not important, as it has nothing to do with
  // consensus. We try to save meta on the disk first to make sure even if
  // the deleting fails or the process crashes (which is unlikely to happen).
  // The new process would see the latest `first_log_index'
  if (SaveMeta(first_index_kept) != 0) {  // NOTE
    int err = errno;
    MRAFT_ERROR("Fail to save meta, path:{}, error:{}", path_, folly::errnoStr(err));
    return -1;
  }
  std::vector<SegmentPtr> popped;
  PopSegments(first_index_kept, popped);
  for (size_t i = 0; i < popped.size(); ++i) {
    popped[i]->Unlink();
    popped[i] = nullptr;
  }

  while (!latest_entries_cache_.empty()) {
    if (latest_entries_cache_[0].Index() < first_index_kept) {
      latest_entries_cache_.pop_front();
    }
  }
  return 0;
}

// delete uncommitted logs from storage's tail, (last_index_kept, last_log_index] will be discarded
int SegmentLogStorage::TruncateSuffix(const int64_t last_index_kept) {
  // segment files
  std::vector<SegmentPtr> popped;
  SegmentPtr last_segment;
  PopSegmentsFromBack(last_index_kept, popped, last_segment);
  bool truncate_last_segment = false;
  int ret = -1;

  if (last_segment) {
    if (first_log_index_.load(std::memory_order_relaxed) <= last_log_index_.load(std::memory_order_relaxed)) {
      truncate_last_segment = true;
    } else {
      // trucate_prefix() and truncate_suffix() to discard entire logs
      popped.push_back(last_segment);
      segments_lock_.withWLock([&](auto&) {
        segments_.erase(last_segment->FirstIndex());
        if (open_segment_) {
          // CHECK(_open_segment.get() == last_segment.get());
          open_segment_ = nullptr;
        }
      });
    }
  }

  // The truncate suffix order is crucial to satisfy log matching property of raft
  // log must be truncated from back to front.
  for (size_t i = 0; i < popped.size(); ++i) {
    ret = popped[i]->Unlink();
    if (ret != 0) {
      return ret;
    }
    popped[i] = nullptr;
  }
  if (truncate_last_segment) {
    bool closed = !last_segment->IsOpen();
    ret = last_segment->Truncate(last_index_kept);
    if (ret == 0 && closed && last_segment->IsOpen()) {
      // CHECK(!_open_segment);
      segments_lock_.withWLock([&](auto&) {
        segments_.erase(last_segment->FirstIndex());
        open_segment_.swap(last_segment);
      });
    }
  }

  while (!latest_entries_cache_.empty()) {
    if (latest_entries_cache_[latest_entries_cache_.size() - 1].Index() > last_index_kept) {
      latest_entries_cache_.pop_back();
    }
  }

  return ret;
}

int SegmentLogStorage::ClearCacheEntry(const int64_t index) {
  if (latest_entries_cache_.empty()) {
    MRAFT_ERROR("Empty cache to clear cache entry:{}", index);
    return -1;
  }
  if (latest_entries_cache_[0].Index() != index) {
    MRAFT_ERROR("First cache entry has index:{}, while expected:{}", latest_entries_cache_[0].Index(), index);
    return -1;
  }
  MRAFT_DEBUG("Remove cache entry:{}", index);
  latest_entries_cache_.pop_front();
  return 0;
}

// Drop all the existing logs and reset next log index to |next_log_index|.
// This function is called after installing snapshot from leader
int SegmentLogStorage::Reset(const int64_t next_log_index, int64_t term) {}
}  // namespace mraft
