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
#include "processor_api.h"
#include "vertex.h"

namespace wrdk {
namespace graph {
typedef std::function<void(int)> DoneClosure;
typedef std::function<void(void)> AnyClosure;
typedef std::function<void(AnyClosure&&)> ConcurrentExecutor;

struct GraphExecuteOptions {
  ConcurrentExecutor concurrent_executor;
  std::shared_ptr<Params> params;
};

class GraphContext;
class VertexContext {
 private:
  GraphContext* _graph_ctx = nullptr;
  Vertex* _vertex = nullptr;
  std::atomic<uint32_t> _waiting_num;
  std::vector<VertexResult> _deps_results;
  VertexResult _result;
  VertexErrCode _code;
  Processor* _processor = nullptr;
  GraphContext* _subgraph = nullptr;
  Params _params;
  std::unordered_map<std::string, Params> _select_params;
  typedef std::pair<DataKey, const GraphData*> FieldData;
  typedef std::map<std::string, FieldData> FieldDataTable;
  FieldDataTable _input_ids;
  FieldDataTable _output_ids;

  int SetupInputOutputIds(const std::vector<DataKey>& fields,
                          const std::vector<GraphData>& config_fields, FieldDataTable& field_ids);

 public:
  VertexContext();
  Vertex* GetVertex() { return _vertex; }
  const FieldDataTable& GetInputIds() { return _input_ids; }
  const FieldDataTable& GetOutputIds() { return _output_ids; }
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
  std::shared_ptr<GraphExecuteOptions> _exec_options;
  std::shared_ptr<GraphCluster> _running_cluster;
  Graph* _graph;
  GraphContext* _parent;
  std::unordered_map<Vertex*, std::shared_ptr<VertexContext>> _vertex_context_table;
  std::atomic<uint32_t> _join_vertex_num;
  GraphDataContext _data_ctx;
  DoneClosure _done;
  tbb::concurrent_queue<GraphContext*> _sub_graphs;
  std::vector<uint8_t> _config_setting_result;
  std::set<DataKey> _all_input_ids;
  std::set<DataKey> _all_output_ids;

 public:
  GraphContext();
  Graph* GetGraph() { return _graph; }
  void SetRunningGraphCluster(std::shared_ptr<GraphCluster> c) { _running_cluster = c; }
  GraphCluster* GetGraphCluster();
  void SetExecuteOptions(std::shared_ptr<GraphExecuteOptions> opt) { _exec_options = opt; }
  std::shared_ptr<GraphExecuteOptions> GetExecuteOptions() { return _exec_options; }
  std::vector<uint8_t>& GetConfigSettingResults() { return _config_setting_result; }
  std::shared_ptr<GraphCluster> GetRunningGraphCluster() { return _running_cluster; }
  void AddSubGraphContext(GraphContext* g);
  VertexContext* FindVertexContext(Vertex* v);
  void OnVertexDone(VertexContext* vertex);

  int Setup(Graph* g);
  void Reset();
  void ExecuteReadyVertexs(std::vector<VertexContext*>& ready_vertexs);
  int Execute(GraphContext* parent, DoneClosure&& done);
  GraphDataContext& GetGraphDataContext();
  void SetGraphDataContext(std::shared_ptr<GraphDataContext> p);
};

class GraphClusterContext {
 private:
  GraphCluster* _cluster = nullptr;
  std::vector<Processor*> _config_setting_processors;

 public:
  int Setup(GraphCluster* c);
  void Execute(GraphDataContext& session_ctx, std::vector<uint8_t>& eval_results);
};

}  // namespace graph
}  // namespace wrdk