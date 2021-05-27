// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/05/26
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include "didagle_log.h"

namespace didagle {

template <typename T>
struct DIObjectTypeHelper {
  typedef const T* read_type;
  typedef T write_type;
  typedef T* read_write_type;
  // static constexpr bool is_shared_ptr = false;
  // static constexpr bool is_unique_ptr = false;
};

template <typename T>
struct DIObjectTypeHelper<std::shared_ptr<T>> {
  typedef std::shared_ptr<T> read_type;
  typedef std::shared_ptr<T> write_type;
  typedef std::shared_ptr<T> read_write_type;
  // static constexpr bool is_shared_ptr = true;
  // static constexpr bool is_unique_ptr = false;
};

template <typename T>
struct DIObjectTypeHelper<std::unique_ptr<T>> {
  typedef std::unique_ptr<T> read_type;
  typedef std::unique_ptr<T> write_type;
  typedef std::unique_ptr<T> read_write_type;
  // static constexpr bool is_shared_ptr = false;
  // static constexpr bool is_unique_ptr = true;
};

struct DIObjectKey {
  std::string name;
  uint32_t id = 0;
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
  }
};
struct DIObjectKeyViewEqual {
  bool operator()(const DIObjectKeyView& x, const DIObjectKeyView& y) const {
    return x.name == y.name && x.id == y.id;
  }
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
  DIObjectBuilder(const std::function<DIObjectType()>& f = std::function<DIObjectType()>(),
                  const std::function<int()> init = std::function<int()>())
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
typedef std::unordered_map<DIObjectKeyView, DIObjectBuilderValue, DIObjectKeyViewHash,
                           DIObjectKeyViewEqual>
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
  static typename DIObjectTypeHelper<T>::read_type Get(const std::string& id) {
    uint32_t type_id = DIContainer::GetTypeId<T>();
    DIObjectKeyView key = {id, type_id};
    DIObjectBuilderTable& builders = GetDIObjectBuilderTable();
    auto found = builders.find(key);
    if (found != builders.end()) {
      DIObjectBuilderPtr builder_ptr = found->second.builder;
      DIObjectBuilder<T>* builder = (DIObjectBuilder<T>*)(builder_ptr.get());
      return builder->Get();
    }
    typename DIObjectTypeHelper<T>::read_type r = {};
    return r;
  }
};
}  // namespace didagle