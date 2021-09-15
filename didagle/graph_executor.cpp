// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "graph_executor.h"
#include <sys/time.h>
#include <time.h>
#include <regex>
#include "didagle_event.h"
#include "didagle_log.h"
#include "graph.h"
#include "graph_processor.h"

namespace didagle {
static inline uint64_t ustime() {
  struct timeval tv;
  uint64_t ust;
  gettimeofday(&tv, nullptr);
  ust = ((long long)tv.tv_sec) * 1000000;
  ust += tv.tv_usec;
  return ust;
}

VertexContext::~VertexContext() {
  delete _processor;
  delete _processor_di;
  // delete _subgraph;
}

int VertexContext::Setup(GraphContext *g, Vertex *v) {
  _graph_ctx = g;
  _vertex = v;
  // todo get processor
  if (!_vertex->graph.empty()) {
    // get subgraph at runtime
    // remove suffix
    size_t lastindex = _vertex->cluster.find_last_of(".");
    if (lastindex != std::string::npos) {
      _full_graph_name = _vertex->cluster.substr(0, lastindex);
    } else {
      _full_graph_name = _vertex->cluster;
    }
    _full_graph_name.append("_").append(_vertex->graph);
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
  _exec_start_ustime = 0;
  _result = V_RESULT_INVALID;
  _code = V_CODE_INVALID;
  _deps_results.assign(_vertex->_deps_idx.size(), V_RESULT_INVALID);
  _waiting_num = _vertex->_deps_idx.size();
  if (nullptr != _processor) {
    _processor->Reset();
  }
  _subgraph_ctx = nullptr;
  if (nullptr != _subgraph_cluster) {
    _subgraph_cluster->Reset();
    _subgraph_cluster->GetCluster()->ReleaseContext(_subgraph_cluster);
    _subgraph_cluster = nullptr;
  }

  for (auto &args : _select_params) {
    args.args.SetParent(nullptr);
  }
  _params.SetParent(nullptr);
}
uint32_t VertexContext::SetDependencyResult(Vertex *v, VertexResult r) {
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
  }
  DAGEventTracker *tracker = _graph_ctx->GetGraphDataContextRef().GetEventTracker();
  if (nullptr != tracker) {
    std::unique_ptr<DAGEvent> event(new DAGEvent);
    event->start_ustime = _exec_start_ustime;
    event->end_ustime = ustime();
    if (nullptr != _processor) {
      event->processor = _processor->Name();
    } else {
      event->graph = _vertex->graph;
      event->cluster = _vertex->_graph->_cluster->_name;
      event->full_graph_name = _full_graph_name;
    }
    event->rc = code;
    tracker->events.push(std::move(event));
  }
  if (nullptr != _subgraph_ctx) {
    _graph_ctx->GetGraphDataContextRef().SetChild(_subgraph_ctx->GetGraphDataContext(), _child_idx);
  }
  _graph_ctx->OnVertexDone(this);
}
int VertexContext::ExecuteProcessor() {
  DIDAGLE_DEBUG("Vertex:{} begin execute", _vertex->GetDotLable());
  _processor->SetDataContext(_graph_ctx->GetGraphDataContext());
  Params *exec_params = nullptr;
  if (!_select_params.empty()) {
    for (auto &args : _select_params) {
      const bool *v = _graph_ctx->GetGraphDataContextRef().Get<bool>(args.match);
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
  const Params *cluster_exec_params = _graph_ctx->GetGraphClusterContext()->GetExecuteParams();
  if (nullptr != cluster_exec_params) {
    exec_params->SetParent(cluster_exec_params);
  }
  if (0 != _processor_di->InjectInputs(_graph_ctx->GetGraphDataContextRef())) {
    DIDAGLE_DEBUG("Vertex:{} inject inputs failed", _vertex->GetDotLable());
    FinishVertexProcess(V_CODE_SKIP);
    return 0;
  }
  if (_processor->IsAsync()) {
    _processor->AsyncExecute(*exec_params, [this](int code) {
      FinishVertexProcess(
          (_vertex->ignore_processor_execute_error && !_vertex->IsCondVertex()) ? 0 : code);
    });
  } else {
    int rc = _processor->Execute(*exec_params);
    FinishVertexProcess((_vertex->ignore_processor_execute_error && !_vertex->IsCondVertex()) ? 0
                                                                                              : rc);
  }
  return 0;
}
int VertexContext::ExecuteSubGraph() {
  if (!_subgraph_cluster) {
    DIDAGLE_ERROR("No subgraph cluster found for {}", _vertex->cluster);
    return -1;
  }
  _subgraph_cluster->SetExternGraphDataContext(_graph_ctx->GetGraphDataContext());
  _subgraph_cluster->SetExecuteParams(_graph_ctx->GetGraphClusterContext()->GetExecuteParams());
  _subgraph_cluster->Execute(
      _vertex->graph, [this](int code) { FinishVertexProcess(code); }, _subgraph_ctx);
  return 0;
}
int VertexContext::Execute() {
  _exec_start_ustime = ustime();
  bool match_dep_expected_result = true;
  if (!_vertex->cluster.empty() && nullptr != _vertex->_graph->_cluster->_graph_manager &&
      nullptr == _subgraph_cluster) {
    _subgraph_cluster =
        _vertex->_graph->_cluster->_graph_manager->GetGraphClusterContext(_vertex->cluster);
  }
  if (nullptr == _processor && nullptr == _subgraph_cluster) {
    match_dep_expected_result = false;
    DIDAGLE_DEBUG("Vertex:{} has empty processor and empty subgraph context.",
                  _vertex->GetDotLable());
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
      std::string_view var_name = _vertex->expect_config;
      bool not_cond = false;
      if (var_name[0] == '!') {
        var_name = var_name.substr(1);
        not_cond = true;
      }
      const bool *v = _graph_ctx->GetGraphDataContextRef().Get<bool>(var_name);
      bool match_result = false;
      if (nullptr != v) {
        match_result = *v;
      }
      if (not_cond) {
        match_result = !match_result;
      }
      if (!match_result) {
        match_dep_expected_result = false;
        DIDAGLE_DEBUG("Vertex:{} match expect_config:{} failed.", _vertex->GetDotLable(),
                      _vertex->expect_config);
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

GraphContext::GraphContext() : _cluster(nullptr), _graph(nullptr), _children_count(0) {
  _join_vertex_num = 0;
  _data_ctx.reset(new GraphDataContext);
}

int GraphContext::Setup(GraphClusterContext *c, Graph *g) {
  _cluster = c;
  _graph = g;

  size_t child_idx = 0;
  for (auto &pair : g->_nodes) {
    Vertex *v = pair.second;
    std::shared_ptr<VertexContext> c(new VertexContext);
    if (0 != c->Setup(this, v)) {
      DIDAGLE_ERROR("Graph:{} setup vertex:{} failed.", g->name, v->GetDotLable());
      return -1;
    }
    if (!v->cluster.empty()) {
      c->SetChildIdx(child_idx);
      _children_count++;
      child_idx++;
    }
    _vertex_context_table[v] = c;
    ProcessorDI *di = c->GetProcessorDI();
    if (nullptr != di) {
      for (const auto &pair : di->GetOutputIds()) {
        const DIObjectKey &key = pair.second.first;
        if (_all_output_ids.count(key) > 0) {
          DIDAGLE_ERROR("Duplicate output name:{} in graph:{}", key.name, g->name);
          return -1;
        }
        _all_output_ids.insert(key);
      }
    }
  }
  _data_ctx->ReserveChildCapacity(_children_count);
  std::set<DIObjectKey> move_ids;
  for (auto &pair : _vertex_context_table) {
    std::shared_ptr<VertexContext> c = pair.second;
    ProcessorDI *di = c->GetProcessorDI();
    if (nullptr != di) {
      for (const auto &pair : di->GetInputIds()) {
        const DIObjectKey &key = pair.second.first;
        const GraphData *data = pair.second.second;
        if (nullptr != data && !data->is_extern && data->aggregate.empty()) {
          if (_all_output_ids.count(key) == 0) {
            DIDAGLE_ERROR(
                "Graph:{} have missing output field with name:{}, "
                "type_id:{} for vertex:{}.",
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

VertexContext *GraphContext::FindVertexContext(Vertex *v) {
  auto found = _vertex_context_table.find(v);
  if (found == _vertex_context_table.end()) {
    return nullptr;
  }
  return found->second.get();
}
void GraphContext::Reset() {
  _join_vertex_num = _vertex_context_table.size();
  for (auto &pair : _vertex_context_table) {
    std::shared_ptr<VertexContext> ctx = pair.second;
    ctx->Reset();
  }
  _data_ctx->Reset();
}
void GraphContext::ExecuteReadyVertexs(std::vector<VertexContext *> &ready_vertexs) {
  DIDAGLE_DEBUG("ExecuteReadyVertexs with {} vertexs.", ready_vertexs.size());
  if (ready_vertexs.size() == 1) {
    // inplace run
    ready_vertexs[0]->Execute();
  } else {
    for (VertexContext *ctx : ready_vertexs) {
      VertexContext *next = ctx;
      auto f = [next]() { next->Execute(); };
      const auto &exec_opts = _cluster->GetCluster()->GetGraphManager()->GetGraphExecuteOptions();
      exec_opts.concurrent_executor(f);
    }
  }
}
void GraphContext::OnVertexDone(VertexContext *vertex) {
  DIDAGLE_DEBUG("[{}]OnVertexDone while _join_vertex_num:{}.", vertex->GetVertex()->id,
                _join_vertex_num.load());
  if (1 == _join_vertex_num.fetch_sub(1)) {  // last vertex
    if (_done) {
      _done(0);
      _done = 0;
    }
    return;
  }
  std::vector<VertexContext *> ready_successors;
  for (Vertex *successor : vertex->GetVertex()->_successor_vertex) {
    VertexContext *successor_ctx = FindVertexContext(successor);
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
GraphCluster *GraphContext::GetGraphCluster() { return _graph->_cluster; }
GraphDataContext *GraphContext::GetGraphDataContext() { return _data_ctx.get(); }
void GraphContext::SetGraphDataContext(GraphDataContext *p) { _data_ctx->SetParent(p); }

int GraphContext::Execute(DoneClosure &&done) {
  _done = std::move(done);
  // make all data entry precreated in data ctx
  for (const auto &id : _all_input_ids) {
    // DIDAGLE_ERROR("Graph register input {}/{}", id.name, id.id);
    _data_ctx->RegisterData(id);
  }
  for (const auto &id : _all_output_ids) {
    // DIDAGLE_ERROR("Graph register output {}/{}", id.name, id.id);
    _data_ctx->RegisterData(id);
  }
  std::vector<VertexContext *> ready_successors;
  for (auto &pair : _vertex_context_table) {
    std::shared_ptr<VertexContext> ctx = pair.second;
    if (ctx->Ready()) {
      ready_successors.push_back(ctx.get());
    }
  }
  ExecuteReadyVertexs(ready_successors);
  return 0;
}
GraphContext *GraphClusterContext::GetRunGraph(const std::string &name) {
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
  for (auto &item : _config_settings) {
    item.eval_proc->Reset();
    item.result = 0;
  }
  //_extern_data_ctx.reset();
  _extern_data_ctx = nullptr;
  // _exec_options.reset();
  GraphClusterContext *sub_graph = nullptr;
  while (_sub_graphs.try_pop(sub_graph)) {
    sub_graph->Reset();
  }
}
int GraphClusterContext::Setup(GraphCluster *c) {
  _cluster = c;
  if (!_cluster) {
    return -1;
  }

  for (const auto &cfg : c->config_setting) {
    Processor *p = ProcessorFactory::GetProcessor(cfg.processor);
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
      ConfigSettingContext item;
      item.eval_proc = p;
      _config_settings.push_back(item);
    }
  }
  for (auto &graph_cfg : _cluster->graph) {
    std::shared_ptr<GraphContext> g(new GraphContext);
    if (0 != g->Setup(this, &graph_cfg)) {
      DIDAGLE_ERROR("Failed to setup graph:{}", graph_cfg.name);
      return -1;
    }
    _graph_context_table[graph_cfg.name] = g;
  }
  return 0;
}
int GraphClusterContext::Execute(const std::string &graph, DoneClosure &&done,
                                 GraphContext *&graph_ctx) {
  graph_ctx = nullptr;
  GraphContext *g = GetRunGraph(graph);
  if (nullptr == g) {
    if (done) {
      done(-1);
    }
    return -1;
  }
  graph_ctx = g;
  GraphDataContext &data_ctx = g->GetGraphDataContextRef();
  //_config_setting_result.assign(_config_setting_processors.size(), 0);
  DIDAGLE_DEBUG("config setting size = {}", _config_settings.size());
  for (size_t i = 0; i < _config_settings.size(); i++) {
    Processor *p = _config_settings[i].eval_proc;
    p->SetDataContext(g->GetGraphDataContext());
    for (const auto &input_id : p->GetInputIds()) {
      p->InjectInputField(g->GetGraphDataContextRef(), input_id.name, input_id.name, false);
    }
    Params args;
    if (0 != p->Execute(args)) {
      _config_settings[i].result = 0;
    } else {
      _config_settings[i].result = 1;
    }
    bool *v = (bool *)(&_config_settings[i].result);
    // DIDAGLE_DEBUG("Set config setting:{} to {}",
    // _cluster->config_setting[i].var, *v);
    if (*v) {
      data_ctx.Set<bool>(_cluster->config_setting[i].name, v);
    }
  }
  /**
   * @brief diable data entry creation since the graph is executing while the
   * creation is not thread-safe.
   */
  data_ctx.DisableEntryCreation();
  return g->Execute(std::move(done));
}

GraphClusterContext::~GraphClusterContext() {
  for (auto &item : _config_settings) {
    delete item.eval_proc;
  }
}

}  // namespace didagle
