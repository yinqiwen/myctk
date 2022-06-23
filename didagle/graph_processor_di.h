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
namespace didagle {
class ProcessorDI {
 private:
  Processor* _proc;
  bool _strict_dsl;
  typedef std::pair<FieldInfo, const GraphData*> FieldData;
  typedef std::map<std::string, FieldData> FieldDataTable;
  FieldDataTable _input_ids;
  FieldDataTable _output_ids;
  int SetupInputOutputIds(const std::vector<FieldInfo>& fields, const std::vector<GraphData>& config_fields,
                          FieldDataTable& field_ids);

 public:
  explicit ProcessorDI(Processor* proc, bool strict_dsl = false);
  const FieldDataTable& GetInputIds() { return _input_ids; }
  const FieldDataTable& GetOutputIds() { return _output_ids; }
  int PrepareInputs(const std::vector<GraphData>& config_inputs = {});
  int PrepareOutputs(const std::vector<GraphData>& config_outputs = {});
  int InjectInputs(GraphDataContext& ctx, const Params* params);
  int CollectOutputs(GraphDataContext& ctx, const Params* params);
  int MoveDataWhenSkipped(GraphDataContext& ctx);
};

}  // namespace didagle
