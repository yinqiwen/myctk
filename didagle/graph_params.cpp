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
#include "graph_params.h"
#include "kcfg_toml.h"

namespace didagle {

Params::Params(bool invalid_) : iv(0), dv(0), bv(false), invalid(invalid_), parent(nullptr) {}
void Params::SetParent(const Params* p) {
  if (nullptr == p) {
    parent = nullptr;
    return;
  }
  if (this == p) {
    return;
  }
  if (nullptr == parent) {
    parent = p;
  } else {
    if (parent == p) {
      return;
    }
    const_cast<Params*>(parent)->SetParent(p);
  }
}
bool Params::Valid() const { return !invalid; }
const std::string& Params::String() const { return str; }
int64_t Params::Int() const { return iv; }
bool Params::Bool() const { return bv; }
double Params::Double() const { return dv; }
void Params::SetString(const std::string& v) {
  if (invalid) {
    return;
  }
  str = v;
}
void Params::SetInt(int64_t v) {
  if (invalid) {
    return;
  }
  iv = v;
}
void Params::SetDouble(double d) {
  if (invalid) {
    return;
  }
  dv = d;
}
void Params::SetBool(bool v) {
  if (invalid) {
    return;
  }
  bv = v;
}
size_t Params::Size() const {
  if (params.size() > 0) {
    return params.size();
  }
  return param_array.size();
}
const Params::ParamValueTable& Params::Members() const { return params; }

void Params::BuildFromString(const std::string& v) {
  if (invalid) {
    return;
  }
  // if (butil::StringToInt64(v, &iv)) {
  //   str = v;
  //   return;
  // }
  // if (butil::StringToDouble(v, &dv)) {
  //   str = v;
  //   return;
  // }
  if (v == "true") {
    str = v;
    bv = true;
    return;
  }
  if (v == "false") {
    str = v;
    bv = false;
    return;
  }
  // if (v.size() >= 2 && v[0] == '[' && v[v.size() - 1] == ']') {
  //   std::string array_val = v.substr(1, v.size() - 2);
  //   std::vector<std::string> sf;
  //   butil::SplitString(v, ',', &sf);
  //   for (uint32_t i = 0; i < sf.size(); i++) {

  //   }
  // }
  str = v;
}
const Params& Params::operator[](const std::string& name) const {
  ParamValueTable::const_iterator it = params.find(name);
  if (it != params.end()) {
    return it->second;
  }
  if (NULL != parent) {
    return (*parent)[name];
  }
  static Params default_value(true);
  return default_value;
}
Params& Params::operator[](const std::string& name) {
  ParamValueTable::iterator it = params.find(name);
  if (it != params.end()) {
    return it->second;
  }
  if (NULL != parent) {
    ParamValueTable::const_iterator cit = parent->params.find(name);
    if (cit != parent->params.end()) {
      return const_cast<Params&>(cit->second);
    }
  }
  return params[name];
}
const Params& Params::operator[](size_t idx) const {
  if (param_array.size() > idx) {
    return param_array[idx];
  }
  static Params default_value(true);
  return default_value;
}
Params& Params::Add() {
  param_array.resize(param_array.size() + 1);
  return param_array[param_array.size() - 1];
}
Params& Params::Put(const std::string& name, const char* value) {
  params[name].SetString(value);
  return *this;
}
Params& Params::Put(const std::string& name, const std::string& value) {
  params[name].SetString(value);
  return *this;
}
Params& Params::Put(const std::string& name, int64_t value) {
  params[name].SetInt(value);
  return *this;
}
Params& Params::Put(const std::string& name, double value) {
  params[name].SetDouble(value);
  return *this;
}
Params& Params::Put(const std::string& name, bool value) {
  params[name].SetBool(value);
  return *this;
}
Params& Params::operator[](size_t idx) {
  if (param_array.size() > idx) {
    return param_array[idx];
  }
  param_array.resize(idx + 1);
  return param_array[idx];
}
bool Params::Contains(const std::string& name) const { return params.count(name) > 0; }
void Params::Insert(const Params& other) {
  for (auto& kv : other.Members()) {
    params[kv.first] = kv.second;
  }
  for (auto& kv : other.param_array) {
    param_array.push_back(kv);
  }
}
void Params::ParseFromString(const std::string& v) {
  // std::vector<std::string> sf;
  // butil::SplitString(v, ',', &sf);
  // for (uint32_t i = 0; i < sf.size(); i++) {
  //   std::vector<std::string> kv;
  //   butil::SplitString(sf[i], '=', &kv);
  //   if (kv.size() == 2) {
  //     std::string new_key, new_value;
  //     butil::TrimWhitespace(kv[0], butil::TRIM_ALL, &new_key);
  //     butil::TrimWhitespace(kv[1], butil::TRIM_ALL, &new_value);
  //     params[new_key].BuildFromString(new_value);
  //   }
  // }
}
const char* Params::GetVar(const char* v) {
  if (v[0] != '$') {
    return v;
  }
  return "";
}

}  // namespace didagle