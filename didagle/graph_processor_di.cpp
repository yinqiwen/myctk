// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
#include "graph_processor_di.h"

namespace didagle {
ProcessorDI::ProcessorDI(Processor* proc, bool strict_dsl) : _proc(proc), _strict_dsl(strict_dsl) {}
int ProcessorDI::SetupInputOutputIds(const std::vector<FieldInfo>& fields,
                                     const std::vector<GraphData>& config_fields,
                                     FieldDataTable& ids) {
  for (const DIObjectKey& id : fields) {
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
    DIObjectKey new_key{data.id, exist_key.id};
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
int ProcessorDI::InjectInputs(GraphDataContext& ctx) {
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
        rc = _proc->InjectInputField(ctx, field, aggregate_id, graph_data->move);
        if (0 != rc && required) {
          break;
        }
      }
    } else {
      if (nullptr != graph_data) {
        rc = _proc->InjectInputField(ctx, field, data.name, graph_data->move);
      } else {
        rc = _proc->InjectInputField(ctx, field, data.name, false);
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
int ProcessorDI::CollectOutputs(GraphDataContext& ctx) {
  for (const auto& pair : _output_ids) {
    const std::string& field = pair.first;
    const DIObjectKey& data = pair.second.first;
    int rc = _proc->EmitOutputField(ctx, field, data.name);
    if (0 != rc) {
      // log
    }
  }
  return 0;
}

}  // namespace didagle