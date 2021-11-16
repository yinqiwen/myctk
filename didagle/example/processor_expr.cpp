// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "didagle_log.h"
#include "expr.h"
#include "graph_processor_api.h"
#include "spirit_expression.h"

GRAPH_OP_BEGIN(expr_phase)
std::string _cond;
ssexpr::SpiritExpression _expr;
int OnSetup(const Params& args) override {
  ssexpr::ExprOptions opt;
  opt.dynamic_var_access = [this](const void* root,
                                  const std::vector<std::string>& args) -> ssexpr::Value {
    if (args.size() == 1) {
      const bool* bv = GetDataContext().Get<bool>(args[0]);
      if (nullptr != bv) {
        bool rv = *bv;
        return rv;
      }
    }

    const Params* dynamic_vars = (const Params*)root;
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
      bool rv = false;
      r = rv;
    }
    return r;
  };
  _cond = args.String();
  int rc = _expr.Init(_cond, opt);
  DIDAGLE_DEBUG("expression:{}, init rc:{}", _cond, rc);
  return rc;
}
int OnExecute(const Params& args) override {
  auto eval_val = _expr.EvalDynamic(args);
  try {
    bool r = std::get<bool>(eval_val);
    DIDAGLE_DEBUG("#####cond:{} eval result:{}", _cond, r);
    return r ? 0 : -1;
  } catch (std::exception& e) {
    auto err = std::get<ssexpr::Error>(eval_val);
    DIDAGLE_DEBUG("cond:{} eval exception:{} with index:{}/{}", _cond, e.what(), err.code,
                  err.reason);
    return -1;
  }
}
GRAPH_OP_END
