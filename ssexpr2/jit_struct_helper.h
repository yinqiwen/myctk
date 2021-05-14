// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/05/11
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once

// #include "flatbuffers/flatbuffers.h"
#include "jit_struct.h"

#define DEFINE_JIT_STRUCT_HELPER_MEMBER_ACCESS_FUNC(r, st, i, elem) \
  static auto elem(const st* p)->decltype(p->elem()) { return p->elem(); }

#define DEFINE_JIT_STRUCT_HELPER_MEMBER_INIT(r, data, i, elem)                                     \
  {                                                                                                \
    using FT = decltype(__root_obj->elem());                                                       \
    using FT2 = typename std::remove_const<typename std::remove_reference<FT>::type>::type;        \
    ssexpr2::FieldJitAccessBuilderTable& builders = HELPER_TYPE::GetFieldJitAccessBuilderTable();  \
    ssexpr2::FieldJitAccessBuilder builder;                                                        \
    ssexpr2::ValueType vtype;                                                                      \
    if constexpr (std::is_same<FT2, char>::value) {                                                \
      vtype = ssexpr2::V_CHAR_VALUE;                                                               \
    } else if constexpr (std::is_same<FT2, bool>::value) {                                         \
      vtype = ssexpr2::V_BOOL_VALUE;                                                               \
    } else if constexpr (std::is_same<FT2, uint8_t>::value) {                                      \
      vtype = ssexpr2::V_UINT8_VALUE;                                                              \
    } else if constexpr (std::is_same<FT2, uint8_t>::value) {                                      \
      vtype = ssexpr2::V_UINT8_VALUE;                                                              \
    } else if constexpr (std::is_same<FT2, int16_t>::value) {                                      \
      vtype = ssexpr2::V_INT16_VALUE;                                                              \
    } else if constexpr (std::is_same<FT2, uint16_t>::value) {                                     \
      vtype = ssexpr2::V_UINT16_VALUE;                                                             \
    } else if constexpr (std::is_same<FT2, int32_t>::value) {                                      \
      vtype = ssexpr2::V_INT32_VALUE;                                                              \
    } else if constexpr (std::is_same<FT2, uint32_t>::value) {                                     \
      vtype = ssexpr2::V_UINT32_VALUE;                                                             \
    } else if constexpr (std::is_same<FT2, int64_t>::value) {                                      \
      vtype = ssexpr2::V_INT64_VALUE;                                                              \
    } else if constexpr (std::is_same<FT2, uint64_t>::value) {                                     \
      vtype = ssexpr2::V_UINT64_VALUE;                                                             \
    } else if constexpr (std::is_same<FT2, float>::value) {                                        \
      vtype = ssexpr2::V_FLOAT_VALUE;                                                              \
    } else if constexpr (std::is_same<FT2, double>::value) {                                       \
      vtype = ssexpr2::V_DOUBLE_VALUE;                                                             \
    } else if constexpr (std::is_same<FT2, std::string>::value) {                                  \
      vtype = ssexpr2::V_STD_STRING;                                                               \
    } else if constexpr (std::is_same<FT2, std::string_view>::value) {                             \
      vtype = ssexpr2::V_STD_STRING_VIEW;                                                          \
    } else if constexpr (std::is_same<FT, const char*>::value) {                                   \
      vtype = ssexpr2::V_CSTRING;                                                                  \
    } else {                                                                                       \
      vtype = ssexpr2::V_JIT_STRUCT;                                                               \
    }                                                                                              \
    builder = [=](Xbyak::CodeGenerator& jit) {                                                     \
      void* tmp = (void*)(&DATA_TYPE::elem);                                                       \
      jit.mov(jit.rax, (size_t)(tmp));                                                             \
      jit.call(jit.rax);                                                                           \
      jit.mov(jit.rdx, vtype);                                                                     \
      if (ssexpr2::V_DOUBLE_VALUE == vtype || ssexpr2::V_FLOAT_VALUE == vtype) {                   \
        jit.movq(jit.rax, jit.xmm0);                                                               \
        jit.movq(jit.rdi, jit.xmm0);                                                               \
      } else {                                                                                     \
        jit.mov(jit.rdi, jit.rax);                                                                 \
      }                                                                                            \
    };                                                                                             \
    if constexpr (std::is_same<FT2, char>::value || std::is_same<FT2, bool>::value ||              \
                  std::is_same<FT2, uint8_t>::value || std::is_same<FT2, int16_t>::value ||        \
                  std::is_same<FT2, uint16_t>::value || std::is_same<FT2, int32_t>::value ||       \
                  std::is_same<FT2, uint32_t>::value || std::is_same<FT2, int64_t>::value ||       \
                  std::is_same<FT2, uint64_t>::value || std::is_same<FT2, float>::value ||         \
                  std::is_same<FT2, double>::value || std::is_same<FT2, std::string>::value ||     \
                  std::is_same<FT2, std::string_view>::value || std::is_same<FT2, int>::value) {   \
      builders[BOOST_PP_STRINGIZE(elem)] = builder;                                                \
    } else {                                                                                       \
      ssexpr2::JitStructHelper<FT2>::InitJitBuilder();                                             \
      builders[BOOST_PP_STRINGIZE(elem)] =                                                         \
                   std::pair<ssexpr2::FieldJitAccessBuilder, ssexpr2::FieldJitAccessBuilderTable>( \
                       builder, ssexpr2::JitStructHelper<FT2>::GetFieldJitAccessBuilderTable());   \
    }                                                                                              \
  }

#define DEFINE_JIT_STRUCT_HELPER(st, ...)                                \
  namespace ssexpr2 {                                                    \
  template <>                                                            \
  struct JitStructHelper<st> {                                           \
    static constexpr bool _jit_struct = true;                            \
    static FieldJitAccessBuilderTable& GetFieldJitAccessBuilderTable() { \
      static FieldJitAccessBuilderTable accessors;                       \
      return accessors;                                                  \
    }                                                                    \
    template <typename fake = void>                                      \
    static void InitJitBuilder() {                                       \
      static bool once = false;                                          \
      if (once) {                                                        \
        return;                                                          \
      }                                                                  \
      once = true;                                                       \
      st* __root_obj = nullptr;                                          \
      using DATA_TYPE = st;                                              \
      using HELPER_TYPE = JitStructHelper<st>;                           \
      BOOST_PP_SEQ_FOR_EACH_I(DEFINE_JIT_STRUCT_HELPER_MEMBER_INIT, _,   \
                              BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))     \
    }                                                                    \
  };                                                                     \
  }
