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
#include "didagle/graph_data.h"
#include <string>

namespace didagle {
bool GraphParams::ParseFromToml(const kcfg::TomlValue& doc) {
  if (doc.is_table()) {
    for (const auto& kv : doc.as_table()) {
      const std::string& name = kv.first;
      const auto& value = kv.second;
      GraphParams tmp;
      if (tmp.ParseFromToml(value)) {
        params[name] = tmp;
      } else {
        return false;
      }
    }
    invalid = false;
    _param_type = PARAM_OBJECT;
  } else if (doc.is_integer()) {
    iv = doc.as_integer();
    str = std::to_string(iv);
    invalid = false;
    _param_type = PARAM_INT;
  } else if (doc.is_boolean()) {
    bv = doc.as_boolean();
    str = std::to_string(bv);
    invalid = false;
    _param_type = PARAM_BOOL;
  } else if (doc.is_floating()) {
    dv = doc.as_floating();
    str = std::to_string(dv);
    invalid = false;
    _param_type = PARAM_DOUBLE;
  } else if (doc.is_string()) {
    std::string tmp = doc.as_string();
    str = tmp;
    invalid = false;
    _param_type = PARAM_STRING;
  } else if (doc.is_array()) {
    for (const auto& item : doc.as_array()) {
      GraphParams tmp;
      if (tmp.ParseFromToml(item)) {
        param_array.push_back(tmp);
      } else {
        // v.clear();
        return false;
      }
    }
    _param_type = PARAM_ARRAY;
    invalid = false;
  } else {
    return false;
  }
  invalid = false;
  return true;
}
}  // namespace didagle
