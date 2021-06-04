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
#include "di_container.h"
#include "graph_data.h"
#include "tbb/concurrent_hash_map.h"

#define ERR_UNIMPLEMENTED -7890

namespace didagle {

enum ProcessorFieldType {
  FIELD_IN = 0,
  FIELD_OUT,
};
typedef std::function<void(int)> DoneClosure;

// struct FieldInfo : public DIObjectKey {
//   bool aggregate = false;
// };

struct DataValue {
  std::atomic<void*> val = nullptr;
  std::shared_ptr<std::string> name;
  DataValue& operator=(const DataValue& other) {
    val.store(other.val.load());
    name = other.name;
    return *this;
  }
};

class GraphDataContext {
 private:
  typedef std::unordered_map<DIObjectKeyView, DataValue, DIObjectKeyViewHash, DIObjectKeyViewEqual>
      DataTable;
  DataTable _data_table;
  std::shared_ptr<GraphDataContext> _parent;
  bool _disable_entry_creation = false;

 public:
  GraphDataContext() {}
  void SetParent(std::shared_ptr<GraphDataContext> p) { _parent = p; }

  void RegisterData(const DIObjectKey& id);

  void DisableEntryCreation() { _disable_entry_creation = true; }

  void Reset();
  template <typename T>
  typename DIObjectTypeHelper<T>::read_type Get(const std::string& name,
                                                bool with_di_container = true) const {
    typedef typename DIObjectTypeHelper<T>::read_type GetValueType;
    uint32_t id = DIContainer::GetTypeId<T>();
    DIObjectKeyView key = {name, id};
    auto found = _data_table.find(key);
    if (found != _data_table.end()) {
      if constexpr (std::is_pointer<GetValueType>::value) {
        GetValueType val = (GetValueType)(found->second.val.load());
        if (nullptr != val) {
          return val;
        }
      }
    }
    if (_parent) {
      GetValueType r = _parent->Get<T>(name, false);
      if (r) {
        return r;
      }
    }
    GetValueType r = {};
    if (with_di_container) {
      r = DIContainer::Get<T>(name);
    }
    return r;
  }
  template <typename T>
  typename DIObjectTypeHelper<T>::read_write_type Move(const std::string& name) {
    typedef typename DIObjectTypeHelper<T>::read_write_type MoveValueType;
    uint32_t id = DIContainer::GetTypeId<T>();
    DIObjectKeyView key = {name, id};
    auto found = _data_table.find(key);
    if (found != _data_table.end()) {
      if constexpr (std::is_pointer<MoveValueType>::value) {
        void* empty = nullptr;
        MoveValueType val = (MoveValueType)(found->second.val.exchange(empty));
        return val;
      }
    }
    MoveValueType empty;
    return empty;
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
  bool Set(const std::string& name, const T* v) {
    uint32_t id = DIContainer::GetTypeId<T>();
    DIObjectKeyView key = {name, id};
    auto found = _data_table.find(key);
    if (found != _data_table.end()) {
      found->second.val = (void*)v;
      return true;
    } else {
      if (_disable_entry_creation) {
        return false;
      }
      DataValue dv;
      dv.val = (void*)v;
      dv.name.reset(new std::string(name));
      DIObjectKeyView key = {*(dv.name), id};
      _data_table[key] = dv;
      return true;
    }
  }
};

class VertexContext;
class GraphClusterContext;
class Processor {
 protected:
  typedef std::function<int(GraphDataContext&, const std::string&, bool)> InjectFunc;
  typedef std::function<int(GraphDataContext&, const std::string&)> EmitFunc;
  typedef std::function<void(void)> ResetFunc;
  typedef std::unordered_map<std::string, InjectFunc> FieldInjectFuncTable;
  typedef std::unordered_map<std::string, EmitFunc> FieldEmitFuncTable;
  std::vector<DIObjectKey> _input_ids;
  std::vector<DIObjectKey> _output_ids;
  FieldInjectFuncTable _field_inject_table;
  FieldEmitFuncTable _field_emit_table;
  std::vector<ResetFunc> _reset_funcs;
  std::shared_ptr<GraphDataContext> _data_ctx;

  template <typename T>
  size_t RegisterInput(const std::string& field, InjectFunc&& inject) {
    // using FIELD_TYPE =
    //     typename std::remove_const<typename std::remove_pointer<decltype(p)>::type>::type;
    DIObjectKey id{field, DIContainer::GetTypeId<T>()};
    _input_ids.push_back(id);
    // DIDAGLE_DEBUG("[{}]RegisterInput field:{}/{}", Name(), field, id.id);
    _field_inject_table.emplace(field, inject);
    return _field_inject_table.size();
  }
  template <typename T>
  size_t RegisterOutput(const std::string& field, EmitFunc&& emit) {
    // using FIELD_TYPE =
    //     typename std::remove_pointer<typename std::remove_reference<decltype(v)>::type>::type;
    DIObjectKey id{field, DIContainer::GetTypeId<T>()};
    // DIDAGLE_DEBUG("[{}]RegisterOutput field:{}/{}", Name(), field, id.id);
    _output_ids.push_back(id);
    _field_emit_table.emplace(field, emit);
    return _field_emit_table.size();
  }
  size_t AddResetFunc(ResetFunc&& f);

  void SetDataContext(std::shared_ptr<GraphDataContext> p) { _data_ctx = p; }
  GraphDataContext& GetDataContext() { return *_data_ctx; }

  virtual int OnSetup(const Params& args) = 0;
  virtual int OnReset() { return 0; }
  virtual int OnExecute(const Params& args) { return ERR_UNIMPLEMENTED; };
  virtual void OnAsyncExecute(const Params& args, DoneClosure&& done) { done(ERR_UNIMPLEMENTED); };

  friend class VertexContext;
  friend class GraphClusterContext;

 public:
  virtual const char* Name() = 0;
  virtual bool IsAsync() const { return false; }
  const std::vector<DIObjectKey>& GetInputIds() { return _input_ids; }
  const std::vector<DIObjectKey>& GetOutputIds() { return _output_ids; }
  int Setup(const Params& args);
  void Reset();
  int Execute(const Params& args);
  void AsyncExecute(const Params& args, DoneClosure&& done);
  int InjectInputField(GraphDataContext& ctx, const std::string& field_name,
                       const std::string& data_name, bool move);
  int EmitOutputField(GraphDataContext& ctx, const std::string& field_name,
                      const std::string& data_name);
  virtual ~Processor();
};
typedef std::function<Processor*(void)> ProcessorCreator;
struct ProcessorRegister {
  ProcessorRegister(const char* name, const ProcessorCreator& creator);
};
}  // namespace didagle

using namespace didagle;
#define GRAPH_PROC_BEGIN(NAME)                              \
  namespace {                                               \
  static const char* _local_processor_name = #NAME;         \
  struct GraphProcessor##NAME##Object;                      \
  typedef GraphProcessor##NAME##Object LocalProcessorClass; \
  struct GraphProcessor##NAME##Object : public Processor {  \
    const char* Name() { return _local_processor_name; }
#define GRAPH_PROC_END                                                                \
  }                                                                                   \
  ;                                                                                   \
  static ProcessorRegister _##NAME##_instance(                                        \
      _local_processor_name, []() -> Processor* { return new LocalProcessorClass; }); \
  }

#define DEF_IN_FIELD(TYPE, NAME)                                                                  \
  typename DIObjectTypeHelper<BOOST_PP_REMOVE_PARENS(TYPE)>::read_type NAME = {};                 \
  size_t __input_##NAME##_code = RegisterInput<BOOST_PP_REMOVE_PARENS(TYPE)>(                     \
      #NAME, [this](GraphDataContext& ctx, const std::string& data, bool move) {                  \
        using FIELD_TYPE =                                                                        \
            typename std::remove_const<typename std::remove_pointer<decltype(NAME)>::type>::type; \
        if (move) {                                                                               \
          NAME = ctx.Move<FIELD_TYPE>(data);                                                      \
        } else {                                                                                  \
          NAME = ctx.Get<FIELD_TYPE>(data);                                                       \
        }                                                                                         \
        return (!NAME) ? -1 : 0;                                                                  \
      });                                                                                         \
  size_t __reset_##NAME##_code = AddResetFunc([this]() { NAME = {}; });

#define DEF_MAP_FIELD(TYPE, NAME)                                                \
  std::map<std::string, const BOOST_PP_REMOVE_PARENS(TYPE)*> NAME;               \
  size_t __input_##NAME##_code = RegisterInput<BOOST_PP_REMOVE_PARENS(TYPE)>(    \
      #NAME, [this](GraphDataContext& ctx, const std::string& data, bool move) { \
        using FIELD_TYPE = BOOST_PP_REMOVE_PARENS(TYPE);                         \
        const FIELD_TYPE* tmp = nullptr;                                         \
        if (move) {                                                              \
          tmp = ctx.Move<FIELD_TYPE>(data);                                      \
        } else {                                                                 \
          tmp = ctx.Get<FIELD_TYPE>(data);                                       \
        }                                                                        \
        if (nullptr == tmp) {                                                    \
          return -1;                                                             \
        }                                                                        \
        NAME[data] = tmp;                                                        \
        return 0;                                                                \
      });                                                                        \
  size_t __output_##NAME##_code = RegisterOutput<decltype(NAME)>(                \
      #NAME, [this](GraphDataContext& ctx, const std::string& data) {            \
        using FIELD_TYPE = decltype(NAME);                                       \
        ctx.Set<FIELD_TYPE>(data, &(this->NAME));                                \
        return 0;                                                                \
      });                                                                        \
  size_t __reset_##NAME##_code = AddResetFunc([this]() { NAME = {}; });

#define DEF_OUT_FIELD(TYPE, NAME)                                                  \
  typename DIObjectTypeHelper<BOOST_PP_REMOVE_PARENS(TYPE)>::write_type NAME = {}; \
  size_t __output_##NAME##_code = RegisterOutput<BOOST_PP_REMOVE_PARENS(TYPE)>(    \
      #NAME, [this](GraphDataContext& ctx, const std::string& data) {              \
        using FIELD_TYPE = decltype(NAME);                                         \
        ctx.Set<FIELD_TYPE>(data, &(this->NAME));                                  \
        return 0;                                                                  \
      });                                                                          \
  size_t __reset_##NAME##_code = AddResetFunc([this]() {});

#define DEF_IN_OUT_FIELD(TYPE, NAME)                                                    \
  typename DIObjectTypeHelper<BOOST_PP_REMOVE_PARENS(TYPE)>::read_write_type NAME = {}; \
  size_t __input_##NAME##_code = RegisterInput<BOOST_PP_REMOVE_PARENS(TYPE)>(           \
      #NAME, [this](GraphDataContext& ctx, const std::string& data, bool move) {        \
        if (!move) {                                                                    \
          return -1;                                                                    \
        }                                                                               \
        using FIELD_TYPE = typename std::remove_pointer<decltype(NAME)>::type;          \
        NAME = ctx.Move<FIELD_TYPE>(data);                                              \
        return (!NAME) ? -1 : 0;                                                        \
      });                                                                               \
  size_t __output_##NAME##_code = RegisterOutput<BOOST_PP_REMOVE_PARENS(TYPE)>(         \
      #NAME, [this](GraphDataContext& ctx, const std::string& data) {                   \
        using FIELD_TYPE = typename std::remove_pointer<decltype(NAME)>::type;          \
        ctx.Set<FIELD_TYPE>(data, NAME);                                                \
        return 0;                                                                       \
      });                                                                               \
  size_t __reset_##NAME##_code = AddResetFunc([this]() { NAME = {}; });
