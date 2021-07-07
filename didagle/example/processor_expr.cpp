// Copyright (c) 2020, Tencent Inc.
// All rights reserved.

#include "didagle_log.h"
#include "expr.h"
#include "graph_processor_api.h"
#include "spirit_expression.h"

GRAPH_OP_BEGIN(expr_phase)
GRAPH_OP_INPUT(RecmdEnv, env)
std::string _cond;
ssexpr::SpiritExpression _expr;
int OnSetup(const Params& args) override {
  ssexpr::ExprOptions opt;
  opt.Init<RecmdEnv>();
  _cond = args.String();
  int rc = _expr.Init(_cond, opt);
  DIDAGLE_DEBUG("expression:{}, init rc:{}", _cond, rc);
  return 0;
}
int OnExecute(const Params& args) override {
  DIDAGLE_DEBUG("expip={}", env->expid);
  auto eval_val = _expr.Eval(*env);
  bool r = std::get<bool>(eval_val);
  DIDAGLE_DEBUG("cond:{} eval result:{}", _cond, r);
  return r ? 0 : -1;
}
GRAPH_OP_END
