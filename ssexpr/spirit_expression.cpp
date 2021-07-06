// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "spirit_expression.h"
#include <boost/config/warning_disable.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/numeric/int.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>

#include <iostream>
#include <list>
#include <sstream>

namespace ssexpr {

namespace ast {
namespace fusion = boost::fusion;
namespace x3 = boost::spirit::x3;

struct Nil {};
struct Unary;
struct Expression;
struct FuncCall;
struct CondExpr;

struct Variable : x3::position_tagged {
  std::vector<std::string> v;

  std::vector<ssexpr::FieldAccessor> accessor_;
};

struct DynamicVariable : x3::position_tagged {
  std::vector<std::string> v;
};

struct Operand
    : public x3::variant<Nil, int64_t, double, bool, std::string, Variable, DynamicVariable,
                         x3::forward_ast<FuncCall>, x3::forward_ast<Unary>,
                         x3::forward_ast<CondExpr>, x3::forward_ast<Expression> > {
  using base_type::base_type;
  using base_type::operator=;
};

enum Optoken {
  op_plus,
  op_minus,
  op_times,
  op_divide,
  op_positive,
  op_negative,
  op_not,
  op_equal,
  op_not_equal,
  op_less,
  op_less_equal,
  op_greater,
  op_greater_equal,
  op_and,
  op_or
};

struct Unary {
  Optoken operator_;
  Operand operand_;
};

struct CondExpr : x3::position_tagged {
  Operand lhs;
  Operand rhs_true;
  Operand rhs_false;
};

struct Operation : x3::position_tagged {
  Optoken operator_;
  Operand operand_;
};

struct FuncCall : x3::position_tagged {
  std::string func;
  std::vector<Operand> args;

  ExprFunction functor_;
};

struct Expression : public x3::position_tagged, Expr {
  Operand first;
  std::vector<Operation> rest;
};
}  // namespace ast
}  // namespace ssexpr
BOOST_FUSION_ADAPT_STRUCT(ssexpr::ast::Unary, operator_, operand_)
BOOST_FUSION_ADAPT_STRUCT(ssexpr::ast::Operation, operator_, operand_)
BOOST_FUSION_ADAPT_STRUCT(ssexpr::ast::Expression, first, rest)
BOOST_FUSION_ADAPT_STRUCT(ssexpr::ast::Variable, v)
BOOST_FUSION_ADAPT_STRUCT(ssexpr::ast::DynamicVariable, v)
BOOST_FUSION_ADAPT_STRUCT(ssexpr::ast::FuncCall, func, args)
BOOST_FUSION_ADAPT_STRUCT(ssexpr::ast::CondExpr, lhs, rhs_true, rhs_false)

namespace ssexpr {
namespace parser {
namespace x3 = boost::spirit::x3;
using x3::alnum;
using x3::alpha;
using x3::bool_;
using x3::char_;
using x3::double_;
using x3::int64;
using x3::lexeme;
using x3::raw;
using x3::uint64;
using x3::uint_;
using x3::ulong_;

using namespace x3::ascii;

template <typename Iterator>
using error_handler = x3::error_handler<Iterator>;

// tag used to get our error handler from the context
using error_handler_tag = x3::error_handler_tag;

struct error_handler_base {
  template <typename Iterator, typename Exception, typename Context>
  x3::error_handler_result on_error(Iterator& first, Iterator const& last, Exception const& x,
                                    Context const& context) {
    std::string message = "Error! Expecting: " + x.which() + " here:";
    auto& error_handler = x3::get<error_handler_tag>(context).get();
    error_handler(x.where(), message);
    return x3::error_handler_result::fail;
  }
};

typedef std::string::const_iterator iterator_type;
typedef x3::phrase_parse_context<x3::ascii::space_type>::type phrase_context_type;
typedef error_handler<iterator_type> error_handler_type;

typedef x3::context<error_handler_tag, std::reference_wrapper<error_handler_type>,
                    phrase_context_type>
    context_type;

////////////////////////////////////////////////////////////////////////////
// Tokens
////////////////////////////////////////////////////////////////////////////

x3::symbols<ast::Optoken> equality_op;
x3::symbols<ast::Optoken> relational_op;
x3::symbols<ast::Optoken> logical_op;
x3::symbols<ast::Optoken> additive_op;
x3::symbols<ast::Optoken> multiplicative_op;
x3::symbols<ast::Optoken> unary_op;
// x3::symbols<> keywords;

void add_keywords() {
  static bool once = false;
  if (once) return;
  once = true;

  logical_op.add("&&", ast::op_and)("||", ast::op_or);

  equality_op.add("==", ast::op_equal)("!=", ast::op_not_equal);

  relational_op.add("<", ast::op_less)("<=", ast::op_less_equal)(">", ast::op_greater)(
      ">=", ast::op_greater_equal);

  additive_op.add("+", ast::op_plus)("-", ast::op_minus);

  multiplicative_op.add("*", ast::op_times)("/", ast::op_divide);

  unary_op.add("+", ast::op_positive)("-", ast::op_negative)("!", ast::op_not);

  //   keywords.add("var")("true")("false")("if")("else")("while");
}

////////////////////////////////////////////////////////////////////////////
// Main expression grammar
////////////////////////////////////////////////////////////////////////////
struct expression_class;
typedef x3::rule<expression_class, ast::Operand> expression_type;
BOOST_SPIRIT_DECLARE(expression_type);

struct equality_expr_class;
struct relational_expr_class;
struct logical_expr_class;
struct additive_expr_class;
struct multiplicative_expr_class;
struct unary_expr_class;
struct primary_expr_class;
struct var_class;
struct dynamic_var_class;
struct func_class;
struct cond_expr_class;

typedef x3::rule<equality_expr_class, ast::Expression> equality_expr_type;
typedef x3::rule<relational_expr_class, ast::Expression> relational_expr_type;
typedef x3::rule<logical_expr_class, ast::Expression> logical_expr_type;
typedef x3::rule<additive_expr_class, ast::Expression> additive_expr_type;
typedef x3::rule<multiplicative_expr_class, ast::Expression> multiplicative_expr_type;
typedef x3::rule<unary_expr_class, ast::Operand> unary_expr_type;
typedef x3::rule<primary_expr_class, ast::Operand> primary_expr_type;
typedef x3::rule<var_class, ast::Variable> var_type;
typedef x3::rule<dynamic_var_class, ast::DynamicVariable> dynamic_var_type;
typedef x3::rule<func_class, ast::FuncCall> func_type;
typedef x3::rule<cond_expr_class, ast::Operand> cond_expr_type;

x3::rule<class identifier_rule_, std::string> const identifier_rule = "identifier_rule";
auto const identifier_rule_def =
    x3::lexeme[(x3::alpha | x3::char_('_')) >> *(x3::alnum | x3::char_('_'))];
BOOST_SPIRIT_DEFINE(identifier_rule)

// struct var_class;
// typedef x3::rule<var_class, std::string> var_type;
var_type const var = "var";
auto const var_def = identifier_rule_def % '.';
BOOST_SPIRIT_DEFINE(var);

dynamic_var_type const dynamic_var = "dynamic_var";
auto const dynamic_var_def = '$' >> identifier_rule_def % '.';
BOOST_SPIRIT_DEFINE(dynamic_var);

cond_expr_type const cond_expr = "cond_expr";
expression_type const expression = "expression";
equality_expr_type const equality_expr = "equality_expr";
relational_expr_type const relational_expr = "relational_expr";
logical_expr_type const logical_expr = "logical_expr";
additive_expr_type const additive_expr = "additive_expr";
multiplicative_expr_type const multiplicative_expr = "multiplicative_expr";
unary_expr_type const unary_expr = "unary_expr";
primary_expr_type const primary_expr = "primary_expr";
func_type const func = "func";

auto make_conditional_op = [](auto& ctx) {
  using boost::fusion::at_c;
  ast::CondExpr n;
  n.lhs = x3::_val(ctx);
  n.rhs_true = at_c<0>(x3::_attr(ctx));
  n.rhs_false = at_c<1>(x3::_attr(ctx));
  x3::_val(ctx) = n;
  // x3::_val(ctx) =
  //     ssexpr::ast::CondExpr{x3::_val(ctx), at_c<0>(x3::_attr(ctx)), at_c<1>(x3::_attr(ctx))};
};

auto const cond_expr_def = logical_expr[([](auto& ctx) { _val(ctx) = _attr(ctx); })] >>
                           -('?' > expression > ':' > expression)[make_conditional_op];

auto const logical_expr_def = equality_expr >> *(logical_op > equality_expr);

auto const equality_expr_def = relational_expr >> *(equality_op > relational_expr);

auto const relational_expr_def = additive_expr >> *(relational_op > additive_expr);

auto const additive_expr_def = multiplicative_expr >> *(additive_op > multiplicative_expr);

auto const multiplicative_expr_def = unary_expr >> *(multiplicative_op > unary_expr);

auto const unary_expr_def = primary_expr | (unary_op > primary_expr);
auto const quoted_string = lexeme['"' >> *(char_ - '"') >> '"'];

auto const func_def = identifier_rule >> '(' >> -(expression % ',') >> ')';

boost::spirit::x3::real_parser<double, boost::spirit::x3::strict_real_policies<double> > const
    strict_double_ = {};
const auto int_or_double = strict_double_ | boost::spirit::x3::int64;
auto const primary_expr_def =
    int_or_double | bool_ | quoted_string | func | dynamic_var | var | '(' > expression > ')';

auto const expression_def = cond_expr;

BOOST_SPIRIT_DEFINE(expression, cond_expr, logical_expr, equality_expr, relational_expr,
                    additive_expr, multiplicative_expr, unary_expr, func, primary_expr);

struct unary_expr_class : x3::annotate_on_success {};
struct primary_expr_class : x3::annotate_on_success {};
// struct var_class : x3::annotate_on_success {};

BOOST_SPIRIT_INSTANTIATE(expression_type, iterator_type, context_type);

expression_type const& get_expression() {
  add_keywords();
  return expression;
}
}  // namespace parser
}  // namespace ssexpr

namespace ssexpr {
namespace ast {

struct CalcVisitor {
  Optoken op;
  template <typename T, typename R>
  Value operator()(const T& left, const R& right) const {
    Value result;
    if constexpr (std::is_same<decltype(left), Error>::value) {
      result = left;
      return result;
    }
    if constexpr (std::is_same<decltype(right), Error>::value) {
      result = right;
      return result;
    }
    switch (op) {
      case op_plus: {
        if constexpr ((std::is_same<decltype(left), const int64_t&>::value &&
                       (std::is_same<decltype(right), const int64_t&>::value ||
                        std::is_same<decltype(right), const double&>::value)) ||
                      (std::is_same<decltype(left), const double&>::value &&
                       (std::is_same<decltype(right), const double&>::value ||
                        std::is_same<decltype(right), const int64_t&>::value))) {
          result = (left + right);
        }
        break;
      }
      case op_minus: {
        if constexpr ((std::is_same<decltype(left), const int64_t&>::value &&
                       (std::is_same<decltype(right), const int64_t&>::value ||
                        std::is_same<decltype(right), const double&>::value)) ||
                      (std::is_same<decltype(left), const double&>::value &&
                       (std::is_same<decltype(right), const double&>::value ||
                        std::is_same<decltype(right), const int64_t&>::value))) {
          result = (left - right);
        }
        break;
      }
      case op_times: {
        if constexpr ((std::is_same<decltype(left), const int64_t&>::value &&
                       (std::is_same<decltype(right), const int64_t&>::value ||
                        std::is_same<decltype(right), const double&>::value)) ||
                      (std::is_same<decltype(left), const double&>::value &&
                       (std::is_same<decltype(right), const double&>::value ||
                        std::is_same<decltype(right), const int64_t&>::value))) {
          result = (left * right);
        }
        break;
      }
      case op_divide: {
        if constexpr ((std::is_same<decltype(left), const int64_t&>::value &&
                       (std::is_same<decltype(right), const int64_t&>::value ||
                        std::is_same<decltype(right), const double&>::value)) ||
                      (std::is_same<decltype(left), const double&>::value &&
                       (std::is_same<decltype(right), const double&>::value ||
                        std::is_same<decltype(right), const int64_t&>::value))) {
          result = (left / right);
        }
        break;
      }
      case op_equal: {
        // printf("####equal test %s %s\n", typeid(left).name(), typeid(right).name());
        if constexpr ((std::is_same<decltype(left), const int64_t&>::value &&
                       (std::is_same<decltype(right), const int64_t&>::value ||
                        std::is_same<decltype(right), const double&>::value)) ||
                      (std::is_same<decltype(left), const double&>::value &&
                       (std::is_same<decltype(right), const double&>::value ||
                        std::is_same<decltype(right), const int64_t&>::value)) ||
                      (std::is_same<decltype(left), const std::string_view&>::value &&
                       std::is_same<decltype(right), const std::string_view&>::value)) {
          result = (left == right);
        }
        break;
      }
      case op_not_equal: {
        if constexpr ((std::is_same<decltype(left), const int64_t&>::value &&
                       (std::is_same<decltype(right), const int64_t&>::value ||
                        std::is_same<decltype(right), const double&>::value)) ||
                      (std::is_same<decltype(left), const double&>::value &&
                       (std::is_same<decltype(right), const double&>::value ||
                        std::is_same<decltype(right), const int64_t&>::value)) ||
                      (std::is_same<decltype(left), const std::string_view&>::value &&
                       std::is_same<decltype(right), const std::string_view&>::value)) {
          result = (left != right);
        }
        break;
      }
      case op_less: {
        if constexpr ((std::is_same<decltype(left), const int64_t&>::value &&
                       (std::is_same<decltype(right), const int64_t&>::value ||
                        std::is_same<decltype(right), const double&>::value)) ||
                      (std::is_same<decltype(left), const double&>::value &&
                       (std::is_same<decltype(right), const double&>::value ||
                        std::is_same<decltype(right), const int64_t&>::value)) ||
                      (std::is_same<decltype(left), const std::string_view&>::value &&
                       std::is_same<decltype(right), const std::string_view&>::value)) {
          result = (left < right);
        }
        break;
      }
      case op_less_equal: {
        if constexpr ((std::is_same<decltype(left), const int64_t&>::value &&
                       (std::is_same<decltype(right), const int64_t&>::value ||
                        std::is_same<decltype(right), const double&>::value)) ||
                      (std::is_same<decltype(left), const double&>::value &&
                       (std::is_same<decltype(right), const double&>::value ||
                        std::is_same<decltype(right), const int64_t&>::value)) ||
                      (std::is_same<decltype(left), const std::string_view&>::value &&
                       std::is_same<decltype(right), const std::string_view&>::value)) {
          result = (left <= right);
        }
        break;
      }
      case op_greater: {
        if constexpr ((std::is_same<decltype(left), const int64_t&>::value &&
                       (std::is_same<decltype(right), const int64_t&>::value ||
                        std::is_same<decltype(right), const double&>::value)) ||
                      (std::is_same<decltype(left), const double&>::value &&
                       (std::is_same<decltype(right), const double&>::value ||
                        std::is_same<decltype(right), const int64_t&>::value)) ||
                      (std::is_same<decltype(left), const std::string_view&>::value &&
                       std::is_same<decltype(right), const std::string_view&>::value)) {
          result = (left > right);
        }
        break;
      }
      case op_greater_equal: {
        if constexpr ((std::is_same<decltype(left), const int64_t&>::value &&
                       (std::is_same<decltype(right), const int64_t&>::value ||
                        std::is_same<decltype(right), const double&>::value)) ||
                      (std::is_same<decltype(left), const double&>::value &&
                       (std::is_same<decltype(right), const double&>::value ||
                        std::is_same<decltype(right), const int64_t&>::value)) ||
                      (std::is_same<decltype(left), const std::string_view&>::value &&
                       std::is_same<decltype(right), const std::string_view&>::value)) {
          result = (left >= right);
        }
        break;
      }
      case op_and: {
        if constexpr (std::is_same<decltype(left), const bool&>::value &&
                      std::is_same<decltype(right), const bool&>::value) {
          result = (left && right);
        }
        break;
      }
      case op_or: {
        if constexpr (std::is_same<decltype(left), const bool&>::value &&
                      std::is_same<decltype(right), const bool&>::value) {
          result = (left || right);
        }
        break;
      }
      default: {
        Error e(ERR_INVALID_OPERATOR, "invalid operator");
        result = e;
        break;
      }
    }
    return result;
  }
};

struct Initializer {
  const ExprOptions& opt_;
  Initializer(const ExprOptions& opt) : opt_(opt) {}
  int operator()(Nil) const { return 0; }
  int operator()(int64_t n) const { return 0; }
  int operator()(bool n) const { return 0; }
  int operator()(double n) const { return 0; }
  int operator()(std::string const& n) const { return 0; }
  int operator()(CondExpr& n) const {
    int rc = boost::apply_visitor(*this, n.lhs);
    if (0 != rc) {
      return rc;
    }
    rc = boost::apply_visitor(*this, n.rhs_true);
    if (0 != rc) {
      return rc;
    }
    rc = boost::apply_visitor(*this, n.rhs_false);
    if (0 != rc) {
      return rc;
    }
    return 0;
  }
  int operator()(Variable& n) const {
    if (!opt_.get_member_access) {
      return ERR_INVALID_STRUCT_MEMBER;
    }
    n.accessor_ = opt_.get_member_access(n.v);
    if (n.accessor_.empty()) {
      return ERR_INVALID_STRUCT_MEMBER;
    }
    return 0;
  }
  int operator()(DynamicVariable& n) const {
    if (!opt_.dynamic_var_access) {
      return ERR_EMPTY_DYNAMIC_VAR_VISITOR;
    }
    return 0;
  }
  int operator()(FuncCall& n) const {
    auto found = opt_.functions.find(n.func);
    if (found == opt_.functions.end()) {
      return ERR_INVALID_FUNCTION;
    }
    n.functor_ = found->second;
    for (auto& operand : n.args) {
      boost::apply_visitor(*this, operand);
    }
    return 0;
  }
  int operator()(Unary& n) const { return boost::apply_visitor(*this, n.operand_); }
  int operator()(Expression& x) const {
    int rc = boost::apply_visitor(*this, x.first);
    if (0 != rc) {
      return rc;
    }
    for (Operation& oper : x.rest) {
      rc = boost::apply_visitor(*this, oper.operand_);
      if (0 != rc) {
        return rc;
      }
    }
    return 0;
  }
};

struct Interpreter {
  EvalContext& ctx_;
  Interpreter(EvalContext& ctx) : ctx_(ctx) {}
  Value operator()(Nil) const {
    Value empty;
    // printf("####Eval empty error\n");
    return empty;
  }
  Value operator()(int64_t n) const {
    Value v;
    v = (int64_t)n;
    return v;
  }
  Value operator()(bool n) const {
    Value v;
    v = n;
    return v;
  }
  Value operator()(double n) const {
    Value v;
    v = n;
    return v;
  }
  Value operator()(std::string const& n) const {
    Value v;
    v = n;
    // printf("####string val %s\n", n.c_str());
    return v;
  }
  Value operator()(CondExpr const& n) const {
    Value test = boost::apply_visitor(*this, n.lhs);
    auto ptest = std::get_if<bool>(&test);
    // printf("####Eval %p \n", ptest);
    if (nullptr != ptest) {
      if (*ptest) {
        return boost::apply_visitor(*this, n.rhs_true);
      } else {
        return boost::apply_visitor(*this, n.rhs_false);
      }
    }
    return test;
  }
  Value operator()(Variable const& n) const {
    Value v;
    if (!ctx_.struct_vistitor) {
      Error e(ERR_EMPTY_STRUCT_VISITOR, "empty struct visitor");
      v = e;
      return v;
    }
    // for (const std::string& nn : n.v) {
    //   printf("####Visit %s\n", nn.c_str());
    // }

    v = ctx_.struct_vistitor(n.accessor_);
    // printf("####Visit value index %d\n", v.index());
    return v;
  }
  Value operator()(DynamicVariable const& n) const {
    if (!ctx_.dynamic_var_access) {
      Error e(ERR_EMPTY_STRUCT_VISITOR, "empty struct visitor");
      Value v = e;
      return v;
    }
    return ctx_.dynamic_var_access(ctx_.dynamic_root, n.v);
  }
  Value operator()(FuncCall const& n) const {
    Value v;
    if (!n.functor_) {
      Error e(ERR_INVALID_FUNCTION, "invalid func name");
      v = e;
      return v;
    }
    std::vector<Value> arg_vals;
    for (auto& operand : n.args) {
      arg_vals.push_back(boost::apply_visitor(*this, operand));
    }
    return n.functor_(arg_vals);
  }
  Value operator()(Unary const& n) const {
    Value v = boost::apply_visitor(*this, n.operand_);
    switch (n.operator_) {
      case op_not: {
        auto pval = std::get_if<bool>(&v);
        if (nullptr == pval) {
          Error e(ERR_INVALID_OPERAND_TYPE, "invalid operaand type");
          v = e;
        } else {
          v = !(*pval);
        }
        break;
      }
      case op_positive: {
        auto pint_val = std::get_if<int64_t>(&v);
        if (nullptr != pint_val) {
          v = *pint_val;
        } else {
          auto pdouble_val = std::get_if<double>(&v);
          if (nullptr != pdouble_val) {
            v = *pdouble_val;
          } else {
            Error e(ERR_INVALID_OPERAND_TYPE, "invalid operaand type");
            v = e;
          }
        }
        break;
      }
      case op_negative: {
        auto pint_val = std::get_if<int64_t>(&v);
        if (nullptr != pint_val) {
          v = 0 - *pint_val;
        } else {
          auto pdouble_val = std::get_if<double>(&v);
          if (nullptr != pdouble_val) {
            v = 0 - *pdouble_val;
          } else {
            Error e(ERR_INVALID_OPERAND_TYPE, "invalid operaand type");
            v = e;
          }
        }
        break;
      }
      default: {
        Error e(ERR_INVALID_OPERATOR, "invalid operator");
        v = e;
        break;
      }
    }
    return v;
  }
  Value operator()(Operation const& x, Value& lhs) const {
    if (x.operator_ == op_and || x.operator_ == op_or) {
      auto pbool = std::get_if<bool>(&lhs);
      if (nullptr != pbool) {
        if (x.operator_ == op_and && !(*pbool)) {
          Value result = false;
          return result;
        }
        if (x.operator_ == op_or && (*pbool)) {
          Value result = true;
          return result;
        }
      }
    }
    Value rhs = boost::apply_visitor(*this, x.operand_);
    CalcVisitor visitor;
    visitor.op = x.operator_;
    // printf("####calc value index %d %d %d\n", x.operator_, lhs.index(), rhs.index());
    Value result = std::visit(visitor, lhs, rhs);
    return result;
  }

  Value operator()(Expression const& x) const {
    Value state = boost::apply_visitor(*this, x.first);
    for (Operation const& oper : x.rest) {
      state = (*this)(oper, state);
    }
    return state;
  }
};
}  // namespace ast
}  // namespace ssexpr

namespace ssexpr {
int SpiritExpression::Init(const std::string& expr, const ExprOptions& options) {
  options_ = options;
  using ssexpr::parser::expression;                            // Our grammar
  ssexpr::ast::Expression* ast = new ssexpr::ast::Expression;  // Our tree

  using boost::spirit::x3::ascii::space;
  std::string::const_iterator iter = expr.begin();
  std::string::const_iterator end = expr.end();

  using boost::spirit::x3::with;
  using ssexpr::parser::error_handler_type;
  std::ostringstream err_stream;
  error_handler_type error_handler(iter, end, err_stream);  // Our error handler
  auto const parser =
      // we pass our error handler to the parser so we can access
      // it later in our on_error and on_sucess handlers
      with<boost::spirit::x3::error_handler_tag>(
          std::ref(error_handler))[ssexpr::parser::get_expression()];
  bool r = phrase_parse(iter, end, parser, space, ast->first);
  if (!r) {
    delete ast;
    return -1;
  }
  ssexpr::ast::Initializer init(options);
  int rc = init(*ast);
  if (0 != rc) {
    delete ast;
    return rc;
  }
  expr_.reset(ast);
  return 0;
}
Value SpiritExpression::DoEval(EvalContext& ctx) {
  Error err;
  ssexpr::ast::Expression* ast = (ssexpr::ast::Expression*)(expr_.get());
  if (nullptr == ast) {
    err.code = ERR_NIL_INTERPRETER;
    err.reason = "SpiritExpression is not inited success";
    return err;
  }
  ssexpr::ast::Interpreter interpreter(ctx);
  return interpreter(*ast);
}
}  // namespace ssexpr
