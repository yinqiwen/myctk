// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/16
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once

#include <stdint.h>
#include <atomic>
#include <functional>
#include <memory>
#include <unordered_set>
#include "graph_data.h"
#include "graph_processor_api.h"
#include "kcfg_toml.h"
namespace didagle {
enum VertexResult {
  V_RESULT_INVALID = 0,
  V_RESULT_OK = 1,
  V_RESULT_ERR = 2,
  V_RESULT_ALL = 3,
};

enum VertexErrCode {
  V_CODE_INVALID = 0,
  V_CODE_OK = 1,
  V_CODE_ERR = 2,
  V_CODE_SKIP = 3,
};

struct CondParams {
  std::string match;
  GraphParams args;
  KCFG_TOML_DEFINE_FIELDS(match, args)
};

struct Graph;
struct Vertex {
  std::string id;

  // phase node
  std::string processor;
  GraphParams args;
  std::vector<CondParams> select_args;
  std::string cond;
  std::string expect;
  std::string expect_config;
  bool is_start = false;

  // sub graph node
  std::string cluster;
  std::string graph;

  // dependents or successors
  std::set<std::string> successor;
  std::set<std::string> successor_on_ok;
  std::set<std::string> successor_on_err;
  std::set<std::string> deps;
  std::set<std::string> deps_on_ok;
  std::set<std::string> deps_on_err;

  std::vector<GraphData> input;
  std::vector<GraphData> output;

  std::unordered_set<Vertex*> _successor_vertex;
  std::vector<VertexResult> _deps_expected_results;
  std::unordered_map<Vertex*, int> _deps_idx;
  Graph* _graph = nullptr;
  Graph* _vertex_graph = nullptr;
  bool _is_id_generated = false;

  KCFG_TOML_DEFINE_FIELD_MAPPING(({"successor_on_ok", "if"}, {"successor_on_err", "else"},
                                  {"is_start", "start"}))

  KCFG_TOML_DEFINE_FIELDS(id, processor, args, cond, expect, expect_config, is_start, select_args,
                          cluster, graph, successor, successor_on_ok, successor_on_err, deps,
                          deps_on_ok, deps_on_err, input, output)
  Vertex();
  bool FindVertexInSuccessors(Vertex* v) const;
  int FillInputOutput();
  void SetGeneratedId(const std::string& id);
  bool IsSuccessorsEmpty();
  bool IsDepsEmpty();
  bool Verify();
  void Depend(Vertex* v, VertexResult expected);
  int BuildDeps(const std::set<std::string>& dependency, VertexResult expected);
  int BuildSuccessors(const std::set<std::string>& sucessor, VertexResult expected);
  int Build();
  std::string GetDotLable() const;
  std::string GetDotId() const;
  int DumpDotDefine(std::string& s);
  int DumpDotEdge(std::string& s);
  int GetDependencyIndex(Vertex* v);
};

}  // namespace didagle