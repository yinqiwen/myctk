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
#include <boost/algorithm/string.hpp>

#include "didagle/graph_params.h"
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
bool Params::IsBool() const {
  if (invalid) {
    return false;
  }
  return _param_type == PARAM_BOOL;
}
bool Params::IsString() const {
  if (invalid) {
    return false;
  }
  return _param_type == PARAM_STRING;
}
bool Params::IsDouble() const {
  if (invalid) {
    return false;
  }
  return _param_type == PARAM_DOUBLE;
}
bool Params::IsInt() const {
  if (invalid) {
    return false;
  }
  return _param_type == PARAM_INT;
}
bool Params::IsObject() const {
  if (invalid) {
    return false;
  }
  return _param_type == PARAM_OBJECT;
}
bool Params::IsArray() const {
  if (invalid) {
    return false;
  }
  return _param_type == PARAM_ARRAY;
}
const ParamsString& Params::String() const { return str; }
int64_t Params::Int() const { return iv; }
bool Params::Bool() const { return bv; }
double Params::Double() const { return dv; }
void Params::SetString(const ParamsString& v) {
  str = v;
  _param_type = PARAM_STRING;
}
void Params::SetInt(int64_t v) {
  iv = v;
  _param_type = PARAM_INT;
}
void Params::SetDouble(double d) {
  dv = d;
  _param_type = PARAM_DOUBLE;
}
void Params::SetBool(bool v) {
  bv = v;
  _param_type = PARAM_BOOL;
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
const Params& Params::Get(const ParamsString& name) const {
  ParamValueTable::const_iterator it = params.find(name);
  if (it != params.end()) {
    return it->second;
  }
  if (NULL != parent) {
    return parent->Get(name);
  }
  static Params default_value(true);
  return default_value;
}
const Params& Params::operator[](const ParamsString& name) const { return Get(name); }
Params& Params::operator[](const ParamsString& name) {
  const Params& p = Get(name);
  if (p.Valid()) {
    return const_cast<Params&>(p);
  }
  _param_type = PARAM_OBJECT;
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
  _param_type = PARAM_ARRAY;
  return param_array[param_array.size() - 1];
}
Params& Params::Put(const ParamsString& name, const char* value) {
  _param_type = PARAM_OBJECT;
  params[name].SetString(value);
  return *this;
}
Params& Params::Put(const ParamsString& name, const ParamsString& value) {
  _param_type = PARAM_OBJECT;
  params[name].SetString(value);
  return *this;
}
Params& Params::Put(const ParamsString& name, int64_t value) {
  _param_type = PARAM_OBJECT;
  params[name].SetInt(value);
  return *this;
}
Params& Params::Put(const ParamsString& name, double value) {
  _param_type = PARAM_OBJECT;
  params[name].SetDouble(value);
  return *this;
}
Params& Params::Put(const ParamsString& name, bool value) {
  _param_type = PARAM_OBJECT;
  params[name].SetBool(value);
  return *this;
}
Params& Params::operator[](size_t idx) {
  if (param_array.size() > idx) {
    return param_array[idx];
  }
  _param_type = PARAM_ARRAY;
  param_array.resize(idx + 1);
  return param_array[idx];
}
bool Params::Contains(const ParamsString& name) const {
  if (params.count(name) > 0) {
    return true;
  }
  if (nullptr != parent) {
    return parent->Contains(name);
  }
  return false;
}
void Params::Insert(const Params& other) {
  _param_type = PARAM_OBJECT;
  for (auto& kv : other.Members()) {
    params[kv.first] = kv.second;
  }
  for (auto& kv : other.param_array) {
    param_array.push_back(kv);
  }
}
void Params::ParseFromString(const std::string& v) {
  std::vector<std::string> sf;
  boost::split(sf, v, boost::is_any_of(","));
  for (uint32_t i = 0; i < sf.size(); i++) {
    std::vector<std::string> kv;
    boost::split(kv, sf[i], boost::is_any_of("="));
    if (kv.size() == 2) {
      boost::algorithm::trim(kv[0]);
      boost::algorithm::trim(kv[1]);
      _param_type = PARAM_OBJECT;
      params[kv[0]].BuildFromString(kv[1]);
    }
  }
}
const Params& Params::GetVar(const ParamsString& name) const {
  ParamsString var_name = name;
  const Params* var_params = this;
  while (true) {
    auto pos = var_name.find('.');
    if (pos == ParamsString::npos) {
      var_params = &(var_params->Get(var_name));
      break;
    } else {
      auto part = var_name.substr(0, pos);
      var_params = &(var_params->Get(part));
      var_name = var_name.substr(pos);
    }
  }
  return *var_params;
}

}  // namespace didagle