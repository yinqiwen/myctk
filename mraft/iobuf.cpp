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
#include "mraft/iobuf.h"
#include <algorithm>

#include "mraft/iobuf.h"

namespace mraft {
IOBufAsZeroCopyOutputStream::IOBufAsZeroCopyOutputStream(uint32_t block_size)
    : current_buf_(nullptr), block_size_(block_size), byte_count_(0) {
  buf_ = folly::IOBuf::create(block_size);
  current_buf_ = buf_.get();
}

IOBufAsZeroCopyOutputStream::IOBufAsZeroCopyOutputStream(std::unique_ptr<folly::IOBuf>&& buf)
    : buf_(std::move(buf)), current_buf_(buf_.get()), block_size_(buf_->capacity()), byte_count_(0) {
  if (0 == block_size_) {
    block_size_ = 4096;
  }
}

bool IOBufAsZeroCopyOutputStream::Next(void** data, int* size) {
  if (current_buf_->tailroom() == 0) {
    auto new_buf = folly::IOBuf::create(block_size_);
    current_buf_->appendToChain(std::move(new_buf));
    current_buf_ = current_buf_->next();
  }
  int64_t n = current_buf_->tailroom();
  *size = current_buf_->tailroom();
  *data = current_buf_->writableData();
  current_buf_->append(current_buf_->tailroom());
  byte_count_ += n;
  return true;
}

void IOBufAsZeroCopyOutputStream::BackUp(int count) {
  byte_count_ -= count;
  while (count > 0) {
    if (count <= current_buf_->length()) {
      current_buf_->trimEnd(count);
      count = 0;
      if (current_buf_->isChained() && current_buf_->length() == 0) {
        folly::IOBuf* prev = current_buf_->prev();
        current_buf_->unlink();
        current_buf_ = prev;
      }
    } else {
      count -= current_buf_->length();
      folly::IOBuf* prev = current_buf_->prev();
      if (nullptr != prev) {
        current_buf_->unlink();
        current_buf_ = prev;
      } else {
        current_buf_->trimEnd(current_buf_->length());
        break;
      }
    }
  }
}

google::protobuf::int64 IOBufAsZeroCopyOutputStream::ByteCount() const { return byte_count_; }

IOBufAsZeroCopyInputStream::IOBufAsZeroCopyInputStream(folly::IOBuf* buf)
    : buf_(buf), current_buf_(buf), byte_count_(0) {}

bool IOBufAsZeroCopyInputStream::Next(const void** data, int* size) {
  while (current_buf_->length() == 0) {
    if (current_buf_->isChained()) {
      auto next = current_buf_->next();
      if (nullptr == next) {
        return false;
      }
      current_buf_ = next;
    } else {
      return false;
    }
  }
  int64_t n = current_buf_->length();
  *data = current_buf_->data();
  *size = n;
  current_buf_->trimEnd(n);
  byte_count_ += n;
  return true;
}

void IOBufAsZeroCopyInputStream::BackUp(int count) {
  while (count > 0) {
    if (count <= current_buf_->tailroom()) {
      current_buf_->append(count);
      count = 0;
    } else {
      count -= current_buf_->tailroom();
      current_buf_->append(current_buf_->tailroom());
      current_buf_ = current_buf_->prev();
    }
  }
}
google::protobuf::int64 IOBufAsZeroCopyInputStream::ByteCount() const { return byte_count_; }
bool IOBufAsZeroCopyInputStream::Skip(int count) {
  while (count > 0) {
    if (count <= current_buf_->length()) {
      current_buf_->trimStart(count);
      count = 0;
    } else {
      count -= current_buf_->length();
      current_buf_->trimStart(current_buf_->length());
      auto next = current_buf_->next();
      if (nullptr == next) {
        return false;
      }
      current_buf_ = next;
    }
  }
  return true;
}

}  // namespace mraft
