// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/05/26
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <boost/preprocessor/library.hpp>
#include "google/protobuf/service.h"

#include "didagle/didagle_log.h"
#include "kcfg_json.h"

namespace didagle {

template <class T>
struct is_shared_ptr : std::false_type {};

template <class T>
struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};

template <class T>
struct is_shared_ptr<const std::shared_ptr<T>> : std::true_type {};

template <class T>
struct is_unique_ptr : std::false_type {};

template <class T>
struct is_unique_ptr<std::unique_ptr<T>> : std::true_type {};

struct ServiceTag {
  virtual ~ServiceTag() {}
};

class DIObject;
template <typename T,
          bool = std::is_base_of<::google::protobuf::Service, T>::value || std::is_base_of<ServiceTag, T>::value>
struct DIObjectTypeHelper {
  typedef const T* read_type;
  typedef T write_type;
  typedef T* read_write_type;
  static constexpr bool is_di_object = std::is_base_of<DIObject, T>::value;
};

template <typename T>
struct DIObjectTypeHelper<T, true> {
  typedef T* read_type;
  typedef T write_type;
  typedef T* read_write_type;
  static constexpr bool is_di_object = std::is_base_of<DIObject, T>::value;
};

template <typename T>
struct DIObjectTypeHelper<std::shared_ptr<T>, false> {
  typedef std::shared_ptr<T> read_type;
  typedef std::shared_ptr<T> write_type;
  typedef std::shared_ptr<T> read_write_type;
  static constexpr bool is_di_object = std::is_base_of<DIObject, T>::value;
};

template <typename T>
struct DIObjectTypeHelper<std::unique_ptr<T>, false> {
  typedef std::unique_ptr<T> read_type;
  typedef std::unique_ptr<T> write_type;
  typedef std::unique_ptr<T> read_write_type;
  static constexpr bool is_di_object = std::is_base_of<DIObject, T>::value;
  // static constexpr bool is_unique_ptr = true;
};

struct DIObjectKey {
  std::string name;
  uint32_t id = 0;

  KCFG_DEFINE_FIELDS(name, id)

  bool operator<(const DIObjectKey& other) const {
    if (id == other.id) {
      return name < other.name;
    }
    return id < other.id;
  }
};
struct DIObjectKeyView {
  std::string_view name;
  uint32_t id = 0;
};
struct DIObjectKeyViewHash {
  size_t operator()(const DIObjectKeyView& id) const noexcept {
    size_t h1 = std::hash<std::string_view>()(id.name);
    size_t h2 = id.id;
    return h1 ^ h2;
    // absl::Hash<DIObjectKeyView> h;
    // return h(id);
  }
};
struct DIObjectKeyViewEqual {
  bool operator()(const DIObjectKeyView& x, const DIObjectKeyView& y) const { return x.name == y.name && x.id == y.id; }
};

struct DIObjectBuilderBase {
  virtual int Init() { return 0; }
  virtual ~DIObjectBuilderBase() {}
};
template <typename T>
struct DIObjectBuilder : public DIObjectBuilderBase {
  typedef typename DIObjectTypeHelper<T>::read_type DIObjectType;

  std::function<DIObjectType()> _functor;
  std::function<int()> _init;
  explicit DIObjectBuilder(const std::function<DIObjectType()>& f = {}, const std::function<int()> init = {})
      : _functor(f), _init(init) {}
  virtual int Init() {
    if (_init) {
      return _init();
    }
    return 0;
  }
  virtual DIObjectType Get() {
    if (_functor) {
      return _functor();
    }
    DIObjectType r = {};
    return r;
  }
  virtual ~DIObjectBuilder() {}
};
typedef std::shared_ptr<DIObjectBuilderBase> DIObjectBuilderPtr;
struct DIObjectBuilderValue {
  DIObjectBuilderPtr builder;
  std::shared_ptr<std::string> id;
  bool inited = false;
};
typedef std::unordered_map<DIObjectKeyView, DIObjectBuilderValue, DIObjectKeyViewHash, DIObjectKeyViewEqual>
    DIObjectBuilderTable;
class DIContainer {
 private:
  static uint32_t nextTypeId() {
    static std::atomic<uint32_t> type_id_seed = {98765};
    return type_id_seed.fetch_add(1);
  }
  static DIObjectBuilderTable& GetDIObjectBuilderTable();

 public:
  static int Init();
  template <typename T>
  static uint32_t GetTypeId() {
    static uint32_t id = nextTypeId();
    return id;
  }

  template <typename T>
  static int RegisterBuilder(const std::string& id, std::unique_ptr<DIObjectBuilder<T>> builder) {
    DIObjectBuilderValue dv;
    dv.id.reset(new std::string(id));
    uint32_t type_id = DIContainer::GetTypeId<T>();
    DIObjectBuilderTable& builders = GetDIObjectBuilderTable();
    DIObjectKeyView key = {*dv.id, type_id};
    if (builders.find(key) != builders.end()) {
      DIDAGLE_ERROR("Duplicate id:{} for DIContainer", id);
      return -1;
    }
    dv.builder.reset(builder.release());
    builders[key] = dv;
    return 0;
  }
  template <typename T>
  static typename DIObjectTypeHelper<T>::read_type Get(const std::string_view& id) {
    uint32_t type_id = DIContainer::GetTypeId<T>();
    DIObjectKeyView key = {id, type_id};
    DIObjectBuilderTable& builders = GetDIObjectBuilderTable();
    auto found = builders.find(key);
    if (found != builders.end()) {
      DIObjectBuilderPtr builder_ptr = found->second.builder;
      DIObjectBuilder<T>* builder = (DIObjectBuilder<T>*)(builder_ptr.get());
      auto val = builder->Get();
      if (val) {
        if constexpr (DIObjectTypeHelper<T>::is_di_object) {
          ((typename DIObjectTypeHelper<T>::read_write_type)(val))->InitDIFields();
        }
      }
      return val;
    }
    typename DIObjectTypeHelper<T>::read_type r = {};
    return r;
  }
};

class DIObject {
 private:
  typedef std::function<int()> InjectFunc;
  typedef std::unordered_map<std::string, InjectFunc> FieldInjectFuncTable;
  std::vector<DIObjectKey> _input_ids;
  FieldInjectFuncTable _field_inject_table;
  bool _di_fields_inited = false;

 protected:
  template <typename T>
  size_t RegisterInput(const std::string& field, InjectFunc&& inject) {
    DIObjectKey id{field, DIContainer::GetTypeId<T>()};
    _input_ids.push_back(id);
    _field_inject_table.emplace(field, inject);
    return _field_inject_table.size();
  }
  void DoInjectInputFields();

 public:
  int InitDIFields();
  virtual ~DIObject() {}
};

}  // namespace didagle
#define DI_DEP(TYPE, NAME)                                                                     \
  typename didagle::DIObjectTypeHelper<BOOST_PP_REMOVE_PARENS(TYPE)>::read_type NAME = {};     \
  size_t __input_##NAME##_code = RegisterInput<BOOST_PP_REMOVE_PARENS(TYPE)>(#NAME, [this]() { \
    NAME = didagle::DIContainer::Get<BOOST_PP_REMOVE_PARENS(TYPE)>(#NAME);                     \
    return NAME ? 0 : -1;                                                                      \
  });
