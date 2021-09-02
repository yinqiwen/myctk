// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/16
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once
#include <atomic>
#include <memory>
#include <string_view>
#include <unordered_map>
#include "graph_processor_api.h"
#include "tbb/concurrent_hash_map.h"

namespace didagle {

struct ProcessorMeta {
  std::string name;
  std::vector<FieldInfo> input;
  std::vector<FieldInfo> output;
  KCFG_DEFINE_FIELDS(name, input, output)
};

class ProcessorFactory {
 public:
  static void Register(const std::string& name, const ProcessorCreator& creator);
  static Processor* GetProcessor(const std::string& name);
  static int DumpAllMetas(const std::string& file = "all_processors.json");
};

struct ProcessorRunResult {
  std::shared_ptr<Processor> processor;
  int rc = -1;
  ProcessorRunResult(int v = -1) : rc(v) {}
};
ProcessorRunResult run_processor(GraphDataContext& ctx, const std::string& proc,
                                 const Params* params = nullptr);

}  // namespace didagle