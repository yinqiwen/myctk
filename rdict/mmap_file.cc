/*
** BSD 3-Clause License
**
** Copyright (c) 2023, qiyingwang <qiyingwang@tencent.com>, the respective contributors, as shown by the AUTHORS file.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
** * Redistributions of source code must retain the above copyright notice, this
** list of conditions and the following disclaimer.
**
** * Redistributions in binary form must reproduce the above copyright notice,
** this list of conditions and the following disclaimer in the documentation
** and/or other materials provided with the distribution.
**
** * Neither the name of the copyright holder nor the names of its
** contributors may be used to endorse or promote products derived from
** this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "rdict/mmap_file.h"
#include <sys/mman.h>
#include "folly/File.h"
#include "folly/FileUtil.h"

namespace rdict {
absl::StatusOr<std::unique_ptr<MmapFile>> MmapFile::Open(const Options& opts) {
  std::unique_ptr<MmapFile> p(new MmapFile);
  auto status = p->Init(opts);
  if (!status.ok()) {
    return status;
  }
  return absl::StatusOr<std::unique_ptr<MmapFile>>(std::move(p));
}
MmapFile::MmapFile() {}

absl::Status MmapFile::Init(const Options& opts) {
  opts_ = opts;
  std::unique_ptr<folly::File> segment_file;
  size_t file_size = 0;
  try {
    int file_flags = 0;
    if (!opts.readonly) {
      file_flags = O_RDWR | O_CREAT | O_CLOEXEC;
    } else {
      file_flags = O_RDONLY | O_CLOEXEC;
    }
    int mode = 0644;
    segment_file = std::make_unique<folly::File>(opts.path, file_flags, mode);
    struct stat st;
    int rc = fstat(segment_file->fd(), &st);
    if (rc != 0) {
      int err = errno;
      return absl::ErrnoToStatus(err, "fstat failed for  file path:" + opts.path);
    }
    file_size = st.st_size;
    write_offset_ = file_size;
    if (!opts.readonly) {
      if (file_size < kSegmentSize) {
        file_size = kSegmentSize;
      } else {
        size_t rest = file_size % kSegmentSize;
        if (rest > 0) {
          file_size += (kSegmentSize - rest);
        }
      }
      int rc = ftruncate(segment_file->fd(), file_size);
      if (rc != 0) {
        int err = errno;
        return absl::ErrnoToStatus(err, "truncate failed for  file path:" + opts.path);
      }
    }
  } catch (...) {
    return absl::InvalidArgumentError("invalid segment file path:" + opts.path);
  }
  capacity_ = file_size;
  size_t reserved_space_bytes = file_size;
  if (!opts.readonly) {
    reserved_space_bytes = opts.reserved_space_bytes;
    if (reserved_space_bytes < file_size) {
      reserved_space_bytes = file_size;
    }
  }
  opts_.reserved_space_bytes = reserved_space_bytes;
  void* reserved_addr_space = mmap(nullptr, reserved_space_bytes, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (reserved_addr_space == MAP_FAILED) {
    return absl::ErrnoToStatus(errno, "create space");
  }
  int mmap_flags = 0;
  if (opts.readonly) {
    mmap_flags = MAP_PRIVATE | MAP_FILE;
  } else {
    mmap_flags = MAP_SHARED | MAP_FILE;
  }
  void* mapping_addr = mmap(reserved_addr_space, file_size, PROT_READ | PROT_WRITE, mmap_flags, segment_file->fd(), 0);
  if (mapping_addr == MAP_FAILED) {
    return absl::ErrnoToStatus(errno, "mmap file failed");
  }
  data_ = reinterpret_cast<uint8_t*>(mapping_addr);
  return absl::OkStatus();
}

absl::Status MmapFile::ExtendBuffer(size_t len) {
  size_t extend_len = (len + kSegmentSize) / kSegmentSize * kSegmentSize;
  size_t new_file_len = capacity_ + extend_len;
  if (new_file_len > opts_.reserved_space_bytes) {
    return absl::InvalidArgumentError("Too large data to fit in reserved space.");
  }
  std::unique_ptr<folly::File> mmap_file;
  try {
    int file_flags = O_RDWR | O_CREAT | O_CLOEXEC;
    int mode = 0644;
    mmap_file = std::make_unique<folly::File>(opts_.path, file_flags, mode);
    struct stat st;
    int rc = fstat(mmap_file->fd(), &st);
    if (rc != 0) {
      int err = errno;
      return absl::ErrnoToStatus(err, "fstat failed for file path:" + opts_.path);
    }
    size_t file_size = st.st_size;
    if (file_size != capacity_) {
      return absl::InvalidArgumentError("invalid local mmap file path:" + opts_.path + " with invalid length");
    }
    rc = ftruncate(mmap_file->fd(), new_file_len);
    if (rc != 0) {
      int err = errno;
      return absl::ErrnoToStatus(err, "truncate failed for file path:" + opts_.path);
    }
  } catch (...) {
    return absl::InvalidArgumentError("invalid mmap file path:" + opts_.path);
  }
  void* mmap_start_addr = data_ + capacity_;
  int mmap_flags = MAP_SHARED | MAP_FILE | MAP_FIXED;
  void* mapping_addr =
      mmap(mmap_start_addr, extend_len, PROT_READ | PROT_WRITE, mmap_flags, mmap_file->fd(), capacity_);
  if (mapping_addr == MAP_FAILED) {
    return absl::ErrnoToStatus(errno, "mmap file failed");
  }
  capacity_ = new_file_len;
  return absl::OkStatus();
}

absl::StatusOr<size_t> MmapFile::Add(const void* data, size_t len) {
  if (readonly_) {
    return absl::PermissionDeniedError("unable to write readonly data");
  }
  if ((write_offset_ + len) > capacity_) {
    auto status = ExtendBuffer(write_offset_ + len - capacity_);
    if (!status.ok()) {
      return status;
    }
  }
  memcpy(data_ + write_offset_, data, len);
  size_t data_offset = write_offset_;
  write_offset_ += len;
  return data_offset;
}

absl::StatusOr<size_t> MmapFile::ShrinkToFit() {
  readonly_ = true;
  if (nullptr != data_) {
    msync(data_, write_offset_, 0);
  }
  int rc = folly::truncateNoInt(opts_.path.c_str(), write_offset_);
  if (0 != rc) {
    return absl::ErrnoToStatus(errno, "shrink to fit file truncate failed");
  }

  return write_offset_;
}

MmapFile::~MmapFile() {
  if (nullptr != data_) {
    munmap(data_, capacity_);
    data_ = nullptr;
  }
}
}  // namespace rdict