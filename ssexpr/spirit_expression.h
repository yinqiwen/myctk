// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/03/16
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once

#include <stdint.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include "expr_struct.h"

#define ERR_INVALID_OPERATOR 10001
#define ERR_INVALID_OPERAND_TYPE 10002
#define ERR_INVALID_FUNCTION 10003
#define ERR_INVALID_STRUCT_MEMBER 10004
#define ERR_EMPTY_STRUCT_VISITOR 10005
#define ERR_NIL_INTERPRETER 10006

namespace ssexpr {
struct Expr {
  virtual ~Expr() {}
};
struct Error {
  int code = 0;
  std::string reason;
  Error(int c = -1, const std::string& r = "unknown error") : code(c), reason(r) {}
};
typedef std::variant<Error, bool, int64_t, double, std::string_view> Value;
typedef std::function<Value(const std::vector<Value>&)> ExprFunction;
typedef std::function<std::vector<expr_struct::FieldAccessor>(const std::vector<std::string>&)>
    GetStructMemberAccessFunction;
typedef std::function<Value(const std::vector<expr_struct::FieldAccessor>&)>
    StructMemberVisitFunction;

struct ExprOptions {
  std::map<std::string, ExprFunction> functions;
  GetStructMemberAccessFunction get_member_access;

  template <typename T>
  void Init() {
    T::InitExpr();
    get_member_access = [](const std::vector<std::string>& names) {
      std::vector<expr_struct::FieldAccessor> accessors;
      T::GetFieldAccessors(names, accessors);
      return accessors;
    };
  }
};

struct EvalContext {
  StructMemberVisitFunction struct_vistitor;
};

class SpiritExpression {
 private:
  std::shared_ptr<Expr> expr_;

 public:
  int Init(const std::string& expr, const ExprOptions& options);
  Value Eval(EvalContext& ctx);
  template <typename T>
  Value Eval(EvalContext& ctx, T& root_obj) {
    ctx.struct_vistitor = [&root_obj](const std::vector<expr_struct::FieldAccessor>& accessors) {
      Value v;
      auto field_val = root_obj.GetFieldValue(accessors);
      switch (field_val.index()) {
        case 0: {
          v = std::get<bool>(field_val);
          break;
        }
        case 1: {
          v = (int64_t)(std::get<char>(field_val));
          break;
        }
        case 2: {
          v = (int64_t)(std::get<uint8_t>(field_val));
          break;
        }
        case 3: {
          v = (int64_t)(std::get<int16_t>(field_val));
          break;
        }
        case 4: {
          v = (int64_t)(std::get<uint16_t>(field_val));
          break;
        }
        case 5: {
          v = (int64_t)(std::get<int32_t>(field_val));
          break;
        }
        case 6: {
          v = (int64_t)(std::get<uint32_t>(field_val));
          break;
        }
        case 7: {
          v = std::get<int64_t>(field_val);
          break;
        }
        case 8: {
          v = (int64_t)(std::get<uint64_t>(field_val));
          break;
        }
        case 9: {
          v = (double)(std::get<float>(field_val));
          break;
        }
        case 10: {
          v = std::get<double>(field_val);
          break;
        }
        case 11: {
          v = std::get<std::string_view>(field_val);
          break;
        }
        default: {
          break;
        }
      }
      return v;
    };
    return Eval(ctx);
  }
};
}  // namespace ssexpr