// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/16
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once
#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "didagle/graph_processor_api.h"

namespace didagle {

struct ProcessorMeta {
  std::string name;
  std::string desc;
  bool isIOProcessor = false;
  std::vector<FieldInfo> input;
  std::vector<FieldInfo> output;
  std::vector<ParamInfo> params;
  KCFG_DEFINE_FIELDS(name, desc, isIOProcessor, input, output, params)
};

class ProcessorFactory {
 public:
  static void Register(std::string_view name, const ProcessorCreator& creator);
  static Processor* GetProcessor(const std::string& name);
  static void GetAllMetas(std::vector<ProcessorMeta>& metas);
  static int DumpAllMetas(const std::string& file = "all_processors.json");
};

struct ProcessorRunResult {
  std::shared_ptr<Processor> processor;
  int rc = -1;
  explicit ProcessorRunResult(int v = -1) : rc(v) {}
};

struct ProcessorRunOptions {
  const Params* params = nullptr;
  std::map<std::string, std::vector<std::string>> map_aggregate_ids;
};

ProcessorRunResult run_processor(GraphDataContext& ctx, const std::string& proc, const ProcessorRunOptions& opts = {});

}  // namespace didagle