/*
 *Copyright (c) 2021, yinqiwen <yinqiwen@gmail.com>
 *All rights reserved.
 *
 *Redistribution and use in source and binary forms, with or without
 *modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of rimos nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 *THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *THE POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

#include <stdint.h>

#include <atomic>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
  // std::vector<CondParams> _select_params;
  std::vector<SelectCondParamsContext> _select_contexts;
  uint64_t _exec_start_ustime = 0;
  size_t _child_idx = (size_t)-1;
  const Params* _exec_params = nullptr;
  int _exec_rc = INT_MAX;

 public:
  VertexContext();
  void SetChildIdx(size_t idx) { _child_idx = idx; }
  Vertex* GetVertex() { return _vertex; }
  const Params* GetExecParams();
  ProcessorDI* GetProcessorDI() { return _processor_di; }
  Processor* GetProcessor() { return _processor; }
  VertexResult GetResult() { return _result; }
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
  GraphDataContextPtr _data_ctx;
  DoneClosure _done;
  std::vector<uint8_t> _config_setting_result;
  std::set<DIObjectKey> _all_input_ids;
  std::set<DIObjectKey> _all_output_ids;
  size_t _children_count;

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
  GraphDataContext* GetGraphDataContext();
  GraphDataContext& GetGraphDataContextRef() { return *(GetGraphDataContext()); }
  void SetGraphDataContext(GraphDataContext* p);
};

struct ConfigSettingContext {
  Processor* eval_proc = nullptr;
  uint8_t result = 0;
};

class GraphClusterContext {
 private:
  const Params* _exec_params = nullptr;
  // const GraphExecuteOptions* _exec_options = nullptr;
  std::shared_ptr<GraphCluster> _running_cluster;
  GraphContext* _running_graph = nullptr;
  GraphCluster* _cluster = nullptr;
  // std::vector<Processor*> _config_setting_processors;
  // std::vector<uint8_t> _config_setting_result;
  std::vector<ConfigSettingContext> _config_settings;
  // tbb::concurrent_queue<GraphClusterContext *> _sub_graphs;
  std::unordered_map<std::string, std::shared_ptr<GraphContext>> _graph_context_table;
  DoneClosure _done;
  GraphDataContext* _extern_data_ctx = nullptr;
  // bool _is_subgraph = false;
  uint64_t _end_ustime = 0;

 public:
  // bool IsSubgraph() const { return _is_subgraph; }
  // void SetSubgraphFlag(bool b) { _is_subgraph = b; }
  void SetExternGraphDataContext(GraphDataContext* p) { _extern_data_ctx = p; }
  // void SetExecuteOptions(const GraphExecuteOptions* opt) { _exec_options =
  // opt; } const GraphExecuteOptions& GetExecuteOptions() { return
  // *_exec_options; }
  uint64_t GetEndTime() { return _end_ustime; }
  void SetEndTime(const uint64_t end_ustime) { _end_ustime = end_ustime; }
  void SetExecuteParams(const Params* p) { _exec_params = p; }
  const Params* GetExecuteParams() const { return _exec_params; }
  GraphCluster* GetCluster() { return _cluster; }
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