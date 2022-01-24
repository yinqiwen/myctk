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
#include <string>
#include <string_view>
#include "folly/Bits.h"
#include "google/protobuf/message.h"

namespace mraft {
template <typename T>
struct LocalInt {
  static T ToLocalValue(T v) {
    if constexpr (folly::Endian::order == folly::Endian::Order::LITTLE) {
      return folly::Endian::big(v);
    } else {
      return v;
    }
  }
  static T Read(const void* b) {
    T v;
    memcpy(&v, b, sizeof(v));
    return ToLocalValue(v);
  }
  static void Write(void* b, T v) {
    if constexpr (folly::Endian::order == folly::Endian::Order::LITTLE) {
      v = folly::Endian::big(v);
      memcpy(b, &v, sizeof(v));
    } else {
      memcpy(b, &v, sizeof(v));
    }
  }
};

bool has_prefix(const std::string_view& str, const std::string_view& prefix);
bool has_suffix(const std::string_view& fullString, const std::string_view& ending);
int pb_write_file(const std::string& file, const ::google::protobuf::Message& msg);
int pb_write_fd(int fd, const ::google::protobuf::Message& msg);
int pb_read_file(const std::string& file, ::google::protobuf::Message& msg);
int pb_read_fd(int fd, ::google::protobuf::Message& msg);

}  // namespace mraft