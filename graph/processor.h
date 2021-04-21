// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/16
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once
#include <atomic>
#include <memory>
#include <string_view>
#include <unordered_map>
#include "processor_api.h"
#include "tbb/concurrent_hash_map.h"

namespace wrdk {
namespace graph {
class ProcessorFactory {
 public:
  static void Register(const std::string& name, const ProcessorCreator& creator);
  static Processor* GetProcessor(const std::string& name);
};
}  // namespace graph
}  // namespace wrdk