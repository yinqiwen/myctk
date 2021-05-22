// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/05/11
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once
#include <stdint.h>
#include <string.h>
#include <boost/preprocessor/library.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "flatbuffers/flatbuffers.h"
#include "jit_err.h"
#include "xbyak/xbyak.h"

namespace ssexpr2 {
enum ValueType {
  V_UNKNOWN = 0,
  V_CHAR,
  V_BOOL,
  V_UINT8,
  V_INT16,
  V_UINT16,
  V_INT32,
  V_UINT32,
  V_INT64,
  V_UINT64,
  V_FLOAT,
  V_DOUBLE,
  V_STD_STRING,
  V_STD_STRING_VIEW,
  V_CSTRING,
  V_FLATBUFFERS_STRING,

  V_CHAR_VALUE = 100,
  V_BOOL_VALUE,
  V_UINT8_VALUE,
  V_INT16_VALUE,
  V_UINT16_VALUE,
  V_INT32_VALUE,
  V_UINT32_VALUE,
  V_INT64_VALUE,
  V_UINT64_VALUE,
  V_FLOAT_VALUE,
  V_DOUBLE_VALUE,
  V_STRUCT = 10000,
  V_JIT_STRUCT = 10001,
};

#define IS_TYPE(dtype, type_v1, type_v2)         \
  if constexpr (std::is_same<T, dtype>::value) { \
    switch (type) {                              \
      case type_v1: {                            \
        return true;                             \
      }                                          \
      case type_v2: {                            \
        return true;                             \
      }                                          \
      default: {                                 \
        return false;                            \
      }                                          \
    }                                            \
  }

#define GET_VALUE(dtype, type_v1, type_v2)       \
  if constexpr (std::is_same<T, dtype>::value) { \
    switch (type) {                              \
      case type_v1: {                            \
        dtype v = *((T*)val);                    \
        return v;                                \
      }                                          \
      case type_v2: {                            \
        dtype v;                                 \
        memcpy(&v, &val, sizeof(v));             \
        return v;                                \
      }                                          \
      default: {                                 \
        throw std::bad_variant_access();         \
      }                                          \
    }                                            \
  }

#define SET_VALUE(dtype, type_v1)                \
  if constexpr (std::is_same<T, dtype>::value) { \
    type = type_v1;                              \
    memcpy(&val, &v, sizeof(v));                 \
    return;                                      \
  }

struct Value {
  // const void* val = nullptr;
  uint64_t val = 0;
  uint32_t type = V_UNKNOWN;

  template <typename T>
  void Set(const T& v) {
    SET_VALUE(char, V_CHAR_VALUE)
    SET_VALUE(bool, V_BOOL_VALUE)
    SET_VALUE(uint8_t, V_UINT8_VALUE)
    SET_VALUE(int16_t, V_INT16_VALUE)
    SET_VALUE(uint16_t, V_UINT16_VALUE)
    SET_VALUE(int32_t, V_INT32_VALUE)
    SET_VALUE(uint32_t, V_UINT32_VALUE)
    SET_VALUE(int64_t, V_INT64_VALUE)
    SET_VALUE(uint64_t, V_UINT64_VALUE)
    SET_VALUE(float, V_FLOAT_VALUE)
    SET_VALUE(double, V_DOUBLE_VALUE)
    if constexpr (std::is_same<T, std::string_view>::value) {
      val = (uint64_t)(&v);
      type = V_STD_STRING_VIEW;
      return;
    }
    if constexpr (std::is_same<T, std::string>::value) {
      val = (uint64_t)(&v);
      type = V_STD_STRING;
      return;
    }
    if constexpr (std::is_same<T, const char*>::value) {
      val = (uint64_t)(v);
      type = V_CSTRING;
      return;
    }
  }

  template <typename T>
  T Get() {
    GET_VALUE(char, V_CHAR, V_CHAR_VALUE)
    GET_VALUE(bool, V_BOOL, V_BOOL_VALUE)
    GET_VALUE(uint8_t, V_UINT8, V_UINT8_VALUE)
    GET_VALUE(int16_t, V_INT16, V_INT16_VALUE)
    GET_VALUE(uint16_t, V_UINT16, V_UINT16_VALUE)
    GET_VALUE(int32_t, V_INT32, V_INT32_VALUE)
    GET_VALUE(uint32_t, V_UINT32, V_UINT32_VALUE)
    GET_VALUE(int64_t, V_INT64, V_INT64_VALUE)
    GET_VALUE(uint64_t, V_UINT64, V_UINT64_VALUE)
    GET_VALUE(float, V_FLOAT, V_FLOAT_VALUE)
    GET_VALUE(double, V_DOUBLE, V_DOUBLE_VALUE)
    if constexpr (std::is_same<T, std::string_view>::value) {
      std::string_view v;
      switch (type) {
        case V_STD_STRING: {
          const std::string* s = (const std::string*)val;
          v = *s;
          return v;
        }
        case V_STD_STRING_VIEW: {
          const std::string_view* s = (const std::string_view*)val;
          v = *s;
          return v;
        }
        case V_FLATBUFFERS_STRING: {
          const flatbuffers::String* s = (const flatbuffers::String*)val;
          std::string_view sv(s->c_str(), s->size());
          return sv;
        }
        case V_CSTRING: {
          const char* s = (const char*)val;
          v = s;
          return v;
        }
        default: {
          throw std::bad_variant_access();
        }
      }
    }
    throw std::bad_variant_access();
  }

  template <typename T>
  bool Is() {
    IS_TYPE(char, V_CHAR, V_CHAR_VALUE)
    IS_TYPE(bool, V_BOOL, V_BOOL_VALUE)
    IS_TYPE(uint8_t, V_UINT8, V_UINT8_VALUE)
    IS_TYPE(int16_t, V_INT16, V_INT16_VALUE)
    IS_TYPE(uint16_t, V_UINT16, V_UINT16_VALUE)
    IS_TYPE(int32_t, V_INT32, V_INT32_VALUE)
    IS_TYPE(uint32_t, V_UINT32, V_UINT32_VALUE)
    IS_TYPE(int64_t, V_INT64, V_INT64_VALUE)
    IS_TYPE(uint64_t, V_UINT64, V_UINT64_VALUE)
    IS_TYPE(float, V_FLOAT, V_FLOAT_VALUE)
    IS_TYPE(double, V_DOUBLE, V_DOUBLE_VALUE)
    if constexpr (std::is_same<T, std::string_view>::value) {
      switch (type) {
        case V_STD_STRING:
        case V_STD_STRING_VIEW:
        case V_FLATBUFFERS_STRING:
        case V_CSTRING: {
          return true;
        }
        default: {
          return false;
        }
      }
    }
    return false;
  }
};

typedef std::function<void(Xbyak::CodeGenerator&)> FieldJitAccessBuilder;
struct FieldJitAccessBuilderTable;
struct FieldJitAccessBuilderTable
    : public std::map<std::string,
                      std::variant<FieldJitAccessBuilder,
                                   std::pair<FieldJitAccessBuilder, FieldJitAccessBuilderTable>>> {
};
typedef Value GetValue(const void*);
class ValueAccessor {
 private:
  std::shared_ptr<Xbyak::CodeGenerator> _jit;
  GetValue* _get;
  bool _own_jit;

 public:
  ValueAccessor(std::shared_ptr<Xbyak::CodeGenerator> jit = nullptr)
      : _jit(jit), _get(nullptr), _own_jit(false) {}
  bool Valid() const { return nullptr != _get; }
  const GetValue* GetFunc() const { return _get; }
  Xbyak::CodeGenerator& GetJit() { return *_jit; }
  int BeginBuild() {
    if (!_jit) {
      _jit.reset(new Xbyak::CodeGenerator);
      _own_jit = true;
    }
    Xbyak::CodeGenerator& jit = GetJit();
    jit.mov(jit.rax, jit.rdi);
    return 0;
  }
  int EndBuild() {
    if (_own_jit) {
      Xbyak::CodeGenerator& jit = GetJit();
      jit.ret();
      _get = jit.getCode<GetValue*>();
    }
    return 0;
  }
  Value operator()(const void* p) const {
    Value v;
    if (nullptr == _get) {
      return v;
    }
    v = _get(p);
    // v.val = (uint64_t)(pp);
    return v;
  }
  ~ValueAccessor() {}
};

template <typename T>
struct HasConstIterator {
 private:
  typedef char yes;
  typedef struct {
    char array[2];
  } no;

  template <typename C>
  static yes test(typename C::const_iterator*);
  template <typename C>
  static no test(...);

 public:
  static const bool value = sizeof(test<T>(0)) == sizeof(yes);
  typedef T type;
};

// SFINAE test
template <typename T>
class HasInitJitBuilder {
  typedef char one;
  struct two {
    char x[2];
  };

  template <typename C>
  static one test(decltype(&C::InitJitBuilder));
  template <typename C>
  static two test(...);

 public:
  enum { value = sizeof(test<T>(0)) == sizeof(char) };
};

template <typename T>
struct JitStructHelper {
  static constexpr bool _jit_struct = false;
  static FieldJitAccessBuilderTable& GetFieldJitAccessBuilderTable() {
    static FieldJitAccessBuilderTable accessors;
    return accessors;
  }
  template <typename fake = void>
  static void InitJitBuilder() {}
};

template <typename T>
ValueAccessor GetFieldAccessors(const std::vector<std::string>& names,
                                std::shared_ptr<Xbyak::CodeGenerator> jit = nullptr) {
  ValueAccessor accessor(jit);
  accessor.BeginBuild();

  FieldJitAccessBuilderTable* table = &(T::GetFieldJitAccessBuilderTable());
  for (size_t i = 0; i < names.size(); i++) {
    auto found = table->find(names[i]);
    if (found == table->end()) {
      return accessor;
    } else {
      if (i == names.size() - 1) {
        try {
          FieldJitAccessBuilder builder = std::get<FieldJitAccessBuilder>(found->second);
          builder(accessor.GetJit());
          accessor.EndBuild();
          return accessor;
        } catch (const std::bad_variant_access&) {
          return accessor;
        }
      } else {
        try {
          std::pair<FieldJitAccessBuilder, FieldJitAccessBuilderTable>* builder =
              std::get_if<std::pair<FieldJitAccessBuilder, FieldJitAccessBuilderTable>>(
                  &found->second);
          if (nullptr == builder) {
            return accessor;
          }
          builder->first(accessor.GetJit());
          table = &(builder->second);
        } catch (const std::bad_variant_access&) {
          return accessor;
        }
      }
    }
  }
  return accessor;
}

}  // namespace ssexpr2
#define STRUCT_FILED_OFFSETOF(TYPE, ELEMENT) ((size_t) & (((TYPE*)0)->ELEMENT))
#define JIT_STRUCT_EAT(...)
#define JIT_STRUCT_STRIP(x) JIT_STRUCT_EAT x  // STRIP((int) x) => EAT(int) x => x

#define JIT_STRUCT_MEMBER_INIT(r, data, i, elem) \
  static FieldInit<i, void> __init__##i(GetFieldJitAccessBuilderTable());
#define DEFINE_JIT_STRUCT_MEMBER(r, data, i, elem)                                                 \
  BOOST_PP_REMOVE_PARENS(elem) = {};                                                               \
  template <typename fake>                                                                         \
  struct FieldInit<i, fake> {                                                                      \
    FieldInit(ssexpr2::FieldJitAccessBuilderTable& builders) {                                     \
      __CurrentDataType* vv = nullptr;                                                             \
      using FT = decltype(vv->JIT_STRUCT_STRIP(elem));                                             \
      using FT2 = typename std::remove_const<typename std::remove_pointer<FT>::type>::type;        \
      ssexpr2::FieldJitAccessBuilder builder;                                                      \
      ssexpr2::ValueType vtype;                                                                    \
      if constexpr (std::is_same<FT2, char>::value) {                                              \
        vtype = ssexpr2::V_CHAR;                                                                   \
      } else if constexpr (std::is_same<FT2, bool>::value) {                                       \
        vtype = ssexpr2::V_BOOL;                                                                   \
      } else if constexpr (std::is_same<FT2, uint8_t>::value) {                                    \
        vtype = ssexpr2::V_UINT8;                                                                  \
      } else if constexpr (std::is_same<FT2, int16_t>::value) {                                    \
        vtype = ssexpr2::V_INT16;                                                                  \
      } else if constexpr (std::is_same<FT2, uint16_t>::value) {                                   \
        vtype = ssexpr2::V_UINT16;                                                                 \
      } else if constexpr (std::is_same<FT2, int32_t>::value) {                                    \
        vtype = ssexpr2::V_INT32;                                                                  \
      } else if constexpr (std::is_same<FT2, uint32_t>::value) {                                   \
        vtype = ssexpr2::V_UINT32;                                                                 \
      } else if constexpr (std::is_same<FT2, int64_t>::value) {                                    \
        vtype = ssexpr2::V_INT64;                                                                  \
      } else if constexpr (std::is_same<FT2, uint64_t>::value) {                                   \
        vtype = ssexpr2::V_UINT64;                                                                 \
      } else if constexpr (std::is_same<FT2, float>::value) {                                      \
        vtype = ssexpr2::V_FLOAT;                                                                  \
      } else if constexpr (std::is_same<FT2, double>::value) {                                     \
        vtype = ssexpr2::V_DOUBLE;                                                                 \
      } else if constexpr (std::is_same<FT2, std::string>::value) {                                \
        vtype = ssexpr2::V_STD_STRING;                                                             \
      } else if constexpr (std::is_same<FT2, std::string_view>::value) {                           \
        vtype = ssexpr2::V_STD_STRING_VIEW;                                                        \
      } else if constexpr (std::is_same<FT, const char*>::value) {                                 \
        vtype = ssexpr2::V_CSTRING;                                                                \
      } else if constexpr (ssexpr2::HasInitJitBuilder<FT2>::value ||                               \
                           ssexpr2::JitStructHelper<FT2>::_jit_struct) {                           \
        vtype = ssexpr2::V_JIT_STRUCT;                                                             \
      } else {                                                                                     \
        vtype = ssexpr2::V_STRUCT;                                                                 \
      }                                                                                            \
      if constexpr (std::is_pointer<FT>::value) {                                                  \
        builder = [=](Xbyak::CodeGenerator& jit) {                                                 \
          jit.mov(jit.rdx, vtype);                                                                 \
          jit.add(jit.rax, STRUCT_FILED_OFFSETOF(__CurrentDataType, JIT_STRUCT_STRIP(elem)));      \
          jit.mov(jit.rax, jit.ptr[jit.rax]);                                                      \
          jit.mov(jit.rdi, jit.rax);                                                               \
        };                                                                                         \
      } else {                                                                                     \
        builder = [=](Xbyak::CodeGenerator& jit) {                                                 \
          jit.mov(jit.rdx, vtype);                                                                 \
          jit.add(jit.rax, STRUCT_FILED_OFFSETOF(__CurrentDataType, JIT_STRUCT_STRIP(elem)));      \
          jit.mov(jit.rdi, jit.rax);                                                               \
        };                                                                                         \
      }                                                                                            \
      if constexpr (ssexpr2::HasInitJitBuilder<FT2>::value) {                                      \
        FT2::InitJitBuilder();                                                                     \
        builders[BOOST_PP_STRINGIZE(JIT_STRUCT_STRIP(elem))] =                                     \
                     std::pair<ssexpr2::FieldJitAccessBuilder,                                     \
                               ssexpr2::FieldJitAccessBuilderTable>(                               \
                         builder, FT2::GetFieldJitAccessBuilderTable());                           \
      } else if constexpr (ssexpr2::JitStructHelper<FT2>::_jit_struct) {                           \
        ssexpr2::JitStructHelper<FT2>::InitJitBuilder();                                           \
        builders[BOOST_PP_STRINGIZE(JIT_STRUCT_STRIP(elem))] =                                     \
                     std::pair<ssexpr2::FieldJitAccessBuilder,                                     \
                               ssexpr2::FieldJitAccessBuilderTable>(                               \
                         builder, ssexpr2::JitStructHelper<FT2>::GetFieldJitAccessBuilderTable()); \
      } else {                                                                                     \
        builders[BOOST_PP_STRINGIZE(JIT_STRUCT_STRIP(elem))] = builder;                            \
      }                                                                                            \
    }                                                                                              \
  };

#define DEFINE_JIT_STRUCT(st, ...)                                                              \
  struct st {                                                                                   \
    typedef st __CurrentDataType;                                                               \
    template <int N, typename fake = void>                                                      \
    struct FieldInit {};                                                                        \
    static ssexpr2::FieldJitAccessBuilderTable& GetFieldJitAccessBuilderTable() {               \
      static ssexpr2::FieldJitAccessBuilderTable accessors;                                     \
      return accessors;                                                                         \
    }                                                                                           \
    BOOST_PP_SEQ_FOR_EACH_I(DEFINE_JIT_STRUCT_MEMBER, T, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \
    static void InitJitBuilder() {                                                              \
      static bool once = false;                                                                 \
      if (once) {                                                                               \
        return;                                                                                 \
      }                                                                                         \
      once = true;                                                                              \
      BOOST_PP_SEQ_FOR_EACH_I(JIT_STRUCT_MEMBER_INIT, T, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \
    }                                                                                           \
  };
