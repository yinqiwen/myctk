// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "executor.h"
#include "graph.h"
#include "log.h"
#include "processor.h"

namespace wrdk {
namespace graph {
VertexContext::~VertexContext() {
  delete _processor;
  // delete _subgraph;
}
int VertexContext::SetupInputOutputIds(const std::vector<DataKey>& fields,
                                       const std::vector<GraphData>& config_fields,
                                       FieldDataTable& ids) {
  for (const DataKey& id : fields) {
    ids[id.name] = std::make_pair(id, (const GraphData*)nullptr);
  }
  for (const GraphData& data : config_fields) {
    auto found = ids.find(data.field);
    if (found == ids.end()) {
      WRDK_GRAPH_ERROR("No field:{} found in processor:{}", data.field, _processor->Name());
      return -1;
    }
    const DataKey& exist_key = found->second.first;
    DataKey new_key{data.id, exist_key.id};
    ids[data.field] = std::make_pair(new_key, &data);
  }
  return 0;
}
int VertexContext::Setup(GraphContext* g, Vertex* v) {
  _graph_ctx = g;
  _vertex = v;
  // todo get processor
  if (!_vertex->graph.empty()) {
    // get subgraph at runtime
  } else {
    _processor = ProcessorFactory::GetProcessor(_vertex->processor);
  }
  if (_graph_ctx->GetGraphCluster()->strict_dsl) {
    if (nullptr == _processor && !_vertex->processor.empty()) {
      WRDK_GRAPH_ERROR("No processor found for {}", _vertex->processor);
      return -1;
    }
  }
  _params = _vertex->args;
  _select_params = _vertex->select_args;

  if (nullptr != _processor) {
    if (0 != SetupInputOutputIds(_processor->GetInputIds(), _vertex->input, _input_ids)) {
      return -1;
    }
    if (0 != SetupInputOutputIds(_processor->GetOutputIds(), _vertex->output, _output_ids)) {
      return -1;
    }
  }
  Reset();
  return 0;
}

void VertexContext::Reset() {
  _result = V_RESULT_INVALID;
  _code = V_CODE_INVALID;
  _deps_results.assign(_vertex->_deps_idx.size(), V_RESULT_INVALID);
  _waiting_num = _vertex->_deps_idx.size();
  if (nullptr != _processor) {
    _processor->Reset();
  }
  if (nullptr != _subgraph) {
    _subgraph->Reset();
    _subgraph->GetGraph()->ReleaseContext(_subgraph);
    _subgraph = nullptr;
  }
  for (auto& pair : _select_params) {
    pair.second.SetParent(nullptr);
  }
  _params.SetParent(nullptr);
}
uint32_t VertexContext::SetDependencyResult(Vertex* v, VertexResult r) {
  int idx = _vertex->GetDependencyIndex(v);
  VertexResult last_result_val = _deps_results[idx];
  _deps_results[idx] = r;
  if (last_result_val == V_RESULT_INVALID) {
    return _waiting_num.fetch_sub(1);
  } else {
    return _waiting_num.load();
  }
}
VertexContext::VertexContext() { _waiting_num = 0; }
bool VertexContext::Ready() { return 0 == _waiting_num.load(); }
void VertexContext::FinishVertexProcess(int code) {
  _code = (VertexErrCode)code;
  if (0 != _code) {
    _result = V_RESULT_ERR;
  } else {
    _result = V_RESULT_OK;
  }
  for (const auto& pair : _output_ids) {
    const std::string& field = pair.first;
    const DataKey& data = pair.second.first;
    int rc = _processor->EmitOutputField(_graph_ctx->GetGraphDataContext(), field, data.name);
    if (0 != rc) {
      // log
    }
  }
  _graph_ctx->OnVertexDone(this);
}
int VertexContext::ExecuteProcessor() {
  WRDK_GRAPH_DEBUG("Vertex:{} begin execute", _vertex->GetDotLable());
  Params* exec_params = nullptr;
  if (!_params.Valid() && !_select_params.empty()) {
    for (auto& pair : _select_params) {
      const bool* v = _graph_ctx->GetGraphDataContext().Get<bool>(pair.first);
      if (nullptr == v || !(*v)) {
        continue;
      }
      exec_params = &(pair.second);
      break;
    }
  }
  if (nullptr == exec_params) {
    exec_params = &_params;
  }
  if (_graph_ctx->GetExecuteOptions()->params) {
    exec_params->SetParent(_graph_ctx->GetExecuteOptions()->params.get());
  }
  for (const auto& pair : _input_ids) {
    const std::string& field = pair.first;
    const DataKey& data = pair.second.first;
    const GraphData* graph_data = pair.second.second;
    bool required = false;
    if (nullptr != graph_data) {
      required = graph_data->required;
    }
    int rc = _processor->InjectInputField(_graph_ctx->GetGraphDataContext(), field, data.name);
    if (0 != rc && required) {
      _result = V_RESULT_ERR;
      _code = V_CODE_SKIP;
      break;
    }
  }
  int rc = _processor->Execute(*exec_params);
  if (ERR_UNIMPLEMENTED == rc) {
    _processor->AsyncExecute(*exec_params, [this](int code) { FinishVertexProcess(code); });
  } else {
    FinishVertexProcess(rc);
  }
  return 0;
}
int VertexContext::ExecuteSubGraph() {
  if (_vertex->_graph->_cluster->_graph_manager) {
    _subgraph = _vertex->_graph->_cluster->_graph_manager->GetGraphContext(_vertex->cluster,
                                                                           _vertex->graph);
  }
  if (nullptr == _subgraph) {
    WRDK_GRAPH_ERROR("No subgraph found for {}::{}", _vertex->cluster, _vertex->graph);
    return -1;
  }
  _graph_ctx->AddSubGraphContext(_subgraph);
  _subgraph->SetExecuteOptions(_graph_ctx->GetExecuteOptions());
  _subgraph->Execute(_graph_ctx, [this](int code) { FinishVertexProcess(code); });
  return 0;
}
int VertexContext::Execute() {
  bool match_dep_expected_result = true;
  if (nullptr == _processor && nullptr == _subgraph) {
    match_dep_expected_result = false;
  } else {
    for (size_t i = 0; i < _deps_results.size(); i++) {
      if (V_RESULT_INVALID == _deps_results[i] ||
          (0 == (_deps_results[i] & _vertex->_deps_expected_results[i]))) {
        match_dep_expected_result = false;
        break;
      }
    }
  }
  WRDK_GRAPH_DEBUG("Vertex:{} match deps {}.", _vertex->GetDotLable(), match_dep_expected_result);
  if (!match_dep_expected_result) {
    _result = V_RESULT_ERR;
    _code = V_CODE_SKIP;
    // no need to exec this
  } else {
    if (nullptr != _processor) {
      ExecuteProcessor();
    } else {
      ExecuteSubGraph();
    }
  }
  return 0;
}

GraphContext::GraphContext() : _graph(nullptr), _parent(nullptr) { _join_vertex_num = 0; }
int GraphContext::Setup(Graph* g) {
  _graph = g;
  for (auto& pair : g->_nodes) {
    Vertex* v = pair.second;
    std::shared_ptr<VertexContext> c(new VertexContext);
    if (0 != c->Setup(this, v)) {
      WRDK_GRAPH_ERROR("Graph:{} setup vertex:{} failed.", g->name, v->GetDotLable());
      return -1;
    }
    _vertex_context_table[v] = c;
    for (const auto& pair : c->GetInputIds()) {
      const DataKey& key = pair.second.first;
      // if (_all_input_ids.count(key) > 0) {
      //   WRDK_GRAPH_ERROR("Duplicate input name:{} in graph:{}", key.name, g->name);
      //   return -1;
      // }
      _all_input_ids.insert(key);
    }
    for (const auto& pair : c->GetOutputIds()) {
      const DataKey& key = pair.second.first;
      if (_all_output_ids.count(key) > 0) {
        WRDK_GRAPH_ERROR("Duplicate output name:{} in graph:{}", key.name, g->name);
        return -1;
      }
      _all_output_ids.insert(key);
    }
  }
  Reset();
  return 0;
}
void GraphContext::AddSubGraphContext(GraphContext* g) {
  if (nullptr != _parent) {
    _parent->AddSubGraphContext(g);
  } else {
    _sub_graphs.push(g);
  }
}
VertexContext* GraphContext::FindVertexContext(Vertex* v) {
  auto found = _vertex_context_table.find(v);
  if (found == _vertex_context_table.end()) {
    return nullptr;
  }
  return found->second.get();
}
void GraphContext::Reset() {
  _join_vertex_num = _vertex_context_table.size();
  for (auto& pair : _vertex_context_table) {
    std::shared_ptr<VertexContext> ctx = pair.second;
    ctx->Reset();
  }
  _parent = nullptr;
  _running_cluster.reset();
  GraphContext* sub_graph = nullptr;
  while (_sub_graphs.try_pop(sub_graph)) {
    sub_graph->Reset();
  }
  _exec_options.reset();
  // session_ctx.Reset();
}
void GraphContext::ExecuteReadyVertexs(std::vector<VertexContext*>& ready_vertexs) {
  WRDK_GRAPH_DEBUG("ExecuteReadyVertexs with {} vertexs.", ready_vertexs.size());
  if (ready_vertexs.size() == 1) {
    // inplace run
    ready_vertexs[0]->Execute();
  } else {
    for (VertexContext* ctx : ready_vertexs) {
      VertexContext* next = ctx;
      auto f = [next]() { next->Execute(); };
      _exec_options->concurrent_executor(f);
    }
  }
}
void GraphContext::OnVertexDone(VertexContext* vertex) {
  WRDK_GRAPH_DEBUG("OnVertexDone while _join_vertex_num:{}.", _join_vertex_num.load());
  if (1 == _join_vertex_num.fetch_sub(1)) {  // last vertex
    if (_done) {
      _done(0);
      _done = 0;
    }
    return;
  }
  std::vector<VertexContext*> ready_successors;
  for (Vertex* successor : vertex->GetVertex()->_successor_vertex) {
    VertexContext* successor_ctx = FindVertexContext(successor);
    if (nullptr == successor_ctx) {
      // error
      abort();
      continue;
    }
    // last dependency
    if (1 == successor_ctx->SetDependencyResult(vertex->GetVertex(), vertex->GetResult())) {
      ready_successors.push_back(successor_ctx);
    }
  }
  ExecuteReadyVertexs(ready_successors);
}
GraphCluster* GraphContext::GetGraphCluster() { return _graph->_cluster; }
GraphDataContext& GraphContext::GetGraphDataContext() { return _data_ctx; }
void GraphContext::SetGraphDataContext(std::shared_ptr<GraphDataContext> p) {
  _data_ctx.SetParent(p);
}

int GraphContext::Execute(GraphContext* parent, DoneClosure&& done) {
  _done = std::move(done);
  _parent = parent;
  // make all data entry precreated in data ctx
  for (const auto& id : _all_input_ids) {
    _data_ctx.RegisterData(id);
  }
  for (const auto& id : _all_output_ids) {
    _data_ctx.RegisterData(id);
  }
  std::vector<VertexContext*> ready_successors;
  for (auto& pair : _vertex_context_table) {
    std::shared_ptr<VertexContext> ctx = pair.second;
    if (ctx->Ready()) {
      ready_successors.push_back(ctx.get());
    }
  }
  ExecuteReadyVertexs(ready_successors);
  return 0;
}
int GraphClusterContext::Setup(GraphCluster* c) {
  _cluster = c;
  if (!_cluster) {
    return -1;
  }
  for (const auto& cfg : c->config_setting) {
    Processor* p = ProcessorFactory::GetProcessor(cfg.processor);
    if (nullptr == p && c->strict_dsl) {
      WRDK_GRAPH_ERROR("No processor found for {}", cfg.processor);
      return -1;
    }
    if (nullptr != p) {
      Params args;
      args["__EXPRESSION__"].SetString(cfg.cond);
      if (0 != p->Setup(args)) {
        WRDK_GRAPH_ERROR("Failed to setup expr processor", cfg.processor);
        return -1;
      }
      _config_setting_processors.push_back(p);
    }
  }
  return 0;
}
void GraphClusterContext::Execute(GraphDataContext& data_ctx, std::vector<uint8_t>& eval_results) {
  eval_results.assign(_config_setting_processors.size(), 0);
  for (size_t i = 0; i < _config_setting_processors.size(); i++) {
    Processor* p = _config_setting_processors[i];
    Params args;
    if (0 != p->Execute(args)) {
      eval_results[i] = 0;
    } else {
      eval_results[i] = 1;
    }
    bool* v = (bool*)(&eval_results[i]);
    data_ctx.Set<bool>(_cluster->config_setting[i].name, v);
  }
}
}  // namespace graph
}  // namespace wrdk
