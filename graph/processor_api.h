// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/16
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once
#include <atomic>
#include <boost/preprocessor/library.hpp>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include "data.h"
#include "tbb/concurrent_hash_map.h"

#define ERR_UNIMPLEMENTED -7890

namespace wrdk {
namespace graph {
typedef std::function<void(int)> DoneClosure;
struct DataKey {
  std::string name;
  uint32_t id = 0;
  bool operator<(const DataKey& k) const {
    if (id < k.id) {
      return true;
    }
    if (id > k.id) {
      return false;
    }
    return name < k.name;
  }
};
struct DataKeyView {
  std::string_view name;
  uint32_t id = 0;
};
struct DataValue {
  void* val = nullptr;
  std::shared_ptr<std::string> name;
};
struct DataKeyViewHash {
  size_t operator()(const DataKeyView& id) const noexcept {
    size_t h1 = std::hash<std::string_view>()(id.name);
    size_t h2 = id.id;
    return h1 ^ h2;
  }
};

struct DataKeyViewEqual {
  bool operator()(const DataKeyView& x, const DataKeyView& y) const {
    return x.name == y.name && x.id == y.id;
  }
};

class GraphDataContext {
 private:
  typedef std::unordered_map<DataKeyView, DataValue, DataKeyViewHash, DataKeyViewEqual> DataTable;
  DataTable _data_table;
  static uint32_t nextTypeId() {
    static std::atomic<uint32_t> type_id_seed;
    return type_id_seed.fetch_add(1);
  }
  std::shared_ptr<GraphDataContext> _parent;

 public:
  template <typename T>
  static uint32_t GetTypeId() {
    static uint32_t id = nextTypeId();
    return id;
  }
  GraphDataContext() {}
  void SetParent(std::shared_ptr<GraphDataContext> p) { _parent = p; }

  void RegisterData(const DataKey& id) {
    DataValue dv;
    dv.name.reset(new std::string(id.name));
    DataKeyView key = {*dv.name, id.id};
    _data_table[key] = dv;
  }

  void Reset() {
    _data_table.clear();
    _parent.reset();
  }
  template <typename T>
  const T* Get(const std::string& name) const {
    uint32_t id = GetTypeId<T>();
    DataKeyView key = {name, id};
    auto found = _data_table.find(key);
    if (found != _data_table.end()) {
      const T* val = (const T*)(found->second.val);
      if (nullptr != val) {
        return val;
      }
    }
    if (_parent) {
      return _parent->Get<T>(name);
    }
    return nullptr;
  }
  template <typename T>
  T* Move(const std::string& name) {
    return nullptr;
  }

  /**
   * @brief when 'create_entry' = true, not thread safe
   *
   * @tparam T any type
   * @param name
   * @param v
   * @param create_entry
   * @return true
   * @return false
   */
  template <typename T>
  bool Set(const std::string& name, const T* v, bool create_entry = false) {
    uint32_t id = GetTypeId<T>();
    DataKeyView key = {name, id};
    auto found = _data_table.find(key);
    if (found != _data_table.end()) {
      found->second.val = (void*)v;
      return true;
    } else {
      if (!create_entry) {
        return false;
      }
      DataValue dv;
      dv.val = (void*)v;
      dv.name.reset(new std::string(name));
      DataKeyView key = {*(dv.name), id};
      _data_table[key] = dv;
      return true;
    }
  }
};

enum ProcessorFieldType {
  FIELD_IN = 0,
  FIELD_OUT,
  FIELD_IN_OUT,
};

class Processor {
 protected:
  typedef std::function<int(GraphDataContext&, const std::string&, bool)> InjectFunc;
  typedef std::function<int(GraphDataContext&, const std::string&)> EmitFunc;
  typedef std::function<void(void)> ResetFunc;
  typedef std::unordered_map<std::string, InjectFunc> FieldInjectFuncTable;
  typedef std::unordered_map<std::string, EmitFunc> FieldEmitFuncTable;
  std::vector<DataKey> _input_ids;
  std::vector<DataKey> _output_ids;
  FieldInjectFuncTable _field_inject_table;
  FieldEmitFuncTable _field_emit_table;
  std::vector<ResetFunc> _reset_funcs;

  template <typename T>
  size_t RegisterInput(const std::string& field, const T* p, InjectFunc&& inject) {
    using FIELD_TYPE =
        typename std::remove_const<typename std::remove_pointer<decltype(p)>::type>::type;
    DataKey id{field, GraphDataContext::GetTypeId<FIELD_TYPE>()};
    _input_ids.push_back(id);
    _field_inject_table.emplace(field, inject);
    return _field_inject_table.size();
  }
  template <typename T>
  size_t RegisterOutput(const std::string& field, T& v, EmitFunc&& emit) {
    using FIELD_TYPE =
        typename std::remove_pointer<typename std::remove_reference<decltype(v)>::type>::type;
    DataKey id{field, GraphDataContext::GetTypeId<FIELD_TYPE>()};
    _output_ids.push_back(id);
    _field_emit_table.emplace(field, emit);
    return _field_emit_table.size();
  }
  size_t AddResetFunc(ResetFunc&& f);
  virtual int OnSetup(const Params& args) = 0;
  virtual int OnReset() { return 0; }
  virtual int OnExecute(const Params& args) { return ERR_UNIMPLEMENTED; };
  virtual void OnAsyncExecute(const Params& args, DoneClosure&& done) { done(ERR_UNIMPLEMENTED); };

 public:
  virtual const char* Name() = 0;
  const std::vector<DataKey>& GetInputIds() { return _input_ids; }
  const std::vector<DataKey>& GetOutputIds() { return _output_ids; }
  int Setup(const Params& args);
  void Reset();
  int Execute(const Params& args);
  void AsyncExecute(const Params& args, DoneClosure&& done);
  int InjectInputField(GraphDataContext& ctx, const std::string& field_name,
                       const std::string& data_name);
  int EmitOutputField(GraphDataContext& ctx, const std::string& field_name,
                      const std::string& data_name);
  virtual ~Processor() {}
};
typedef std::function<Processor*(void)> ProcessorCreator;
struct ProcessorRegister {
  ProcessorRegister(const char* name, const ProcessorCreator& creator);
};
}  // namespace graph
}  // namespace wrdk

using namespace wrdk::graph;
#define GRAPH_PROC_BEGIN(NAME)                              \
  namespace {                                               \
  static const char* _local_processor_name = #NAME;         \
  struct GraphProcessor##NAME##Object;                      \
  typedef GraphProcessor##NAME##Object LocalProcessorClass; \
  struct GraphProcessor##NAME##Object : public Processor {  \
    const char* Name() { return _local_processor_name; }
#define GRAPH_PROC_END                                                                       \
  }                                                                                          \
  ;                                                                                          \
  static ProcessorRegister instance(_local_processor_name,                                   \
                                    []() -> Processor* { return new LocalProcessorClass; }); \
  }

#define DEF_IN_FIELD(TYPE, NAME)                                                                  \
  const BOOST_PP_REMOVE_PARENS(TYPE)* NAME = nullptr;                                             \
  size_t __input_##NAME##_code = RegisterInput(                                                   \
      #NAME, NAME, [this](GraphDataContext& ctx, const std::string& data, bool move) {            \
        using FIELD_TYPE =                                                                        \
            typename std::remove_const<typename std::remove_pointer<decltype(NAME)>::type>::type; \
        if (move) {                                                                               \
          NAME = ctx.Move<FIELD_TYPE>(data);                                                      \
        } else {                                                                                  \
          NAME = ctx.Get<FIELD_TYPE>(data);                                                       \
        }                                                                                         \
        return (nullptr == NAME) ? -1 : 0;                                                        \
      });                                                                                         \
  size_t __reset_##NAME##_code = AddResetFunc([this]() { NAME = nullptr; });

#define DEF_IN_MAP_FIELD(TYPE, NAME)                                                             \
  std::map<std::string, const BOOST_PP_REMOVE_PARENS(TYPE)*> NAME;                               \
  const BOOST_PP_REMOVE_PARENS(TYPE)* __NAME_helper = nullptr;                                   \
  size_t __input_##NAME##_code = RegisterInput(                                                  \
      #NAME, __NAME_helper, [this](GraphDataContext& ctx, const std::string& data, bool move) {  \
        const BOOST_PP_REMOVE_PARENS(TYPE)* tmp = nullptr;                                       \
        using FIELD_TYPE =                                                                       \
            typename std::remove_const<typename std::remove_pointer<decltype(tmp)>::type>::type; \
        if (move) {                                                                              \
          tmp = ctx.Move<FIELD_TYPE>(data);                                                      \
        } else {                                                                                 \
          tmp = ctx.Get<FIELD_TYPE>(data);                                                       \
        }                                                                                        \
        if (nullptr == tmp) {                                                                    \
          return -1;                                                                             \
        }                                                                                        \
        NAME[data] = tmp;                                                                        \
        return 0;                                                                                \
      });                                                                                        \
  size_t __reset_##NAME##_code = AddResetFunc([this]() { NAME = {}; });

#define DEF_OUT_FIELD(TYPE, NAME)                                                          \
  BOOST_PP_REMOVE_PARENS(TYPE) NAME = {};                                                  \
  size_t __output_##NAME##_code =                                                          \
      RegisterOutput(#NAME, NAME, [this](GraphDataContext& ctx, const std::string& data) { \
        using FIELD_TYPE = decltype(NAME);                                                 \
        ctx.Set<FIELD_TYPE>(data, &(this->NAME));                                          \
        return 0;                                                                          \
      });                                                                                  \
  size_t __reset_##NAME##_code = AddResetFunc([this]() {});

#define DEF_IN_OUT_FIELD(TYPE, NAME)                                                       \
  BOOST_PP_REMOVE_PARENS(TYPE)* NAME = nullptr;                                            \
  size_t __input_##NAME##_code = RegisterInput(                                            \
      #NAME, NAME, [this](GraphDataContext& ctx, const std::string& data, bool move) {     \
        using FIELD_TYPE = typename std::remove_pointer<decltype(NAME)>::type;             \
        if (move) {                                                                        \
          NAME = ctx.Move<FIELD_TYPE>(data);                                               \
        } else {                                                                           \
          NAME = ctx.Get<FIELD_TYPE>(data);                                                \
        }                                                                                  \
        return (nullptr == NAME) ? -1 : 0;                                                 \
      });                                                                                  \
  size_t __output_##NAME##_code =                                                          \
      RegisterOutput(#NAME, NAME, [this](GraphDataContext& ctx, const std::string& data) { \
        using FIELD_TYPE = typename std::remove_pointer<decltype(NAME)>::type;             \
        ctx.Set<FIELD_TYPE>(data, NAME);                                                   \
        return 0;                                                                          \
      });                                                                                  \
  size_t __reset_##NAME##_code = AddResetFunc([this]() { NAME = {}; });

#define DEF_IF_INPUT_FIELD(TYPE, NAME, FTYPE) \
  BOOST_PP_REMOVE_PARENS(                     \
      BOOST_PP_IIF(BOOST_PP_EQUAL(FTYPE, FIELD_IN), (DEF_IN_FIELD(TYPE, NAME)), BOOST_PP_EMPTY()))
#define DEF_IF_OUTPUT_FIELD(TYPE, NAME, FTYPE)                          \
  BOOST_PP_REMOVE_PARENS(BOOST_PP_IIF(BOOST_PP_EQUAL(FTYPE, FIELD_OUT), \
                                      (DEF_OUT_FIELD(TYPE, NAME)), BOOST_PP_EMPTY()))
#define DEF_IF_INPUT_OUPUT_FIELD(TYPE, NAME, FTYPE)                        \
  BOOST_PP_REMOVE_PARENS(BOOST_PP_IIF(BOOST_PP_EQUAL(FTYPE, FIELD_IN_OUT), \
                                      (DEF_IN_OUT_FIELD(TYPE, NAME)), BOOST_PP_EMPTY()))

#define DEF_FIELD(TYPE, NAME, FIELD_TYPE) \
  DEF_IF_INPUT_FIELD(TYPE, NAME, FTYPE)   \
  DEF_IF_OUTPUT_FIELD(TYPE, NAME, FTYPE)  \
  DEF_IF_INPUT_OUPUT_FIELD(TYPE, NAME, FTYPE)
