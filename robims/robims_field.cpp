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
#include "robims_field.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <robims_common.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <string_view>
#include <variant>
#include "robims_err.h"
#include "robims_log.h"
#include "robims_table.h"
namespace robims {

int RobimsField::Put(uint32_t id, const std::string_view& val) {
  ROBIMS_ERROR("Unimplemented!");
  return ROBIMS_ERR_UNIMPLEMENTED;
}
int RobimsField::Put(uint32_t id, int64_t val) {
  ROBIMS_ERROR("Unimplemented!");
  return ROBIMS_ERR_UNIMPLEMENTED;
}
int RobimsField::Put(uint32_t id, float val) {
  ROBIMS_ERROR("Unimplemented!");
  return ROBIMS_ERR_UNIMPLEMENTED;
}
int RobimsField::Put(uint32_t id) {
  ROBIMS_ERROR("Unimplemented!");
  return ROBIMS_ERR_UNIMPLEMENTED;
}
int RobimsField::Put(uint32_t id, const std::string_view& val, float weight) {
  ROBIMS_ERROR("Unimplemented!");
  return ROBIMS_ERR_UNIMPLEMENTED;
}
int RobimsField::Remove(uint32_t id) {
  ROBIMS_ERROR("Unimplemented!");
  return ROBIMS_ERR_UNIMPLEMENTED;
}
int RobimsField::Select(FieldOperator op, FieldArg arg, CRoaringBitmapPtr& out) {
  ROBIMS_ERROR("Unimplemented!");
  return ROBIMS_ERR_UNIMPLEMENTED;
}
// int RobimsField::Visit(const roaring_bitmap_t* b, const VisitOptions& options) {
//   iterate_bitmap(
//       [&options](uint32_t v) {
//         if (options.filter) {
//           if (options.filter(v)) {
//             return true;
//           }
//         }
//         if (options.visit) {
//           return options.visit(v, 0);
//         }
//         return true;
//       },
//       b);
//   return 0;
// }
int RobimsField::DoLoad(FILE* fp) {
  ROBIMS_ERROR("Unimplemented!");
  return ROBIMS_ERR_UNIMPLEMENTED;
}
int RobimsField::DoSave(FILE* fp, bool readonly) {
  ROBIMS_ERROR("Unimplemented!");
  return ROBIMS_ERR_UNIMPLEMENTED;
}
int RobimsField::Save(FILE* fp, bool readonly) {
  std::string meta = _meta.SerializeAsString();
  int rc = file_write_string(fp, meta);
  if (0 != rc) {
    return rc;
  }
  return DoSave(fp, readonly);
}
int RobimsField::Load(FILE* fp) {
  std::string meta_bin;
  if (0 != file_read_string(fp, meta_bin)) {
    return -1;
  }
  if (!_meta.ParseFromString(meta_bin)) {
    ROBIMS_ERROR("Failed to parse field meta!");
    return -1;
  }
  return DoLoad(fp);
}

int RobimsField::Init(RobimsTable* table, const FieldMeta& meta) {
  _meta = meta;
  _table = table;
  int rc = OnInit();
  return rc;
}

RobimsField* RobimsFieldBuilder::Build(RobimsTable* table, const FieldMeta& meta) {
  RobimsField* field = nullptr;
  switch (meta.index_type()) {
    case MUTEX_INDEX:
    case SET_INDEX: {
      field = new RobimsSetField;
      break;
    }
    case BOOL_INDEX: {
      field = new RobimsBoolField;
      break;
    }
    case WEIGHT_SET_INDEX: {
      field = new RobimsWeightSetField;
      break;
    }
    case INT_INDEX: {
      field = new RobimsIntField;
      break;
    }
    case FLOAT_INDEX: {
      field = new RobimsFloatField;
      break;
    }
    default: {
      break;
    }
  }
  if (nullptr == field) {
    ROBIMS_ERROR("Create field failed with type:{}", meta.index_type());
    return nullptr;
  }
  int rc = field->Init(table, meta);
  if (0 != rc) {
    ROBIMS_ERROR("Init field failed with type:{}", meta.index_type());
    delete field;
    return nullptr;
  }
  return field;
}
}  // namespace robims
