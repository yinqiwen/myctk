// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/16
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once

#include <stdint.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "boost/functional/hash.hpp"
#include "concurrentqueue.h"
#include "folly/concurrency/AtomicSharedPtr.h"
#include "folly/container/F14Map.h"

#include "didagle/graph_executor.h"
#include "didagle/graph_vertex.h"
#include "kcfg_toml.h"

namespace didagle {

struct GraphCluster;
struct Graph {
  std::string name;
  std::vector<Vertex> vertex;
  std::string expect_version = "";
  int priority = -1;

  typedef std::unordered_map<std::string, Vertex*> VertexTable;
  std::vector<std::shared_ptr<Vertex>> _gen_vertex;
  VertexTable _nodes;
  VertexTable _data_mapping_table;
  int64_t _idx = 0;
  GraphCluster* _cluster = nullptr;

  KCFG_TOML_DEFINE_FIELDS(name, vertex, expect_version, priority)
  std::string generateNodeId();
  Vertex* geneatedCondVertex(const std::string& cond);
  Vertex* FindVertexByData(const std::string& data);
  Vertex* FindVertexById(const std::string& id);

  int Build();
  int DumpDot(std::string& s);
  bool TestCircle();
  ~Graph();
};

class GraphManager;
class GraphClusterContext;
struct GraphCluster {
  std::string desc;
  bool strict_dsl = true;
  std::string default_expr_processor;
  int64_t default_context_pool_size = 256;
  std::vector<Graph> graph;
  std::vector<ConfigSetting> config_setting;

  std::string _name;
  typedef std::map<std::string, Graph*> GraphTable;
  GraphTable _graphs;
  GraphManager* _graph_manager = nullptr;
  bool _builded = false;

  // tbb::concurrent_queue<GraphClusterContext *> _graph_cluster_context_pool;
  moodycamel::ConcurrentQueue<GraphClusterContext*> _graph_cluster_context_pool;
  KCFG_TOML_DEFINE_FIELDS(desc, strict_dsl, default_expr_processor, default_context_pool_size, graph, config_setting)

  int Build();
  bool ContainsConfigSetting(const std::string& name);
  int DumpDot(std::string& s);
  Graph* FindGraphByName(const std::string& name);
  GraphClusterContext* GetContext();
  void ReleaseContext(GraphClusterContext* p);
  inline const GraphManager* GetGraphManager() const { return _graph_manager; }
  bool Exists(const std::string& graph);
  ~GraphCluster();
};

class GraphContext;
class GraphManager {
 private:
  // typedef tbb::concurrent_hash_map<std::string, std::shared_ptr<GraphCluster>> ClusterGraphTable;
  // typedef folly::ConcurrentHashMap<std::string, folly::atomic_shared_ptr<GraphCluster>>
  //     ClusterGraphTable;
  typedef folly::F14NodeMap<std::string, folly::atomic_shared_ptr<GraphCluster>> ClusterGraphTable;
  ClusterGraphTable _graphs;
  GraphExecuteOptions _exec_options;
  std::mutex _graphs_mutex;

 public:
  explicit GraphManager(const GraphExecuteOptions& options);
  inline const GraphExecuteOptions& GetGraphExecuteOptions() const { return _exec_options; }
  std::shared_ptr<GraphCluster> Load(const std::string& file);
  std::shared_ptr<GraphCluster> FindGraphClusterByName(const std::string& name);
  GraphClusterContext* GetGraphClusterContext(const std::string& cluster);
  int Execute(GraphDataContextPtr& data_ctx, const std::string& cluster, const std::string& graph, const Params* params,
              DoneClosure&& done, uint64_t = 0);
  bool Exists(const std::string& cluster, const std::string& graph);
};
}  // namespace didagle