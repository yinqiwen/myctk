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
//     return ssexpr::GetFieldAccessors<HELPER_TYPE>(names, accessors);
//   }
//   template <typename fake = void>
//   static int InitExpr();
// };
// }  // namespace expr_struct

#define PB_FIELD_EACH_INIT(N, i, arg)                                                          \
  {                                                                                            \
    using FIELD_TYPE = decltype(__root_obj->arg());                                            \
    ssexpr::FieldAccessorTable& accessors = PB_HELPER_TYPE::GetFieldAccessorTable();           \
    ssexpr::FieldAccessor field_accessor = [](const void* v) -> ssexpr::FieldValue {           \
      const PB_TYPE* data = (const PB_TYPE*)v;                                                 \
      return ssexpr::toFieldValue(data->arg());                                                \
    };                                                                                         \
    ssexpr::FieldAccessorTable value;                                                          \
    if (ssexpr::fillHelperFieldAccessorTable<FIELD_TYPE>(value)) {                             \
      accessors[STRING(arg)] =                                                                 \
          std::pair<ssexpr::FieldAccessor, ssexpr::FieldAccessorTable>(field_accessor, value); \
    } else {                                                                                   \
      accessors[STRING(arg)] = field_accessor;                                                 \
    }                                                                                          \
  }

#define DEFINE_EXPR_STRUCT_HELPER(pb, ...)       \
  namespace ssexpr {                             \
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
