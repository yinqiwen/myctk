/*
 *Copyright (c) 2021, qiyingwang <qiyingwang@tencent.com>
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
#include <functional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "roaring/roaring.h"
#include "robims.pb.h"
#include "robims_id_mapping.h"

namespace robims {
typedef std::variant<std::string_view, int64_t, double> FieldArg;
class RobimsDBImpl;
class RobimsDB {
 private:
  RobimsDB(const RobimsDB&) = delete;
  RobimsDB& operator=(const RobimsDB&) = delete;
  RobimsDBImpl* db_impl_;

 public:
  RobimsDB();
  int Load(const std::string& file);
  int Save(const std::string& file, bool readonly);
  int SaveTable(const std::string& file, const std::string& table, bool readonly);
  int CreateTable(const std::string& schema);
  int CreateTable(const TableSchema& schema);
  int Put(const std::string& table, const std::string& json);
  int Remove(const std::string& table, const std::string& json);
  int Select(const std::string& query, std::vector<std::string>& ids);
  ~RobimsDB();
};
}  // namespace robims