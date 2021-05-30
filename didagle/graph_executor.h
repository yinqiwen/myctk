// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/17
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once

#include <stdint.h>
#include <tbb/concurrent_queue.h>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "graph_processor_api.h"
#include "graph_processor_di.h"
#include "graph_vertex.h"

namespace didagle {
typedef std::function<void(int)> DoneClosure;
typedef std::function<void(void)> AnyClosure;
typedef std::function<void(AnyClosure&&)> ConcurrentExecutor;

struct GraphExecuteOptions {
  ConcurrentExecutor concurrent_executor;
  std::shared_ptr<Params> params;
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
  Params _params;
  std::vector<CondParams> _select_params;
  // typedef std::pair<DIObjectKey, const GraphData*> FieldData;
  // typedef std::map<std::string, FieldData> FieldDataTable;
  // FieldDataTable _input_ids;
  // FieldDataTable _output_ids;

  // int SetupInputOutputIds(const std::vector<DIObjectKey>& fields,
  //                         const std::vector<GraphData>& config_fields, FieldDataTable&
  //                         field_ids);

 public:
  VertexContext();
  Vertex* GetVertex() { return _vertex; }
  ProcessorDI* GetProcessorDI() { return _processor_di; }
  // const FieldDataTable& GetInputIds() { return _input_ids; }
  // const FieldDataTable& GetOutputIds() { return _output_ids; }
  VertexResult GetResult() { return _result; };
  void FinishVertexProcess(int code);
  int ExecuteProcessor();
  int ExecuteSubGraph();
  uint32_t SetDependencyResult(Vertex* v, VertexResult r);
  bool Ready();
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
  std::unordered_map<Vertex*, std::shared_ptr<VertexContext>> _vertex_context_table;
  std::atomic<uint32_t> _join_vertex_num;
  std::shared_ptr<GraphDataContext> _data_ctx;
  DoneClosure _done;
  std::vector<uint8_t> _config_setting_result;
  std::set<DIObjectKey> _all_input_ids;
  std::set<DIObjectKey> _all_output_ids;

 public:
  GraphContext();

  Graph* GetGraph() { return _graph; }
  GraphCluster* GetGraphCluster();
  GraphClusterContext* GetGraphClusterContext() { return _cluster; }

  VertexContext* FindVertexContext(Vertex* v);
  void OnVertexDone(VertexContext* vertex);

  int Setup(GraphClusterContext* c, Graph* g);
  void Reset();
  void ExecuteReadyVertexs(std::vector<VertexContext*>& ready_vertexs);
  int Execute(DoneClosure&& done);
  std::shared_ptr<GraphDataContext> GetGraphDataContext();
  GraphDataContext& GetGraphDataContextRef() { return *(GetGraphDataContext()); }
  void SetGraphDataContext(std::shared_ptr<GraphDataContext> p);
};

class GraphClusterContext {
 private:
  const Params* _exec_params = nullptr;
  // const GraphExecuteOptions* _exec_options = nullptr;
  std::shared_ptr<GraphCluster> _running_cluster;
  GraphContext* _running_graph = nullptr;
  GraphCluster* _cluster = nullptr;
  std::vector<Processor*> _config_setting_processors;
  std::vector<uint8_t> _config_setting_result;
  tbb::concurrent_queue<GraphClusterContext*> _sub_graphs;
  std::unordered_map<std::string, std::shared_ptr<GraphContext>> _graph_context_table;
  DoneClosure _done;
  std::shared_ptr<GraphDataContext> _extern_data_ctx;

 public:
  void SetGraphDataContext(std::shared_ptr<GraphDataContext> p) { _extern_data_ctx = p; }
  // void SetExecuteOptions(const GraphExecuteOptions* opt) { _exec_options = opt; }
  // const GraphExecuteOptions& GetExecuteOptions() { return *_exec_options; }
  void SetExecuteParams(const Params* p) { _exec_params = p; }
  const Params* GetExecuteParams() const { return _exec_params; }
  GraphCluster* GetCluster() { return _cluster; }
  GraphContext* GetRunGraph(const std::string& name);
  void SetRunningCluster(std::shared_ptr<GraphCluster> c) { _running_cluster = c; }
  int Setup(GraphCluster* c);
  void Reset();
  int Execute(const std::string& graph, DoneClosure&& done);
  void Execute(GraphDataContext& session_ctx, std::vector<uint8_t>& eval_results);
  ~GraphClusterContext();
};

}  // namespace didagle