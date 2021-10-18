// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "graph.h"
#include <iostream>
#include <regex>
#include "didagle_log.h"

namespace didagle {
std::string Graph::generateNodeId() {
  std::string id = name + "_" + std::to_string(_idx);
  _idx++;
  return id;
}
Vertex *Graph::geneatedCondVertex(const std::string &cond) {
  std::shared_ptr<Vertex> v(new Vertex);
  v->SetGeneratedId(generateNodeId());
  v->processor = _cluster->default_expr_processor;
  v->cond = cond;
  v->_graph = this;
  v->MergeSuccessor();
  Vertex *r = v.get();
  _nodes[v->id] = r;
  _gen_vertex.push_back(v);
  return r;
}
Vertex *Graph::FindVertexByData(const std::string &data) {
  auto found = _data_mapping_table.find(data);
  if (found == _data_mapping_table.end()) {
    // DIDAGLE_ERROR("No dep input id:{}", data);
    return nullptr;
  }
  return found->second;
}
Vertex *Graph::FindVertexById(const std::string &id) {
  auto found = _nodes.find(id);
  if (found == _nodes.end()) {
    // DIDAGLE_ERROR("No dep input id:{}", id);
    return nullptr;
  }
  return found->second;
}
bool Graph::TestCircle() {
  for (auto &pair : _nodes) {
    Vertex *v = pair.second;
    if (v->FindVertexInSuccessors(v)) {
      DIDAGLE_ERROR("[{}]Found vertex:{} has circle", name, v->GetDotLable());
      return true;
    }
  }
  return false;
}
int Graph::Build() {
  VertexTable generated_cond_nodes;
  for (auto &n : vertex) {
    if (n.processor.empty() && !n.cond.empty()) {
      // use default expr processor
      n.processor = _cluster->default_expr_processor;
    }
    if (n.id.empty()) {
      // generate unique id for node without id
      if (n.processor.empty()) {
        n.SetGeneratedId(generateNodeId());
      } else {
        n.id = n.processor;
      }
    }
    if (!n.graph.empty()) {
      if (n.cluster == "." || n.cluster.empty()) {
        n.cluster = _cluster->_name;
      }
    }
    if (!n.expect.empty() && !n.expect_config.empty()) {
      DIDAGLE_ERROR("Vertex:{} can NOT both config 'expect' & 'expect_config'", n.id);
      return -1;
    }
    if (!n.expect_config.empty()) {
      if (!_cluster->ContainsConfigSetting(n.expect_config)) {
        DIDAGLE_ERROR("No config_setting with name:{} defined.", n.expect_config);
        return -1;
      }
    }
    if (!n.expect.empty()) {
      if (generated_cond_nodes.find(n.expect) == generated_cond_nodes.end()) {
        generated_cond_nodes[n.expect] = geneatedCondVertex(n.expect);
      }
      Vertex *cond_vertex = generated_cond_nodes[n.expect];
      n.deps_on_ok.insert(cond_vertex->id);
    }
    if (_nodes.find(n.id) != _nodes.end()) {
      DIDAGLE_ERROR("Duplicate node id:{}", n.id);
      return -1;
    }
    n._graph = this;
    n.MergeSuccessor();
    _nodes[n.id] = &n;

    if (_cluster->strict_dsl) {
      if (0 != n.FillInputOutput()) {
        DIDAGLE_ERROR("Failed to fill input outpt for node:{}", n.id);
        return -1;
      }
    } else {
      n.FillInputOutput();
    }

    for (auto &data : n.output) {
      if (data.field.empty()) {
        DIDAGLE_ERROR("Empty data field in output for node:{}", n.id);
        return -1;
      }
      if (data.id.empty()) {
        data.id = data.field;
      }
      auto found = _data_mapping_table.find(data.id);
      if (found != _data_mapping_table.end()) {
        DIDAGLE_ERROR("Duplicate data name:{} in node:{}", data.id, n.id);
        return -1;
      }
      _data_mapping_table[data.id] = &n;
    }

    for (auto &data : n.input) {
      if (data.field.empty()) {
        DIDAGLE_ERROR("Empty data field in input for node:{}", n.id);
        return -1;
      }
      if (data.id.empty()) {
        data.id = data.field;
      }
      // if (!data.cond.empty()) {
      //   if (generated_cond_nodes.find(data.cond) == generated_cond_nodes.end()) {
      //     generated_cond_nodes[data.cond] = geneatedCondVertex(data.cond);
      //   }
      //   Vertex* cond_vertex = generated_cond_nodes[data.cond];
      //   data._cond_vertex_id = cond_vertex->id;
      // }
    }
  }
  for (auto &n : vertex) {
    if (0 != n.Build()) {
      DIDAGLE_ERROR("Failed to build vertex:{} in graph:{}", n.GetDotLable(), name);
      return -1;
    }
  }
  for (auto &n : _gen_vertex) {
    if (0 != n->Build()) {
      DIDAGLE_ERROR("Failed to build vertex:{} in graph:{}", n->GetDotLable(), name);
      return -1;
    }
  }

  for (auto &n : vertex) {
    if (!n.Verify()) {
      // DIDAGLE_ERROR("Vertex:{} has no deps and successors", n.id);
      return -1;
    }
  }
  for (auto &n : _gen_vertex) {
    if (!n->Verify()) {
      // DIDAGLE_ERROR("Generated Vertex:{} has no deps and successors", n->id);
      return -1;
    }
  }
  if (TestCircle()) {
    return -1;
  }
  return 0;
}
int Graph::DumpDot(std::string &s) {
  s.append("  subgraph cluster_").append(name).append("{\n");
  s.append("    style = rounded;\n");
  s.append("    label = \"").append(name).append("\";\n");
  s.append("    ")
      .append(name + "__START__")
      .append("[color=black fillcolor=deepskyblue style=filled shape=Msquare label=\"START\"];\n");
  s.append("    ")
      .append(name + "__STOP__")
      .append("[color=black fillcolor=deepskyblue style=filled shape=Msquare label=\"STOP\"];\n");
  for (auto &pair : _nodes) {
    Vertex *v = pair.second;
    v->DumpDotDefine(s);
  }
  std::set<std::string> cfg_setting_vars;
  for (auto &config_setting : _cluster->config_setting) {
    const std::string &var_name = config_setting.name;
    if (cfg_setting_vars.insert(var_name).second) {
      std::string dot_var_name = var_name;
      std::string label_name = var_name;
      s.append("    ").append(dot_var_name).append(" [label=\"").append(label_name).append("\"");
      s.append(" shape=diamond color=black fillcolor=aquamarine style=filled];\n");
    }
  }
  for (auto &pair : _nodes) {
    Vertex *v = pair.second;
    v->DumpDotEdge(s);
  }
  s.append("};\n");
  return 0;
}

Graph::~Graph() {}

bool GraphCluster::ContainsConfigSetting(const std::string &name) {
  for (ConfigSetting &cfg : config_setting) {
    if (!name.empty() && name[0] == '!') {
      if (cfg.name == name.substr(1)) {
        return true;
      }
    } else {
      if (cfg.name == name) {
        return true;
      }
    }
  }
  return false;
}
int GraphCluster::Build() {
  if (!_graph_cluster_context_pool.empty()) {
    // already builded
    return 0;
  }
  for (auto &f : graph) {
    f._cluster = this;
    _graphs[f.name] = &f;
    if (0 != f.Build()) {
      DIDAGLE_ERROR("Failed to build graph:{}", f.name);
      return -1;
    }
  }
  for (ConfigSetting &cfg : config_setting) {
    if (cfg.processor.empty()) {
      cfg.processor = default_expr_processor;
      if (default_expr_processor.empty()) {
        DIDAGLE_ERROR("Empty 'default_expr_processor'");
        return -1;
      }
    }
  }
  if (strict_dsl) {
    GraphClusterContext ctx;
    if (0 != ctx.Setup(this)) {
      return -1;
    }
    ctx.Reset();
  }
  for (int i = 0; i < default_context_pool_size; i++) {
    GraphClusterContext *ctx = GetContext();
    _graph_cluster_context_pool.push(ctx);
  }
  return 0;
}
int GraphCluster::DumpDot(std::string &s) {
  s.append("digraph G {\n");
  s.append("    rankdir=LR;\n");
  for (auto &f : graph) {
    f.DumpDot(s);
  }
  s.append("}\n");

  return 0;
}
Graph *GraphCluster::FindGraphByName(const std::string &name) {
  auto found = _graphs.find(name);
  if (found != _graphs.end()) {
    return found->second;
  }
  return nullptr;
}
bool GraphCluster::Exists(const std::string &graph) { return FindGraphByName(graph) != nullptr; }

GraphClusterContext *GraphCluster::GetContext() {
  GraphClusterContext *ctx = nullptr;
  if (_graph_cluster_context_pool.try_pop(ctx)) {
    return ctx;
  }
  ctx = new GraphClusterContext;
  ctx->Setup(this);
  return ctx;
}
void GraphCluster::ReleaseContext(GraphClusterContext *p) {
  p->Reset();
  _graph_cluster_context_pool.push(p);
}

GraphCluster::~GraphCluster() {
  GraphClusterContext *ctx = nullptr;
  while (_graph_cluster_context_pool.try_pop(ctx)) {
    delete ctx;
  }
}

static std::string get_basename(const std::string &filename) {
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
GraphManager::GraphManager(const GraphExecuteOptions &options) : _exec_options(options) {}
std::shared_ptr<GraphCluster> GraphManager::Load(const std::string &file) {
  std::shared_ptr<GraphCluster> g(new GraphCluster);
  bool v = kcfg::ParseFromTomlFile(file, *g);
  if (!v) {
    DIDAGLE_ERROR("Failed to parse didagle toml script:{}", file);
    return nullptr;
  }
  std::string name = get_basename(file);
  g->_name = name;
  g->_graph_manager = this;
  if (0 != g->Build()) {
    DIDAGLE_ERROR("Failed to build toml script:{}", file);
    return nullptr;
  }
  auto &&[itr, success] = _graphs.emplace(std::move(name), g);
  if (!success) {
    const auto &graph_ptr = itr->second;
    const_cast<folly::atomic_shared_ptr<GraphCluster> &>(graph_ptr).store(g);
  }
  return g;
}
std::shared_ptr<GraphCluster> GraphManager::FindGraphClusterByName(const std::string &name) {
  // std::shared_ptr<GraphCluster> ret;
  auto found = _graphs.find(name);
  if (found != _graphs.end()) {
    return found->second.load();
  }
  return nullptr;
}
GraphClusterContext *GraphManager::GetGraphClusterContext(const std::string &cluster) {
  std::shared_ptr<GraphCluster> c = FindGraphClusterByName(cluster);
  if (!c) {
    return nullptr;
  }
  GraphClusterContext *ctx = c->GetContext();
  ctx->SetRunningCluster(c);
  return ctx;
}

bool GraphManager::Exists(const std::string &cluster, const std::string &graph) {
  std::shared_ptr<GraphCluster> c = FindGraphClusterByName(cluster);
  if (!c) {
    return false;
  }
  return c->Exists(graph);
}

int GraphManager::Execute(GraphDataContextPtr &data_ctx, const std::string &cluster,
                          const std::string &graph, const Params *params, DoneClosure &&done) {
  if (!_exec_options.concurrent_executor) {
    DIDAGLE_ERROR("Empty concurrent executor");
    return -1;
  }
  if (!data_ctx) {
    DIDAGLE_ERROR("Empty 'GraphDataContext'");
    return -1;
  }
  GraphClusterContext *ctx = GetGraphClusterContext(cluster);
  if (!ctx) {
    DIDAGLE_ERROR("Find graph cluster {} failed.", cluster);
    return -1;
  }
  ctx->SetExternGraphDataContext(data_ctx.get());
  ctx->SetExecuteParams(params);
  auto graph_done = [ctx, done](int code) {
    done(code);
    ctx->GetCluster()->ReleaseContext(ctx);
    // DIDAGLE_DEBUG("#####0  {}", (uintptr_t)ctx);
  };
  GraphContext *graph_ctx = nullptr;
  // ctx->SetExecuteOptions(&_exec_opt);
  return ctx->Execute(graph, graph_done, graph_ctx);
}
}  // namespace didagle
