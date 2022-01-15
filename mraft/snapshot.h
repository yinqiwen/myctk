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

#pragma once
#include <stdint.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "folly/File.h"
#include "mraft.pb.h"

namespace mraft {

class Snapshot {
 public:
  Snapshot(const std::string& path, int64_t index);
  ~Snapshot();

  int Open(const SnapshotMeta* meta = nullptr);
  int Remove();

  const std::string& GetPath() const;
  int64_t GetIndex() const;
  const SnapshotMeta& GetMeta() const;
  int Add(const std::string& file_name);
  bool Exists(const std::string& file_name) const;
  int Write(int64_t offset, const void* data, size_t len);
  int Read(int64_t offset, void*& chunk, int64_t& count, bool& last_chunk);
  int64_t Size() const;

 private:
  void Close();

  int SyncMeta();
  int GetFileByOffset(int64_t offset, int& file_idx, int64_t& file_start_offset);

  std::string path_;
  int64_t index_;
  SnapshotMeta meta_;
  std::vector<std::unique_ptr<folly::File>> files_;
  std::vector<char*> file_mapbufs_;
  std::mutex read_mutex_;
};

}  // namespace mraft