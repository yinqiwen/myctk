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
#include "didagle_event.h"
#include "graph_data.h"

#include "tbb/concurrent_hash_map.h"

#define ERR_UNIMPLEMENTED -7890

namespace didagle {

enum ProcessorFieldType {
  FIELD_IN = 0,
  FIELD_OUT,
};
typedef std::function<void(int)> DoneClosure;

struct FieldFlags {
  uint8_t is_extern;
  uint8_t is_aggregate;
  uint8_t is_in_out;
  KCFG_DEFINE_FIELDS(is_extern, is_aggregate)
  FieldFlags(uint8_t v0 = 0, uint8_t v1 = 0, uint8_t v2 = 0)
      : is_extern(v0), is_aggregate(v1), is_in_out(v2) {}
};

struct FieldInfo : public DIObjectKey {
  FieldFlags flags;
  KCFG_DEFINE_FIELDS(name, id, flags)
};

struct DataValue {
  std::atomic<void*> val = nullptr;
  std::shared_ptr<std::string> name;
  DataValue& operator=(const DataValue& other) {
    val.store(other.val.load());
    name = other.name;
    return *this;
  }
};

struct GraphDataGetOptions {
  unsigned int with_parent : 1;
  unsigned int with_di_container : 1;
  unsigned int with_children : 1;
  GraphDataGetOptions() {
    with_parent = 1;
    with_di_container = 1;
    with_children = 1;
  }
};

class GraphDataContext {
 private:
  typedef std::unordered_map<DIObjectKeyView, DataValue, DIObjectKeyViewHash, DIObjectKeyViewEqual>
      DataTable;
  DataTable _data_table;
  std::shared_ptr<GraphDataContext> _parent;
  std::unique_ptr<DAGEventTracker> _event_tracker;
  std::vector<const GraphDataContext*> _executed_childrens;
  bool _disable_entry_creation = false;

 public:
  GraphDataContext() {}
  void SetParent(std::shared_ptr<GraphDataContext> p) { _parent = p; }
  std::shared_ptr<GraphDataContext> GetParent() { return _parent; }
  void ReserveChildCapacity(size_t n);
  void SetChild(const GraphDataContext* c, size_t idx);

  void RegisterData(const DIObjectKey& id);

  void DisableEntryCreation() { _disable_entry_creation = true; }

  DAGEventTracker* GetEventTracker();
  bool EnableEventTracker();

  void Reset();
  template <typename T>
  typename DIObjectTypeHelper<T>::read_type Get(std::string_view name,
                                                GraphDataGetOptions opt = {}) const {
    typedef typename DIObjectTypeHelper<T>::read_type GetValueType;
    uint32_t id = DIContainer::GetTypeId<T>();
    DIObjectKeyView key = {name, id};
    if constexpr (std::is_pointer<GetValueType>::value) {
      auto found = _data_table.find(key);
      if (found != _data_table.end()) {
        GetValueType val = (GetValueType)(found->second.val.load());
        if (nullptr != val) {
          return val;
        }
      }
    }
    GetValueType r = {};
    if (opt.with_parent && _parent) {
      GraphDataGetOptions parent_opt;
      parent_opt.with_parent = 1;
      parent_opt.with_children = 0;
      parent_opt.with_di_container = 0;
      r = _parent->Get<T>(name, parent_opt);
      if (r) {
        return r;
      }
    }
    if (opt.with_di_container) {
      r = DIContainer::Get<T>(name);
      if (r) {
        return r;
      }
    }
    if (opt.with_children) {
      GraphDataGetOptions child_opt;
      child_opt.with_children = 1;
      opt.with_parent = 0;
      opt.with_di_container = 0;
      for (const GraphDataContext* child_ctx : _executed_childrens) {
        if (nullptr != child_ctx) {
          r = child_ctx->Get<T>(name, child_opt);
          if (r) {
            return r;
          }
        }
      }
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

typedef std::shared_ptr<GraphDataContext> GraphDataContextPtr;

class VertexContext;
class GraphClusterContext;
class Processor {
 protected:
  typedef std::function<int(GraphDataContext&, const std::string&, bool)> InjectFunc;
  typedef std::function<int(GraphDataContext&, const std::string&)> EmitFunc;
  typedef std::function<void(void)> ResetFunc;
  typedef std::unordered_map<std::string, InjectFunc> FieldInjectFuncTable;
  typedef std::unordered_map<std::string, EmitFunc> FieldEmitFuncTable;
  std::vector<FieldInfo> _input_ids;
  std::vector<FieldInfo> _output_ids;
  FieldInjectFuncTable _field_inject_table;
  FieldEmitFuncTable _field_emit_table;
  std::vector<ResetFunc> _reset_funcs;
  GraphDataContextPtr _data_ctx;

  template <typename T>
  size_t RegisterInput(const std::string& field, InjectFunc&& inject, FieldFlags flags) {
    FieldInfo info;
    info.flags = flags;
    info.name = field;
    info.id = DIContainer::GetTypeId<T>();
    _input_ids.push_back(info);
    // DIDAGLE_DEBUG("[{}]RegisterInput field:{}/{}", Name(), field, id.id);
    _field_inject_table.emplace(field, inject);
    return _field_inject_table.size();
  }
  template <typename T>
  size_t RegisterOutput(const std::string& field, EmitFunc&& emit) {
    FieldInfo info;
    info.name = field;
    info.id = DIContainer::GetTypeId<T>();
    // DIDAGLE_DEBUG("[{}]RegisterOutput field:{}/{}", Name(), field, id.id);
    _output_ids.push_back(info);
    _field_emit_table.emplace(field, emit);
    return _field_emit_table.size();
  }
  size_t AddResetFunc(ResetFunc&& f);

  GraphDataContext& GetDataContext() { return *_data_ctx; }

  virtual int OnSetup(const Params& args) { return 0; }
  virtual int OnReset() { return 0; }
  virtual int OnExecute(const Params& args) { return ERR_UNIMPLEMENTED; };
  virtual void OnAsyncExecute(const Params& args, DoneClosure&& done) { done(ERR_UNIMPLEMENTED); };

  friend class VertexContext;
  friend class GraphClusterContext;

 public:
  void SetDataContext(GraphDataContextPtr p) { _data_ctx = p; }
  virtual const std::string& Name() const = 0;
  virtual bool IsAsync() const { return false; }
  const std::vector<FieldInfo>& GetInputIds() { return _input_ids; }
  const std::vector<FieldInfo>& GetOutputIds() { return _output_ids; }
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
#define GRAPH_OP_BEGIN(NAME)                                \
  namespace {                                               \
  static const char* _local_processor_name = #NAME;         \
  struct GraphProcessor##NAME##Object;                      \
  typedef GraphProcessor##NAME##Object LocalProcessorClass; \
  struct GraphProcessor##NAME##Object : public Processor {  \
    const std::string& Name() const {                       \
      static std::string __name = _local_processor_name;    \
      return __name;                                        \
    }
#define GRAPH_OP_END                                                                  \
  }                                                                                   \
  ;                                                                                   \
  static ProcessorRegister _##NAME##_instance(                                        \
      _local_processor_name, []() -> Processor* { return new LocalProcessorClass; }); \
  }

#define __GRAPH_OP_INPUT(TYPE, NAME, FLAGS)                                                       \
  typename DIObjectTypeHelper<BOOST_PP_REMOVE_PARENS(TYPE)>::read_type NAME = {};                 \
  size_t __input_##NAME##_code = RegisterInput<BOOST_PP_REMOVE_PARENS(TYPE)>(                     \
      #NAME,                                                                                      \
      [this](GraphDataContext& ctx, const std::string& data, bool move) {                         \
        using FIELD_TYPE =                                                                        \
            typename std::remove_const<typename std::remove_pointer<decltype(NAME)>::type>::type; \
        if (move) {                                                                               \
          NAME = ctx.Move<FIELD_TYPE>(data);                                                      \
        } else {                                                                                  \
          NAME = ctx.Get<FIELD_TYPE>(data);                                                       \
        }                                                                                         \
        return (!NAME) ? -1 : 0;                                                                  \
      },                                                                                          \
      BOOST_PP_REMOVE_PARENS(FLAGS));                                                             \
  size_t __reset_##NAME##_code = AddResetFunc([this]() { NAME = {}; });

#define GRAPH_OP_INPUT(TYPE, NAME) __GRAPH_OP_INPUT(TYPE, NAME, ({0, 0, 0}))
#define GRAPH_OP_EXTERN_INPUT(TYPE, NAME) __GRAPH_OP_INPUT(TYPE, NAME, ({1, 0, 0}))

#define GRAPH_OP_MAP_INPUT(TYPE, NAME)                                        \
  std::map<std::string, const BOOST_PP_REMOVE_PARENS(TYPE)*> NAME;            \
  size_t __input_##NAME##_code = RegisterInput<BOOST_PP_REMOVE_PARENS(TYPE)>( \
      #NAME,                                                                  \
      [this](GraphDataContext& ctx, const std::string& data, bool move) {     \
        using FIELD_TYPE = BOOST_PP_REMOVE_PARENS(TYPE);                      \
        const FIELD_TYPE* tmp = nullptr;                                      \
        if (move) {                                                           \
          tmp = ctx.Move<FIELD_TYPE>(data);                                   \
        } else {                                                              \
          tmp = ctx.Get<FIELD_TYPE>(data);                                    \
        }                                                                     \
        if (nullptr == tmp) {                                                 \
          return -1;                                                          \
        }                                                                     \
        NAME[data] = tmp;                                                     \
        return 0;                                                             \
      },                                                                      \
      {0, 1, 0});                                                             \
  size_t __output_##NAME##_code = RegisterOutput<decltype(NAME)>(             \
      #NAME, [this](GraphDataContext& ctx, const std::string& data) {         \
        using FIELD_TYPE = decltype(NAME);                                    \
        ctx.Set<FIELD_TYPE>(data, &(this->NAME));                             \
        return 0;                                                             \
      });                                                                     \
  size_t __reset_##NAME##_code = AddResetFunc([this]() { NAME = {}; });

#define GRAPH_OP_OUTPUT(TYPE, NAME)                                                \
  typename DIObjectTypeHelper<BOOST_PP_REMOVE_PARENS(TYPE)>::write_type NAME = {}; \
  size_t __output_##NAME##_code = RegisterOutput<BOOST_PP_REMOVE_PARENS(TYPE)>(    \
      #NAME, [this](GraphDataContext& ctx, const std::string& data) {              \
        using FIELD_TYPE = decltype(NAME);                                         \
        ctx.Set<FIELD_TYPE>(data, &(this->NAME));                                  \
        return 0;                                                                  \
      });                                                                          \
  size_t __reset_##NAME##_code = AddResetFunc([this]() {});

#define __GRAPH_OP_IN_OUT(TYPE, NAME, FLAGS)                                            \
  typename DIObjectTypeHelper<BOOST_PP_REMOVE_PARENS(TYPE)>::read_write_type NAME = {}; \
  size_t __input_##NAME##_code = RegisterInput<BOOST_PP_REMOVE_PARENS(TYPE)>(           \
      #NAME,                                                                            \
      [this](GraphDataContext& ctx, const std::string& data, bool move) {               \
        if (!move) {                                                                    \
          return -1;                                                                    \
        }                                                                               \
        using FIELD_TYPE = typename std::remove_pointer<decltype(NAME)>::type;          \
        NAME = ctx.Move<FIELD_TYPE>(data);                                              \
        return (!NAME) ? -1 : 0;                                                        \
      },                                                                                \
      BOOST_PP_REMOVE_PARENS(FLAGS));                                                   \
  size_t __output_##NAME##_code = RegisterOutput<BOOST_PP_REMOVE_PARENS(TYPE)>(         \
      #NAME, [this](GraphDataContext& ctx, const std::string& data) {                   \
        using FIELD_TYPE = typename std::remove_pointer<decltype(NAME)>::type;          \
        ctx.Set<FIELD_TYPE>(data, NAME);                                                \
        return 0;                                                                       \
      });                                                                               \
  size_t __reset_##NAME##_code = AddResetFunc([this]() { NAME = {}; });

#define GRAPH_OP_IN_OUT(TYPE, NAME) __GRAPH_OP_IN_OUT(TYPE, NAME, ({0, 0, 1}))
#define GRAPH_OP_EXTERN_IN_OUT(TYPE, NAME) __GRAPH_OP_IN_OUT(TYPE, NAME, ({1, 0, 1}))