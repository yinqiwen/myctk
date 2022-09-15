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
#include <utility>
#include <vector>
#include "didagle/graph_data.h"
#include "didagle/graph_processor_api.h"
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
  // 是否是条件表达式 包含有$符号算条件表达式
  bool IsCondExpr() const {
    if (match.empty()) return false;
    for (const auto& ch : match) {
      if (!isalpha(ch) && !isdigit(ch) && ch != '_') {
        return true;
      }
    }
    return false;
  }
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
  std::set<std::string> expect_deps;  // expect可能有独立的依赖
  std::string expect_config;
  bool is_start = false;

  // sub graph node
  std::string cluster;
  std::string graph;

  // dependents or successors
  std::set<std::string> successor;
  std::set<std::string> successor_on_ok;
  std::set<std::string> successor_on_err;
  std::set<std::string> consequent;
  std::set<std::string> alternative;
  std::set<std::string> deps;
  std::set<std::string> deps_on_ok;
  std::set<std::string> deps_on_err;

  std::vector<GraphData> input;
  std::vector<GraphData> output;

  bool ignore_processor_execute_error = true;

  std::unordered_set<Vertex*> _successor_vertex;
  std::vector<VertexResult> _deps_expected_results;
  std::unordered_map<Vertex*, int> _deps_idx;
  Graph* _graph = nullptr;
  Graph* _vertex_graph = nullptr;
  bool _is_id_generated = false;
  bool _is_cond_processor = false;

  KCFG_TOML_DEFINE_FIELD_MAPPING(({"consequent", "if"}, {"alternative", "else"}, {"is_start", "start"}))

  KCFG_TOML_DEFINE_FIELDS(id, processor, args, cond, expect, expect_deps, expect_config, is_start, select_args, cluster,
                          graph, successor, successor_on_ok, successor_on_err, consequent, alternative, deps,
                          deps_on_ok, deps_on_err, input, output, ignore_processor_execute_error)
  Vertex();
  bool IsCondVertex() const;
  void MergeSuccessor();
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