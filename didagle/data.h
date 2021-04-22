// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/17
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once

#include <stdint.h>
#include <atomic>
#include <functional>
#include <memory>
#include <unordered_set>
#include "toml_helper.h"

namespace didagle {
struct GraphData {
  std::string id;
  std::string field;
  std::vector<std::string> merge;
  bool required = false;
  bool move = false;
  bool is_extern = false;

  WRDK_TOML_DEFINE_FIELD_MAPPING(({"is_extern", "extern"}))
  WRDK_TOML_DEFINE_FIELDS(id, field, required, move, is_extern, merge)
};

struct ConfigSetting {
  std::string name;
  std::string cond;
  std::string processor;
  WRDK_TOML_DEFINE_FIELD_MAPPING(({"data", "if"}))
  WRDK_TOML_DEFINE_FIELDS(name, cond, processor)
};

class Params {
 public:
  typedef std::map<std::string, Params> ParamValueTable;

 private:
  std::string str;
  int64_t iv;
  double dv;
  bool bv;
  bool invalid;

  typedef std::vector<Params> ParamValueArray;
  ParamValueTable params;
  ParamValueArray param_array;

  const Params* parent = nullptr;

 public:
  Params(bool invalid_ = false);
  void SetParent(const Params* p);
  bool Valid() const;
  const std::string& String() const;
  int64_t Int() const;
  bool Bool() const;
  double Double() const;
  void SetString(const std::string& v);
  void SetInt(int64_t v);
  void SetDouble(double d);
  void SetBool(bool v);
  size_t Size() const;
  const ParamValueTable& Members() const;

  const Params& operator[](const std::string& name) const;
  Params& operator[](const std::string& name);
  const Params& operator[](size_t idx) const;
  Params& operator[](size_t idx);

  Params& Add();
  Params& Put(const std::string& name, const char* value);
  Params& Put(const std::string& name, const std::string& value);
  Params& Put(const std::string& name, int64_t value);
  Params& Put(const std::string& name, double value);
  Params& Put(const std::string& name, bool value);
  bool Contains(const std::string& name) const;
  void Insert(const Params& other);
  void BuildFromString(const std::string& v);
  void ParseFromString(const std::string& v);
  const char* GetVar(const char* v);
  bool ParseFromToml(const wrdk::TomlValue& doc);
};

}  // namespace didag