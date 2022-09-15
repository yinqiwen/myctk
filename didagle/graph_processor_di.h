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
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "didagle/graph_processor.h"
namespace didagle {
class ProcessorDI {
 private:
  Processor* _proc;
  bool _strict_dsl;
  typedef std::pair<FieldInfo, const GraphData*> FieldData;
  typedef std::map<std::string, FieldData> FieldDataTable;
  FieldDataTable _input_ids;
  FieldDataTable _output_ids;
  int SetupInputOutputIds(const std::vector<FieldInfo>& fields, const std::vector<GraphData>& config_fields,
                          FieldDataTable& field_ids);

 public:
  explicit ProcessorDI(Processor* proc, bool strict_dsl = false);
  const FieldDataTable& GetInputIds() { return _input_ids; }
  const FieldDataTable& GetOutputIds() { return _output_ids; }
  int PrepareInputs(const std::vector<GraphData>& config_inputs = {});
  int PrepareOutputs(const std::vector<GraphData>& config_outputs = {});
  int InjectInputs(GraphDataContext& ctx, const Params* params);
  int CollectOutputs(GraphDataContext& ctx, const Params* params);
  int MoveDataWhenSkipped(GraphDataContext& ctx);
};

}  // namespace didagle
