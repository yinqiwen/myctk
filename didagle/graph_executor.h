// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/17
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once

#include <stdint.h>

#include <atomic>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "folly/container/F14Map.h"

#include "didagle/graph_processor_api.h"
#include "didagle/graph_processor_di.h"
#include "didagle/graph_vertex.h"

namespace didagle {
typedef std::function<void(int)> DoneClosure;
typedef std::function<void(void)> AnyClosure;
typedef std::function<void(AnyClosure&&)> ConcurrentExecutor;
using EventReporter = std::function<void(DAGEvent)>;

struct GraphExecuteOptions {
  ConcurrentExecutor concurrent_executor;
  std::shared_ptr<Params> params;
  EventReporter event_reporter;
  std::function<bool(const std::string&)> check_version;
};

struct SelectCondParamsContext {
  Processor* p = nullptr;
  CondParams param;
};

class GraphContext;
class GraphClusterContext;
class VertexContext {
 private:
  GraphContext* _graph_ctx = nullptr;
  Vertex* _vertex = nullptr;
  std::atomic<uint32_t> _waiting_num;
  std::vector<VertexResult> _deps_results;
  VertexResult _result;
  VertexErrCode _code;
  Processor* _processor = nullptr;
  ProcessorDI* _processor_di = nullptr;
  GraphClusterContext* _subgraph_cluster = nullptr;
  GraphContext* _subgraph_ctx = nullptr;
  std::string _full_graph_name;
  Params _params;
  std::vector<SelectCondParamsContext> _select_contexts;
  uint64_t _exec_start_ustime = 0;
  size_t _child_idx = (size_t)-1;
  const Params* _exec_params = nullptr;
  std::string_view _exec_mathced_cond;
  int _exec_rc = INT_MAX;

  std::vector<VertexContext*> _successor_ctxs;
  std::vector<int> _successor_dep_idxs;

  void SetupSuccessors();

  friend class GraphContext;

 public:
  VertexContext();
  inline void SetChildIdx(size_t idx) { _child_idx = idx; }
  inline Vertex* GetVertex() { return _vertex; }
  const Params* GetExecParams(std::string_view* matched_cond);
  inline ProcessorDI* GetProcessorDI() { return _processor_di; }
  inline Processor* GetProcessor() { return _processor; }
  inline VertexResult GetResult() { return _result; }
  void FinishVertexProcess(int code);
  int ExecuteProcessor();
  int ExecuteSubGraph();
  inline uint32_t SetDependencyResult(int idx, VertexResult r) {
    // int idx = _vertex->GetDependencyIndex(v);
    VertexResult last_result_val = _deps_results[idx];
    _deps_results[idx] = r;
    if (last_result_val == V_RESULT_INVALID) {
      return _waiting_num.fetch_sub(1);
    } else {
      return _waiting_num.load();
    }
  }
  inline bool Ready() { return 0 == _waiting_num.load(); }
  int Setup(GraphContext* g, Vertex* v);
  void Reset();
  int Execute();
  ~VertexContext();
};

struct Graph;
struct GraphCluster;
class GraphContext {
 private:
  GraphClusterContext* _cluster;
  Graph* _graph;
  folly::F14FastMap<Vertex*, std::shared_ptr<VertexContext>> _vertex_context_table;
  std::atomic<uint32_t> _join_vertex_num;
  GraphDataContextPtr _data_ctx;
  DoneClosure _done;
  std::vector<uint8_t> _config_setting_result;
  size_t _children_count;

  std::vector<VertexContext*> _start_ctxs;

 public:
  GraphContext();

  inline Graph* GetGraph() { return _graph; }
  GraphCluster* GetGraphCluster();
  inline GraphClusterContext* GetGraphClusterContext() { return _cluster; }

  VertexContext* FindVertexContext(Vertex* v);
  void OnVertexDone(VertexContext* vertex);

  int Setup(GraphClusterContext* c, Graph* g);
  void Reset();
  void ExecuteReadyVertexs(std::vector<VertexContext*>& ready_vertexs);
  inline void ExecuteReadyVertex(VertexContext* v) { v->Execute(); }
  int Execute(DoneClosure&& done);
  inline GraphDataContext* GetGraphDataContext() { return _data_ctx.get(); }
  inline GraphDataContext& GetGraphDataContextRef() { return *(GetGraphDataContext()); }
  void SetGraphDataContext(GraphDataContext* p);
};

struct ConfigSettingContext {
  Processor* eval_proc = nullptr;
  uint8_t result = 0;
};

class GraphClusterContext {
 private:
  const Params* _exec_params = nullptr;
  std::shared_ptr<GraphCluster> _running_cluster;
  GraphContext* _running_graph = nullptr;
  GraphContext* _last_runnin_graph = nullptr;
  GraphCluster* _cluster = nullptr;
  std::vector<ConfigSettingContext> _config_settings;
  folly::F14FastMap<std::string, std::shared_ptr<GraphContext>> _graph_context_table;
  DoneClosure _done;
  GraphDataContext* _extern_data_ctx = nullptr;
  uint64_t _end_ustime = 0;

 public:
  void SetExternGraphDataContext(GraphDataContext* p) { _extern_data_ctx = p; }
  uint64_t GetEndTime() { return _end_ustime; }
  void SetEndTime(const uint64_t end_ustime) { _end_ustime = end_ustime; }
  void SetExecuteParams(const Params* p) { _exec_params = p; }
  const Params* GetExecuteParams() const { return _exec_params; }
  inline GraphCluster* GetCluster() { return _cluster; }
  GraphContext* GetRunGraph(const std::string& name);
  void SetRunningCluster(std::shared_ptr<GraphCluster> c) { _running_cluster = c; }
  std::shared_ptr<GraphCluster> GetRunningCluster() { return _running_cluster; }
  int Setup(GraphCluster* c);
  void Reset();
  int Execute(const std::string& graph, DoneClosure&& done, GraphContext*& graph_ctx);
  void Execute(GraphDataContext& session_ctx, std::vector<uint8_t>& eval_results);
  ~GraphClusterContext();
};

}  // namespace didagle
