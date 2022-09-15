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
#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "didagle/graph_processor_api.h"

namespace didagle {

struct ProcessorMeta {
  std::string name;
  std::string desc;
  bool isIOProcessor = false;
  std::vector<FieldInfo> input;
  std::vector<FieldInfo> output;
  std::vector<ParamInfo> params;
  KCFG_DEFINE_FIELDS(name, desc, isIOProcessor, input, output, params)
};

class ProcessorFactory {
 public:
  static void Register(std::string_view name, const ProcessorCreator& creator);
  static Processor* GetProcessor(const std::string& name);
  static void GetAllMetas(std::vector<ProcessorMeta>& metas);
  static int DumpAllMetas(const std::string& file = "all_processors.json");
};

struct ProcessorRunResult {
  std::shared_ptr<Processor> processor;
  int rc = -1;
  explicit ProcessorRunResult(int v = -1) : rc(v) {}
};

struct ProcessorRunOptions {
  const Params* params = nullptr;
  std::map<std::string, std::vector<std::string>> map_aggregate_ids;
};

ProcessorRunResult run_processor(GraphDataContext& ctx, const std::string& proc, const ProcessorRunOptions& opts = {});

}  // namespace didagle