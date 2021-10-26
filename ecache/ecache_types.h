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
#include <folly/Range.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "ecache.pb.h"
#include "flatbuffers/flatbuffers.h"
#include "folly/FBString.h"
#include "folly/FBVector.h"
#include "folly/lang/Bits.h"
namespace ecache {

enum InsertOrReplaceResult {
  kInserted,
  kReplaced,
};
enum RemoveRes {
  kSuccess,
  kNotFoundInRam,
};
struct CacheValue {
  std::shared_ptr<void> _handle;
  folly::StringPiece field_view;
  folly::StringPiece value_view;

  template <typename T>
  const T* GetAs() const noexcept {
    return reinterpret_cast<const T*>(value_view.data());
  }
  template <typename T>
  const T* GetAsFlatBuffers() const noexcept {
    return flatbuffers::GetRoot<T>(value_view.data());
  }

  template <typename T>
  std::unique_ptr<T> GetAsPB() const noexcept {
    std::unique_ptr<T> p = std::make_unique<T>();
    if (!p->ParseFromArray(value_view.data(), value_view.size())) {
      return nullptr;
    }
    return p;
  }

  // Cast item's writable memory to a writable user type
  template <typename T>
  T* GetWritableAs() noexcept {
    return reinterpret_cast<T*>(const_cast<char*>(value_view.data()));
  }

  template <typename T>
  T GetField() {
    T t;
    t.Decode(field_view);
    return t;
  }
};

struct MapValue;
struct MapValueDeleter {
  void operator()(MapValue* ptr) const;
};

struct MapValue {
  typedef std::unique_ptr<MapValue, MapValueDeleter> MapValuePtr;
  uint32_t length = 0;
  char _data[0];
  uint8_t* data() { return reinterpret_cast<std::uint8_t*>(this + 1); }
  uint32_t getStorageSize() const { return length + sizeof(MapValue); }

  static MapValuePtr fromString(std::string_view data) {
    MapValue* buf = (MapValue*)malloc(data.size() + sizeof(MapValue));
    buf->length = data.size();
    memcpy(buf->data(), data.data(), data.size());
    MapValuePtr ptr(buf);
    return ptr;
  }
};

template <typename T>
struct MapFieldCodec {
  folly::fbstring Encode(const T& field) {
    static_assert(sizeof(T) == std::size_t(-1),
                  "Need to speciliaze 'MapFieldCodec' for given struct type.");
    folly::fbstring s;
    return s;
  }
  bool Decode(std::string_view data, T& field) {
    static_assert(sizeof(T) == std::size_t(-1),
                  "Need to speciliaze 'MapFieldCodec' for given struct type.");
    return false;
  }
};

folly::fbstring getActualKey(uint8_t pool_id, CacheKeyType type, std::string_view key,
                             size_t field_size = 0);

template <typename T>
void field_encode_int(folly::fbstring& s, T v) {
  T vv = folly::Endian::big(v);
  s.append((const char*)(&vv), sizeof(T));
}

template <typename T>
bool field_decode_int(folly::StringPiece s, size_t pos, T& v) {
  if (pos + sizeof(T) > s.size()) {
    return false;
  }
  memcpy(&v, s.data() + pos, sizeof(T));
  v = folly::Endian::big(v);
  return true;
}

}  // namespace ecache