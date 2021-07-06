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
#include "flatbuffers/flatbuffers.h"

namespace ssexpr {

typedef std::variant<bool, char, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t,
                     float, double, std::string_view, const void*>
    FieldValueVariant;

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

template <typename T>
FieldValue toFieldValue(const T& v) {
  using R = typename std::remove_const<typename std::remove_pointer<T>::type>::type;
  if constexpr (std::is_same<T, double>::value || std::is_same<T, float>::value ||
                std::is_same<T, char>::value || std::is_same<T, uint8_t>::value ||
                std::is_same<T, int32_t>::value || std::is_same<T, uint32_t>::value ||
                std::is_same<T, int16_t>::value || std::is_same<T, uint16_t>::value ||
                std::is_same<T, int64_t>::value || std::is_same<T, uint64_t>::value ||
                std::is_same<T, std::string_view>::value || std::is_same<T, bool>::value ||
                std::is_same<T, std::string>::value) {
    return v;
  } else if constexpr (std::is_same<T, const flatbuffers::String*>::value) {
    if (nullptr == v) {
      std::string_view empty;
      return empty;
    }
    std::string_view rv(v->c_str(), v->size());
    return rv;
  } else if constexpr (is_container<R>::value || std::is_base_of<NoExpr, R>::value) {
    if constexpr (std::is_pointer<T>::value) {
      return v;
    } else {
      return &v;
    }
  } else {
    FieldAccessorTable value;
    if constexpr (HasGetFieldAccessors<R>::value) {
      R::InitExpr();
      value = R::GetFieldAccessorTable();
    } else {
      ExprStructHelper<R>::InitExpr();
      value = ExprStructHelper<R>::GetFieldAccessorTable();
    }
    if constexpr (std::is_pointer<T>::value) {
      return v;
    } else {
      return &v;
    }
  }
}
template <typename T>
bool fillFieldAccessorTable(FieldAccessorTable& v) {
  using R = typename std::remove_const<typename std::remove_pointer<T>::type>::type;
  if constexpr (std::is_same<T, double>::value || std::is_same<T, float>::value ||
                std::is_same<T, char>::value || std::is_same<T, uint8_t>::value ||
                std::is_same<T, int32_t>::value || std::is_same<T, uint32_t>::value ||
                std::is_same<T, int16_t>::value || std::is_same<T, uint16_t>::value ||
                std::is_same<T, int64_t>::value || std::is_same<T, uint64_t>::value ||
                std::is_same<T, std::string_view>::value || std::is_same<T, bool>::value ||
                std::is_same<T, std::string>::value) {
    return false;
  } else if constexpr (is_container<R>::value || std::is_base_of<NoExpr, R>::value) {
    return false;
  } else {
    if constexpr (HasGetFieldAccessors<R>::value) {
      R::InitExpr();
      v = R::GetFieldAccessorTable();
    } else {
      ExprStructHelper<R>::InitExpr();
      v = ExprStructHelper<R>::GetFieldAccessorTable();
    }
    return true;
  }
}

template <typename T>
bool fillHelperFieldAccessorTable(FieldAccessorTable& v) {
  using R = typename std::remove_reference<T>::type;
  using NR = typename std::remove_pointer<R>::type;
  using RR = typename std::remove_const<NR>::type;
  if constexpr (std::is_same<RR, double>::value || std::is_same<T, float>::value ||
                std::is_same<RR, char>::value || std::is_same<T, uint8_t>::value ||
                std::is_same<RR, int32_t>::value || std::is_same<T, uint32_t>::value ||
                std::is_same<RR, int16_t>::value || std::is_same<T, uint16_t>::value ||
                std::is_same<RR, int64_t>::value || std::is_same<T, uint64_t>::value ||
                std::is_same<RR, std::string_view>::value || std::is_same<T, bool>::value ||
                std::is_same<RR, std::string>::value ||
                std::is_same<RR, flatbuffers::String>::value) {
    return false;
  } else {
    ExprStructHelper<RR>::InitExpr();
    v = ExprStructHelper<RR>::GetFieldAccessorTable();
    return true;
  }
}

}  // namespace ssexpr

#define FIELD_EACH(N, i, arg)                                                                    \
  PAIR(arg){};                                                                                   \
  template <typename fake>                                                                       \
  struct FieldInit<i, fake> {                                                                    \
    FieldInit(ssexpr::FieldAccessorTable& accessors) {                                           \
      __CurrentDataType* vv = nullptr;                                                           \
      using DT = decltype(vv->STRIP(arg));                                                       \
      using R = typename std::remove_const<typename std::remove_pointer<DT>::type>::type;        \
      ssexpr::FieldAccessor field_accessor = [](const void* v) -> ssexpr::FieldValue {           \
        const __CurrentDataType* vdata = (const __CurrentDataType*)v;                            \
        return ssexpr::toFieldValue(vdata->STRIP(arg));                                          \
      };                                                                                         \
      ssexpr::FieldAccessorTable value;                                                          \
      if (ssexpr::fillFieldAccessorTable<DT>(value)) {                                           \
        accessors[STRING(STRIP(arg))] =                                                          \
            std::pair<ssexpr::FieldAccessor, ssexpr::FieldAccessorTable>(field_accessor, value); \
      } else {                                                                                   \
        accessors[STRING(STRIP(arg))] = field_accessor;                                          \
      }                                                                                          \
    }                                                                                            \
  };

#define FIELD_EACH_INIT(N, i, arg) static FieldInit<i, void> __init__##N(GetFieldAccessorTable());

#define DEFINE_EXPR_STRUCT(st, ...)                                                            \
  struct st {                                                                                  \
    typedef st __CurrentDataType;                                                              \
    template <int N, typename fake>                                                            \
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
