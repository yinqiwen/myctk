// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/17
// Authors: qiyingwang (qiyingwang@tencent.com)

#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "didagle/graph_processor.h"
#include "folly/FBVector.h"
namespace didagle {
class ProcessorDI {
 public:
  struct FieldData {
    std::string name;
    FieldInfo info;
    const GraphData* data = nullptr;
    int32_t idx = -1;
    FieldData(const FieldInfo& id) {
      name = id.name;
      info = id;
    }
  };

 private:
  Processor* _proc;
  bool _strict_dsl;

  // using FieldData = std::pair<FieldInfo, const GraphData*>;
  //  typedef std::map<std::string, FieldData> FieldDataTable;
  using FieldDataArray = folly::fbvector<FieldData>;

  FieldDataArray _input_ids;
  FieldDataArray _output_ids;
  int SetupInputOutputIds(const std::vector<FieldInfo>& fields, const std::vector<GraphData>& config_fields,
                          FieldDataArray& field_ids);

 public:
  explicit ProcessorDI(Processor* proc, bool strict_dsl = false);
  FieldDataArray& GetInputIds() { return _input_ids; }
  FieldDataArray& GetOutputIds() { return _output_ids; }
  int PrepareInputs(const std::vector<GraphData>& config_inputs = {});
  int PrepareOutputs(const std::vector<GraphData>& config_outputs = {});
  int InjectInputs(GraphDataContext& ctx, const Params* params);
  int CollectOutputs(GraphDataContext& ctx, const Params* params);
  int MoveDataWhenSkipped(GraphDataContext& ctx);
};

}  // namespace didagle
