// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/17
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once

#include <stdint.h>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>
#include "didagle/graph_params.h"
#include "kcfg_toml.h"

namespace didagle {
struct GraphData {
  std::string id;
  std::string field;
  std::string move_from_when_skipped;
  std::vector<std::string> aggregate;
  bool required = false;
  bool move = false;
  bool is_extern = false;
  bool _is_in_out = false;

  KCFG_TOML_DEFINE_FIELD_MAPPING(({"is_extern", "extern"}))
  KCFG_TOML_DEFINE_FIELDS(id, field, move_from_when_skipped, required, move, is_extern, aggregate)
};

struct ConfigSetting {
  std::string name;
  std::string cond;
  std::string processor;
  KCFG_TOML_DEFINE_FIELDS(name, cond, processor)
};

struct GraphParams : public Params {
  bool ParseFromToml(const kcfg::TomlValue& doc);
};

}  // namespace didagle