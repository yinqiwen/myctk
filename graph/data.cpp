// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "data.h"

namespace wrdk {
namespace graph {

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
bool Params::ParseFromToml(const wrdk::TomlValue& doc) {
  if (doc.is_table()) {
    for (const auto& kv : doc.as_table()) {
      const std::string& name = kv.first;
      const auto& value = kv.second;
      Params tmp;
      if (tmp.ParseFromToml(value)) {
        params[name] = tmp;
      } else {
        return false;
      }
    }
    invalid = false;
  } else if (doc.is_integer()) {
    iv = doc.as_integer();
    str = std::to_string(iv);
    invalid = false;
  } else if (doc.is_boolean()) {
    bv = doc.as_boolean();
    str = std::to_string(bv);
    invalid = false;
  } else if (doc.is_floating()) {
    dv = doc.as_floating();
    str = std::to_string(dv);
    invalid = false;
  } else if (doc.is_string()) {
    str = doc.as_string();
    invalid = false;
  } else if (doc.is_array()) {
    for (const auto& item : doc.as_array()) {
      Params tmp;
      if (tmp.ParseFromToml(item)) {
        param_array.push_back(tmp);
      } else {
        // v.clear();
        return false;
      }
    }
    invalid = false;
  } else {
    return false;
  }
  invalid = false;
  return true;
}
}  // namespace graph
}  // namespace wrdk
