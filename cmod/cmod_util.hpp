/*
 *Copyright (c) 2017-2018, yinqiwen <yinqiwen@gmail.com>
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
 *  * Neither the name of Redis nor the names of its contributors may be used
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

#ifndef SRC_MMDATA_UTIL_HPP_
#define SRC_MMDATA_UTIL_HPP_

#include <flatbuffers/idl.h>

#include <fstream>
#include <iostream>

#include "cmod.hpp"
#include "cmod_kcfg.hpp"
#include "kcfg.hpp"

namespace cmod {
struct DataImageBuildOptions {
  int64_t max_image_size;
  std::string src_file;
  std::string dst_file;
  void *dst_buf;
  size_t dst_buf_size;
  size_t reserve_size;
  DataImageBuildOptions()
      : max_image_size(10 * 1024 * 1024 * 1024LL),
        dst_buf(NULL),
        dst_buf_size(0),
        reserve_size(0) {}
};
struct DataImageMergeOptions {
  std::string base_file;
  std::vector<std::string> incr_files;
  std::string dst_file;
  DataImageMergeOptions() {}
};
template <typename T>
struct DataImageBuilder {
  static int64_t BatchBuild(cmod::DataImageBuildOptions &options, std::string &err) {
    std::istream *is = nullptr;
    std::ifstream file(options.src_file.c_str());
    if (options.src_file == "stdin") {
      is = &(std::cin);
    } else {
      if (!file.is_open()) {
        err = "Open source file failed.";
        return -1;
      }
      is = &file;
    }

    if (NULL == options.dst_buf && options.dst_file.empty()) {
      err = "No dst file or buf specified.";
      return -1;
    }
    typedef typename T::table_type RootTable;
    RootTable *table = NULL;
    CharAllocator *alloc = NULL;
    CMOD mdata;
    CMODFile mfile(options.dst_file);
    if (!options.dst_file.empty()) {
      table = mfile.LoadRootWriteObject<RootTable>();
      alloc = &(mfile.GetAllocator());
    } else {
      table = mdata.LoadRootWriteObject<RootTable>(options.dst_buf, options.dst_buf_size);
      alloc = &(mdata.GetAllocator());
    }
    if (options.reserve_size > 0) {
      table->Reserve(options.reserve_size);
    }
    std::string line;
    while (std::getline(*is, line)) {
      if (line.empty()) {
        continue;
      }
      T entry(*alloc);
      if (kcfg::ParseFromJsonString(line, entry)) {
        table->Insert(entry);
      }
    }
    if (!options.dst_file.empty()) {
      return mfile.ShrinkToFit();
    }
    return alloc->used_space();
  }

  static int64_t Merge(const DataImageMergeOptions &options, std::string &err) {
    if (options.dst_file.empty()) {
      err = "No base file specified.";
      return -1;
    }
    if (options.incr_files.empty()) {
      err = "No incr file specified.";
      return -1;
    }
    if (options.base_file.empty()) {
      err = "No dst file specified.";
      return -1;
    }
    typedef typename T::table_type RootTable;
    if (options.dst_file != options.base_file) {
      std::ifstream source_fs(options.base_file, std::ios::binary);
      std::ofstream dest_fs(options.dst_file, std::ios::binary);
      dest_fs << source_fs.rdbuf();
      source_fs.close();
      dest_fs.close();
    }

    CMODFile base(options.base_file);
    base.LoadRootReadObject<RootTable>();
    size_t dst_fsize = base.GetOriginSize();
    base.Close();
    if (dst_fsize == 0) {
      dst_fsize = 10 * 1024 * 1024 * 1024LL;
    }

    CMODFile mfile(options.dst_file, dst_fsize);

    RootTable *table = mfile.LoadRootWriteObject<RootTable>(false);
    CharAllocator &alloc = mfile.GetAllocator();
    // printf("Merge table base size:%lld\n", table->size());
    for (const std::string &ifile : options.incr_files) {
      CMODFile incr_file(ifile);
      const RootTable *incr_table = incr_file.LoadRootReadObject<RootTable>();
      if (nullptr != incr_table) {
        // printf("Merge table incr size:%lld\n", incr_table->size());
        for (const auto &kv : *incr_table) {
          T entry(alloc);
          typename T::key_type &ek = entry.GetKey();
          typename T::value_type &ev = entry.GetValue();
          StructCopy(ek, kv.first);
          StructCopy(ev, kv.second);

          typename RootTable::const_iterator found = table->find(ek);
          if (found != table->end()) {
            table->erase(found);
          }
          table->insert(typename RootTable::value_type(ek, ev));
        }
      } else {
        // printf("Merge table incr size:0\n");
      }
    }
    return mfile.ShrinkToFit();
  }
  static int StreamBuild(CMODFile &builder, const std::string &line) {
    CharAllocator &alloc = builder.GetAllocator();
    typedef typename T::table_type RootTable;
    RootTable *table = builder.LoadRootWriteObject<RootTable>(true, true);
    if (nullptr == table) {
      return -1;
    }
    if (line.empty()) {
      return 0;
    }
    T entry(alloc);
    if (!kcfg::ParseFromJsonString(line, entry)) {
      return -1;
    }

    std::lock_guard<std::mutex> guard(builder.GetLock());
    table->Insert(entry);
    return 0;
  }
  static int ParseToString(const std::string &line, std::string &key, std::string &value) {
    CharAllocator alloc;
    T entry(alloc);
    if (!kcfg::ParseFromJsonString(line, entry)) {
      return -1;
    }
    key.assign(entry.GetKey().data(), entry.GetKey().size());
    value.assign(entry.GetValue().data(), entry.GetValue().size());
    return 0;
  }
  static const void *GetValue(const void *mem, const std::string &json_key) {
    rapidjson::Document d;
    d.Parse<0>(json_key.c_str());
    if (d.HasParseError()) {
      return nullptr;
    }
    typedef typename T::table_type RootTable;
    const RootTable *root = (const RootTable *)(mem);
    if (NULL == root) {
      return nullptr;
    }
    CharAllocator alloc;
    typename T::key_type key(alloc);
    kcfg::Parse(d, "", key);
    typename RootTable::const_iterator found = root->find(key);
    if (found != root->end()) {
      return &(found->second);
    }
    return nullptr;
  }
  static int GetJsonValue(const void *mem, const std::string &json_key, std::string &json_value) {
    rapidjson::Document d;
    d.Parse<0>(json_key.c_str());
    if (d.HasParseError()) {
      return -1;
    }
    typedef typename T::table_type RootTable;
    const RootTable *root = (const RootTable *)(mem);
    if (NULL == root) {
      return -1;
    }
    CharAllocator alloc;
    typename T::key_type key(alloc);
    kcfg::Parse(d, "", key);
    typename RootTable::const_iterator found = root->find(key);
    if (found != root->end()) {
      kcfg::WriteToJsonString(found->second, json_value);
      return 0;
    }
    return -1;
  }
  static int TestMemory(const void *mem, const std::string &json_key) {
    rapidjson::Document d;
    d.Parse<0>(json_key.c_str());
    if (d.HasParseError()) {
      std::cout << "Invalid json key:" << json_key << std::endl;
      return -1;
    }
    typedef typename T::table_type RootTable;
    CMOD buf;
    const RootTable *root = buf.LoadRootReadObject<RootTable>(mem);
    if (NULL == root) {
      return -1;
    }
    CharAllocator alloc;
    typename T::key_type key(alloc);
    kcfg::Parse(d, "", key);
    typename RootTable::const_iterator found = root->find(key);
    if (found != root->end()) {
      std::cout << "Found entry " << found->first << "->" << found->second << std::endl;
      return 0;
    }
    std::cout << "NO Entry found for jsno_key:" << json_key << std::endl;
    return -1;
  }
};

template <typename Container>
int WriteListToJson(const Container &v, const std::string &file, bool append = true) {
  FILE *f = fopen(file.c_str(), append ? "a+" : "w+");
  if (NULL == f) {
    return -1;
  }
  typename Container::const_iterator it = v.begin();
  while (it != v.end()) {
    std::string json;
    kcfg::WriteToJsonString(*it, json);
    fprintf(f, "%s\n", json.c_str());
    it++;
  }
  fclose(f);
  return 0;
}

typedef int64_t DataImageBuildFunc(cmod::DataImageBuildOptions &options, std::string &err);
typedef int TestMemoryFunc(const void *mem, const std::string &json_key);
typedef int StreamBuildFunc(CMODFile &builder, const std::string &line);
typedef const void *GetValueFunc(const void *mem, const std::string &json_key);
typedef int GetJsonValueFunc(const void *mem, const std::string &json_key, std::string &json_value);
typedef int64_t MergeFunc(const DataImageMergeOptions &options, std::string &err);
typedef int ParseToStringFunc(const std::string &line, std::string &key, std::string &value);
struct BuilderFuncs {
  DataImageBuildFunc *batch_builder = nullptr;
  StreamBuildFunc *stream_builder = nullptr;
  TestMemoryFunc *test = nullptr;
  GetValueFunc *get_value = nullptr;
  MergeFunc *merge = nullptr;
  GetJsonValueFunc *get_json_value = nullptr;
  ParseToStringFunc *parse_to_string = nullptr;
};

int RegisterBuilder(const char *name, uint64_t struct_hash, BuilderFuncs funcs);
int UnregisterBuilder(const char *name, uint64_t struct_hash, BuilderFuncs funcs);

typedef flatbuffers::Parser *FlatbuffersParserConstructor();
int RegisterFlatBufferParserConstructor(const char *name, FlatbuffersParserConstructor *creator);
flatbuffers::Parser *GetFlatBufferParser(const char *name);
flatbuffers::Parser *GetPoolFlatBufferParser(const char *name);
void RecyclePoolFlatBufferParser(const char *name, flatbuffers::Parser *p);
int UnregisterFlatBufferParserConstructor(const char *name, FlatbuffersParserConstructor *creator);

DataImageBuildFunc *GetBatchBuildFuncByName(const std::string &name);
TestMemoryFunc *GetTestFuncByName(const std::string &name);
StreamBuildFunc *GetStreamBuildFuncByName(const std::string &name);
MergeFunc *GetMergeFunc(const std::string &name);
GetValueFunc *GetGetValueFunc(const std::string &name);
GetJsonValueFunc *GetGetJsonValueFunc(const std::string &name);
ParseToStringFunc *GetParseToStringFunc(const std::string &name);
uint64_t GetRootStructHash(const std::string &nam);

}  // namespace cmod

#endif /* SRC_MMDATA_UTIL_HPP_ */
