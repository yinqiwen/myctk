// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/16
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once

#include <fmt/core.h>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "boost/preprocessor/comparison/equal.hpp"
#include "boost/preprocessor/control/if.hpp"
#include "boost/preprocessor/library.hpp"
#include "boost/preprocessor/variadic/elem.hpp"
#include "boost/preprocessor/variadic/size.hpp"

#include "google/protobuf/arena.h"

#include "didagle/di_container.h"
#include "didagle/di_reset.h"
#include "didagle/didagle_event.h"
#include "didagle/graph_data.h"

#include "folly/FBVector.h"
#include "folly/Likely.h"
#include "folly/container/F14Map.h"
#include "folly/container/F14Set.h"
#include "folly/futures/Future.h"

#include "ispine/coro/coroutine.h"

#define ERR_UNIMPLEMENTED -7890
#define ERR_COROUTINE_EXCEPTION -7891

namespace didagle {

enum ProcessorFieldType {
  FIELD_IN = 0,
  FIELD_OUT,
};
typedef std::function<void(int)> DoneClosure;

struct FieldFlags {
  uint8_t is_extern = 0;
  uint8_t is_aggregate = 0;
  uint8_t is_in_out = 0;
  KCFG_DEFINE_FIELDS(is_extern, is_aggregate, is_in_out)
};

struct ParamInfo {
  std::string name;
  std::string type;
  std::string default_value;
  std::string desc;
  KCFG_DEFINE_FIELDS(name, type, default_value, desc)
};

struct DataValue {
  std::atomic<void*> val = nullptr;
  std::shared_ptr<std::string> name;
  std::shared_ptr<void> _sval;  // store shared_ptr
  uint32_t _idx = 0;
  DataValue() = default;
  DataValue(const DataValue& other) {
    val.store(other.val.load());
    name = other.name;
    _sval = other._sval;
  }
  DataValue& operator=(const DataValue& other) {
    val.store(other.val.load());
    name = other.name;
    _sval = other._sval;
    return *this;
  }
  void Reset() {
    val.store(nullptr);
    _sval.reset();
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

class GraphDataContext;
using GraphDataContextPtr = std::shared_ptr<GraphDataContext>;
class GraphDataContext {
 public:
  using ExcludeGraphDataContextSet = folly::F14ValueSet<const GraphDataContext*>;

 private:
  GraphDataContext() = default;
  using DataValuePtr = std::unique_ptr<DataValue>;
  using DataTable = folly::F14FastMap<DIObjectKeyView, DataValuePtr, DIObjectKeyViewHash, DIObjectKeyViewEqual>;
  using DataArray = folly::fbvector<DataValue*>;
  DataTable _data_table;
  DataArray _data_array;
  const GraphDataContext* _parent = nullptr;
  std::unique_ptr<DAGEventTracker> _event_tracker;
  std::vector<const GraphDataContext*> _executed_childrens;
  google::protobuf::Arena* arena_ = nullptr;
  std::unique_ptr<google::protobuf::Arena> own_arena_;
  bool _disable_entry_creation = false;

  DataValue* GetValue(const DIObjectKeyView& key, GraphDataGetOptions opt = {},
                      ExcludeGraphDataContextSet* excludes = nullptr);

  inline const DataValue* GetDataValue(const DIObjectKeyView& key, int32_t idx) const {
    if (idx >= 0) {
      if (idx < _data_array.size()) {
        return _data_array[idx];
      }
    }
    auto found = _data_table.find(key);
    if (found != _data_table.end()) {
      return found->second.get();
    }
    return nullptr;
  }
  inline DataValue* GetDataValue(const DIObjectKeyView& key, int32_t idx) {
    if (idx >= 0) {
      if (idx < _data_array.size()) {
        return _data_array[idx];
      }
    }
    auto found = _data_table.find(key);
    if (found != _data_table.end()) {
      return found->second.get();
    }
    return nullptr;
  }

 public:
  static GraphDataContextPtr New() {
    GraphDataContextPtr p(new GraphDataContext);
    return p;
  }
  void SetParent(const GraphDataContext* p) { _parent = p; }
  const GraphDataContext* GetParent() { return _parent; }
  void ReserveChildCapacity(size_t n);
  void SetChild(const GraphDataContext* c, size_t idx);

  uint32_t RegisterData(const DIObjectKey& id);
  int Move(const DIObjectKey& from, const DIObjectKey& to);

  void DisableEntryCreation() { _disable_entry_creation = true; }

  inline DAGEventTracker* GetEventTracker() const {
    if (_event_tracker) {
      return _event_tracker.get();
    }
    if (_parent) {
      return _parent->GetEventTracker();
    }
    return nullptr;
  }
  bool EnableEventTracker();

  void SetArena(google::protobuf::Arena* arena);
  void SetArena(std::unique_ptr<google::protobuf::Arena>&& arena);
  google::protobuf::Arena* GetArena() const;

  void Reset();
  template <typename T>
  inline typename DIObjectTypeHelper<T>::read_type Get(const std::string_view& name, int32_t idx = -1,
                                                       GraphDataGetOptions opt = {},
                                                       ExcludeGraphDataContextSet* excludes = nullptr) const {
    using GetValueType = typename DIObjectTypeHelper<T>::read_type;
    uint32_t id = DIContainer::GetTypeId<T>();
    DIObjectKeyView key = {name, id};
    // auto found = _data_table.find(key);
    auto found = GetDataValue(key, idx);
    if (FOLLY_LIKELY(found != nullptr)) {
      if constexpr (std::is_pointer<GetValueType>::value) {
        GetValueType val = static_cast<GetValueType>(found->val.load());
        if (nullptr != val) {
          return val;
        }
        // NOCC:readability/braces(工具误报:大括号是存在的)
      } else if constexpr (is_shared_ptr<GetValueType>::value) {
        GetValueType val = std::static_pointer_cast<typename GetValueType::element_type>(found->_sval);
        if (nullptr != val) {
          return val;
        }
      } else {
        GetValueType* val = static_cast<GetValueType*>(found->val.load());
        if (nullptr != val) {
          return *val;
        }
      }
    }

    std::unique_ptr<ExcludeGraphDataContextSet> empty_execludes(new ExcludeGraphDataContextSet);
    ExcludeGraphDataContextSet* new_excludes = excludes;
    if (nullptr == new_excludes) {
      new_excludes = empty_execludes.get();
    }
    new_excludes->insert(this);
    GetValueType r = {};
    if (opt.with_parent && _parent) {
      GraphDataGetOptions parent_opt;
      parent_opt.with_parent = 1;
      parent_opt.with_children = opt.with_children;
      parent_opt.with_di_container = 0;
      if (new_excludes->count(_parent) == 0) {
        r = _parent->Get<T>(name, -1, parent_opt, new_excludes);
      }
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
        if (nullptr != child_ctx && new_excludes->count(child_ctx) == 0) {
          r = child_ctx->Get<T>(name, -1, child_opt, new_excludes);
          if (r) {
            return r;
          }
        }
      }
    }
    return r;
  }
  template <typename T>
  inline typename DIObjectTypeHelper<T>::read_write_type Move(const std::string_view& name, int32_t idx = -1,
                                                              GraphDataGetOptions opt = {},
                                                              ExcludeGraphDataContextSet* excludes = nullptr) {
    using MoveValueType = typename DIObjectTypeHelper<T>::read_write_type;
    uint32_t id = DIContainer::GetTypeId<T>();
    DIObjectKeyView key = {name, id};
    // auto found = _data_table.find(key);
    auto found = GetDataValue(key, idx);
    if (found != nullptr) {
      if constexpr (std::is_pointer<MoveValueType>::value) {
        void* empty = nullptr;
        MoveValueType val = (MoveValueType)(found->val.exchange(empty));
        if (val) {
          return val;
        }
      }
    }
    std::unique_ptr<ExcludeGraphDataContextSet> empty_execludes(new ExcludeGraphDataContextSet);
    ExcludeGraphDataContextSet* new_excludes = excludes;
    if (nullptr == new_excludes) {
      new_excludes = empty_execludes.get();
    }
    new_excludes->insert(this);
    if (opt.with_parent && _parent) {
      GraphDataGetOptions parent_opt;
      parent_opt.with_parent = 1;
      parent_opt.with_children = opt.with_children;
      parent_opt.with_di_container = 0;

      if (new_excludes->count(_parent) == 0) {
        MoveValueType r = const_cast<GraphDataContext*>(_parent)->Move<T>(name, -1, parent_opt, new_excludes);
        if (r) {
          return r;
        }
      }
    }
    if (opt.with_children) {
      GraphDataGetOptions child_opt;
      child_opt.with_children = 1;
      opt.with_parent = 0;
      opt.with_di_container = 0;
      for (const GraphDataContext* child_ctx : _executed_childrens) {
        if (nullptr != child_ctx && new_excludes->count(child_ctx) == 0) {
          MoveValueType r = const_cast<GraphDataContext*>(child_ctx)->Move<T>(name, -1, child_opt, new_excludes);
          if (r) {
            return r;
          }
        }
      }
    }
    MoveValueType empty = {};
    return empty;
  }

  template <typename T>
  inline bool Set(const std::string_view& name, const T& v, int32_t idx = -1) {
    using RT = typename std::remove_const<typename std::remove_pointer<T>::type>::type;
    uint32_t id = DIContainer::GetTypeId<RT>();
    DIObjectKeyView key = {name, id};
    // auto found = _data_table.find(key);
    auto found = GetDataValue(key, idx);
    if (found != nullptr) {
      if constexpr (is_shared_ptr<T>::value) {
        found->_sval = v;
        found->val = found->_sval.get();
        // NOCC:readability/braces(工具误报:大括号是存在的)
      } else if constexpr (is_shared_ptr<RT>::value && std::is_pointer_v<T>) {
        found->_sval = *v;
        found->val = found->_sval.get();
      } else if constexpr (std::is_pointer_v<T>) {
        found->val.store(const_cast<void*>(static_cast<const void*>(v)));
      } else {
        found->val.store(const_cast<void*>(static_cast<const void*>(v)));
      }
      return true;
    } else {
      if (_disable_entry_creation) {
        return false;
      }
      auto dv = std::make_unique<DataValue>();
      if constexpr (is_shared_ptr<T>::value) {
        dv->_sval = v;
        dv->val = dv->_sval.get();
        // NOCC:readability/braces(工具误报:大括号是存在的)
      } else if constexpr (is_shared_ptr<RT>::value && std::is_pointer_v<T>) {
        dv->_sval = *v;
        dv->val = dv->_sval.get();
      } else if constexpr (std::is_pointer_v<T>) {
        dv->val.store(const_cast<void*>(static_cast<const void*>(v)));
      } else {
        dv->val.store(const_cast<void*>(static_cast<const void*>(v)));
      }
      dv->name.reset(new std::string(name.data(), name.size()));
      DIObjectKeyView key = {*(dv->name), id};
      _data_table.emplace(key, std::move(dv));
      return true;
    }
  }
};

constexpr char* kExprParamKey = "__expr";
constexpr char* kWhileExecCluterParamKey = "__while_cluster";
constexpr char* kWhileExecGraphParamKey = "__while_graph";
constexpr char* kWhileAsyncExecParamKey = "__while_async_exec";
constexpr char* kDefaultWhileOperatorName = "ispine_while_processor";

using InjectFunc = std::function<int(GraphDataContext&, int32_t, const std::string_view&, bool)>;
using EmitFunc = std::function<int(GraphDataContext&, int32_t, const std::string_view&)>;

struct FieldInfo : public DIObjectKey {
  std::string type;
  FieldFlags flags;

  InjectFunc inject;
  EmitFunc emit;
  KCFG_DEFINE_FIELDS(type, name, id, flags)
};

class VertexContext;
class GraphClusterContext;
class Processor {
 public:
  enum class ExecMode : uint8_t {
    EXEC_SYNC = 0,
    EXEC_ASYNC_FUTURE,
    EXEC_STD_COROUTINE,
    EXEC_ADAPTIVE,
  };

  enum class GetStringMode : int {
    kWithNoExtraData = 0,
    // ToString will contain the cond.
    kWithCond = 1 << 0,
    // ToString will include all the extra data this Status has.
    kWithEverything = ~kWithNoExtraData,
    // Default mode used by ToString. Its exact value might change in the future.
    kDefault = kWithNoExtraData,
  };

 protected:
  // typedef std::function<int(GraphDataContext&, const std::string_view&, bool)> InjectFunc;
  // typedef std::function<int(GraphDataContext&, const std::string_view&)> EmitFunc;
  typedef std::function<void(const Params&)> ParamSetFunc;
  typedef std::function<void(void)> ResetFunc;
  typedef std::function<void(void)> PrepareFunc;
  // typedef folly::F14FastMap<std::string, InjectFunc> FieldInjectFuncTable;
  // typedef folly::F14FastMap<std::string, EmitFunc> FieldEmitFuncTable;
  std::vector<FieldInfo> _input_ids;
  std::vector<FieldInfo> _output_ids;
  std::vector<ParamInfo> _params;
  std::vector<ParamSetFunc> _params_settings;
  // FieldInjectFuncTable _field_inject_table;
  // FieldEmitFuncTable _field_emit_table;
  std::vector<ResetFunc> _reset_funcs;
  std::vector<PrepareFunc> _prepare_funcs;
  GraphDataContext* _data_ctx = nullptr;
  std::string id_;

  size_t RegisterParam(const std::string& name, const std::string& type, const std::string& deafult_value,
                       const std::string& desc, ParamSetFunc&& f);

  template <typename T>
  size_t RegisterInput(const std::string& field, const std::string& type, InjectFunc&& inject, FieldFlags flags) {
    FieldInfo info;
    info.flags = flags;
    info.name = field;
    info.type = type;
    info.id = DIContainer::GetTypeId<T>();
    info.inject = std::move(inject);
    _input_ids.push_back(info);
    // _field_inject_table.emplace(field, inject);
    return _input_ids.size();
  }
  template <typename T>
  size_t RegisterOutput(const std::string& field, const std::string& type, EmitFunc&& emit) {
    FieldInfo info;
    info.name = field;
    info.type = type;
    info.id = DIContainer::GetTypeId<T>();
    info.emit = std::move(emit);
    _output_ids.push_back(info);
    //_field_emit_table.emplace(field, emit);
    return _output_ids.size();
  }
  size_t AddResetFunc(ResetFunc&& f);
  size_t AddPrepareFunc(PrepareFunc&& f);

  GraphDataContext& GetDataContext() { return *_data_ctx; }
  google::protobuf::Arena* GetArena() { return _data_ctx->GetArena(); }

  const std::string& GetID() const { return id_; }

  virtual int OnSetup(const Params& args) { return 0; }
  virtual int OnPrepare(const Params& args) { return 0; }
  virtual int OnReset() { return 0; }
  virtual int OnExecute(const Params& args) { return ERR_UNIMPLEMENTED; }
#if ISPINE_HAS_COROUTINES
  virtual ispine::Awaitable<int> OnCoroExecute(const Params& args) { co_return ERR_UNIMPLEMENTED; }
#endif
  virtual folly::Future<int> OnFutureExecute(const Params& args) { return folly::makeFuture<int>(ERR_UNIMPLEMENTED); }
  virtual ispine::AdaptiveWait<int> OnAExecute(const Params& args) { co_return ERR_UNIMPLEMENTED; }

  friend class VertexContext;
  friend class GraphClusterContext;

 public:
  inline void SetDataContext(GraphDataContext* p) { _data_ctx = p; }
  virtual std::string_view Desc() const { return ""; }
  virtual std::string_view Name() const = 0;
  virtual ExecMode GetExecMode() const { return ExecMode::EXEC_SYNC; }
  virtual std::string_view GetString(GetStringMode mode = GetStringMode::kDefault) const { return ""; }

  // io bound processor
  virtual bool isIOProcessor() const { return false; }
  const std::vector<FieldInfo>& GetInputIds() { return _input_ids; }
  const std::vector<FieldInfo>& GetOutputIds() { return _output_ids; }
  const std::vector<ParamInfo>& GetParams() { return _params; }
  int Setup(const Params& args);
  int Prepare(const Params& args);
  void Reset();
  int Execute(const Params& args);
#if ISPINE_HAS_COROUTINES
  ispine::Awaitable<int> CoroExecute(const Params& args);
#endif
  ispine::AdaptiveWait<int> AExecute(const Params& args);
  folly::Future<int> FutureExecute(const Params& args);
  // int InjectInputField(GraphDataContext& ctx, const std::string& field_name, const std::string_view& data_name,
  //                      bool move);
  // int EmitOutputField(GraphDataContext& ctx, const std::string& field_name, const std::string_view& data_name);
  virtual ~Processor();
};

typedef std::function<Processor*(void)> ProcessorCreator;
struct ProcessorRegister {
  ProcessorRegister(std::string_view name, const ProcessorCreator& creator);
};
}  // namespace didagle

#define GRAPH_OP_CLASS_NAME(...) \
  BOOST_PP_CAT(GraphProcessor, BOOST_PP_CAT(BOOST_PP_VARIADIC_ELEM(0, __VA_ARGS__), Object))

#define GRAPH_OP_BEGIN(...)                                                                                         \
  namespace {                                                                                                       \
  namespace BOOST_PP_CAT(didagle_ops, __COUNTER__) {                                                                \
  struct GRAPH_OP_CLASS_NAME(__VA_ARGS__);                                                                          \
  using LocalProcessorClass = GRAPH_OP_CLASS_NAME(__VA_ARGS__);                                                     \
  struct GRAPH_OP_CLASS_NAME(__VA_ARGS__) : public didagle::Processor {                                             \
    static const constexpr std::string_view _local_processor_name =                                                 \
        BOOST_PP_STRINGIZE(BOOST_PP_VARIADIC_ELEM(0, __VA_ARGS__));                                                 \
    static const constexpr std::string_view _local_processor_desc =                                                 \
        BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 2), BOOST_PP_VARIADIC_ELEM(1, __VA_ARGS__), \
                    ("Empty Description"));                                                                         \
    std::string_view Name() const override { return _local_processor_name; }                                        \
    std::string_view Desc() const override { return _local_processor_desc; }

#define GRAPH_CORO_OP_BEGIN(...) \
  GRAPH_OP_BEGIN(__VA_ARGS__)    \
  ExecMode GetExecMode() const override { return ExecMode::EXEC_STD_COROUTINE; }

#define GRAPH_AUTO_OP_BEGIN(...) \
  GRAPH_OP_BEGIN(__VA_ARGS__)    \
  ExecMode GetExecMode() const override { return ExecMode::EXEC_ADAPTIVE; }

#define GRAPH_FUTURE_OP_BEGIN(...) \
  GRAPH_OP_BEGIN(__VA_ARGS__)      \
  ExecMode GetExecMode() const override { return ExecMode::EXEC_ASYNC_FUTURE; }

#define GRAPH_OP_END                                                                                                \
  }                                                                                                                 \
  ;                                                                                                                 \
  static didagle::ProcessorRegister BOOST_PP_CAT(instance_, __COUNTER__)(                                           \
      LocalProcessorClass::_local_processor_name, []() -> didagle::Processor* { return new LocalProcessorClass; }); \
  }  /* namespace BOOST_PP_CAT */                                                                                   \
  }  // namespace

#define __GRAPH_OP_INPUT(TYPE, NAME, FLAGS)                                                                     \
  typename didagle::DIObjectTypeHelper<BOOST_PP_REMOVE_PARENS(TYPE)>::read_type NAME = {};                      \
  size_t __input_##NAME##_code = RegisterInput<BOOST_PP_REMOVE_PARENS(TYPE)>(                                   \
      #NAME, BOOST_PP_STRINGIZE(BOOST_PP_REMOVE_PARENS(TYPE)),                                                                 \
                 [this](didagle::GraphDataContext& ctx, int32_t idx, const std::string_view& data, bool move) { \
                   using FIELD_TYPE =                                                                           \
                       typename std::remove_const<typename std::remove_pointer<decltype(NAME)>::type>::type;    \
                   if (FOLLY_LIKELY(!move)) {                                                                   \
                     NAME = ctx.Get<FIELD_TYPE>(data, idx);                                                     \
                   } else {                                                                                     \
                     NAME = ctx.Move<FIELD_TYPE>(data, idx);                                                    \
                   }                                                                                            \
                   return (!NAME) ? -1 : 0;                                                                     \
                 },                                                                                             \
                 BOOST_PP_REMOVE_PARENS(FLAGS));                                                                \
  size_t __reset_##NAME##_code = AddResetFunc([this]() { NAME = {}; });

#define GRAPH_OP_INPUT(TYPE, NAME) __GRAPH_OP_INPUT(TYPE, NAME, ({0, 0, 0}))
#define GRAPH_OP_EXTERN_INPUT(TYPE, NAME) __GRAPH_OP_INPUT(TYPE, NAME, ({1, 0, 0}))

#define GRAPH_OP_MAP_INPUT(TYPE, NAME)                                                                            \
  std::map<std::string, typename didagle::DIObjectTypeHelper<BOOST_PP_REMOVE_PARENS(TYPE)>::read_type> NAME;      \
  size_t __input_##NAME##_code = RegisterInput<BOOST_PP_REMOVE_PARENS(TYPE)>(                                     \
      #NAME, BOOST_PP_STRINGIZE(BOOST_PP_REMOVE_PARENS(TYPE)),                                                    \
                                [this](didagle::GraphDataContext& ctx, int32_t idx, const std::string_view& data, \
                                       bool move) {                                                               \
                                  using FIELD_TYPE = BOOST_PP_REMOVE_PARENS(TYPE);                                \
                                  typename didagle::DIObjectTypeHelper<FIELD_TYPE>::read_type tmp = {};           \
                                  if (FOLLY_LIKELY(!move)) {                                                      \
                                    tmp = ctx.Get<FIELD_TYPE>(data, idx);                                         \
                                  } else {                                                                        \
                                    tmp = ctx.Move<FIELD_TYPE>(data, idx);                                        \
                                  }                                                                               \
                                  if (!tmp) {                                                                     \
                                    return -1;                                                                    \
                                  }                                                                               \
                                  std::string name_key(data.data(), data.size());                                 \
                                  NAME[name_key] = tmp;                                                           \
                                  return 0;                                                                       \
                                },                                                                                \
                                {0, 1, 0});                                                                       \
  size_t __reset_##NAME##_code = AddResetFunc([this]() { NAME = {}; });

#define GRAPH_OP_OUTPUT(TYPE, NAME)                                                                                 \
  typename didagle::DIObjectTypeHelper<BOOST_PP_REMOVE_PARENS(TYPE)>::write_type NAME = {};                         \
  size_t __output_##NAME##_code = RegisterOutput<BOOST_PP_REMOVE_PARENS(TYPE)>(                                     \
      #NAME, BOOST_PP_STRINGIZE(BOOST_PP_REMOVE_PARENS(TYPE)),                                                      \
                                [this](didagle::GraphDataContext& ctx, int32_t idx, const std::string_view& data) { \
                                  ctx.Set(data, &(this->NAME), idx);                                                \
                                  return 0;                                                                         \
                                });                                                                                 \
  size_t __reset_##NAME##_code = AddResetFunc([this]() {                                                            \
    didagle::Reset<decltype(NAME)> reset;                                                                           \
    reset(NAME);                                                                                                    \
  });                                                                                                               \
  size_t __prepare_##NAME##_code = AddPrepareFunc([this]() {                                                        \
    didagle::Prepare<decltype(NAME)> prepare;                                                                       \
    prepare(NAME);                                                                                                  \
  });

#define GRAPH_OP_ARENA_OUTPUT(TYPE, NAME)                                                                           \
  typename didagle::DIObjectTypeHelper<BOOST_PP_REMOVE_PARENS(TYPE)>::write_type* NAME = {};                        \
  size_t __output_##NAME##_code = RegisterOutput<BOOST_PP_REMOVE_PARENS(TYPE)>(                                     \
      #NAME, BOOST_PP_STRINGIZE(BOOST_PP_REMOVE_PARENS(TYPE)),                                                      \
                                [this](didagle::GraphDataContext& ctx, int32_t idx, const std::string_view& data) { \
                                  ctx.Set(data, this->NAME, idx);                                                   \
                                  return 0;                                                                         \
                                });                                                                                 \
  size_t __reset_##NAME##_code = AddResetFunc([this]() {                                                            \
    if (nullptr != NAME && nullptr == NAME->GetArena()) {                                                           \
      delete NAME;                                                                                                  \
    }                                                                                                               \
    NAME = nullptr;                                                                                                 \
  });                                                                                                               \
  size_t __prepare_##NAME##_code = AddPrepareFunc(                                                                  \
      [this]() { NAME = google::protobuf::Arena::CreateMessage<BOOST_PP_REMOVE_PARENS(TYPE)>(GetArena()); });

#define __GRAPH_OP_IN_OUT(TYPE, NAME, FLAGS)                                                                        \
  typename didagle::DIObjectTypeHelper<BOOST_PP_REMOVE_PARENS(TYPE)>::read_write_type NAME = {};                    \
  size_t __input_##NAME##_code = RegisterInput<BOOST_PP_REMOVE_PARENS(TYPE)>(                                       \
      #NAME, BOOST_PP_STRINGIZE(BOOST_PP_REMOVE_PARENS(TYPE)),                                                      \
                                [this](didagle::GraphDataContext& ctx, int32_t idx, const std::string_view& data,   \
                                       bool move) {                                                                 \
                                  if (!move) {                                                                      \
                                    return -1;                                                                      \
                                  }                                                                                 \
                                  using FIELD_TYPE = typename std::remove_pointer<decltype(NAME)>::type;            \
                                  NAME = ctx.Move<FIELD_TYPE>(data, idx);                                           \
                                  return (!NAME) ? -1 : 0;                                                          \
                                },                                                                                  \
                                BOOST_PP_REMOVE_PARENS(FLAGS));                                                     \
  size_t __output_##NAME##_code = RegisterOutput<BOOST_PP_REMOVE_PARENS(TYPE)>(                                     \
      #NAME, BOOST_PP_STRINGIZE(BOOST_PP_REMOVE_PARENS(TYPE)),                                                      \
                                [this](didagle::GraphDataContext& ctx, int32_t idx, const std::string_view& data) { \
                                  ctx.Set(data, NAME, idx);                                                         \
                                  return 0;                                                                         \
                                });                                                                                 \
  size_t __reset_##NAME##_code = AddResetFunc([this]() { NAME = {}; });

#define GRAPH_OP_IN_OUT(TYPE, NAME) __GRAPH_OP_IN_OUT(TYPE, NAME, ({0, 0, 1}))
#define GRAPH_OP_EXTERN_IN_OUT(TYPE, NAME) __GRAPH_OP_IN_OUT(TYPE, NAME, ({1, 0, 1}))

#define GRAPH_PARAMS_string(name, val, txt)                                                             \
  didagle::ParamsString PARAMS_##name = val;                                                            \
  size_t __PARAMS_##name##_code =                                                                       \
      RegisterParam(BOOST_PP_STRINGIZE(name), "string", val, txt, [this](const didagle::Params& args) { \
        if (args[BOOST_PP_STRINGIZE(name)].IsString()) {                                                \
          PARAMS_##name = args[BOOST_PP_STRINGIZE(name)].String();                                      \
        }                                                                                               \
      });                                                                                               \
  size_t __reset_PARAMS_##name##_code = AddResetFunc([this]() { PARAMS_##name = val; });

#define GRAPH_PARAMS_string_vector(name, val, txt)                                                                    \
  std::vector<didagle::ParamsString> PARAMS_##name;                                                                   \
  size_t __PARAMS_##name##_code = RegisterParam(                                                                      \
      BOOST_PP_STRINGIZE(name), "vector<string>", BOOST_PP_STRINGIZE(val), txt, [this](const didagle::Params& args) { \
        if (args.Contains(BOOST_PP_STRINGIZE(name))) {                                                                \
          PARAMS_##name.clear();                                                                                      \
          const auto& member = args[BOOST_PP_STRINGIZE(name)];                                                        \
          for (size_t i = 0; i < member.Size(); i++) {                                                                \
            PARAMS_##name.emplace_back(member[i].String());                                                           \
          }                                                                                                           \
        }                                                                                                             \
      });                                                                                                             \
  size_t __reset_PARAMS_##name##_code = AddResetFunc([this]() { PARAMS_##name = BOOST_PP_REMOVE_PARENS(val); });

#define GRAPH_PARAMS_int(name, val, txt)                                                                   \
  int64_t PARAMS_##name = val;                                                                             \
  size_t __PARAMS_##name##_code = RegisterParam(                                                           \
      BOOST_PP_STRINGIZE(name), "int", BOOST_PP_STRINGIZE(val), txt, [this](const didagle::Params& args) { \
        if (args[BOOST_PP_STRINGIZE(name)].IsInt()) {                                                      \
          PARAMS_##name = args[BOOST_PP_STRINGIZE(name)].Int();                                            \
        }                                                                                                  \
      });                                                                                                  \
  size_t __reset_PARAMS_##name##_code = AddResetFunc([this]() { PARAMS_##name = val; });

#define GRAPH_PARAMS_int_vector(name, val, txt)                                                                    \
  std::vector<int64_t> PARAMS_##name;                                                                              \
  size_t __PARAMS_##name##_code = RegisterParam(                                                                   \
      BOOST_PP_STRINGIZE(name), "vector<int>", BOOST_PP_STRINGIZE(val), txt, [this](const didagle::Params& args) { \
        if (args.Contains(BOOST_PP_STRINGIZE(name))) {                                                             \
          PARAMS_##name.clear();                                                                                   \
          const auto& member = args[BOOST_PP_STRINGIZE(name)];                                                     \
          for (size_t i = 0; i < member.Size(); i++) {                                                             \
            PARAMS_##name.emplace_back(member[i].Int());                                                           \
          }                                                                                                        \
        }                                                                                                          \
      });                                                                                                          \
  size_t __reset_PARAMS_##name##_code = AddResetFunc([this]() { PARAMS_##name = BOOST_PP_REMOVE_PARENS(val); });

#define GRAPH_PARAMS_bool(name, val, txt)                                                                   \
  bool PARAMS_##name = val;                                                                                 \
  size_t __PARAMS_##name##_code = RegisterParam(                                                            \
      BOOST_PP_STRINGIZE(name), "bool", BOOST_PP_STRINGIZE(val), txt, [this](const didagle::Params& args) { \
        if (args[BOOST_PP_STRINGIZE(name)].IsBool()) {                                                      \
          PARAMS_##name = args[BOOST_PP_STRINGIZE(name)].Bool();                                            \
        }                                                                                                   \
      });                                                                                                   \
  size_t __reset_PARAMS_##name##_code = AddResetFunc([this]() { PARAMS_##name = val; });

#define GRAPH_PARAMS_bool_vector(name, val, txt)                                                                   \
  std::vector<bool> PARAMS_##name;                                                                                 \
  size_t __PARAMS_##name##_code = RegisterParam(                                                                   \
      BOOST_PP_STRINGIZE(name), "vector<int>", BOOST_PP_STRINGIZE(val), txt, [this](const didagle::Params& args) { \
        if (args.Contains(BOOST_PP_STRINGIZE(name))) {                                                             \
          PARAMS_##name.clear();                                                                                   \
          const auto& member = args[BOOST_PP_STRINGIZE(name)];                                                     \
          for (size_t i = 0; i < member.Size(); i++) {                                                             \
            PARAMS_##name.emplace_back(member[i].Bool());                                                          \
          }                                                                                                        \
        }                                                                                                          \
      });                                                                                                          \
  size_t __reset_PARAMS_##name##_code = AddResetFunc([this]() { PARAMS_##name = BOOST_PP_REMOVE_PARENS(val); });

#define GRAPH_PARAMS_double(name, val, txt)                                                                   \
  double PARAMS_##name = val;                                                                                 \
  size_t __PARAMS_##name##_code = RegisterParam(                                                              \
      BOOST_PP_STRINGIZE(name), "double", BOOST_PP_STRINGIZE(val), txt, [this](const didagle::Params& args) { \
        if (args[BOOST_PP_STRINGIZE(name)].IsDouble()) {                                                      \
          PARAMS_##name = args[BOOST_PP_STRINGIZE(name)].Double();                                            \
        }                                                                                                     \
      });                                                                                                     \
  size_t __reset_PARAMS_##name##_code = AddResetFunc([this]() { PARAMS_##name = val; });

#define GRAPH_PARAMS_double_vector(name, val, txt)                                                                    \
  std::vector<double> PARAMS_##name;                                                                                  \
  size_t __PARAMS_##name##_code = RegisterParam(                                                                      \
      BOOST_PP_STRINGIZE(name), "vector<double>", BOOST_PP_STRINGIZE(val), txt, [this](const didagle::Params& args) { \
        if (args.Contains(BOOST_PP_STRINGIZE(name))) {                                                                \
          PARAMS_##name.clear();                                                                                      \
          const auto& member = args[BOOST_PP_STRINGIZE(name)];                                                        \
          for (size_t i = 0; i < member.Size(); i++) {                                                                \
            PARAMS_##name.emplace_back(member[i].Double());                                                           \
          }                                                                                                           \
        }                                                                                                             \
      });                                                                                                             \
  size_t __reset_PARAMS_##name##_code = AddResetFunc([this]() { PARAMS_##name = BOOST_PP_REMOVE_PARENS(val); });
