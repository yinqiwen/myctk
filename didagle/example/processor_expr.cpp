// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "didagle/didagle_log.h"
#include "didagle/graph_processor_api.h"
#include "expr.h"
#include "folly/String.h"
#include "spirit_expression.h"

GRAPH_OP_BEGIN(expr_phase)
std::string _cond;  // NOLINT
ssexpr::SpiritExpression _expr;
int OnSetup(const didagle::Params& args) override {
  ssexpr::ExprOptions opt;
  opt.dynamic_var_access = [this](const void* root, const std::vector<std::string>& args) -> ssexpr::Value {
    if (args.size() == 1) {
      if (args[0].empty()) {
        return root;
      }
      const bool* bv = GetDataContext().Get<bool>(args[0]);
      if (nullptr != bv) {
        bool rv = *bv;
        return rv;
      }
    }

    const didagle::Params* dynamic_vars = (const didagle::Params*)root;
    for (const std::string& name : args) {
      dynamic_vars = &((*dynamic_vars)[name]);
    }
    ssexpr::Value r;
    if (dynamic_vars->IsInt()) {
      r = dynamic_vars->Int();
    } else if (dynamic_vars->IsDouble()) {
      r = dynamic_vars->Double();
    } else if (dynamic_vars->IsBool()) {
      r = dynamic_vars->Bool();
      DIDAGLE_DEBUG("####{} {}", args[0], dynamic_vars->Bool());
    } else if (dynamic_vars->IsString()) {
      r = dynamic_vars->String();
    } else {
      if (dynamic_vars->Valid()) {
        r = reinterpret_cast<const void*>(dynamic_vars);
      } else {
        bool rv = false;
        r = rv;
        std::string full_args;
        folly::join(".", args, full_args);
        DIDAGLE_ERROR("Param:{} is not exist, use 'false' as param value.", full_args);
      }
    }
    return r;
  };
  opt.functions["has_param"] = [](const std::vector<ssexpr::Value>& args) {
    ssexpr::Value r;
    if (args.size() < 2) {
      r = false;
      return r;
    }
    try {
      const void* v = std::get<const void*>(args[0]);
      if (nullptr == v) {
        r = false;
        return r;
      }
      const didagle::Params* params = reinterpret_cast<const didagle::Params*>(v);
      std::string_view param_key = std::get<std::string_view>(args[1]);
      folly::fbstring param_key_str(param_key.data(), param_key.size());
      r = params->Contains(param_key_str);
    } catch (std::exception& e) {
      r = false;
    }
    return r;
  };
  _cond = args.String();
  int rc = _expr.Init(_cond, opt);
  DIDAGLE_DEBUG("expression:{}, init rc:{}", _cond, rc);
  return 0;
}
int OnExecute(const didagle::Params& args) override {
  auto eval_val = _expr.EvalDynamic(args);
  try {
    bool r = std::get<bool>(eval_val);
    DIDAGLE_DEBUG("#####cond:{} eval result:{}", _cond, r);
    return r ? 0 : -1;
  } catch (std::exception& e) {
    auto err = std::get<ssexpr::Error>(eval_val);
    DIDAGLE_DEBUG("cond:{} eval exception:{} with index:{}/{}", _cond, e.what(), err.code, err.reason);
    return -1;
  }
}
GRAPH_OP_END
