// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "graph.h"
#include <iostream>

namespace wrdk {
std::string GraphNode::ToString() const {
  std::string s;
  if (!phase.empty()) {
    return phase;
  }
  if (!script.empty()) {
    return script + "::" + function;
  }
  return cond;
}
int GraphFunction::Init() {
  for (auto& n : nodes) {
    if (n.id.empty()) {
      // generate unique id for node without id
      n.id = name + "_" + std::to_string(_idx);
      _idx++;
    }
    if (_nodes.find(n.id) != _nodes.end()) {
      printf("Duplicate id:%s\n", n.id.c_str());
      return -1;
    }
    _nodes[n.id] = &n;

    // construct data_id->node mapping
    for (const auto& id : n.output) {
      auto found = data_mapping_table.find(id);
      if (found != data_mapping_table.end()) {
        printf("Duplicate data name:%s\n", id.c_str());
        return -1;
      }
      _data_mapping_table[id] = &n;
    }
  }
  for (auto& n : nodes) {
    for (const auto& id : n.input) {
      auto found = _data_mapping_table.find(id);
      if (found == _data_mapping_table.end()) {
        printf("No dep input id:%s\n", id.c_str());
        return -1;
      }
      // compute node dependency by input data
      n.dependents.insert(found->second->id);
    }

    for (const auto& dep : n.optional) {
      auto found = _nodes.find(dep.cond_id);
      if (found == _nodes.end()) {
        printf("No dep cond id:%s\n", dep.cond_id.c_str());
        return -1;
      }
      auto cond_node = found->second;
      cond_node->consequent.insert(n.id);
      // n.dependents.insert(dep.cond_id);
      for (const auto& id : dep.data) {
        auto found = _data_mapping_table.find(id);
        if (found == _data_mapping_table.end()) {
          printf("No cond dep input id:%s\n", id.c_str());
          return -1;
        }
        found->second->successor.insert(dep.cond_id);
        // found->second->optional_successor[dep.cond_id].insert(n.id);
      }
    }
    for (const auto& dep : n.dependents) {
      auto found = _nodes.find(dep);
      if (found == _nodes.end()) {
        printf("No dep %s found\n", dep.c_str());
        return -1;
      }
      found->second->successor.insert(n.id);
    }
  }
  return 0;
}
std::string GraphFunction::GetNodeId(const std::string& id) { return name + "_" + id; }
int GraphFunction::DumpDot(std::string& s) {
  s.append("  subgraph cluster_").append(name).append("{\n");
  s.append("    style = rounded;\n");
  s.append("    label = \"").append(name).append("\";\n");
  for (auto& n : nodes) {
    s.append("    ").append(GetNodeId(n.id)).append(" [label=\"").append(n.ToString()).append("\"");
    if (!n.cond.empty()) {
      s.append(" shape=diamond color=black fillcolor=aquamarine style=filled");
    }
    s.append("];\n");
  }
  for (auto& n : nodes) {
    for (const auto& successor : n.successor) {
      s.append("    ")
          .append(GetNodeId(n.id))
          .append(" -> ")
          .append(GetNodeId(successor))
          .append(";\n");
    }
    for (const auto& successor : n.consequent) {
      s.append("    ")
          .append(GetNodeId(n.id))
          .append(" -> ")
          .append(GetNodeId(successor))
          .append(" [style=dashed label=\"true\"];\n");
    }
    for (const auto& successor : n.alternative) {
      s.append("    ")
          .append(GetNodeId(n.id))
          .append(" -> ")
          .append(GetNodeId(successor))
          .append(" [style=dashed label=\"false\"];\n");
    }
  }
  s.append("};\n");
  return 0;
}
int GraphScript::Init() {
  for (auto& f : function) {
    _funcs[f.name] = &f;
    if (0 != f.Init()) {
      return -1;
    }
  }
  return 0;
}
int GraphScript::DumpDot(std::string& s) {
  s.append("digraph G {\n");
  s.append("    rankdir=LR;\n");
  for (auto& f : function) {
    f.DumpDot(s);
  }
  s.append("}\n");

  return 0;
}
}  // namespace wrdk

using namespace wrdk;

// template <class T, typename std::enable_if<has_field_mapping<T>::value, bool>::type* = nullptr>
// void do_stuff(T& t) {
//   std::cout << "do_stuff mapping\n";
//   // an implementation for integral types (int, char, unsigned, etc.)
// }

// template <class T, typename std::enable_if<!has_field_mapping<T>::value, T>::type* = nullptr>
// void do_stuff(T& t) {
//   // an implementation for class types
//   std::cout << "do_stuff other\n";
// }

int main() {
  GraphScript script;
  bool v = ParseFromTomlFile("graph.toml", script);
  if (!v) {
    printf("Failed to parse toml\n");
    return -1;
  }
  if (0 != script.Init()) {
    printf("Failed to init script\n");
    return -1;
  }
  std::string dot;
  script.DumpDot(dot);
  printf("%s\n", dot.c_str());
  return 0;
}
