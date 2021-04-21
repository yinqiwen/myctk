// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "vertex.h"
#include "graph.h"
#include "log.h"
namespace wrdk {
namespace graph {
Vertex::Vertex() {
  Params empty(true);
  args = empty;
}
void Vertex::SetGeneratedId(const std::string& v) {
  id = v;
  _is_id_generated = true;
}
bool Vertex::IsSuccessorsEmpty() { return _successor_vertex.empty(); }
bool Vertex::IsDepsEmpty() { return _deps_idx.empty(); }
std::string Vertex::GetDotId() const { return _graph->name + "_" + id; }
std::string Vertex::GetDotLable() const {
  if (!cond.empty()) {
    return cond;
  }
  if (!processor.empty()) {
    if (!_is_id_generated) {
      return id;
    }
    if (!select_args.empty()) {
      std::string s = processor;
      s.append("[");
      for (const auto& cond_args : select_args) {
        s.append(cond_args.match).append(",");
      }
      s.append("]");
      return s;
    } else {
      return processor;
    }
  }
  if (!graph.empty()) {
    return cluster + "::" + graph;
  }
  return "unknown";
}
int Vertex::DumpDotDefine(std::string& s) {
  s.append("    ").append(GetDotId()).append(" [label=\"").append(GetDotLable()).append("\"");
  if (!cond.empty()) {
    s.append(" shape=diamond color=black fillcolor=aquamarine style=filled");
  }
  s.append("];\n");
  return 0;
}
int Vertex::DumpDotEdge(std::string& s) {
  for (auto& pair : _deps_idx) {
    VertexResult expected = _deps_expected_results[pair.second];
    const Vertex* dep = pair.first;
    s.append("    ").append(dep->GetDotId()).append(" -> ").append(GetDotId());
    switch (expected) {
      case V_RESULT_OK: {
        s.append(" [style=dashed label=\"ok\"];\n");
        break;
      }
      case V_RESULT_ERR: {
        s.append(" [style=dashed color=red label=\"err\"];\n");
        break;
      }
      default: {
        s.append(" [style=bold label=\"all\"];\n");
        break;
      }
    }
  }
  return 0;
}
void Vertex::Depend(Vertex* v, VertexResult expected) {
  v->_successor_vertex.insert(this);
  if (_deps_idx.count(v) == 0) {
    int idx = _deps_idx.size();
    _deps_idx[v] = idx;
    _deps_expected_results.resize(_deps_idx.size());
    _deps_expected_results[idx] = expected;
  }
}
int Vertex::BuildSuccessors(const std::set<std::string>& sucessor, VertexResult expected) {
  for (const std::string& id : sucessor) {
    Vertex* successor_vertex = _graph->FindVertexById(id);
    if (nullptr == successor_vertex) {
      WRDK_GRAPH_ERROR("No successor id:{}", id);
      return -1;
    }
    successor_vertex->Depend(this, expected);
  }
  return 0;
}
int Vertex::BuildDeps(const std::set<std::string>& dependency, VertexResult expected) {
  for (const std::string& dep : dependency) {
    Vertex* dep_vertex = _graph->FindVertexById(dep);
    if (nullptr == dep_vertex) {
      WRDK_GRAPH_ERROR("No dep  id:{}", dep);
      return -1;
    }
    Depend(dep_vertex, expected);
  }
  return 0;
}
int Vertex::Build() {
  for (auto& data : input) {
    if (data.merge.empty()) {
      Vertex* dep_vertex = _graph->FindVertexByData(data.id);
      if (nullptr == dep_vertex && !data.is_extern) {
        WRDK_GRAPH_ERROR("No dep input id:{}", data.id);
        return -1;
      }
      if (data._cond_vertex_id.empty()) {
        if (nullptr == dep_vertex) {
          continue;
        }
        Depend(dep_vertex, data.required ? V_RESULT_OK : V_RESULT_ALL);
      } else {
        Vertex* cond_vertex = _graph->FindVertexById(data._cond_vertex_id);
        Depend(cond_vertex, V_RESULT_OK);
        if (nullptr != dep_vertex) {
          cond_vertex->Depend(dep_vertex, data.required ? V_RESULT_OK : V_RESULT_ALL);
        }
      }
    } else {
      for (const std::string& merge_id : data.merge) {
        Vertex* dep_vertex = _graph->FindVertexByData(merge_id);
        if (nullptr == dep_vertex && !data.is_extern) {
          WRDK_GRAPH_ERROR("No dep input id:{}", merge_id);
          return -1;
        }
        if (nullptr == dep_vertex) {
          continue;
        }
        Depend(dep_vertex, data.required ? V_RESULT_OK : V_RESULT_ALL);
      }
    }
  }
  if (0 != BuildDeps(deps, V_RESULT_ALL)) {
    return -1;
  }
  if (0 != BuildDeps(deps_on_ok, V_RESULT_OK)) {
    return -1;
  }
  if (0 != BuildDeps(deps_on_err, V_RESULT_ERR)) {
    return -1;
  }
  if (0 != BuildSuccessors(successor, V_RESULT_ALL)) {
    return -1;
  }
  if (0 != BuildSuccessors(successor_on_ok, V_RESULT_OK)) {
    return -1;
  }
  if (0 != BuildSuccessors(successor_on_err, V_RESULT_ERR)) {
    return -1;
  }
  return 0;
}
int Vertex::GetDependencyIndex(Vertex* v) {
  auto found = _deps_idx.find(v);
  if (found == _deps_idx.end()) {
    return -1;
  }
  return found->second;
}
}  // namespace graph
}  // namespace wrdk
