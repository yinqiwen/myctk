// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "graph_executor.h"
#include "didagle_log.h"
#include "graph.h"
#include "graph_processor.h"

namespace didagle {
VertexContext::~VertexContext() {
  delete _processor;
  delete _processor_di;
  // delete _subgraph;
}
// int VertexContext::SetupInputOutputIds(const std::vector<DIObjectKey>& fields,
//                                        const std::vector<GraphData>& config_fields,
//                                        FieldDataTable& ids) {
//   for (const DIObjectKey& id : fields) {
//     ids[id.name] = std::make_pair(id, (const GraphData*)nullptr);
//   }
//   for (const GraphData& data : config_fields) {
//     auto found = ids.find(data.field);
//     if (found == ids.end()) {
//       if (!_graph_ctx->GetGraphCluster()->strict_dsl) {
//         continue;
//       }
//       DIDAGLE_ERROR("No field:{} found in processor:{}", data.field, _processor->Name());
//       return -1;
//     }
//     const DIObjectKey& exist_key = found->second.first;
//     DIObjectKey new_key{data.id, exist_key.id};
//     ids[data.field] = std::make_pair(new_key, &data);
//   }
//   return 0;
// }
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
      DIDAGLE_ERROR("No processor found for {}", _vertex->processor);
      return -1;
    }
  }
  _params = _vertex->args;
  _select_params = _vertex->select_args;

  if (nullptr != _processor) {
    _processor_di = new ProcessorDI(_processor, _graph_ctx->GetGraphCluster()->strict_dsl);
    if (0 != _processor_di->PrepareInputs(_vertex->input)) {
      return -1;
    }
    if (0 != _processor_di->PrepareOutputs(_vertex->output)) {
      return -1;
    }
    // if (0 != SetupInputOutputIds(_processor->GetInputIds(), _vertex->input, _input_ids)) {
    //   return -1;
    // }
    // if (0 != SetupInputOutputIds(_processor->GetOutputIds(), _vertex->output, _output_ids)) {
    //   return -1;
    // }
  }
  Reset();
  if (nullptr != _processor) {
    if (!_vertex->cond.empty()) {
      _params.SetString(_vertex->cond);
    }
    return _processor->Setup(_params);
  }
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
  if (nullptr != _subgraph_cluster) {
    _subgraph_cluster->Reset();
    _subgraph_cluster->GetCluster()->ReleaseContext(_subgraph_cluster);
    _subgraph_cluster = nullptr;
  }
  for (auto& args : _select_params) {
    args.args.SetParent(nullptr);
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
    if (nullptr != _processor_di) {
      _processor_di->CollectOutputs(_graph_ctx->GetGraphDataContextRef());
    }
    // for (const auto& pair : _output_ids) {
    //   const std::string& field = pair.first;
    //   const DIObjectKey& data = pair.second.first;
    //   int rc = _processor->EmitOutputField(_graph_ctx->GetGraphDataContextRef(), field,
    //   data.name); if (0 != rc) {
    //     // log
    //   }
    // }
  }
  _graph_ctx->OnVertexDone(this);
}
int VertexContext::ExecuteProcessor() {
  DIDAGLE_DEBUG("Vertex:{} begin execute", _vertex->GetDotLable());
  _processor->SetDataContext(_graph_ctx->GetGraphDataContext());
  Params* exec_params = nullptr;
  if (!_select_params.empty()) {
    for (auto& args : _select_params) {
      const bool* v = _graph_ctx->GetGraphDataContextRef().Get<bool>(args.match);
      if (nullptr == v) {
        DIDAGLE_DEBUG("0 Vertex:{} match {} null", _vertex->GetDotLable(), args.match);
        continue;
      }
      DIDAGLE_DEBUG("0 Vertex:{} match {} args {}", _vertex->GetDotLable(), args.match, *v);
      if (!(*v)) {
        continue;
      }
      DIDAGLE_DEBUG("Vertex:{} match {} args", _vertex->GetDotLable(), args.match);
      exec_params = &(args.args);
      break;
    }
  }
  if (nullptr == exec_params) {
    exec_params = &_params;
  }
  const Params* cluster_exec_params = _graph_ctx->GetGraphClusterContext()->GetExecuteParams();
  if (nullptr != cluster_exec_params) {
    exec_params->SetParent(cluster_exec_params);
  }
  if (0 != _processor_di->InjectInputs(_graph_ctx->GetGraphDataContextRef())) {
    DIDAGLE_DEBUG("Vertex:{} inject inputs failed", _vertex->GetDotLable());
    FinishVertexProcess(V_CODE_SKIP);
    return 0;
  }
  // for (const auto& pair : _input_ids) {
  //   const std::string& field = pair.first;
  //   const DIObjectKey& data = pair.second.first;
  //   const GraphData* graph_data = pair.second.second;
  //   bool required = false;
  //   if (nullptr != graph_data) {
  //     required = graph_data->required;
  //   }
  //   int rc = 0;
  //   if (nullptr != graph_data && !graph_data->aggregate.empty()) {
  //     for (const std::string& aggregate_id : graph_data->aggregate) {
  //       rc = _processor->InjectInputField(_graph_ctx->GetGraphDataContextRef(), field,
  //       aggregate_id,
  //                                         graph_data->move);
  //       if (0 != rc && required) {
  //         break;
  //       }
  //     }
  //   } else {
  //     if (nullptr != graph_data) {
  //       rc = _processor->InjectInputField(_graph_ctx->GetGraphDataContextRef(), field, data.name,
  //                                         graph_data->move);
  //     } else {
  //       rc = _processor->InjectInputField(_graph_ctx->GetGraphDataContextRef(), field, data.name,
  //                                         false);
  //     }
  //   }
  //   if (0 != rc && required) {
  //     _result = V_RESULT_ERR;
  //     _code = V_CODE_SKIP;
  //     break;
  //   }
  // }

  if (_processor->IsAsync()) {
    _processor->AsyncExecute(*exec_params, [this](int code) { FinishVertexProcess(code); });
  } else {
    int rc = _processor->Execute(*exec_params);
    FinishVertexProcess(rc);
  }
  return 0;
}
int VertexContext::ExecuteSubGraph() {
  if (_vertex->_graph->_cluster->_graph_manager) {
    _subgraph_cluster =
        _vertex->_graph->_cluster->_graph_manager->GetGraphClusterContext(_vertex->cluster);
  }
  if (!_subgraph_cluster) {
    DIDAGLE_ERROR("No subgraph cluster found for {}", _vertex->cluster);
    return -1;
  }
  _subgraph_cluster->SetGraphDataContext(_graph_ctx->GetGraphDataContext());
  _subgraph_cluster->SetExecuteParams(_graph_ctx->GetGraphClusterContext()->GetExecuteParams());
  _subgraph_cluster->Execute(_vertex->graph, [this](int code) { FinishVertexProcess(code); });
  return 0;
}
int VertexContext::Execute() {
  bool match_dep_expected_result = true;
  if (nullptr == _processor && nullptr == _subgraph_cluster) {
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
  DIDAGLE_DEBUG("Vertex:{} match deps result:{}.", _vertex->GetDotLable(),
                match_dep_expected_result);
  if (match_dep_expected_result) {
    if (!_vertex->expect_config.empty()) {
      const bool* v = _graph_ctx->GetGraphDataContextRef().Get<bool>(_vertex->expect_config);
      if (nullptr == v || !(*v)) {
        DIDAGLE_DEBUG("Vertex:{} match expect_config:{} failed.", _vertex->GetDotLable(),
                      _vertex->expect_config);
        match_dep_expected_result = false;
      } else {
        DIDAGLE_DEBUG("Vertex:{} match expect_config:{} success.", _vertex->GetDotLable(),
                      _vertex->expect_config);
      }
    }
  }

  if (!match_dep_expected_result) {
    _result = V_RESULT_ERR;
    _code = V_CODE_SKIP;
    // no need to exec this
    FinishVertexProcess(_code);
  } else {
    if (nullptr != _processor) {
      ExecuteProcessor();
    } else {
      ExecuteSubGraph();
    }
  }
  return 0;
}

GraphContext::GraphContext() : _cluster(nullptr), _graph(nullptr) {
  _join_vertex_num = 0;
  _data_ctx.reset(new GraphDataContext);
}

int GraphContext::Setup(GraphClusterContext* c, Graph* g) {
  _cluster = c;
  _graph = g;

  for (auto& pair : g->_nodes) {
    Vertex* v = pair.second;
    std::shared_ptr<VertexContext> c(new VertexContext);
    if (0 != c->Setup(this, v)) {
      DIDAGLE_ERROR("Graph:{} setup vertex:{} failed.", g->name, v->GetDotLable());
      return -1;
    }
    _vertex_context_table[v] = c;
    ProcessorDI* di = c->GetProcessorDI();
    if (nullptr != di) {
      for (const auto& pair : di->GetOutputIds()) {
        const DIObjectKey& key = pair.second.first;
        if (_all_output_ids.count(key) > 0) {
          DIDAGLE_ERROR("Duplicate output name:{} in graph:{}", key.name, g->name);
          return -1;
        }
        _all_output_ids.insert(key);
      }
    }
  }
  std::set<DIObjectKey> move_ids;
  for (auto& pair : _vertex_context_table) {
    std::shared_ptr<VertexContext> c = pair.second;
    ProcessorDI* di = c->GetProcessorDI();
    if (nullptr != di) {
      for (const auto& pair : di->GetInputIds()) {
        const DIObjectKey& key = pair.second.first;
        const GraphData* data = pair.second.second;
        if (nullptr != data && !data->is_extern && data->aggregate.empty()) {
          if (_all_output_ids.count(key) == 0) {
            DIDAGLE_ERROR(
                "Graph:{} have missing output field with name:{}, type_id:{} for vertex:{}.",
                g->name, key.name, key.id, c->GetVertex()->GetDotLable());
            return -1;
          }
        }
        _all_input_ids.insert(key);
        if (nullptr != data && data->move) {
          if (move_ids.count(key) > 0) {
            DIDAGLE_ERROR("Graph:{} have duplicate moved data name:{}.", g->name, key.name);
            return -1;
          }
          move_ids.insert(key);
        }
      }
    }
  }
  Reset();
  return 0;
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
  _data_ctx->Reset();
}
void GraphContext::ExecuteReadyVertexs(std::vector<VertexContext*>& ready_vertexs) {
  DIDAGLE_DEBUG("ExecuteReadyVertexs with {} vertexs.", ready_vertexs.size());
  if (ready_vertexs.size() == 1) {
    // inplace run
    ready_vertexs[0]->Execute();
  } else {
    for (VertexContext* ctx : ready_vertexs) {
      VertexContext* next = ctx;
      auto f = [next]() { next->Execute(); };
      const auto& exec_opts = _cluster->GetCluster()->GetGraphManager()->GetGraphExecuteOptions();
      exec_opts.concurrent_executor(f);
    }
  }
}
void GraphContext::OnVertexDone(VertexContext* vertex) {
  DIDAGLE_DEBUG("[{}]OnVertexDone while _join_vertex_num:{}.", vertex->GetVertex()->id,
                _join_vertex_num.load());
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
    uint32_t wait_num =
        successor_ctx->SetDependencyResult(vertex->GetVertex(), vertex->GetResult());
    DIDAGLE_DEBUG("[{}]Successor:{} wait_num:{}.", vertex->GetVertex()->id, successor->id,
                  wait_num);
    // last dependency
    if (1 == wait_num) {
      ready_successors.push_back(successor_ctx);
    }
  }
  ExecuteReadyVertexs(ready_successors);
}
GraphCluster* GraphContext::GetGraphCluster() { return _graph->_cluster; }
std::shared_ptr<GraphDataContext> GraphContext::GetGraphDataContext() { return _data_ctx; }
void GraphContext::SetGraphDataContext(std::shared_ptr<GraphDataContext> p) {
  _data_ctx->SetParent(p);
}

int GraphContext::Execute(DoneClosure&& done) {
  _done = std::move(done);
  // make all data entry precreated in data ctx
  for (const auto& id : _all_input_ids) {
    DIDAGLE_ERROR("Graph register input {}/{}", id.name, id.id);
    _data_ctx->RegisterData(id);
  }
  for (const auto& id : _all_output_ids) {
    DIDAGLE_ERROR("Graph register output {}/{}", id.name, id.id);
    _data_ctx->RegisterData(id);
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
GraphContext* GraphClusterContext::GetRunGraph(const std::string& name) {
  if (nullptr != _running_graph) {
    return _running_graph;
  }
  auto found = _graph_context_table.find(name);
  if (found == _graph_context_table.end()) {
    DIDAGLE_ERROR("No graph:{} found in cluster:{}", name, _cluster->_name);
    return nullptr;
  }
  _running_graph = found->second.get();
  _running_graph->SetGraphDataContext(_extern_data_ctx);
  return _running_graph;
}
void GraphClusterContext::Reset() {
  _exec_params = nullptr;
  _running_cluster.reset();
  if (nullptr != _running_graph) {
    _running_graph->Reset();
    _running_graph = nullptr;
  }
  for (Processor* p : _config_setting_processors) {
    p->Reset();
  }
  _config_setting_result.clear();
  _extern_data_ctx.reset();
  // _exec_options.reset();
  GraphClusterContext* sub_graph = nullptr;
  while (_sub_graphs.try_pop(sub_graph)) {
    sub_graph->Reset();
  }
}
int GraphClusterContext::Setup(GraphCluster* c) {
  _cluster = c;
  if (!_cluster) {
    return -1;
  }
  for (const auto& cfg : c->config_setting) {
    Processor* p = ProcessorFactory::GetProcessor(cfg.processor);
    if (nullptr == p && c->strict_dsl) {
      DIDAGLE_ERROR("No processor found for {}", cfg.processor);
      return -1;
    }
    if (nullptr != p) {
      Params args;
      args.SetString(cfg.cond);
      if (0 != p->Setup(args)) {
        DIDAGLE_ERROR("Failed to setup expr processor", cfg.processor);
        return -1;
      }
      _config_setting_processors.push_back(p);
    }
  }
  for (auto& graph_cfg : _cluster->graph) {
    std::shared_ptr<GraphContext> g(new GraphContext);
    if (0 != g->Setup(this, &graph_cfg)) {
      DIDAGLE_ERROR("Failed to setup graph:{}", graph_cfg.name);
      return -1;
    }
    _graph_context_table[graph_cfg.name] = g;
  }
  return 0;
}
int GraphClusterContext::Execute(const std::string& graph, DoneClosure&& done) {
  GraphContext* g = GetRunGraph(graph);
  if (nullptr == g) {
    if (done) {
      done(-1);
    }
    return -1;
  }
  GraphDataContext& data_ctx = g->GetGraphDataContextRef();
  _config_setting_result.assign(_config_setting_processors.size(), 0);
  DIDAGLE_DEBUG("config setting size = {}", _config_setting_processors.size());
  for (size_t i = 0; i < _config_setting_processors.size(); i++) {
    Processor* p = _config_setting_processors[i];
    p->SetDataContext(g->GetGraphDataContext());
    for (const auto& input_id : p->GetInputIds()) {
      p->InjectInputField(g->GetGraphDataContextRef(), input_id.name, input_id.name, false);
    }
    Params args;
    if (0 != p->Execute(args)) {
      _config_setting_result[i] = 0;
    } else {
      _config_setting_result[i] = 1;
    }
    bool* v = (bool*)(&_config_setting_result[i]);
    DIDAGLE_DEBUG("Set config setting:{} to {}", _cluster->config_setting[i].name, *v);
    data_ctx.Set<bool>(_cluster->config_setting[i].name, v);
  }
  /**
   * @brief diable data entry creation since the graph is executing while the creation is not
   * thread-safe.
   */
  data_ctx.DisableEntryCreation();
  return g->Execute(std::move(done));
}

GraphClusterContext::~GraphClusterContext() {
  for (Processor* p : _config_setting_processors) {
    delete p;
  }
}

}  // namespace didagle
