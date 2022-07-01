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
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>
#include "didagle/graph_params.h"
#include "kcfg_toml.h"

namespace didagle {
struct GraphData {
  std::string id;
  std::string field;
  std::string move_from_when_skipped;
  std::vector<std::string> aggregate;
  bool required = false;
  bool move = false;
  bool is_extern = false;
  bool _is_in_out = false;

  KCFG_TOML_DEFINE_FIELD_MAPPING(({"is_extern", "extern"}))
  KCFG_TOML_DEFINE_FIELDS(id, field, move_from_when_skipped, required, move, is_extern, aggregate)
};

struct ConfigSetting {
  std::string name;
  std::string cond;
  std::string processor;
  KCFG_TOML_DEFINE_FIELDS(name, cond, processor)
};

struct GraphParams : public Params {
  bool ParseFromToml(const kcfg::TomlValue& doc);
};

}  // namespace didagle