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
#include <stdint.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "folly/SharedMutex.h"
#include "folly/container/F14Map.h"
#include "robims.pb.h"
#include "robims_field.h"
#include "robims_id_mapping.h"
#include "simdjson.h"

namespace robims {

class RobimsTable {
 private:
  RobimsTable(RobimsTable&) = delete;
  RobimsTable& operator=(const RobimsTable&) = delete;
  TableSchema _schema;

  typedef folly::F14FastMap<std::string_view, std::unique_ptr<RobimsField>> RobimsFieldTable;
  RobimsFieldTable _fields;
  IDMapping* _id_mapping;
  RoaringBitmap _id_bitmap;

  int GetLocalId(simdjson::ondemand::document& doc, uint32_t& id);

 public:
  RobimsTable(IDMapping* id_mapping);
  const TableSchema& GetSchema();
  RoaringBitmap& GetTableBitmap();
  IDMapping* GetIDMapping();
  RobimsField* GetField(const std::string& name);
  int Init(const TableSchema& schema);
  int CreateTable(const std::string& schema);
  int Put(const std::string& json);
  int Remove(const std::string& json);
  int Select(const std::string_view& field, FieldOperator op, FieldArg arg, CRoaringBitmapPtr& out);
  // int Visit(const roaring_bitmap_t* b, const VisitOptions& options);
  int Save(FILE* fp, bool readonly);
  int Load(FILE* fp);
};
}  // namespace robims