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
#include "robims_table.h"
#include <algorithm>
#include <cctype>
#include <memory>
#include <string_view>
#include "robims_common.h"
#include "robims_err.h"
#include "robims_log.h"
#include "simdjson.h"
namespace robims {

RobimsTable::RobimsTable(IDMapping* id_mapping) : _id_mapping(id_mapping) {}
const TableSchema& RobimsTable::GetSchema() { return _schema; }
RoaringBitmap& RobimsTable::GetTableBitmap() { return _id_bitmap; }
IDMapping* RobimsTable::GetIDMapping() { return _id_mapping; }
int RobimsTable::Init(const TableSchema& schema) {
  _schema = schema;
  _id_bitmap.NewCRoaringBitmap();
  for (const auto& field_meta : schema.index_field()) {
    auto field = RobimsFieldBuilder::Build(this, field_meta);
    if (nullptr == field) {
      return -1;
    }

    _fields[field->GetFieldMeta().name()].reset(field);
  }
  return 0;
}

int RobimsTable::GetLocalId(simdjson::ondemand::document& doc, uint32_t& id) {
  id = 0;
  auto id_result = doc.find_field(_schema.id_field());
  if (id_result.is_null()) {
    ROBIMS_ERROR("No '{}' ID field found.", _schema.id_field());
    return -1;
  }
  std::string_view id_view;
  // int64_t id_int = 0;
  if (0 == id_result.get_string().error()) {
    id_view = id_result.get_string().value();
  } else if (0 == id_result.get_int64().error()) {
    id_view = id_result.raw_json_token();
    // id_int = id_result.get_int64().value();
    // std::string_view tmp((const char*)(&id_int), sizeof(id_int));
    // id_view = tmp;
  } else {
    ROBIMS_ERROR("Invalid '{}' ID field type.", _schema.id_field());
    return -1;
  }
  int rc = _id_mapping->GetLocalID(id_view, !_schema.disable_local_id_creation(), id);
  if (0 != rc) {
    ROBIMS_ERROR("No local id found for {}.", id_result.raw_json_token().value());
    return -1;
  }
  // ROBIMS_INFO("Get local_id:{} for id:{}", id, id_view);
  return 0;
}
RobimsField* RobimsTable::GetField(const std::string& field) {
  auto found = _fields.find(field);
  if (found == _fields.end()) {
    return nullptr;
  }

  return found->second.get();
}
int RobimsTable::Select(const std::string_view& field, FieldOperator op, FieldArg arg,
                        CRoaringBitmapPtr& out) {
  auto found = _fields.find(field);
  if (found == _fields.end()) {
    return ROBIMS_ERR_NOTFOUND;
  }

  return found->second->Select(op, arg, out);
}
// int RobimsTable::Visit(const roaring_bitmap_t* b, const VisitOptions& options) {
//   auto found = _fields.find(options.field);
//   if (found == _fields.end()) {
//     return ROBIMS_ERR_NOTFOUND;
//   }
//   return found->second->Visit(b, options);
// }

int RobimsTable::Remove(const std::string& json) {
  simdjson::ondemand::parser parser;
  simdjson::ondemand::document doc = parser.iterate(json);
  uint32_t id = 0;
  if (0 != GetLocalId(doc, id)) {
    return -1;
  }
  for (const auto& field_pair : _fields) {
    field_pair.second->Remove(id);
  }
  return 0;
}
int RobimsTable::Put(const std::string& json) {
  simdjson::padded_string padded(json);
  simdjson::ondemand::parser parser;
  simdjson::ondemand::document doc;
  auto error = parser.iterate(padded).get(doc);
  if (error) {
    ROBIMS_ERROR("Parse json error:{}", error);
    return -1;
  }
  uint32_t id = 0;
  if (0 != GetLocalId(doc, id)) {
    ROBIMS_ERROR("Faield to get local id");
    return -1;
  }
  roaring_bitmap_add(_id_bitmap.bitmap.get(), id);
  for (const auto& field_pair : _fields) {
    auto field_result = doc.find_field_unordered(field_pair.first);
    RobimsField* field = field_pair.second.get();
    if (field_result.error()) {
      if (BOOL_INDEX != field->GetFieldMeta().index_type()) {
        continue;
      }
    }
    // ROBIMS_ERROR("enter for field:{}/{}", field_pair.first, field->GetFieldMeta().index_type());
    switch (field->GetFieldMeta().index_type()) {
      case SET_INDEX: {
        if (field_result.get_array().error()) {
          ROBIMS_ERROR("{} is not array field", field_pair.first);
          return -1;
        }
        field->Remove(id);
        simdjson::ondemand::array array;
        doc[field_pair.first].get(array);
        auto it = array.begin();
        while (it != array.end()) {
          auto f = *(it.value());
          if (f.get_string().error()) {
            ROBIMS_ERROR("Invalid field type for SET");
            return -1;
          }
          field->Put(id, f.get_string().value());
          ++it;
        }
        break;
      }
      case MUTEX_INDEX: {
        if (field_result.get_string().error()) {
          ROBIMS_ERROR("{} is not string field", field_pair.first);
          return -1;
        }
        field->Remove(id);
        field->Put(id, field_result.get_string().value());
        break;
      }
      case BOOL_INDEX: {
        int64_t v = 0;
        if (!field_result.error()) {  // field exist
          if (field_result.get_bool().error()) {
            ROBIMS_ERROR("{} is not bool field", field_pair.first);
            return -1;
          }

          if (field_result.get_bool().value()) {
            v = 1;
          }
        }
        field->Put(id, v);
        break;
      }
      case WEIGHT_SET_INDEX: {
        if (field_result.get_object().error()) {
          ROBIMS_ERROR("{} is not object field", field_pair.first);
          return -1;
        }
        field->Remove(id);
        auto it = field_result.get_object().begin();
        while (it != field_result.get_object().end()) {
          auto f = *(it.value());
          auto key = f.unescaped_key();
          auto val = f.value();
          if (val.get_double().error()) {
            ROBIMS_ERROR("Invalid '{}' weight field type.", key.value());
            return -1;
          }
          field->Put(id, key.value(), val.get_double().value());
          ++it;
        }
        break;
      }
      case INT_INDEX: {
        if (field_result.get_int64().error()) {
          ROBIMS_ERROR("{} is not int field", field_pair.first);
          return -1;
        }

        field->Put(id, field_result.get_int64().value());
        break;
      }
      case FLOAT_INDEX: {
        if (field_result.get_double().error()) {
          ROBIMS_ERROR("{} is not double field", field_pair.first);
          return -1;
        }
        field->Put(id, (float)(field_result.get_double().value()));
        break;
      }
      default: {
        break;
      }
    }
  }
  return 0;
}

int RobimsTable::Save(FILE* fp, bool readonly) {
  std::string meta = _schema.SerializeAsString();
  int rc = file_write_string(fp, meta);
  if (0 != rc) {
    ROBIMS_ERROR("Failed to save table schema");
    return rc;
  }
  rc = _id_bitmap.Save(fp, readonly);
  if (0 != rc) {
    ROBIMS_ERROR("Failed to save table id bitmap");
    return rc;
  }
  for (int i = 0; i < _schema.index_field_size(); i++) {
    auto found = _fields.find(_schema.index_field(i).name());
    if (found == _fields.end()) {
      ROBIMS_ERROR("No robims field:{} instance.", _schema.index_field(i).name());
      return -1;
    }
    rc = found->second->Save(fp, readonly);
    if (0 != rc) {
      ROBIMS_ERROR("Failed to save robims field:{} instance.", _schema.index_field(i).name());
      return rc;
    }
  }
  return 0;
}
int RobimsTable::Load(FILE* fp) {
  std::string meta_bin;
  if (0 != file_read_string(fp, meta_bin)) {
    ROBIMS_ERROR("Failed to read table schema");
    return -1;
  }
  if (!_schema.ParseFromString(meta_bin)) {
    ROBIMS_ERROR("Failed to parse table schema!");
    return -1;
  }
  ROBIMS_INFO("Load table:{}", _schema.DebugString());
  int rc = _id_bitmap.Load(fp);
  if (0 != rc) {
    ROBIMS_ERROR("Failed to load table id bitmap");
    return rc;
  }
  _fields.clear();
  for (int i = 0; i < _schema.index_field_size(); i++) {
    const auto& meta = _schema.index_field(i);
    auto field = RobimsFieldBuilder::Build(this, meta);
    int rc = field->Load(fp);
    if (0 != rc) {
      ROBIMS_ERROR("Failed to load robims field:{} instance.", meta.name());
      return rc;
    }
    _fields[meta.name()].reset(field);
  }
  return 0;
}
}  // namespace robims
