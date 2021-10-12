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

#include "cmod_util.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "common_types.hpp"

namespace cmod {

struct HelperFuncs {
    BuilderFuncs funcs;
    uint64_t struct_hash;
    HelperFuncs() : struct_hash(0) {}
};
typedef std::map<std::string, HelperFuncs> HelperFuncTable;
static HelperFuncTable *g_helper_func_table = NULL;
int RegisterBuilder(const char *name, uint64_t struct_hash, BuilderFuncs funcs) {
    if (NULL == g_helper_func_table) {
        g_helper_func_table = new HelperFuncTable;
    }
    DataImageBuildFunc *current = GetBatchBuildFuncByName(name);
    if (current != NULL) {
        // duplicate name
        // abort();
    }
    HelperFuncs helper;
    helper.funcs = funcs;
    helper.struct_hash = struct_hash;
    (*g_helper_func_table)[name] = helper;
    return 0;
}
int UnregisterBuilder(const char *name, uint64_t struct_hash, BuilderFuncs funcs) {
    if (NULL == g_helper_func_table) {
        g_helper_func_table = new HelperFuncTable;
    }
    auto found = g_helper_func_table->find(name);
    if (found != g_helper_func_table->end()) {
        if (found->second.funcs.batch_builder == funcs.batch_builder) {
            g_helper_func_table->erase(name);
        }
    }

    return 0;
}

DataImageBuildFunc *GetBatchBuildFuncByName(const std::string &name) {
    if (NULL == g_helper_func_table) {
        return NULL;
    }
    HelperFuncTable::const_iterator found = g_helper_func_table->find(name);
    if (found != g_helper_func_table->end()) {
        return (found->second.funcs.batch_builder);
    }
    return NULL;
}
StreamBuildFunc *GetStreamBuildFuncByName(const std::string &name) {
    if (NULL == g_helper_func_table) {
        return NULL;
    }
    HelperFuncTable::const_iterator found = g_helper_func_table->find(name);
    if (found != g_helper_func_table->end()) {
        return (found->second.funcs.stream_builder);
    }
    return NULL;
}
MergeFunc *GetMergeFunc(const std::string &name) {
    if (NULL == g_helper_func_table) {
        return NULL;
    }
    HelperFuncTable::const_iterator found = g_helper_func_table->find(name);
    if (found != g_helper_func_table->end()) {
        return (found->second.funcs.merge);
    }
    return NULL;
}
GetValueFunc *GetGetValueFunc(const std::string &name) {
    if (NULL == g_helper_func_table) {
        return NULL;
    }
    HelperFuncTable::const_iterator found = g_helper_func_table->find(name);
    if (found != g_helper_func_table->end()) {
        return (found->second.funcs.get_value);
    }
    return NULL;
}
GetJsonValueFunc *GetGetJsonValueFunc(const std::string &name) {
    if (NULL == g_helper_func_table) {
        return NULL;
    }
    HelperFuncTable::const_iterator found = g_helper_func_table->find(name);
    if (found != g_helper_func_table->end()) {
        return (found->second.funcs.get_json_value);
    }
    return NULL;
}
ParseToStringFunc *GetParseToStringFunc(const std::string &name) {
    if (NULL == g_helper_func_table) {
        return NULL;
    }
    HelperFuncTable::const_iterator found = g_helper_func_table->find(name);
    if (found != g_helper_func_table->end()) {
        return (found->second.funcs.parse_to_string);
    }
    return NULL;
}
TestMemoryFunc *GetTestFuncByName(const std::string &name) {
    if (NULL == g_helper_func_table) {
        return NULL;
    }
    HelperFuncTable::const_iterator found = g_helper_func_table->find(name);
    if (found != g_helper_func_table->end()) {
        return (found->second.funcs.test);
    }
    return NULL;
}
uint64_t GetRootStructHash(const std::string &name) {
    if (NULL == g_helper_func_table) {
        return 0;
    }
    HelperFuncTable::const_iterator found = g_helper_func_table->find(name);
    if (found != g_helper_func_table->end()) {
        return found->second.struct_hash;
    }
    return 0;
}
typedef std::map<std::string, FlatbuffersParserConstructor *> FlatbuffersParserConstructorTable;
static FlatbuffersParserConstructorTable *g_fbs_cons_table = nullptr;
int RegisterFlatBufferParserConstructor(const char *name, FlatbuffersParserConstructor *creator) {
    if (nullptr == g_fbs_cons_table) {
        g_fbs_cons_table = new FlatbuffersParserConstructorTable;
    }
    (*g_fbs_cons_table)[name] = creator;
    return 0;
}

flatbuffers::Parser *GetFlatBufferParser(const char *name) {
    if (nullptr == g_fbs_cons_table) {
        return nullptr;
    }
    auto found = g_fbs_cons_table->find(name);
    if (found == g_fbs_cons_table->end()) {
        return nullptr;
    }
    FlatbuffersParserConstructor *cons = found->second;
    return cons();
}
static std::unordered_map<std::string, std::deque<flatbuffers::Parser *>> g_pooled_parsers;
static std::mutex g_pooled_parsers_mutex;

flatbuffers::Parser *GetPoolFlatBufferParser(const char *name) {
    std::lock_guard<std::mutex> guard(g_pooled_parsers_mutex);
    auto found = g_pooled_parsers.find(name);
    if (found != g_pooled_parsers.end()) {
        std::deque<flatbuffers::Parser *> &pool = found->second;
        if (pool.empty()) {
            flatbuffers::Parser *p = pool.front();
            pool.pop_front();
            return p;
        }
    }
    return GetFlatBufferParser(name);
}
void RecyclePoolFlatBufferParser(const char *name, flatbuffers::Parser *p) {
    p->builder_.Reset();
    {
        std::lock_guard<std::mutex> guard(g_pooled_parsers_mutex);
        auto found = g_pooled_parsers.find(name);
        if (found != g_pooled_parsers.end()) {
            std::deque<flatbuffers::Parser *> &pool = found->second;
            if (pool.size() < 128) {
                pool.push_back(p);
                return;
            }
        }
    }
    delete p;
}
int UnregisterFlatBufferParserConstructor(const char *name, FlatbuffersParserConstructor *creator) {
    if (nullptr == g_fbs_cons_table) {
        return -1;
    }
    auto found = g_fbs_cons_table->find(name);
    if (found == g_fbs_cons_table->end()) {
        return -1;
    }
    FlatbuffersParserConstructor *cons = found->second;
    if (cons == creator) {
        g_fbs_cons_table->erase(found);
    }
    return -1;
}

}  // namespace cmod
