// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
#include "didagle/graph_processor_di.h"
#include <string>
#include <utility>
#include <vector>
#include "didagle/didagle_log.h"

namespace didagle {
ProcessorDI::ProcessorDI(Processor* proc, bool strict_dsl) : _proc(proc), _strict_dsl(strict_dsl) {}
int ProcessorDI::SetupInputOutputIds(const std::vector<FieldInfo>& fields, const std::vector<GraphData>& config_fields,
                                     FieldDataTable& ids) {
  for (const FieldInfo& id : fields) {
    ids[id.name] = std::make_pair(id, (const GraphData*)nullptr);
  }
  for (const GraphData& data : config_fields) {
    auto found = ids.find(data.field);
    if (found == ids.end()) {
      if (!_strict_dsl) {
        continue;
      }
      DIDAGLE_ERROR("No field:{} found in processor:{}", data.field, _proc->Name());
      return -1;
    }
    const DIObjectKey& exist_key = found->second.first;
    FieldInfo new_key;
    new_key.name = data.id;
    new_key.id = exist_key.id;
    // DIObjectKey new_key{data.id, exist_key.id};
    ids[data.field] = std::make_pair(new_key, &data);
  }
  return 0;
}
int ProcessorDI::PrepareInputs(const std::vector<GraphData>& config_inputs) {
  _input_ids.clear();
  return SetupInputOutputIds(_proc->GetInputIds(), config_inputs, _input_ids);
}
int ProcessorDI::PrepareOutputs(const std::vector<GraphData>& config_outputs) {
  _output_ids.clear();
  return SetupInputOutputIds(_proc->GetOutputIds(), config_outputs, _output_ids);
}
int ProcessorDI::InjectInputs(GraphDataContext& ctx, const Params* params) {
  for (const auto& pair : _input_ids) {
    const std::string& field = pair.first;
    const DIObjectKey& data = pair.second.first;
    const GraphData* graph_data = pair.second.second;
    bool required = false;
    if (nullptr != graph_data) {
      required = graph_data->required;
    }
    int rc = 0;
    if (nullptr != graph_data && !graph_data->aggregate.empty()) {
      for (const std::string& aggregate_id : graph_data->aggregate) {
        if (!aggregate_id.empty() && aggregate_id[0] == '$' && nullptr != params) {
          ParamsString var_name = aggregate_id.substr(1);
          const Params& var_value = params->GetVar(var_name);
          DIDAGLE_DEBUG("Get Var value:{} for {}", var_value.String(), aggregate_id);
          if (!var_value.String().empty()) {
            std::string_view data_name(var_value.String().data(), var_value.String().size());
            rc = _proc->InjectInputField(ctx, field, data_name, graph_data->move);
          } else {
            rc = -1;
            // 需要的时候，打印error日志； 不需要的时候，打印info日志;
            if (required) {
              DIDAGLE_ERROR("[{}]inject {} failed with var aggregate_id:{}", _proc->Name(), field, aggregate_id);
            } else {
              DIDAGLE_DEBUG("[{}]inject {} failed with var aggregate_id:{}", _proc->Name(), field, aggregate_id);
            }
          }
        }

        rc = _proc->InjectInputField(ctx, field, aggregate_id, graph_data->move);
        if (0 != rc && required) {
          break;
        }
      }
    } else {
      bool move_data = false;
      if (nullptr != graph_data) {
        move_data = graph_data->move;
      } else {
        move_data = pair.second.first.flags.is_in_out;
      }
      if (!data.name.empty() && data.name[0] == '$' && nullptr != params) {
        ParamsString var_name = data.name.substr(1);
        const Params& var_value = params->GetVar(var_name);
        DIDAGLE_DEBUG("Get Var value:{} for {}", var_value.String(), data.name);
        if (!var_value.String().empty()) {
          std::string_view data_name(var_value.String().data(), var_value.String().size());
          rc = _proc->InjectInputField(ctx, field, data_name, move_data);
        } else {
          rc = -1;
          DIDAGLE_ERROR("[{}]inject {} failed with var data name:{}", _proc->Name(), field, data.name);
        }
      } else {
        rc = _proc->InjectInputField(ctx, field, data.name, move_data);
      }

      if (0 != rc) {
        if (required) {
          DIDAGLE_ERROR("[{}]inject {}:{} failed with move:{}, null data:{}", _proc->Name(), field, data.name,
                        move_data, nullptr == graph_data);
        } else {
          DIDAGLE_DEBUG("[{}]inject {}:{} failed with move:{}, null data:{}", _proc->Name(), field, data.name,
                        move_data, nullptr == graph_data);
        }
      }
    }
    if (0 != rc && required) {
      return -1;
      //   _result = V_RESULT_ERR;
      //   _code = V_CODE_SKIP;
      //   break;
    }
  }
  return 0;
}
int ProcessorDI::CollectOutputs(GraphDataContext& ctx, const Params* params) {
  for (const auto& pair : _output_ids) {
    const std::string& field = pair.first;
    const DIObjectKey& data = pair.second.first;
    if (!data.name.empty() && data.name[0] == '$' && nullptr != params) {
      ParamsString var_name = data.name.substr(1);
      const Params& var_value = params->GetVar(var_name);
      DIDAGLE_DEBUG("Get Var value:{} for {}", var_value.String(), data.name);
      if (!var_value.String().empty()) {
        std::string_view data_name(var_value.String().data(), var_value.String().size());
        DIDAGLE_DEBUG("[{}]Collect output for field {}:{} with actual name:{}", _proc->Name(), field, data.name,
                      data_name);
        int rc = _proc->EmitOutputField(ctx, field, data_name);
        if (0 != rc) {
          DIDAGLE_ERROR("[{}]Collect output for field {}:{} failed with actual name:{}", _proc->Name(), field,
                        data.name, data_name);
        }
      } else {
        DIDAGLE_ERROR("[{}]Collect output for field {}:{} failed with var name:{}", _proc->Name(), field, data.name,
                      var_name);
      }
      continue;
    }
    DIDAGLE_DEBUG("[{}]Collect output for field {}:{} ", _proc->Name(), field, data.name);
    int rc = _proc->EmitOutputField(ctx, field, data.name);
    if (0 != rc) {
      DIDAGLE_ERROR("[{}]Collect output for field {}:{} failed.", _proc->Name(), field, data.name);
    }
  }
  return 0;
}

int ProcessorDI::MoveDataWhenSkipped(GraphDataContext& ctx) {
  for (const auto& pair : _output_ids) {
    const std::string& field = pair.first;
    const FieldInfo& field_info = pair.second.first;
    const GraphData* data = pair.second.second;
    if (nullptr != data && !data->move_from_when_skipped.empty()) {
      DIObjectKey from;
      from.id = field_info.id;
      from.name = data->move_from_when_skipped;
      int rc = ctx.Move(from, field_info);
      if (0 != rc) {
        DIDAGLE_ERROR("[{}]Filed:{} move {} to {} when skipped failed.", _proc->Name(), field,
                      data->move_from_when_skipped, field_info.name);
      }
    }
  }
  return 0;
}

}  // namespace didagle