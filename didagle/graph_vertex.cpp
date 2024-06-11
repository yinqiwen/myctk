// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "graph_vertex.h"
#include <regex>
#include <set>
#include <string>
#include "didagle/didagle_log.h"
#include "didagle/graph.h"
#include "didagle/graph_processor.h"
namespace didagle {
Vertex::Vertex() {
  // Params empty(true);
  // args = empty;
}
bool Vertex::FindVertexInSuccessors(Vertex* v) const {
  if (_successor_vertex.empty()) {
    return false;
  }
  if (_successor_vertex.count(v) > 0) {
    return true;
  }
  for (Vertex* succeed : _successor_vertex) {
    if (succeed->FindVertexInSuccessors(v)) {
      return true;
    }
  }
  return false;
}
int Vertex::FillInputOutput() {
  if (processor.empty()) {
    return 0;
  }
  auto p = ProcessorFactory::GetProcessor(processor);
  if (nullptr == p) {
    DIDAGLE_ERROR("No processor found for {}", processor);
    return -1;
  }
  for (const auto& input_id : p->GetInputIds()) {
    bool match = false;
    GraphData* matched_input = nullptr;
    for (auto& in : input) {
      if (in.field == input_id.name) {
        match = true;
        matched_input = &in;
        break;
      }
    }
    if (!match) {
      GraphData field;
      field.id = input_id.name;
      field.field = input_id.name;
      field.is_extern = input_id.flags.is_extern;
      field._is_in_out = input_id.flags.is_in_out;
      field.move = field._is_in_out ? true : false;
      input.push_back(field);
    } else {
      if (input_id.flags.is_in_out) {
        matched_input->move = true;
        matched_input->_is_in_out = true;
        if (input_id.flags.is_extern) {
          matched_input->is_extern = true;
        }
      }
    }
  }
  for (const auto& output_id : p->GetOutputIds()) {
    bool match = false;
    for (const auto& out : output) {
      if (out.field == output_id.name) {
        match = true;
        break;
      }
    }
    if (!match) {
      GraphData field;
      field.id = output_id.name;
      field.field = output_id.name;
      output.push_back(field);
    }
  }
  delete p;
  return 0;
}
void Vertex::SetGeneratedId(const std::string& v) {
  id = v;
  _is_id_generated = true;
}
bool Vertex::IsSuccessorsEmpty() { return _successor_vertex.empty(); }
bool Vertex::IsDepsEmpty() { return _deps_idx.empty(); }
bool Vertex::Verify() {
  if (is_start) {
    if (!deps.empty() || !deps_on_ok.empty() || !deps_on_err.empty()) {
      DIDAGLE_ERROR("Vertex:{} is marked as start vertex, but got deps configured.", GetDotLable());
      return false;
    }
  } else {
    if (IsSuccessorsEmpty() && IsDepsEmpty()) {
      DIDAGLE_ERROR("Vertex:{} has no deps and successors", id);
      return false;
    }
  }
  return true;
}
std::string Vertex::GetDotId() const { return _graph->name + "_" + id; }
std::string Vertex::GetDotLable() const {
  if (!cond.empty()) {
    return std::regex_replace(cond, std::regex("\""), "\\\"");
    // return cond;
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
  } else if (!graph.empty()) {
    s.append(" shape=box3d, color=blue fillcolor=aquamarine style=filled");
  } else {
    s.append(" color=black fillcolor=linen style=filled");
  }
  s.append("];\n");
  return 0;
}
int Vertex::DumpDotEdge(std::string& s) {
  if (!expect_config.empty()) {
    std::string expect_config_id = _graph->name + "_" + std::regex_replace(expect_config, std::regex("!"), "");
    // std::string expect_config_name =
    //     _graph->name + "_" + std::regex_replace(expect_config,
    //     std::regex("!"), "NOT_");
    s.append("    ").append(expect_config_id).append(" -> ").append(GetDotId());
    if (expect_config[0] == '!') {
      s.append(" [style=dashed color=red label=\"err\"];\n");
    } else {
      s.append(" [style=bold label=\"ok\"];\n");
    }

    s.append("    ").append(_graph->name + "__START__").append(" -> ").append(expect_config_id).append(";\n");
  }
  if (_successor_vertex.empty()) {
    s.append("    ").append(GetDotId()).append(" -> ").append(_graph->name + "__STOP__;\n");
  }
  if (_deps_idx.empty()) {
    s.append("    ").append(_graph->name + "__START__").append(" -> ").append(GetDotId()).append(";\n");
  }
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
      DIDAGLE_ERROR("No successor id:{}", id);
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
      DIDAGLE_ERROR("No dep  id:{}", dep);
      return -1;
    }
    Depend(dep_vertex, expected);
  }
  return 0;
}
void Vertex::MergeSuccessor() {
  successor_on_ok.insert(consequent.begin(), consequent.end());
  successor_on_err.insert(alternative.begin(), alternative.end());
}
bool Vertex::IsCondVertex() const { return !cond.empty(); }
int Vertex::Build() {
  for (auto& select : select_args) {
    if (select.IsCondExpr()) {
      continue;
    }
    if (!_graph->_cluster->ContainsConfigSetting(select.match)) {
      DIDAGLE_ERROR("No config_setting with name:{} defined.", select.match);
      return -1;
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
  for (auto& data : input) {
    if (data.aggregate.empty()) {
      Vertex* dep_vertex = _graph->FindVertexByData(data.id);
      if (nullptr == dep_vertex && !data.is_extern) {
        DIDAGLE_ERROR("[{}]No dep input id:{}", GetDotLable(), data.id);
        return -1;
      }
      if (nullptr == dep_vertex) {
        continue;
      }
      if (data._is_in_out && (dep_vertex == this)) {
        continue;
      }
      if (dep_vertex == this) {
        DIDAGLE_ERROR("[{}] unexpected:{}", GetDotLable(), data.id);
      }
      Depend(dep_vertex, data.required ? V_RESULT_OK : V_RESULT_ALL);
    } else {
      for (const std::string& aggregate_id : data.aggregate) {
        Vertex* dep_vertex = _graph->FindVertexByData(aggregate_id);
        if (nullptr == dep_vertex && !data.is_extern) {
          DIDAGLE_ERROR("No dep input id:{}", aggregate_id);
          return -1;
        }
        if (nullptr == dep_vertex) {
          continue;
        }
        Depend(dep_vertex, data.required ? V_RESULT_OK : V_RESULT_ALL);
      }
    }
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
}  // namespace didagle
