// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "didagle/graph_executor.h"

#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include <cstdint>
#include <memory>
#include <regex>
#include <set>
#include <utility>

#include "didagle/didagle_event.h"
#include "didagle/didagle_log.h"
#include "didagle/graph.h"
#include "didagle/graph_processor.h"
#include "folly/executors/InlineExecutor.h"

namespace didagle {
static inline uint64_t ustime() {
  struct timeval tv;
  uint64_t ust;
  gettimeofday(&tv, nullptr);
  ust = ((uint64_t)tv.tv_sec) * 1000000;
  ust += tv.tv_usec;
  return ust;
}

VertexContext::~VertexContext() {
  delete _processor;
  delete _processor_di;
  for (auto select : _select_contexts) {
    if (select.p != nullptr) {
      delete select.p;
    }
  }
  // delete _subgraph;
}

int VertexContext::Setup(GraphContext* g, Vertex* v) {
  _graph_ctx = g;
  _vertex = v;
  // todo get processor
  if (!_vertex->graph.empty()) {
    if (!_vertex->while_cluster.empty()) {
      _processor = ProcessorFactory::GetProcessor(kDefaultWhileOperatorName);
    } else {
      // get subgraph at runtime
      // remove suffix
      size_t lastindex = _vertex->cluster.find_last_of(".");
      if (lastindex != std::string::npos) {
        _full_graph_name = _vertex->cluster.substr(0, lastindex);
      } else {
        _full_graph_name = _vertex->cluster;
      }
      _full_graph_name.append("_").append(_vertex->graph);
    }
  } else {
    _processor = ProcessorFactory::GetProcessor(_vertex->processor);
    if (_processor) {
      _processor->id_ = _vertex->id;
    }
  }
  if (_graph_ctx->GetGraphCluster()->strict_dsl) {
    if (nullptr == _processor && !_vertex->processor.empty()) {
      DIDAGLE_ERROR("No processor found for {}", _vertex->processor);
      return -1;
    }
  }
  _params = _vertex->args;

  for (const auto& select : _vertex->select_args) {
    SelectCondParamsContext select_ctx;
    select_ctx.param = select;
    if (select.IsCondExpr()) {
      select_ctx.p = ProcessorFactory::GetProcessor(_graph_ctx->GetGraphCluster()->default_expr_processor);
      if (select_ctx.p == nullptr && _graph_ctx->GetGraphCluster()->strict_dsl) {
        DIDAGLE_ERROR("No processor found for {}", _graph_ctx->GetGraphCluster()->default_expr_processor);
        return -1;
      }
      if (nullptr != select_ctx.p) {
        didagle::Params param;
        param.SetString(select.match);
        if (0 != select_ctx.p->Setup(param)) {
          DIDAGLE_ERROR("Failed to setup expr processor", _graph_ctx->GetGraphCluster()->default_expr_processor);
          return -1;
        }
      }
    }
    _select_contexts.emplace_back(select_ctx);
  }

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
    if (!_vertex->while_cluster.empty()) {
      _params[kExprParamKey].SetString(_vertex->while_cluster);
      _params[kWhileExecCluterParamKey].SetString(_vertex->cluster);
      _params[kWhileExecGraphParamKey].SetString(_vertex->graph);
      _params[kWhileAsyncExecParamKey].SetBool(_vertex->while_async);
    }
    return _processor->Setup(_params);
  }
  return 0;
}

void VertexContext::Reset() {
  _exec_start_ustime = 0;
  _result = V_RESULT_INVALID;
  _code = V_CODE_INVALID;
  _exec_rc = INT_MAX;
  _deps_results.assign(_vertex->_deps_idx.size(), V_RESULT_INVALID);
  _waiting_num = _vertex->_deps_idx.size();
  if (nullptr != _processor) {
    _processor->Reset();
  }
  _subgraph_ctx = nullptr;
  if (nullptr != _subgraph_cluster) {
    std::shared_ptr<GraphCluster> runing_cluster = _subgraph_cluster->GetRunningCluster();
    _subgraph_cluster->Reset();
    _subgraph_cluster->GetCluster()->ReleaseContext(_subgraph_cluster);
    _subgraph_cluster = nullptr;
    runing_cluster.reset();
  }

  for (auto& select : _select_contexts) {
    select.param.args.SetParent(nullptr);
  }
  _params.SetParent(nullptr);
  _exec_params = nullptr;
  _exec_mathced_cond = "";
}

VertexContext::VertexContext() { _waiting_num = 0; }

void VertexContext::SetupSuccessors() {
  for (Vertex* successor : GetVertex()->_successor_vertex) {
    VertexContext* successor_ctx = _graph_ctx->FindVertexContext(successor);
    if (nullptr == successor_ctx) {
      // error
      abort();
      continue;
    }
    _successor_ctxs.emplace_back(successor_ctx);
    int idx = successor->GetDependencyIndex(_vertex);
    _successor_dep_idxs.emplace_back(idx);
  }
}

void VertexContext::FinishVertexProcess(int code) {
  _code = (VertexErrCode)code;
  DAGEventTracker* tracker = _graph_ctx->GetGraphDataContextRef().GetEventTracker();
  if (_exec_rc == INT_MAX) {
    _exec_rc = _code;
  }
  auto exec_end_ustime = ustime();
  if (0 != _code) {
    _result = V_RESULT_ERR;
    if (nullptr != _processor_di) {
      _processor_di->MoveDataWhenSkipped(_graph_ctx->GetGraphDataContextRef());
    }
  } else {
    _result = V_RESULT_OK;
    if (nullptr != _processor_di) {
      uint64_t post_exec_start_ustime = ustime();
      _processor_di->CollectOutputs(_graph_ctx->GetGraphDataContextRef(), _exec_params);
      if (nullptr != tracker) {
        auto event = std::make_unique<DAGEvent>();
        event->start_ustime = post_exec_start_ustime;
        event->end_ustime = ustime();
        event->phase = PhaseType::DAG_PHASE_OP_POST_EXECUTE;
        tracker->Add(std::move(event));
      }
    }
  }
  if (nullptr != tracker) {
    auto event = std::make_unique<DAGEvent>();
    event->start_ustime = _exec_start_ustime;
    event->end_ustime = exec_end_ustime;
    if (nullptr != _processor) {
      event->processor = _processor->Name();
    } else {
      event->graph = _vertex->graph;
      event->cluster = _vertex->_graph->_cluster->_name;
      event->full_graph_name = _full_graph_name;
    }
    event->matched_cond = _exec_mathced_cond;
    event->rc = _exec_rc;
    tracker->Add(std::move(event));
  }
  if (nullptr != _subgraph_ctx) {
    _graph_ctx->GetGraphDataContextRef().SetChild(_subgraph_ctx->GetGraphDataContext(), _child_idx);
  }
  _graph_ctx->OnVertexDone(this);
}
const Params* VertexContext::GetExecParams(std::string_view* matched_cond) {
  Params* exec_params = nullptr;
  const Params* cluster_exec_params = _graph_ctx->GetGraphClusterContext()->GetExecuteParams();
  // std::string_view matched_cond;
  if (!_select_contexts.empty()) {
    for (auto& select : _select_contexts) {
      auto& args = select.param;
      if (args.IsCondExpr() && nullptr != select.p) {
        select.p->SetDataContext(_graph_ctx->GetGraphDataContext());
        int eval_rc;
        if (cluster_exec_params != nullptr) {
          eval_rc = select.p->Execute(*cluster_exec_params);
        } else {
          Params params;
          eval_rc = select.p->Execute(params);
        }
        if (eval_rc == 0) {
          exec_params = &(args.args);
          *matched_cond = select.p->GetString(Processor::GetStringMode::kDefault);
          break;
        }
      } else {
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
        *matched_cond = args.match;
        break;
      }
    }
  }
  if (nullptr == exec_params) {
    exec_params = &_params;
  }

  if (nullptr != cluster_exec_params) {
    exec_params->SetParent(cluster_exec_params);
  }
  return exec_params;
}

int VertexContext::ExecuteProcessor() {
  DIDAGLE_DEBUG("Vertex:{} begin execute", _vertex->GetDotLable());
  auto prepare_start_us = ustime();
  _processor->SetDataContext(_graph_ctx->GetGraphDataContext());
  _exec_params = GetExecParams(&_exec_mathced_cond);
  _processor->Prepare(*_exec_params);
  if (0 != _processor_di->InjectInputs(_graph_ctx->GetGraphDataContextRef(), _exec_params)) {
    DIDAGLE_DEBUG("Vertex:{} inject inputs failed", _vertex->GetDotLable());
    FinishVertexProcess(V_CODE_SKIP);
    return 0;
  }
  auto prepare_end_ustime = ustime();
  DAGEventTracker* tracker = _graph_ctx->GetGraphDataContextRef().GetEventTracker();
  if (nullptr != tracker) {
    auto event = std::make_unique<DAGEvent>();
    event->start_ustime = prepare_start_us;
    event->end_ustime = prepare_end_ustime;
    event->phase = PhaseType::DAG_PHASE_OP_PREPARE_EXECUTE;
    tracker->Add(std::move(event));
  }
  _exec_start_ustime = ustime();
  switch (_processor->GetExecMode()) {
    case Processor::ExecMode::EXEC_ASYNC_FUTURE: {
      try {
        _processor->FutureExecute(*_exec_params).thenValue([this](int rc) {
          _exec_rc = rc;
          FinishVertexProcess((_vertex->ignore_processor_execute_error && !_vertex->IsCondVertex()) ? 0 : _exec_rc);
        });
      } catch (std::exception& ex) {
        DIDAGLE_ERROR("Vertex:{} execute with caught excetion:{} ", _vertex->GetDotLable(), ex.what());
        _exec_rc = V_CODE_ERR;
        FinishVertexProcess((_vertex->ignore_processor_execute_error && !_vertex->IsCondVertex()) ? 0 : _exec_rc);
      } catch (...) {
        DIDAGLE_ERROR("Vertex:{} execute with caught unknown excetion.", _vertex->GetDotLable());
        _exec_rc = V_CODE_ERR;
        FinishVertexProcess((_vertex->ignore_processor_execute_error && !_vertex->IsCondVertex()) ? 0 : _exec_rc);
      }
      break;
    }
#if ISPINE_HAS_COROUTINES
    case Processor::ExecMode::EXEC_STD_COROUTINE: {
      folly::QueuedImmediateExecutor* ex = &(folly::QueuedImmediateExecutor::instance());
      ispine::coro_spawn(ex, _processor->CoroExecute(*_exec_params)).via(ex).thenValue([this](int rc) {
        _exec_rc = rc;
        FinishVertexProcess((_vertex->ignore_processor_execute_error && !_vertex->IsCondVertex()) ? 0 : _exec_rc);
      });
      break;
    }
#endif
    case Processor::ExecMode::EXEC_ADAPTIVE: {
#if ISPINE_HAS_COROUTINES
      folly::QueuedImmediateExecutor* ex = &(folly::QueuedImmediateExecutor::instance());
      ispine::coro_spawn(ex, _processor->AExecute(*_exec_params)).via(ex).thenValue([this](int rc) {
        _exec_rc = rc;
        FinishVertexProcess((_vertex->ignore_processor_execute_error && !_vertex->IsCondVertex()) ? 0 : _exec_rc);
      });
#else
      try {
        _exec_rc = _processor->AExecute(*_exec_params);
      } catch (std::exception& ex) {
        DIDAGLE_ERROR("Vertex:{} execute with caught excetion:{} ", _vertex->GetDotLable(), ex.what());
        _exec_rc = V_CODE_ERR;
      } catch (...) {
        DIDAGLE_ERROR("Vertex:{} execute with caught unknown excetion.", _vertex->GetDotLable());
        _exec_rc = V_CODE_ERR;
      }
      FinishVertexProcess((_vertex->ignore_processor_execute_error && !_vertex->IsCondVertex()) ? 0 : _exec_rc);
#endif
      break;
    }
    default: {
      try {
        _exec_rc = _processor->Execute(*_exec_params);
      } catch (std::exception& ex) {
        DIDAGLE_ERROR("Vertex:{} execute with caught excetion:{} ", _vertex->GetDotLable(), ex.what());
        _exec_rc = V_CODE_ERR;
      } catch (...) {
        DIDAGLE_ERROR("Vertex:{} execute with caught unknown excetion.", _vertex->GetDotLable());
        _exec_rc = V_CODE_ERR;
      }
      FinishVertexProcess((_vertex->ignore_processor_execute_error && !_vertex->IsCondVertex()) ? 0 : _exec_rc);
      break;
    }
  }
  return 0;
}
int VertexContext::ExecuteSubGraph() {
  if (!_subgraph_cluster) {
    DIDAGLE_ERROR("No subgraph cluster found for {}", _vertex->cluster);
    return -1;
  }
  _exec_start_ustime = ustime();
  _exec_params = GetExecParams(&_exec_mathced_cond);
  _subgraph_cluster->SetExternGraphDataContext(_graph_ctx->GetGraphDataContext());
  _subgraph_cluster->SetExecuteParams(_exec_params);
  // succeed end time
  _subgraph_cluster->SetEndTime(_graph_ctx->GetGraphClusterContext()->GetEndTime());
  _subgraph_cluster->Execute(
      _vertex->graph, [this](int code) { FinishVertexProcess(code); }, _subgraph_ctx);
  return 0;
}
int VertexContext::Execute() {
  bool match_dep_expected_result = true;
  if (!_vertex->cluster.empty() && nullptr != _vertex->_graph->_cluster->_graph_manager &&
      nullptr == _subgraph_cluster) {
    _subgraph_cluster = _vertex->_graph->_cluster->_graph_manager->GetGraphClusterContext(_vertex->cluster);
  }
  if (nullptr == _processor && nullptr == _subgraph_cluster) {
    match_dep_expected_result = false;
    DIDAGLE_DEBUG("Vertex:{} has empty processor and empty subgraph context.", _vertex->GetDotLable());
  } else {
    for (size_t i = 0; i < _deps_results.size(); i++) {
      if (V_RESULT_INVALID == _deps_results[i] || (0 == (_deps_results[i] & _vertex->_deps_expected_results[i]))) {
        match_dep_expected_result = false;
        break;
      }
    }
  }
  DIDAGLE_DEBUG("Vertex:{} match deps result:{}.", _vertex->GetDotLable(), match_dep_expected_result);
  if (match_dep_expected_result) {
    if (!_vertex->expect_config.empty()) {
      std::string_view var_name = _vertex->expect_config;
      bool not_cond = false;
      if (var_name[0] == '!') {
        var_name = var_name.substr(1);
        not_cond = true;
      }
      const bool* v = _graph_ctx->GetGraphDataContextRef().Get<bool>(var_name);
      bool match_result = false;
      if (nullptr != v) {
        match_result = *v;
      }
      if (not_cond) {
        match_result = !match_result;
      }
      if (!match_result) {
        match_dep_expected_result = false;
        DIDAGLE_DEBUG("Vertex:{} match expect_config:{} failed.", _vertex->GetDotLable(), _vertex->expect_config);
      } else {
        DIDAGLE_DEBUG("Vertex:{} match expect_config:{} success.", _vertex->GetDotLable(), _vertex->expect_config);
      }
    }
  }

  if (!match_dep_expected_result || (_graph_ctx->GetGraphClusterContext()->GetEndTime() != 0 &&
                                     ustime() >= _graph_ctx->GetGraphClusterContext()->GetEndTime())) {
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
  // _data_ctx.reset(new GraphDataContext);
  _data_ctx = GraphDataContext::New();
}

int GraphContext::Setup(GraphClusterContext* c, Graph* g) {
  _cluster = c;
  _graph = g;

  size_t child_idx = 0;
  std::set<DIObjectKey> all_output_ids;
  for (auto& pair : g->_nodes) {
    Vertex* v = pair.second;
    auto c = std::make_shared<VertexContext>();
    // std::shared_ptr<VertexContext> c(new VertexContext);
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
    ProcessorDI* di = c->GetProcessorDI();
    if (nullptr != di) {
      for (const auto& entry : di->GetOutputIds()) {
        const DIObjectKey& key = entry.info;
        if (all_output_ids.count(key) > 0) {
          DIDAGLE_ERROR("Duplicate output name:{} in graph:{}", key.name, g->name);
          return -1;
        }
        all_output_ids.insert(key);
      }
    }
  }
  for (auto& [_, vetex_ctx] : _vertex_context_table) {
    vetex_ctx->SetupSuccessors();
  }
  _data_ctx->ReserveChildCapacity(_children_count);
  // std::set<DIObjectKey> move_ids;
  for (auto& pair : _vertex_context_table) {
    std::shared_ptr<VertexContext>& c = pair.second;
    ProcessorDI* di = c->GetProcessorDI();
    if (nullptr != di) {
      for (auto& entry : di->GetInputIds()) {
        const DIObjectKey& key = entry.info;
        const GraphData* data = entry.data;
        if (nullptr != data && !data->is_extern && data->aggregate.empty()) {
          if (all_output_ids.count(key) == 0) {
            DIDAGLE_ERROR(
                "Graph:{} have missing output field with name:{}, "
                "type_id:{} for vertex:{}.",
                g->name, key.name, key.id, c->GetVertex()->GetDotLable());
            return -1;
          }
        }
        if (!entry.info.flags.is_extern) {
          entry.idx = static_cast<int32_t>(_data_ctx->RegisterData(key));
        }
        //_all_input_ids.insert(key);
        // if (nullptr != data && data->move) {
        //   if (move_ids.count(key) > 0) {
        //     DIDAGLE_ERROR("Graph:{} have duplicate moved data name:{}.",
        //     g->name, key.name); return -1;
        //   }
        //   move_ids.insert(key);
        // }
      }
      for (auto& entry : di->GetOutputIds()) {
        const DIObjectKey& key = entry.info;
        if (key.name[0] != '$') {
          entry.idx = static_cast<int32_t>(_data_ctx->RegisterData(key));
        }
      }
    }
  }

  for (auto& pair : _vertex_context_table) {
    std::shared_ptr<VertexContext>& ctx = pair.second;
    if (ctx->Ready()) {
      _start_ctxs.emplace_back(ctx.get());
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
    std::shared_ptr<VertexContext>& ctx = pair.second;
    ctx->Reset();
  }
  _data_ctx->Reset();
}
void GraphContext::ExecuteReadyVertexs(std::vector<VertexContext*>& ready_vertexs) {
  DIDAGLE_DEBUG("ExecuteReadyVertexs with {} vertexs.", ready_vertexs.size());
  if (ready_vertexs.empty()) {
    return;
  }
  if (ready_vertexs.size() == 1) {
    // inplace run
    ready_vertexs[0]->Execute();
  } else {
    VertexContext* local_execute = nullptr;
    for (size_t i = 0; i < ready_vertexs.size(); i++) {
      // select first non IO op to run directly
      if (nullptr == ready_vertexs[i]->GetProcessor()) {
        // use subgraph as local executor
        local_execute = ready_vertexs[i];
        break;
      }
      if (!ready_vertexs[i]->GetProcessor()->isIOProcessor()) {
        local_execute = ready_vertexs[i];
        break;
      }
    }
    // select first op run directly if there is no non IO op
    if (nullptr == local_execute) {
      local_execute = ready_vertexs[0];
    }
    const auto& exec_opts = _cluster->GetCluster()->GetGraphManager()->GetGraphExecuteOptions();
    uint64_t sched_start_ustime = ustime();
    DAGEventTracker* tracker = _data_ctx->GetEventTracker();
    for (VertexContext* ctx : ready_vertexs) {
      if (ctx == local_execute) {
        continue;
      }
      VertexContext* next = ctx;
      exec_opts.concurrent_executor([tracker, next, sched_start_ustime]() {
        if (nullptr != tracker) {
          auto event = std::make_unique<DAGEvent>();
          event->start_ustime = sched_start_ustime;
          event->end_ustime = ustime();
          event->phase = PhaseType::DAG_PHASE_CONCURRENT_SCHED;
          tracker->Add(std::move(event));
        }
        next->Execute();
      });
    }
    local_execute->Execute();
  }
}
void GraphContext::OnVertexDone(VertexContext* vertex) {
  DIDAGLE_DEBUG("[{}]OnVertexDone while _join_vertex_num:{}.", vertex->GetVertex()->id, _join_vertex_num.load());
  if (1 == _join_vertex_num.fetch_sub(1)) {  // last vertex
    if (_done) {
      _done(0);
    }
    return;
  }
  std::vector<VertexContext*> ready_successors;
  size_t successor_num = vertex->_successor_ctxs.size();
  for (size_t i = 0; i < successor_num; i++) {
    // VertexContext* successor_ctx = FindVertexContext(successor);
    // if (nullptr == successor_ctx) {
    //   // error
    //   abort();
    //   continue;
    // }
    VertexContext* successor_ctx = vertex->_successor_ctxs[i];
    uint32_t wait_num = successor_ctx->SetDependencyResult(vertex->_successor_dep_idxs[i], vertex->GetResult());
    // DIDAGLE_DEBUG("[{}]Successor:{} wait_num:{}.", vertex->GetVertex()->id, successor->id, wait_num);
    //  last dependency
    if (1 == wait_num) {
      if (i == successor_num - 1 && ready_successors.empty()) {
        ExecuteReadyVertex(successor_ctx);
        return;
      } else {
        ready_successors.emplace_back(successor_ctx);
      }
    }
  }
  ExecuteReadyVertexs(ready_successors);
}
GraphCluster* GraphContext::GetGraphCluster() { return _graph->_cluster; }
// GraphDataContext* GraphContext::GetGraphDataContext() { return _data_ctx.get(); }
void GraphContext::SetGraphDataContext(GraphDataContext* p) { _data_ctx->SetParent(p); }

int GraphContext::Execute(DoneClosure&& done) {
  _done = std::move(done);
  // make all data entry precreated in data ctx
  // for (const auto& id : _all_input_ids) {
  //   _data_ctx->RegisterData(id);
  // }
  // for (const auto& id : _all_output_ids) {
  //   if (id.name[0] == '$') {
  //     auto new_id = id;
  //     auto var_value = GetGraphClusterContext()->GetExecuteParams()->GetVar(id.name.substr(1));
  //     if (!var_value.String().empty()) {
  //       new_id.name = var_value.String();
  //       _data_ctx->RegisterData(new_id);
  //     } else {
  //       DIDAGLE_ERROR("[{}/{}]has invalid output with name:{}", _cluster->GetCluster()->_name, _graph->name,
  //       id.name);
  //     }
  //   } else {
  //     _data_ctx->RegisterData(id);
  //   }
  // }
  // std::vector<VertexContext*> ready_successors;
  // for (auto& pair : _vertex_context_table) {
  //   std::shared_ptr<VertexContext>& ctx = pair.second;
  //   if (ctx->Ready()) {
  //     ready_successors.emplace_back(ctx.get());
  //   }
  // }
  ExecuteReadyVertexs(_start_ctxs);
  return 0;
}
GraphContext* GraphClusterContext::GetRunGraph(const std::string& name) {
  if (nullptr != _running_graph) {
    return _running_graph;
  }
  if (nullptr != _last_runnin_graph && _last_runnin_graph->GetGraph()->name == name) {
    _running_graph = _last_runnin_graph;
  } else {
    auto found = _graph_context_table.find(name);
    if (found == _graph_context_table.end()) {
      DIDAGLE_ERROR("No graph:{} found in cluster:{}", name, _cluster->_name);
      return nullptr;
    }
    _running_graph = found->second.get();
    _last_runnin_graph = _running_graph;
  }
  _running_graph->SetGraphDataContext(_extern_data_ctx);
  return _running_graph;
}
void GraphClusterContext::Reset() {
  _end_ustime = 0;
  _exec_params = nullptr;
  if (nullptr != _running_graph) {
    _running_graph->Reset();
    _running_graph = nullptr;
  }
  for (auto& item : _config_settings) {
    item.eval_proc->Reset();
    item.result = 0;
  }
  _extern_data_ctx = nullptr;
  _running_cluster.reset();
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
      ConfigSettingContext item;
      item.eval_proc = p;
      _config_settings.push_back(item);
    }
  }
  for (auto& pair : _cluster->_graphs) {
    std::shared_ptr<GraphContext> g(new GraphContext);
    if (0 != g->Setup(this, pair.second)) {
      DIDAGLE_ERROR("Failed to setup graph:{}", pair.second->name);
      return -1;
    }
    _graph_context_table[pair.second->name] = g;
  }
  return 0;
}
int GraphClusterContext::Execute(const std::string& graph, DoneClosure&& done, GraphContext*& graph_ctx) {
  uint64_t start_exec_ustime = ustime();
  graph_ctx = nullptr;
  GraphContext* g = GetRunGraph(graph);
  if (nullptr == g) {
    if (done) {
      done(-1);
    }
    return 0;
  }
  graph_ctx = g;
  GraphDataContext& data_ctx = g->GetGraphDataContextRef();
  DIDAGLE_DEBUG("config setting size = {}", _config_settings.size());
  for (size_t i = 0; i < _config_settings.size(); i++) {
    Processor* p = _config_settings[i].eval_proc;
    p->SetDataContext(g->GetGraphDataContext());

    int eval_rc = 0;
    if (nullptr != _exec_params) {
      eval_rc = p->Execute(*_exec_params);
    } else {
      Params args;
      eval_rc = p->Execute(args);
    }
    if (0 != eval_rc) {
      _config_settings[i].result = 0;
    } else {
      _config_settings[i].result = 1;
    }
    bool* v = reinterpret_cast<bool*>(&_config_settings[i].result);
    data_ctx.Set(_cluster->config_setting[i].name, v);
  }

  /**
   * @brief diable data entry creation since the graph is executing while the
   * creation is not thread-safe.
   */
  data_ctx.DisableEntryCreation();
  DAGEventTracker* tracker = data_ctx.GetEventTracker();
  if (nullptr != tracker) {
    auto event = std::make_unique<DAGEvent>();
    event->start_ustime = start_exec_ustime;
    event->end_ustime = ustime();
    event->phase = PhaseType::DAG_GRAPH_GRAPH_PREPARE_EXECUTE;
    tracker->Add(std::move(event));
  }
  return g->Execute(std::move(done));
}

GraphClusterContext::~GraphClusterContext() {
  for (auto& item : _config_settings) {
    delete item.eval_proc;
  }
}

}  // namespace didagle
