/*
 *Copyright (c) 2022, qiyingwang <qiyingwang@tencent.com>
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
#include "mraft/snapshot.h"
#include <bits/types/FILE.h>
#include <sys/mman.h>
#include <algorithm>
#include <memory>
#include <string>

#include "folly/FileUtil.h"
#include "folly/String.h"
#include "folly/portability/Filesystem.h"
#include "mraft/logger.h"
#include "mraft/utils.h"

namespace mraft {
static constexpr std::string_view kSnapshotMeta = "snapshot_meta";

int SnapshotWritableFile::GetFD() {
  if (!file_) {
    return -1;
  }
  return file_->fd();
}
void SnapshotWritableFile::Close() {
  if (!file_) {
    return;
  }
  file_->close();
}
SnapshotWritableFile::~SnapshotWritableFile() {
  if (snapshot_) {
    snapshot_->Add(name_);
  }
}

Snapshot::Snapshot(const std::string& path, int64_t index) : index_(index) {
  path_ = path + "/" + std::to_string(index_);
  folly::fs::create_directories(path_);
}
Snapshot::~Snapshot() { Close(); }

void Snapshot::Close() {
  for (size_t i = 0; i < file_mapbufs_.size(); i++) {
    if (nullptr != file_mapbufs_[i]) {
      munmap(file_mapbufs_[i], meta_.files(i).size());
    }
  }
  files_.clear();
}

int Snapshot::Remove() {
  Close();
  folly::fs::remove_all(path_);
}

int Snapshot::Open(const SnapshotMeta* meta) {
  if (nullptr != meta) {
    meta_.CopyFrom(*meta);
    int rc = SyncMeta();
    if (0 != rc) {
      return rc;
    }
  } else {
    std::string snapshot_meta_file = path_ + "/";
    snapshot_meta_file.append(kSnapshotMeta);
    if (folly::fs::is_regular_file(snapshot_meta_file)) {
      int rc = pb_read_file(snapshot_meta_file, meta_);
      if (0 != rc) {
        MRAFT_ERROR("Snapshot meta:{} failed to read.", snapshot_meta_file);
        return rc;
      }
    }
  }
  if (meta_.files_size() == 0) {
    MRAFT_ERROR("Empty files in meta to open snapshot:{}", path_);
    return -1;
  }

  for (const auto& file_meta : meta_.files()) {
    std::string file_path = path_ + "/" + file_meta.name();
    try {
      std::unique_ptr<folly::File> file;
      if (folly::fs::is_regular_file(file_path)) {
        file = std::make_unique<folly::File>(file_path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
      } else {
        file = std::make_unique<folly::File>(file_path, O_RDWR);
      }
      files_.emplace_back(std::move(file));
      MRAFT_INFO("Create snapshor:{} success", file_path);
    } catch (const std::exception& ex) {
      MRAFT_ERROR("Created snapshot:{} error:{}", file_path, ex.what());
      files_.emplace_back(nullptr);
    }
  }
  return 0;
}

int Snapshot::SyncMeta() {
  std::string snapshot_meta_file = path_ + "/";
  snapshot_meta_file.append(kSnapshotMeta);
  int rc = pb_write_file(snapshot_meta_file, meta_);
  if (0 != rc) {
    MRAFT_ERROR("Failed to sync snapshot:{} meta with rc:{}", path_, rc);
  }
  return rc;
}
const SnapshotMeta& Snapshot::GetMeta() const { return meta_; }
const std::string& Snapshot::GetPath() const { return path_; }
int64_t Snapshot::GetIndex() const { return index_; }

int64_t Snapshot::Size() const {
  int64_t n = 0;
  for (const auto& file : meta_.files()) {
    n += file.size();
  }
  return n;
}

bool Snapshot::Exists(const std::string& file_name) const {
  for (const auto& file : meta_.files()) {
    if (file.name() == file_name) {
      return true;
    }
  }
  return false;
}

std::unique_ptr<folly::File> Snapshot::GetReadableFile(const std::string& file_name) const {
  std::unique_ptr<folly::File> file;
  std::string file_path = path_ + "/" + file_name;
  try {
    file = std::make_unique<folly::File>(file_path, O_RDWR);
  } catch (const std::runtime_error& e) {
    MRAFT_ERROR("Failed to open file:{} to read with reason:{}", file_path, e.what());
    return nullptr;
  }
  return file;
}

std::unique_ptr<SnapshotWritableFile> Snapshot::GetWritableFile(const std::string& file_name) {
  std::unique_ptr<folly::File> file;
  std::string file_path = path_ + "/" + file_name;
  try {
    file = std::make_unique<folly::File>(file_path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  } catch (const std::runtime_error& e) {
    MRAFT_ERROR("Failed to open file:{} to write with reason:{}", file_path, e.what());
    return nullptr;
  }
  return std::make_unique<SnapshotWritableFile>(this, file_name, std::move(file));
}

int Snapshot::Add(const std::string& file_name) {
  std::unique_ptr<folly::File> file;
  std::string file_path = path_ + "/" + file_name;
  try {
    file = std::make_unique<folly::File>(file_path, O_RDWR);
  } catch (...) {
    // MRAFT_ERROR("Failed to open file:{} with msg:{}", file_path, ex.what());
    MRAFT_ERROR("Failed to open file:{}", file_path);
    return -1;
  }
  // get file size
  struct stat st_buf;
  if (fstat(file->fd(), &st_buf) != 0) {
    int save_err = errno;
    MRAFT_ERROR("Failed to get file stat:{} with error:{}", file_path, folly::errnoStr(save_err));
    return -1;
  }
  int64_t file_size = st_buf.st_size;
  SnapshotFileMeta* file_meta = meta_.add_files();
  file_meta->set_name(file_name);
  file_meta->set_size(file_size);
  return SyncMeta();
}
int Snapshot::GetFileByOffset(int64_t offset, int& file_idx, int64_t& file_start_offset) {
  file_idx = -1;
  file_start_offset = 0;

  int64_t file_total_size = 0;
  for (int i = 0; i < meta_.files_size(); i++) {
    file_start_offset = file_total_size;
    file_total_size += meta_.files(i).size();
    if (offset < file_total_size) {
      file_idx = i;
      break;
    }
  }
  if (file_idx < 0) {
    return -1;
  }
  return 0;
}

int Snapshot::Write(int64_t offset, const void* data, size_t len) {
  int file_idx = -1;
  int64_t file_start_offset = 0;
  if (GetFileByOffset(offset, file_idx, file_start_offset) < 0) {
    return -1;
  }
  if (!files_[file_idx]) {
    MRAFT_ERROR("Empty file for file:{} in path:{}", meta_.files(file_idx).name(), path_);
    return -1;
  }
  ssize_t n = folly::pwriteFull(files_[file_idx]->fd(), data, len, offset - file_start_offset);
  if (n != len) {
    int err = errno;
    MRAFT_ERROR("Fail to write to file={}, path:{}, error:{}", meta_.files(file_idx).name(), path_,
                folly::errnoStr(err));
    return err;
  }

  return 0;
}
int Snapshot::Read(int64_t offset, void*& chunk, int64_t& read_count, bool& last_chunk) {
  read_count = 0;
  chunk = nullptr;
  last_chunk = false;
  int file_idx = -1;
  int64_t file_start_offset = 0;
  if (GetFileByOffset(offset, file_idx, file_start_offset) < 0) {
    return -1;
  }
  const int64_t MAX_CHUNK_SIZE = 32 * 1024;
  read_count = std::min(MAX_CHUNK_SIZE, meta_.files(file_idx).size() - (offset - file_start_offset));
  {
    std::string file_path = path_ + "/" + meta_.files(file_idx).name();
    std::lock_guard<std::mutex> guard(read_mutex_);
    if (file_mapbufs_.size() != meta_.files_size()) {
      file_mapbufs_.resize(meta_.files_size());
    }
    if (!file_mapbufs_[file_idx]) {
      int mode = O_RDONLY, permission = S_IRUSR;
      int mmap_mode = PROT_READ;
      int fd = open(file_path.c_str(), mode, permission);
      if (fd < 0) {
        int err = errno;
        MRAFT_ERROR("Fail to open file={}, error:{}", file_path, folly::errnoStr(err));
        return -1;
      }
      char* mapbuf = (char*)mmap(nullptr, meta_.files(file_idx).size(), mmap_mode, MAP_SHARED, fd, 0);
      if (mapbuf == MAP_FAILED) {
        int err = errno;
        MRAFT_ERROR("Fail to mmap file={}, error:{}", file_path, folly::errnoStr(err));
        close(fd);
        return -1;
      }
      close(fd);
      file_mapbufs_[file_idx] = mapbuf;
    }
  }
  chunk = file_mapbufs_[file_idx] + (offset - file_start_offset);
  // MRAFT_ERROR("{} {} {}", file_idx, read_count, meta_.files(file_idx).size());
  if (file_idx == (files_.size() - 1) && (offset + read_count) == Size()) {
    last_chunk = true;
  }
  return 0;
}
}  // namespace mraft
