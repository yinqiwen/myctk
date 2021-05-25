// Created on 2021/03/21
// Authors: yinqiwen (yinqiwen@gmail.com)
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include "expr_macro.h"

namespace ssexpr {

typedef std::variant<bool, char, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t,
                     float, double, std::string_view, const void*>
    FieldValueVariant;

// union FieldValueUnion {
//   bool bv;
//   char cv;
//   int16_t i16;
//   int32_t i32;
//   int64_t i64;
//   float fv;
//   double dv;
//   const char* sv;
//   void* data;
//   inline FieldValueUnion() : data(nullptr) {}
//   inline FieldValueUnion(bool v) : bv(v) {}
//   inline FieldValueUnion(char v) : cv(v) {}
//   inline FieldValueUnion(int16_t v) : i16(v) {}
//   inline FieldValueUnion(int32_t v) : i32(v) {}
//   inline FieldValueUnion(int64_t v) : i64(v) {}
//   inline FieldValueUnion(float v) : fv(v) {}
//   inline FieldValueUnion(double v) : dv(v) {}
//   inline FieldValueUnion(const std::string& v) : sv(v.c_str()) {}
//   inline FieldValueUnion(void* v) : data(v) {}

//   template <typename T>
//   T Get() {
//     return (T)data;
//   }
// };

// template <>
// double FieldValueUnion::Get() {
//   return dv;
// }
// template <>
// float FieldValueUnion::Get() {
//   return fv;
// }
// template <>
// int16_t FieldValueUnion::Get() {
//   return i16;
// }
// template <>
// int32_t FieldValueUnion::Get() {
//   return i32;
// }
// template <>
// int64_t FieldValueUnion::Get() {
//   return i64;
// }
// template <>
// std::string FieldValueUnion::Get() {
//   return sv;
// }
// template <>
// void* FieldValueUnion::Get() {
//   return data;
// }
// typedef FieldValueUnion FieldValue;
// template <typename T, typename R>
// inline T GetValue(R& v) {
//   return v.template Get<T>();
// }

typedef FieldValueVariant FieldValue;
template <typename T, typename R>
inline T GetValue(R& v) {
  return std::get<T>(v);
}

typedef std::function<FieldValue(const void*)> FieldAccessor;

struct FieldAccessorTable;
struct FieldAccessorTable
    : public std::map<std::string,
                      std::variant<FieldAccessor, std::pair<FieldAccessor, FieldAccessorTable>>> {};

inline FieldValue GetFieldValue(const void* root, const std::vector<FieldAccessor>& accessors) {
  const void* obj = root;
  FieldValue empty;
  for (size_t i = 0; i < accessors.size(); i++) {
    auto val = accessors[i](obj);
    if (i == accessors.size() - 1) {
      return val;
    } else {
      try {
        const void* data = GetValue<const void*, FieldValue>(val);
        obj = data;
        if (nullptr == obj) {
          break;
        }
      } catch (const std::bad_variant_access&) {
        break;
      }
    }
  }
  return empty;
}

template <typename T>
int GetFieldAccessors(const std::vector<std::string>& names,
                      std::vector<FieldAccessor>& accessors) {
  accessors.clear();
  FieldAccessorTable* table = &(T::GetFieldAccessorTable());
  for (size_t i = 0; i < names.size(); i++) {
    auto found = table->find(names[i]);
    if (found == table->end()) {
      return -1;
    } else {
      if (i == names.size() - 1) {
        try {
          FieldAccessor accessor = std::get<FieldAccessor>(found->second);
          accessors.emplace_back(accessor);
        } catch (const std::bad_variant_access&) {
          return -1;
        }
      } else {
        try {
          std::pair<FieldAccessor, FieldAccessorTable>* accessor =
              std::get_if<std::pair<FieldAccessor, FieldAccessorTable>>(&found->second);
          if (nullptr == accessor) {
            return -1;
          }
          accessors.emplace_back(accessor->first);
          table = &(accessor->second);
        } catch (const std::bad_variant_access&) {
          return -1;
        }
      }
    }
  }
  return 0;
}

template <typename T>
struct ExprStructHelper {
  static FieldAccessorTable& GetFieldAccessorTable() {
    static FieldAccessorTable accessors;
    return accessors;
  }
  static int GetFieldAccessors(const std::vector<std::string>& names,
                               std::vector<FieldAccessor>& accessors) {
    using HELPER_TYPE = ExprStructHelper<T>;
    return ssexpr::GetFieldAccessors<HELPER_TYPE>(names, accessors);
  }
  template <typename fake = void>
  static int InitExpr();
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

template <typename T>
struct HasBeginEnd {
  template <typename C>
  static char (
      &f(typename std::enable_if<
          std::is_same<decltype(static_cast<typename C::const_iterator (C::*)() const>(&C::begin)),
                       typename C::const_iterator (C::*)() const>::value,
          void>::type*))[1];

  template <typename C>
  static char (&f(...))[2];

  template <typename C>
  static char (
      &g(typename std::enable_if<
          std::is_same<decltype(static_cast<typename C::const_iterator (C::*)() const>(&C::end)),
                       typename C::const_iterator (C::*)() const>::value,
          void>::type*))[1];

  template <typename C>
  static char (&g(...))[2];

  static bool const beg_value = sizeof(f<T>(0)) == 1;
  static bool const end_value = sizeof(g<T>(0)) == 1;
};

// SFINAE test
template <typename T>
class HasGetFieldAccessors {
  typedef char one;
  struct two {
    char x[2];
  };

  template <typename C>
  static one test(decltype(&C::GetFieldAccessors));
  template <typename C>
  static two test(...);

 public:
  enum { value = sizeof(test<T>(0)) == sizeof(char) };
};

template <typename T>
struct is_container
    : std::integral_constant<bool, HasConstIterator<T>::value && HasBeginEnd<T>::beg_value &&
                                       HasBeginEnd<T>::end_value> {};

struct NoExpr {
  virtual ~NoExpr() {}
};
}  // namespace ssexpr

#define FIELD_EACH(N, i, arg)                                                                     \
  PAIR(arg){};                                                                                    \
  template <typename fake>                                                                        \
  struct FieldInit<i, fake> {                                                                     \
    FieldInit(ssexpr::FieldAccessorTable& accessors) {                                            \
      __CurrentDataType* vv = nullptr;                                                            \
      using DT = decltype(vv->STRIP(arg));                                                        \
      using R = typename std::remove_const<typename std::remove_pointer<DT>::type>::type;         \
      if constexpr (std::is_same<DT, double>::value || std::is_same<DT, float>::value ||          \
                    std::is_same<DT, char>::value || std::is_same<DT, uint8_t>::value ||          \
                    std::is_same<DT, int32_t>::value || std::is_same<DT, uint32_t>::value ||      \
                    std::is_same<DT, int16_t>::value || std::is_same<DT, uint16_t>::value ||      \
                    std::is_same<DT, int64_t>::value || std::is_same<DT, uint64_t>::value ||      \
                    std::is_same<DT, std::string_view>::value || std::is_same<DT, bool>::value || \
                    std::is_same<DT, std::string>::value) {                                       \
        accessors[STRING(STRIP(arg))] = [](const void* v) -> ssexpr::FieldValue {                 \
          const __CurrentDataType* data = (const __CurrentDataType*)v;                            \
          return data->STRIP(arg);                                                                \
        };                                                                                        \
      } else if constexpr (ssexpr::is_container<R>::value ||                                      \
                           std::is_base_of<ssexpr::NoExpr, R>::value) {                           \
        if constexpr (std::is_pointer<DT>::value) {                                               \
          accessors[STRING(STRIP(arg))] = [](const void* v) -> ssexpr::FieldValue {               \
            const __CurrentDataType* data = (const __CurrentDataType*)v;                          \
            return data->STRIP(arg);                                                              \
          };                                                                                      \
        } else {                                                                                  \
          accessors[STRING(STRIP(arg))] = [](const void* v) -> ssexpr::FieldValue {               \
            const __CurrentDataType* data = (const __CurrentDataType*)v;                          \
            return &(data->STRIP(arg));                                                           \
          };                                                                                      \
        }                                                                                         \
      } else {                                                                                    \
        ssexpr::FieldAccessorTable value;                                                         \
        if constexpr (ssexpr::HasGetFieldAccessors<R>::value) {                                   \
          R::InitExpr();                                                                          \
          value = R::GetFieldAccessorTable();                                                     \
        } else {                                                                                  \
          ssexpr::ExprStructHelper<R>::InitExpr();                                                \
          value = ssexpr::ExprStructHelper<R>::GetFieldAccessorTable();                           \
        }                                                                                         \
        ssexpr::FieldAccessor field_accessor;                                                     \
        if constexpr (std::is_pointer<DT>::value) {                                               \
          field_accessor = [](const void* v) -> ssexpr::FieldValue {                              \
            const __CurrentDataType* data = (const __CurrentDataType*)v;                          \
            return data->STRIP(arg);                                                              \
          };                                                                                      \
        } else {                                                                                  \
          field_accessor = [](const void* v) -> ssexpr::FieldValue {                              \
            const __CurrentDataType* data = (const __CurrentDataType*)v;                          \
            return &(data->STRIP(arg));                                                           \
          };                                                                                      \
        }                                                                                         \
        accessors[STRING(STRIP(arg))] =                                                           \
            std::pair<ssexpr::FieldAccessor, ssexpr::FieldAccessorTable>(field_accessor, value);  \
      }                                                                                           \
    }                                                                                             \
  };

#define FIELD_EACH_INIT(N, i, arg) static FieldInit<i, void> __init__##N(GetFieldAccessorTable());

#define DEFINE_EXPR_STRUCT(st, ...)                                                            \
  struct st {                                                                                  \
    typedef st __CurrentDataType;                                                              \
    template <int N, typename fake = void>                                                     \
    struct FieldInit {};                                                                       \
    static ssexpr::FieldAccessorTable& GetFieldAccessorTable() {                               \
      static ssexpr::FieldAccessorTable accessors;                                             \
      return accessors;                                                                        \
    }                                                                                          \
    static int GetFieldAccessors(const std::vector<std::string>& names,                        \
                                 std::vector<ssexpr::FieldAccessor>& accessors) {              \
      return ssexpr::GetFieldAccessors<st>(names, accessors);                                  \
    }                                                                                          \
    inline ssexpr::FieldValue GetFieldValue(                                                   \
        const std::vector<ssexpr::FieldAccessor>& accessors) const {                           \
      return ssexpr::GetFieldValue(this, accessors);                                           \
    }                                                                                          \
    static constexpr size_t _field_count_ = GET_ARG_COUNT(__VA_ARGS__);                        \
    PASTE(REPEAT_, GET_ARG_COUNT(__VA_ARGS__))                                                 \
    (FIELD_EACH, 0, __VA_ARGS__)                                                               \
                                                                                               \
        static void InitExpr() {                                                               \
      static bool once = false;                                                                \
      if (once) {                                                                              \
        return;                                                                                \
      }                                                                                        \
      PASTE(REPEAT_, GET_ARG_COUNT(__VA_ARGS__))(FIELD_EACH_INIT, 0, __VA_ARGS__) once = true; \
      return;                                                                                  \
    }                                                                                          \
  };
