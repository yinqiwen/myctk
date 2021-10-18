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
#include "robims_db_impl.h"
#include <fcntl.h>
#include <google/protobuf/util/json_util.h>
#include <robims_common.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string_view>
#include "robims_log.h"
#include "robims_simple_id_mapping.h"

namespace robims {
static const uint8_t kDBSaveType = 0;
static const uint8_t kTableSaveType = 1;
RobimsDB::RobimsDB() { db_impl_ = new RobimsDBImpl; }
int RobimsDB ::Load(const std::string& file) { return db_impl_->Load(file); }
int RobimsDB ::CreateTable(const TableSchema& schema) { return db_impl_->CreateTable(schema); }
int RobimsDB::CreateTable(const std::string& schema) { return db_impl_->CreateTable(schema); }
int RobimsDB::Put(const std::string& table, const std::string& json) {
  return db_impl_->Put(table, json);
}
int RobimsDB::Remove(const std::string& table, const std::string& json) {
  return db_impl_->Remove(table, json);
}
int RobimsDB::Save(const std::string& file, bool readonly) {
  return db_impl_->Save(file, readonly);
}
int RobimsDB::SaveTable(const std::string& file, const std::string& table, bool readonly) {
  return db_impl_->SaveTable(file, table, readonly);
}
int RobimsDB::Select(const std::string& query, int64_t offset, int64_t limit,
                     SelectResult& result) {
  return db_impl_->Select(query, offset, limit, result);
}
void RobimsDB::DisableThreadSafe() { db_impl_->DisableThreadSafe(); }
void RobimsDB::EnableThreadSafe() { db_impl_->EnableThreadSafe(); }

RobimsDB::~RobimsDB() { delete db_impl_; }
RobimsDBData::RobimsDBData() : id_mapping(new SimpleIDMapping), query_cache(1024) {}
RobimsDBData::~RobimsDBData() { delete id_mapping; }
RobimsDBImpl::RobimsDBImpl() : shared_mutex_(nullptr) {
  std::shared_ptr<RobimsDBData> p(new RobimsDBData);
  db_data_.store(p);
}
RobimsDBImpl::~RobimsDBImpl() { DisableThreadSafe(); }

void RobimsDBImpl::DisableThreadSafe() {
  if (nullptr != shared_mutex_) {
    delete shared_mutex_;
    shared_mutex_ = nullptr;
  }
}
void RobimsDBImpl::EnableThreadSafe() {
  DisableThreadSafe();
  shared_mutex_ = new folly::SharedMutex;
}

int RobimsDBImpl::Load(const std::string& file) {
  FILE* fp = fopen(file.c_str(), "r");
  if (nullptr == fp) {
    ROBIMS_ERROR("Failed to open file:{} to load robims db", file);
    return -1;
  }
  uint8_t save_type = 0;
  int rc = fread(&save_type, 1, 1, fp);
  if (1 != rc) {
    ROBIMS_ERROR("Failed to read save_type from file:{}", file);
    fclose(fp);
    return -1;
  }
  rc = 0;
  if (kTableSaveType == save_type) {
    std::shared_ptr<RobimsDBData> db = db_data_.load();
    std::string table_name;
    if (0 != file_read_string(fp, table_name)) {
      ROBIMS_ERROR("Failed to read table name from file:{}", file);
      fclose(fp);
      return -1;
    }
    auto found = db->tables.find(table_name);
    if (found == db->tables.end()) {
      ROBIMS_ERROR("No table found for name:{}", table_name);
      fclose(fp);
      return -1;
    }
    auto old_table = found->second.load();
    auto new_table = CreateTableInstance(old_table->GetSchema());
    if (!new_table) {
      fclose(fp);
      return -1;
    }
    rc = new_table->Load(fp);
    if (0 == rc) {
      found->second.store(new_table);
    }
  } else {
    std::shared_ptr<RobimsDBData> new_db(new RobimsDBData);
    uint32_t table_size = 0;
    file_read_uint32(fp, table_size);
    ROBIMS_INFO("There is {} tables in robims db", table_size);
    for (uint32_t i = 0; i < table_size; i++) {
      std::shared_ptr<RobimsTable> table(new RobimsTable(new_db->id_mapping));
      if (0 != table->Load(fp)) {
        ROBIMS_ERROR("Failed to load table:{}", table->GetSchema().DebugString());
        fclose(fp);
        return -1;
      }
      RobimsTablePtr store_table(table);
      std::string_view table_name = table->GetSchema().name();
      // new_db->tables.insert(std::make_pair(table_name, store_table));
      new_db->tables[table_name].store(table);
    }
    rc = new_db->id_mapping->Load(fp);
    if (0 == rc) {
      db_data_.store(new_db);
    }
  }
  fclose(fp);
  return rc;
}
int RobimsDBImpl::SaveTable(const std::string& file, const std::string& table, bool readonly) {
  std::shared_ptr<RobimsDBData> db = db_data_.load();
  auto found = db->tables.find(table);
  if (found == db->tables.end()) {
    ROBIMS_ERROR("Table:{} not found while tables:{}", table, db->tables.size());
    return -1;
  }
  auto table_obj = found->second.load();
  FILE* fp = fopen(file.c_str(), "w+");
  if (nullptr == fp) {
    ROBIMS_ERROR("Failed to create file:{} to save robims table", file);
    return -1;
  }
  uint8_t type = kTableSaveType;
  int rc = fwrite(&type, 1, 1, fp);
  if (1 != rc) {
    ROBIMS_ERROR("Failed to write save type to file:{}", file);
    fclose(fp);
    return -1;
  }
  rc = file_write_string(fp, table_obj->GetSchema().name());
  if (0 != rc) {
    ROBIMS_ERROR("Failed to write save table name to file:{}", file);
    fclose(fp);
    return -1;
  }
  rc = table_obj->Save(fp, readonly);
  fclose(fp);
  return rc;
}
int RobimsDBImpl::Save(const std::string& file, bool readonly) {
  std::shared_ptr<RobimsDBData> db = db_data_.load();
  FILE* fp = fopen(file.c_str(), "w+");
  if (nullptr == fp) {
    ROBIMS_ERROR("Failed to create file:{} to save robims db", file);
    return -1;
  }
  uint8_t type = kDBSaveType;
  int rc = fwrite(&type, 1, 1, fp);
  if (1 != rc) {
    ROBIMS_ERROR("Failed to write save type to file:{}", file);
    return -1;
  }

  uint32_t table_size = db->tables.size();
  rc = file_write_uint32(fp, table_size);
  if (0 != rc) {
    ROBIMS_ERROR("Failed to write table size to file:{} to save robims db", file);
    return -1;
  }
  for (auto& pair : db->tables) {
    auto table = pair.second.load();
    int rc = table->Save(fp, readonly);
    if (0 != rc) {
      fclose(fp);
      return rc;
    }
  }
  rc = db->id_mapping->Save(fp, readonly);
  fclose(fp);
  return rc;
}

void RobimsDBImpl::GetRealIDs(const std::vector<uint32_t>& local_ids,
                              std::vector<std::string>& ids) {
  std::shared_ptr<RobimsDBData> db = db_data_.load();
  ids.reserve(local_ids.size());
  for (uint32_t local_id : local_ids) {
    std::string id;
    std::string_view id_view;
    if (0 == db->id_mapping->GetID(local_id, id_view)) {
      id.assign(id_view.data(), id_view.size());
      ids.emplace_back(std::move(id));
    } else {
      ids.push_back(id);
    }
  }
}

std::shared_ptr<RobimsTable> RobimsDBImpl::CreateTableInstance(const TableSchema& table_schema) {
  std::shared_ptr<RobimsDBData> db = db_data_.load();
  std::shared_ptr<RobimsTable> table(new RobimsTable(db->id_mapping));
  if (0 != table->Init(table_schema)) {
    ROBIMS_ERROR("Table create failed by schema:{}", table_schema.DebugString());
    return nullptr;
  }
  return table;
}

int RobimsDBImpl::CreateTable(const TableSchema& table_schema) {
  std::shared_ptr<RobimsTable> table = CreateTableInstance(table_schema);
  if (!table) {
    return -1;
  }
  std::string_view table_name = table->GetSchema().name();
  ROBIMS_INFO("Table:{} create with schema:{}", table_name, table_schema.DebugString());
  std::shared_ptr<RobimsDBData> db = db_data_.load();
  db->tables[table_name].store(table);
  // db->tables[table_name];
  return 0;
}

int RobimsDBImpl::CreateTable(const std::string& schema) {
  TableSchema table_schema;
  google::protobuf::util::JsonParseOptions opt;
  opt.ignore_unknown_fields = true;
  auto rc = google::protobuf::util::JsonStringToMessage(schema, &table_schema, opt);
  if (!rc.ok()) {
    ROBIMS_ERROR("Table schema parse from json failed:{}", rc.ToString());
    return -1;
  }
  return CreateTable(table_schema);
}
RobimsTable* RobimsDBImpl::GetTable(const std::string& table) {
  std::shared_ptr<RobimsDBData> db = db_data_.load();
  auto found = db->tables.find(table);
  if (found == db->tables.end()) {
    return nullptr;
  }
  return found->second.load().get();
}
int RobimsDBImpl::Put(const std::string& table, const std::string& json) {
  folly::SharedMutex::WriteHolder lock(shared_mutex_);
  std::shared_ptr<RobimsDBData> db = db_data_.load();
  auto found = db->tables.find(table);
  if (found == db->tables.end()) {
    ROBIMS_ERROR("Table:{} not found while tables:{}", table, db->tables.size());
    return -1;
  }
  return found->second.load()->Put(json);
}
int RobimsDBImpl::Remove(const std::string& table, const std::string& json) {
  folly::SharedMutex::WriteHolder lock(shared_mutex_);
  std::shared_ptr<RobimsDBData> db = db_data_.load();
  auto found = db->tables.find(table);
  if (found == db->tables.end()) {
    ROBIMS_ERROR("Table {} not found", table);
    return -1;
  }
  return found->second.load()->Remove(json);
}
int RobimsDBImpl::Select(const std::string& query, int64_t offset, int64_t limit,
                         SelectResult& result) {
  if (limit <= 0) {
    limit = 100;
  }
  if (offset < 0) {
    offset = 0;
  }
  result.ids.clear();
  result.offset = 0;
  result.total = 0;
  folly::SharedMutex::ReadHolder lock(shared_mutex_);
  std::shared_ptr<RobimsDBData> db = db_data_.load();
  RobimsQueryPtr query_obj;
  {
    std::lock_guard<std::mutex> guard(db->query_cache_mutex);
    auto found = db->query_cache.find(query);
    if (found != db->query_cache.end()) {
      query_obj = found->second;
    } else {
      query_obj.reset(new RobimsQuery);
      int rc = query_obj->Init(this, query);
      if (0 != rc) {
        ROBIMS_ERROR("Parse query:{} faield with code:{}", query, rc);
        return -1;
      }
      db->query_cache.insert(query, query_obj);
    }
  }
  RobimsQueryValue val = query_obj->Execute(this);
  CRoaringBitmapPtr* bitmap = std::get_if<CRoaringBitmapPtr>(&val);
  if (nullptr != bitmap) {
    std::vector<uint32_t> local_ids;
    local_ids.resize(limit);
    if (!roaring_bitmap_range_uint32_array(bitmap->get(), offset, limit, &local_ids[0])) {
      ROBIMS_ERROR("Failed to extract ids");
      return -1;
    }
    result.total = roaring_bitmap_get_cardinality(bitmap->get());
    for (size_t i = 0; i < local_ids.size(); i++) {
      if (0 == local_ids[i]) {
        if (i > 0) {
          break;
        }
        if (offset > 0) {
          break;
        }
        if (!roaring_bitmap_contains(bitmap->get(), 0)) {
          break;
        }
      }
      std::string id;
      std::string_view id_view;
      if (0 == db->id_mapping->GetID(local_ids[i], id_view)) {
        id.assign(id_view.data(), id_view.size());
        result.ids.emplace_back(std::move(id));
      } else {
        result.ids.push_back(id);
      }
      result.offset = local_ids[i];
    }
    return 0;
  } else {
    switch (val.index()) {
      case 0: {
        RobimsQueryError err = std::get<RobimsQueryError>(val);
        ROBIMS_ERROR("Query execute result:{}/{}", err.code, err.reason);
        break;
      }
      default: {
        ROBIMS_ERROR("Query execute result value index:{}", val.index());
        break;
      }
    }
  }
  return -1;
}

}  // namespace robims
