// Copyright (c) 2021
// All rights reserved.
// Created on 2021/04/11
// Authors: yinqiwen (yinqiwen@gmail.com)
#pragma once
#include "expr_struct.h"
#include "flatbuffers/flatbuffers.h"

// namespace expr_struct {

// template <typename T>
// struct ExprStructHelper {
//   static FieldAccessorTable& GetFieldAccessorTable() {
//     static FieldAccessorTable accessors;
//     return accessors;
//   }
//   static int GetFieldAccessors(const std::vector<std::string>& names,
//                                std::vector<FieldAccessor>& accessors) {
//     using HELPER_TYPE = ExprStructHelper<T>;
//     return expr_struct::GetFieldAccessors<HELPER_TYPE>(names, accessors);
//   }
//   template <typename fake = void>
//   static int InitExpr();
// };
// }  // namespace expr_struct

#define PB_FIELD_EACH_INIT(N, i, arg)                                                              \
  {                                                                                                \
    using FIELD_TYPE = decltype(__root_obj->arg());                                                \
    expr_struct::FieldAccessorTable& accessors = PB_HELPER_TYPE::GetFieldAccessorTable();          \
    if constexpr (std::is_same<FIELD_TYPE, double>::value ||                                       \
                  std::is_same<FIELD_TYPE, float>::value ||                                        \
                  std::is_same<FIELD_TYPE, char>::value ||                                         \
                  std::is_same<FIELD_TYPE, uint8_t>::value ||                                      \
                  std::is_same<FIELD_TYPE, int32_t>::value ||                                      \
                  std::is_same<FIELD_TYPE, uint32_t>::value ||                                     \
                  std::is_same<FIELD_TYPE, int16_t>::value ||                                      \
                  std::is_same<FIELD_TYPE, uint16_t>::value ||                                     \
                  std::is_same<FIELD_TYPE, int64_t>::value ||                                      \
                  std::is_same<FIELD_TYPE, uint64_t>::value ||                                     \
                  std::is_same<FIELD_TYPE, bool>::value ||                                         \
                  std::is_same<FIELD_TYPE, const std::string&>::value) {                           \
      accessors[STRING(arg)] = [](const void* v) -> expr_struct::FieldValue {                      \
        const PB_TYPE* data = (const PB_TYPE*)v;                                                   \
        return data->arg();                                                                        \
      };                                                                                           \
    } else if constexpr (std::is_same<FIELD_TYPE, const flatbuffers::String*>::value) {            \
      accessors[STRING(arg)] = [](const void* v) -> expr_struct::FieldValue {                      \
        const PB_TYPE* data = (const PB_TYPE*)v;                                                   \
        const flatbuffers::String* sv = data->arg();                                               \
        std::string_view rv(sv->c_str(), sv->size());                                              \
        return rv;                                                                                 \
      };                                                                                           \
    } else {                                                                                       \
      using R = typename std::remove_reference<FIELD_TYPE>::type;                                  \
      using NR = typename std::remove_pointer<R>::type;                                            \
      using RR = typename std::remove_const<NR>::type;                                             \
      expr_struct::ExprStructHelper<RR>::InitExpr();                                               \
      expr_struct::FieldAccessorTable value =                                                      \
          expr_struct::ExprStructHelper<RR>::GetFieldAccessorTable();                              \
      if constexpr (std::is_pointer<FIELD_TYPE>::value) {                                          \
        expr_struct::FieldAccessor field_accessor = [](const void* v) -> expr_struct::FieldValue { \
          const PB_TYPE* data = (const PB_TYPE*)v;                                                 \
          FIELD_TYPE fv = data->arg();                                                             \
          return fv;                                                                               \
        };                                                                                         \
        accessors[STRING(arg)] =                                                                   \
            std::pair<expr_struct::FieldAccessor, expr_struct::FieldAccessorTable>(field_accessor, \
                                                                                   value);         \
      } else {                                                                                     \
        expr_struct::FieldAccessor field_accessor = [](const void* v) -> expr_struct::FieldValue { \
          const PB_TYPE* data = (const PB_TYPE*)v;                                                 \
          FIELD_TYPE fv = data->arg();                                                             \
          return &fv;                                                                              \
        };                                                                                         \
        accessors[STRING(arg)] =                                                                   \
            std::pair<expr_struct::FieldAccessor, expr_struct::FieldAccessorTable>(field_accessor, \
                                                                                   value);         \
      }                                                                                            \
    }                                                                                              \
  }

#define DEFINE_EXPR_STRUCT_HELPER(pb, ...)       \
  namespace expr_struct {                        \
  template <>                                    \
  template <typename fake>                       \
  int ExprStructHelper<pb>::InitExpr() {         \
    static bool once = false;                    \
    if (once) {                                  \
      return 0;                                  \
    }                                            \
    once = true;                                 \
    pb* __root_obj = nullptr;                    \
    using PB_TYPE = pb;                          \
    using PB_HELPER_TYPE = ExprStructHelper<pb>; \
    PASTE(REPEAT_, GET_ARG_COUNT(__VA_ARGS__))   \
    (PB_FIELD_EACH_INIT, 0, __VA_ARGS__)   \ 
    return 0;                                    \
  }                                              \
  }
