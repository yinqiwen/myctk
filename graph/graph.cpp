// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "graph.h"
#include <iostream>
#include "log.h"

namespace wrdk {
namespace graph {
std::string Graph::generateNodeId() {
  std::string id = name + "_" + std::to_string(_idx);
  _idx++;
  return id;
}
Vertex* Graph::geneatedCondVertex(const std::string& cond) {
  std::shared_ptr<Vertex> v(new Vertex);
  v->id = generateNodeId();
  v->processor = _cluster->default_expr_processor;
  v->cond = cond;
  v->_graph = this;
  Vertex* r = v.get();
  _nodes[v->id] = r;
  _gen_vertex.push_back(v);
  return r;
}
Vertex* Graph::FindVertexByData(const std::string& data) {
  auto found = _data_mapping_table.find(data);
  if (found == _data_mapping_table.end()) {
    // WRDK_GRAPH_ERROR("No dep input id:{}", data);
    return nullptr;
  }
  return found->second;
}
Vertex* Graph::FindVertexById(const std::string& id) {
  auto found = _nodes.find(id);
  if (found == _nodes.end()) {
    // WRDK_GRAPH_ERROR("No dep input id:{}", id);
    return nullptr;
  }
  return found->second;
}
int Graph::Build() {
  VertexTable generated_cond_nodes;
  for (auto& n : vertex) {
    if (n.id.empty()) {
      // generate unique id for node without id
      if (n.processor.empty()) {
        n.id = generateNodeId();
      } else {
        n.id = n.processor;
      }
    }
    if (_nodes.find(n.id) != _nodes.end()) {
      WRDK_GRAPH_ERROR("Duplicate id:{}", n.id);
      return -1;
    }
    n._graph = this;
    _nodes[n.id] = &n;

    for (auto& data : n.output) {
      if (data.id.empty()) {
        data.id = data.field;
      }
      auto found = _data_mapping_table.find(data.id);
      if (found != _data_mapping_table.end()) {
        WRDK_GRAPH_ERROR("Duplicate data name:{}", data.id);
        return -1;
      }
      _data_mapping_table[data.id] = &n;
    }

    for (auto& data : n.input) {
      if (data.id.empty()) {
        data.id = data.field;
      }
      if (!data.cond.empty()) {
        if (generated_cond_nodes.find(data.cond) == generated_cond_nodes.end()) {
          generated_cond_nodes[data.cond] = geneatedCondVertex(data.cond);
        }
        Vertex* cond_vertex = generated_cond_nodes[data.cond];
        data._cond_vertex_id = cond_vertex->id;
      }
    }
  }
  for (auto& n : vertex) {
    if (0 != n.Build()) {
      WRDK_GRAPH_ERROR("Failed to build vertex:{} in graph:{}", n.GetDotLable(), name);
      return -1;
    }
  }
  for (auto& n : _gen_vertex) {
    if (0 != n->Build()) {
      WRDK_GRAPH_ERROR("Failed to build vertex:{} in graph:{}", n->GetDotLable(), name);
      return -1;
    }
  }

  for (auto& n : vertex) {
    if (n.IsSuccessorsEmpty() && n.IsDepsEmpty()) {
      WRDK_GRAPH_ERROR("Vertex:{} has no deps and successors", n.id);
      return -1;
    }
  }
  for (auto& n : _gen_vertex) {
    if (n->IsSuccessorsEmpty() && n->IsDepsEmpty()) {
      WRDK_GRAPH_ERROR("Generated Vertex:{} has no deps and successors", n->id);
      return -1;
    }
  }

  std::vector<GraphContext*> tmp_pool;
  for (int64_t i = 0; i < _cluster->default_context_pool_size; i++) {
    GraphContext* ctx = new GraphContext;
    _graph_context_pool.push(ctx);
    tmp_pool.push_back(ctx);
  }
  for (GraphContext* ctx : tmp_pool) {
    if (0 != ctx->Setup(this)) {
      return -1;
    }
  }
  return 0;
}
int Graph::DumpDot(std::string& s) {
  s.append("  subgraph cluster_").append(name).append("{\n");
  s.append("    style = rounded;\n");
  s.append("    label = \"").append(name).append("\";\n");
  for (auto& pair : _nodes) {
    Vertex* v = pair.second;
    v->DumpDotDefine(s);
  }
  for (auto& pair : _nodes) {
    Vertex* v = pair.second;
    v->DumpDotEdge(s);
  }
  s.append("};\n");
  return 0;
}
GraphContext* Graph::GetContext() {
  GraphContext* ctx = nullptr;
  if (_graph_context_pool.try_pop(ctx)) {
    return ctx;
  }
  ctx = new GraphContext;
  ctx->Setup(this);
  return ctx;
}
void Graph::ReleaseContext(GraphContext* p) {
  p->Reset();
  _graph_context_pool.push(p);
}
Graph::~Graph() {
  GraphContext* ctx = nullptr;
  while (_graph_context_pool.try_pop(ctx)) {
    delete ctx;
  }
}

int GraphCluster::Build() {
  for (auto& f : graph) {
    f._cluster = this;
    _graphs[f.name] = &f;
    if (0 != f.Build()) {
      WRDK_GRAPH_ERROR("Failed to build graph:{}", f.name);
      return -1;
    }
  }
  for (ConfigSetting& cfg : config_setting) {
    if (cfg.processor.empty()) {
      cfg.processor = default_expr_processor;
      if (default_expr_processor.empty()) {
        WRDK_GRAPH_ERROR("Empty 'default_expr_processor'");
        return -1;
      }
    }
  }
  _context.reset(new GraphClusterContext);
  if (0 != _context->Setup(this)) {
    WRDK_GRAPH_ERROR("Failed to setup graph cluster context:{}", _name);
    return -1;
  }
  return 0;
}
int GraphCluster::DumpDot(std::string& s) {
  s.append("digraph G {\n");
  s.append("    rankdir=LR;\n");
  for (auto& f : graph) {
    f.DumpDot(s);
  }
  s.append("}\n");

  return 0;
}
Graph* GraphCluster::FindGraphByName(const std::string& name) {
  auto found = _graphs.find(name);
  if (found != _graphs.end()) {
    return found->second;
  }
  return nullptr;
}
GraphCluster::~GraphCluster() {}

static std::string get_basename(const std::string& filename) {
#if defined(_WIN32)
  char dir_sep('\\');
#else
  char dir_sep('/');
#endif

  std::string::size_type pos = filename.rfind(dir_sep);
  if (pos != std::string::npos)
    return filename.substr(pos + 1);
  else
    return filename;
}

std::shared_ptr<GraphCluster> GraphManager::Load(const std::string& file) {
  std::shared_ptr<GraphCluster> g(new GraphCluster);
  bool v = wrdk::ParseFromTomlFile(file, *g);
  if (!v) {
    printf("Failed to parse toml\n");
    return nullptr;
  }
  std::string name = get_basename(file);
  g->_name = name;
  ClusterGraphTable::accessor accessor;
  _graphs.insert(accessor, name);
  accessor->second = g;
  return g;
}
std::shared_ptr<GraphCluster> GraphManager::FindGraphClusterByName(const std::string& name) {
  std::shared_ptr<GraphCluster> ret;
  ClusterGraphTable::const_accessor accessor;
  if (_graphs.find(accessor, name)) {
    ret = accessor->second;
  }
  return ret;
}
GraphContext* GraphManager::GetGraphContext(const std::string& cluster, const std::string& graph) {
  std::shared_ptr<GraphCluster> c = FindGraphClusterByName(cluster);
  if (!c) {
    return nullptr;
  }
  Graph* g = c->FindGraphByName(graph);
  if (!g) {
    return nullptr;
  }
  GraphContext* ctx = g->GetContext();
  ctx->SetRunningGraphCluster(c);  // make ctx has same lifetime with GraphCluster
  return ctx;
}
int GraphManager::Execute(const GraphExecuteOptions& options,
                          std::shared_ptr<GraphDataContext> data_ctx, const std::string& cluster,
                          const std::string& graph, DoneClosure&& done) {
  if (!options.concurrent_executor) {
    WRDK_GRAPH_ERROR("Empty concurrent executor");
    return -1;
  }
  if (!data_ctx) {
    WRDK_GRAPH_ERROR("Empty 'GraphDataContext'");
    return -1;
  }
  GraphContext* ctx = GetGraphContext(cluster, graph);
  if (!ctx) {
    WRDK_GRAPH_ERROR("Find graph {}::{} failed.", cluster, graph);
    return -1;
  }
  WRDK_GRAPH_DEBUG("Find graph {}::{} success.", cluster, graph);
  ctx->SetGraphDataContext(data_ctx);
  std::shared_ptr<GraphExecuteOptions> exec_opt(new GraphExecuteOptions(options));
  auto graph_done = [ctx, done](int code) {
    ctx->GetGraph()->ReleaseContext(ctx);
    done(code);
  };
  ctx->SetExecuteOptions(exec_opt);
  ctx->GetRunningGraphCluster()->_context->Execute(ctx->GetGraphDataContext(),
                                                   ctx->GetConfigSettingResults());
  return ctx->Execute(nullptr, graph_done);
}
}  // namespace graph
}  // namespace wrdk
