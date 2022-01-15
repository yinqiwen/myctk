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

#include "folly/io/IOBuf.h"
#include "google/protobuf/io/zero_copy_stream.h"

namespace mraft {
class IOBufAsZeroCopyOutputStream : public google::protobuf::io::ZeroCopyOutputStream {
 public:
  IOBufAsZeroCopyOutputStream(uint32_t block_size = 4096);
  IOBufAsZeroCopyOutputStream(std::unique_ptr<folly::IOBuf>&& buf);
  ~IOBufAsZeroCopyOutputStream() = default;

  bool Next(void** data, int* size) override;
  void BackUp(int count) override;  // `count' can be as long as ByteCount()
  google::protobuf::int64 ByteCount() const override;

  std::unique_ptr<folly::IOBuf>& GetBuf() { return buf_; }

 private:
  std::unique_ptr<folly::IOBuf> buf_;
  folly::IOBuf* current_buf_;
  uint32_t block_size_;
  google::protobuf::int64 byte_count_;
};

class IOBufAsZeroCopyInputStream : public google::protobuf::io::ZeroCopyInputStream {
 public:
  IOBufAsZeroCopyInputStream(folly::IOBuf* buf);
  ~IOBufAsZeroCopyInputStream() = default;

  bool Next(const void** data, int* size) override;
  void BackUp(int count) override;  // `count' can be as long as ByteCount()
  google::protobuf::int64 ByteCount() const override;
  bool Skip(int count) override;

 private:
  folly::IOBuf* buf_;
  folly::IOBuf* current_buf_;
  google::protobuf::int64 byte_count_;
};
}  // namespace mraft