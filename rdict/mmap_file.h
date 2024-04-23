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

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"

namespace rdict {
class MmapFile {
 public:
  static constexpr size_t kSegmentSize = 64 * 1024 * 1024;
  struct Options {
    std::string path;
    size_t reserved_space_bytes = 100 * 1024 * 1024 * 1024LL;
    bool readonly = false;
  };
  static absl::StatusOr<std::unique_ptr<MmapFile>> Open(const Options& opts);

  absl::StatusOr<size_t> Add(const void* data, size_t len);
  uint8_t* GetRawData() { return data_; }
  const uint8_t* GetRawData() const { return data_; }
  uint64_t GetWriteOffset() const { return write_offset_; }
  bool Writable() const { return !readonly_; }
  absl::StatusOr<size_t> ShrinkToFit();

  ~MmapFile();

 private:
  MmapFile();
  absl::Status Init(const Options& opts);
  absl::Status ExtendBuffer(size_t len);
  Options opts_;
  uint8_t* data_ = nullptr;
  size_t capacity_ = 0;
  size_t write_offset_ = 0;
  size_t reserved_space_bytes_ = 0;
  bool readonly_ = false;
};
}  // namespace rdict