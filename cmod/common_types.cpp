/*
 *Copyright (c) 2019, qiyingwang <qiyingwang@tencent.com>
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

#include "common_types.hpp"
#include <memory>
namespace cmod {
inline static std::string b64_encode(const char *s, size_t len) {
  typedef unsigned char u1;
  static const char lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const u1 *data = (const u1 *)s;
  std::string r;
  r.reserve(len * 4 / 3 + 3);
  for (size_t i = 0; i < len; i += 3) {
    unsigned n = data[i] << 16;
    if (i + 1 < len) n |= data[i + 1] << 8;
    if (i + 2 < len) n |= data[i + 2];

    u1 n0 = (u1)(n >> 18) & 0x3f;
    u1 n1 = (u1)(n >> 12) & 0x3f;
    u1 n2 = (u1)(n >> 6) & 0x3f;
    u1 n3 = (u1)(n)&0x3f;

    r.push_back(lookup[n0]);
    r.push_back(lookup[n1]);
    if (i + 1 < len) r.push_back(lookup[n2]);
    if (i + 2 < len) r.push_back(lookup[n3]);
  }
  for (size_t i = 0; i < (3 - len % 3) % 3; i++) r.push_back('=');
  return r;
}

inline static std::string b64_decode(const char *s, size_t len) {
  typedef unsigned char u1;
  static const char lookup[] =
      ""
      "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80"  // 0x00
      "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80"  // 0x10
      "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x3e\x80\x80\x80\x3f"  // 0x20
      "\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x80\x80\x80\x00\x80\x80"  // 0x30
      "\x80\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"  // 0x40
      "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x80\x80\x80\x80\x80"  // 0x50
      "\x80\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28"  // 0x60
      "\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\x80\x80\x80\x80\x80"  // 0x70
      "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80"  // 0x80
      "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80"  // 0x90
      "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80"  // 0xa0
      "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80"  // 0xb0
      "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80"  // 0xc0
      "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80"  // 0xd0
      "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80"  // 0xe0
      "\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80"  // 0xf0
      "";
  std::string r;
  if (len % 4) throw std::runtime_error("Invalid base64 data size");
  size_t pad = 0;
  if (s[len - 1] == '=') pad++;
  if (s[len - 2] == '=') pad++;

  r.reserve(len * 3 / 4 + 3);
  for (size_t i = 0; i < len; i += 4) {
    u1 n0 = lookup[(u1)s[i + 0]];
    u1 n1 = lookup[(u1)s[i + 1]];
    u1 n2 = lookup[(u1)s[i + 2]];
    u1 n3 = lookup[(u1)s[i + 3]];
    if (0x80 & (n0 | n1 | n2 | n3)) throw std::runtime_error("Invalid hex data ");
    unsigned n = (n0 << 18) | (n1 << 12) | (n2 << 6) | n3;
    r.push_back((n >> 16) & 0xff);
    if (s[i + 2] != '=') r.push_back((n >> 8) & 0xff);
    if (s[i + 3] != '=') r.push_back((n)&0xff);
  }
  return r;
}
void Base64Value::Copy(const Base64Value &src) { assign(src.data(), src.size()); }
bool Base64Value::PraseFromJson(const rapidjson::Value &json) {
  if (!json.IsString()) {
    return false;
  }
  std::string tmp2 = b64_decode(json.GetString(), json.GetStringLength());
  this->assign(tmp2.data(), tmp2.size());
  return true;
}
void Base64Value::WriteToJson(rapidjson::Value &json, rapidjson::Value::AllocatorType &allocator,
                              bool ignore_default) const {
  std::string tmp = b64_encode(data(), size());
  json.SetString(tmp.data(), tmp.size());
}
}  // namespace cmod
