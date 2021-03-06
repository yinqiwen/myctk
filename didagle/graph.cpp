// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "graph.h"
#include <iostream>
#include "didagle_log.h"

namespace didagle {
std::string Graph::generateNodeId() {
  std::string id = name + "_" + std::to_string(_idx);
  _idx++;
  return id;
}
Vertex* Graph::geneatedCondVertex(const std::string& cond) {
  std::shared_ptr<Vertex> v(new Vertex);
  v->SetGeneratedId(generateNodeId());
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
    // DIDAGLE_ERROR("No dep input id:{}", data);
    return nullptr;
  }
  return found->second;
}
Vertex* Graph::FindVertexById(const std::string& id) {
  auto found = _nodes.find(id);
  if (found == _nodes.end()) {
    // DIDAGLE_ERROR("No dep input id:{}", id);
    return nullptr;
  }
  return found->second;
}
bool Graph::TestCircle() {
  for (auto& pair : _nodes) {
    Vertex* v = pair.second;
    if (v->FindVertexInSuccessors(v)) {
      DIDAGLE_ERROR("[{}]Found vertex:{} has circle", name, v->GetDotLable());
      return true;
    }
  }
  return false;
}
int Graph::Build() {
  VertexTable generated_cond_nodes;
  for (auto& n : vertex) {
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
      Vertex* cond_vertex = generated_cond_nodes[n.expect];
      n.deps_on_ok.insert(cond_vertex->id);
    }
    if (_nodes.find(n.id) != _nodes.end()) {
      DIDAGLE_ERROR("Duplicate node id:{}", n.id);
      return -1;
    }
    n._graph = this;
    _nodes[n.id] = &n;

    if (_cluster->strict_dsl) {
      if (0 != n.FillInputOutput()) {
        DIDAGLE_ERROR("Failed to fill input outpt for node:{}", n.id);
        return -1;
      }
    }

    for (auto& data : n.output) {
      if (data.field.empty()) {
        DIDAGLE_ERROR("Empty data field in output for node:{}", n.id);
        return -1;
      }
      if (data.id.empty()) {
        data.id = data.field;
      }
      auto found = _data_mapping_table.find(data.id);
      if (found != _data_mapping_table.end()) {
        DIDAGLE_ERROR("Duplicate data name:{}", data.id);
        return -1;
      }
      _data_mapping_table[data.id] = &n;
    }

    for (auto& data : n.input) {
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
  for (auto& n : vertex) {
    if (0 != n.Build()) {
      DIDAGLE_ERROR("Failed to build vertex:{} in graph:{}", n.GetDotLable(), name);
      return -1;
    }
  }
  for (auto& n : _gen_vertex) {
    if (0 != n->Build()) {
      DIDAGLE_ERROR("Failed to build vertex:{} in graph:{}", n->GetDotLable(), name);
      return -1;
    }
  }

  for (auto& n : vertex) {
    if (n.IsSuccessorsEmpty() && n.IsDepsEmpty()) {
      DIDAGLE_ERROR("Vertex:{} has no deps and successors", n.id);
      return -1;
    }
  }
  for (auto& n : _gen_vertex) {
    if (n->IsSuccessorsEmpty() && n->IsDepsEmpty()) {
      DIDAGLE_ERROR("Generated Vertex:{} has no deps and successors", n->id);
      return -1;
    }
  }
  if (TestCircle()) {
    return -1;
  }
  return 0;
}
int Graph::DumpDot(std::string& s) {
  s.append("  subgraph cluster_").append(name).append("{\n");
  s.append("    style = rounded;\n");
  s.append("    label = \"").append(name).append("\";\n");
  s.append("    ")
      .append(name + "__START__")
      .append("[color=black fillcolor=deepskyblue style=filled shape=Msquare label=\"START\"];\n");
  s.append("    ")
      .append(name + "__STOP__")
      .append("[color=black fillcolor=deepskyblue style=filled shape=Msquare label=\"STOP\"];\n");
  for (auto& pair : _nodes) {
    Vertex* v = pair.second;
    v->DumpDotDefine(s);
  }
  for (auto& config_setting : _cluster->config_setting) {
    s.append("    ")
        .append(name + "_" + config_setting.name)
        .append(" [label=\"")
        .append(config_setting.name)
        .append("\"");
    s.append(" shape=diamond color=black fillcolor=aquamarine style=filled];\n");
  }
  for (auto& pair : _nodes) {
    Vertex* v = pair.second;
    v->DumpDotEdge(s);
  }
  s.append("};\n");
  return 0;
}

Graph::~Graph() {}

bool GraphCluster::ContainsConfigSetting(const std::string& name) {
  for (ConfigSetting& cfg : config_setting) {
    if (cfg.name == name) {
      return true;
    }
  }
  return false;
}
int GraphCluster::Build() {
  for (auto& f : graph) {
    f._cluster = this;
    _graphs[f.name] = &f;
    if (0 != f.Build()) {
      DIDAGLE_ERROR("Failed to build graph:{}", f.name);
      return -1;
    }
  }
  for (ConfigSetting& cfg : config_setting) {
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
    GraphClusterContext* ctx = GetContext();
    _graph_cluster_context_pool.push(ctx);
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

GraphClusterContext* GraphCluster::GetContext() {
  GraphClusterContext* ctx = nullptr;
  if (_graph_cluster_context_pool.try_pop(ctx)) {
    return ctx;
  }
  ctx = new GraphClusterContext;
  ctx->Setup(this);
  return ctx;
}
void GraphCluster::ReleaseContext(GraphClusterContext* p) {
  p->Reset();
  _graph_cluster_context_pool.push(p);
}

GraphCluster::~GraphCluster() {
  GraphClusterContext* ctx = nullptr;
  while (_graph_cluster_context_pool.try_pop(ctx)) {
    delete ctx;
  }
}

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
GraphManager::GraphManager(const GraphExecuteOptions& options) : _exec_options(options) {}
std::shared_ptr<GraphCluster> GraphManager::Load(const std::string& file) {
  std::shared_ptr<GraphCluster> g(new GraphCluster);
  bool v = kcfg::ParseFromTomlFile(file, *g);
  if (!v) {
    DIDAGLE_ERROR("Failed to parse toml");
    return nullptr;
  }
  std::string name = get_basename(file);
  g->_name = name;
  g->_graph_manager = this;
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
GraphClusterContext* GraphManager::GetGraphClusterContext(const std::string& cluster) {
  std::shared_ptr<GraphCluster> c = FindGraphClusterByName(cluster);
  if (!c) {
    return nullptr;
  }
  GraphClusterContext* ctx = c->GetContext();
  ctx->SetRunningCluster(c);
  return ctx;
}

int GraphManager::Execute(std::shared_ptr<GraphDataContext> data_ctx, const std::string& cluster,
                          const std::string& graph, const Params* params, DoneClosure&& done) {
  if (!_exec_options.concurrent_executor) {
    DIDAGLE_ERROR("Empty concurrent executor");
    return -1;
  }
  if (!data_ctx) {
    DIDAGLE_ERROR("Empty 'GraphDataContext'");
    return -1;
  }
  GraphClusterContext* ctx = GetGraphClusterContext(cluster);
  if (!ctx) {
    DIDAGLE_ERROR("Find graph cluster {} failed.", cluster);
    return -1;
  }
  ctx->SetGraphDataContext(data_ctx);
  ctx->SetExecuteParams(params);
  auto graph_done = [ctx, done](int code) {
    done(code);
    ctx->GetCluster()->ReleaseContext(ctx);
  };
  // ctx->SetExecuteOptions(&_exec_opt);
  return ctx->Execute(graph, graph_done);
}
}  // namespace didagle
