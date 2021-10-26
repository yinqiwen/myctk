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

#include "ecache_types.h"

namespace ecache {
void MapValueDeleter::operator()(MapValue* ptr) const {
  if (nullptr != ptr) {
    free(ptr);
  }
}
folly::fbstring getActualKey(uint8_t pool_id, CacheKeyType type, std::string_view key,
                             size_t field_size) {
  folly::fbstring s;
  s.push_back((char)pool_id);
  switch (type) {
    case CACHE_KEY_STRING:
    case CACHE_KEY_RANGE_MAP:
    case CACHE_KEY_HASH_MAP: {
      break;
    }
    default: {
      return s;
    }
  }
  s.push_back((char)type);
  if (CACHE_KEY_RANGE_MAP == type || CACHE_KEY_HASH_MAP == type) {
    if (field_size > UINT16_MAX) {
      throw std::invalid_argument("Invalid field size.");
    }
    uint16_t n = (uint16_t)field_size;
    s.append((const char*)&n, 2);
  }
  s.append(key.data(), key.size());
  return s;
}
}  // namespace ecache