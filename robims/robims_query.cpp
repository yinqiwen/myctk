/*
 *Copyright (c) 2021, qiyingwang <qiyingwang@tencent.com>
 *All rights reserved.
 *
 *Redistribution and use in source and binary forms, with or without
 *modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of rimos nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 *THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "robims_query.h"
#include <iostream>
#include <list>
#include <memory>
#include <sstream>

#include <boost/config/warning_disable.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/numeric/int.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>

#include "folly/Demangle.h"

#include "robims_cache.h"
#include "robims_db_impl.h"
#include "robims_err.h"
#include "robims_log.h"

namespace robims {
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
  RobimsField* field = nullptr;
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
  //   op_not,
  op_equal,
  op_not_equal,
  op_less,
  op_less_equal,
  op_greater,
  op_greater_equal,
  op_and,
  op_or,
  op_and_not,
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

  // ExprFunction functor_;
};

struct Expression : public x3::position_tagged, Expr {
  Operand first;
  std::vector<Operation> rest;
};
}  // namespace ast
}  // namespace robims
BOOST_FUSION_ADAPT_STRUCT(robims::ast::Unary, operator_, operand_)
BOOST_FUSION_ADAPT_STRUCT(robims::ast::Operation, operator_, operand_)
BOOST_FUSION_ADAPT_STRUCT(robims::ast::Expression, first, rest)
BOOST_FUSION_ADAPT_STRUCT(robims::ast::Variable, v)
BOOST_FUSION_ADAPT_STRUCT(robims::ast::DynamicVariable, v)
BOOST_FUSION_ADAPT_STRUCT(robims::ast::FuncCall, func, args)
BOOST_FUSION_ADAPT_STRUCT(robims::ast::CondExpr, lhs, rhs_true, rhs_false)

namespace robims {
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

  logical_op.add("&&", ast::op_and)("||", ast::op_or)("&&!", ast::op_and_not);

  equality_op.add("==", ast::op_equal)("!=", ast::op_not_equal);

  relational_op.add("<", ast::op_less)("<=", ast::op_less_equal)(">", ast::op_greater)(
      ">=", ast::op_greater_equal);

  additive_op.add("+", ast::op_plus)("-", ast::op_minus);

  multiplicative_op.add("*", ast::op_times)("/", ast::op_divide);

  unary_op.add("+", ast::op_positive)("-", ast::op_negative);

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
}  // namespace robims

namespace robims {
namespace ast {

struct QueryCalc {
  Optoken op;
  template <typename T, typename R>
  RobimsQueryValue operator()(const T& left, const R& right) const {
    RobimsQueryValue result;
    if constexpr (std::is_same<decltype(left), RobimsQueryError>::value) {
      result = left;
      return result;
    }
    if constexpr (std::is_same<decltype(right), RobimsQueryError>::value) {
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
        } else if constexpr (std::is_same<T, RobimsField*>::value) {
          CRoaringBitmapPtr out(acquire_bitmap());
          robims::FieldArg arg;
          if constexpr (std::is_same<decltype(right), const int64_t&>::value ||
                        std::is_same<decltype(right), const double&>::value ||
                        std::is_same<decltype(right), const std::string_view&>::value) {
            arg = right;
          } else {
            RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND,
                               "invalid right operand type for EQ operator");
            result = e;
            return result;
          }
          int rc = left->Select(robims::FIELD_OP_EQ, arg, out);
          if (0 == rc) {
            result = std::move(out);
          } else {
            RobimsQueryError e(rc, "invalid result for EQ operator");
            result = e;
            ROBIMS_ERROR("EQ return error:{}", rc);
          }
        } else if constexpr (std::is_same<R, RobimsField*>::value) {
          CRoaringBitmapPtr out(acquire_bitmap());
          robims::FieldArg arg;
          if constexpr (std::is_same<decltype(left), const int64_t&>::value ||
                        std::is_same<decltype(left), const double&>::value ||
                        std::is_same<decltype(left), const std::string_view&>::value) {
            arg = left;
          } else {
            RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND,
                               "invalid left operand type for EQ operator");
            result = e;
            return result;
          }
          int rc = right->Select(robims::FIELD_OP_EQ, arg, out);
          if (0 == rc) {
            result = std::move(out);
          } else {
            RobimsQueryError e(rc, "invalid result for EQ operator");
            result = e;
          }
        } else {
          RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND, "invalid operand for EQ operator");
          result = e;
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
        } else if constexpr (std::is_same<T, RobimsField*>::value) {
          CRoaringBitmapPtr out(acquire_bitmap());
          robims::FieldArg arg;
          if constexpr (std::is_same<decltype(right), const int64_t&>::value ||
                        std::is_same<decltype(right), const double&>::value ||
                        std::is_same<decltype(right), const std::string_view&>::value) {
            arg = right;
          } else {
            RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND,
                               "invalid right operand type for NEQ operator");
            result = e;
            return result;
          }
          int rc = left->Select(robims::FIELD_OP_NEQ, arg, out);
          if (0 == rc) {
            result = std::move(out);
          } else {
            RobimsQueryError e(rc, "invalid result for NEQ operator");
            result = e;
          }
        } else if constexpr (std::is_same<R, RobimsField*>::value) {
          CRoaringBitmapPtr out(acquire_bitmap());
          robims::FieldArg arg;
          if constexpr (std::is_same<decltype(left), const int64_t&>::value ||
                        std::is_same<decltype(left), const double&>::value ||
                        std::is_same<decltype(left), const std::string_view&>::value) {
            arg = left;
          } else {
            RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND,
                               "invalid left operand type for NEQ operator");
            result = e;
            return result;
          }
          int rc = right->Select(robims::FIELD_OP_NEQ, arg, out);
          if (0 == rc) {
            result = std::move(out);
          } else {
            RobimsQueryError e(rc, "invalid result for NEQ operator");
            result = e;
          }
        } else {
          RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND, "invalid operand for NEQ operator");
          result = e;
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
        } else if constexpr (std::is_same<T, RobimsField*>::value) {
          CRoaringBitmapPtr out(acquire_bitmap());
          robims::FieldArg arg;
          if constexpr (std::is_same<decltype(right), const int64_t&>::value ||
                        std::is_same<decltype(right), const double&>::value) {
            arg = right;
          } else {
            RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND,
                               "invalid right operand type for LT operator");
            result = e;
            return result;
          }
          int rc = left->Select(robims::FIELD_OP_LT, arg, out);
          if (0 == rc) {
            result = std::move(out);
          } else {
            RobimsQueryError e(rc, "invalid result for LT operator");
            result = e;
          }
        } else if constexpr (std::is_same<R, RobimsField*>::value) {
          CRoaringBitmapPtr out(acquire_bitmap());
          robims::FieldArg arg;
          if constexpr (std::is_same<decltype(left), const int64_t&>::value ||
                        std::is_same<decltype(left), const double&>::value) {
            arg = left;
          } else {
            RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND,
                               "invalid left operand type for LT operator");
            result = e;
            return result;
          }
          int rc = right->Select(robims::FIELD_OP_GT, arg, out);
          if (0 == rc) {
            result = std::move(out);
          } else {
            RobimsQueryError e(rc, "invalid result for LT operator");
            result = e;
          }
        } else {
          RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND, "invalid operand for LT operator");
          result = e;
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
        } else if constexpr (std::is_same<T, RobimsField*>::value) {
          CRoaringBitmapPtr out(acquire_bitmap());
          robims::FieldArg arg;
          if constexpr (std::is_same<decltype(right), const int64_t&>::value ||
                        std::is_same<decltype(right), const double&>::value) {
            arg = right;
          } else {
            RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND,
                               "invalid right operand type for LTE operator");
            result = e;
            return result;
          }
          int rc = left->Select(robims::FIELD_OP_LTE, arg, out);
          if (0 == rc) {
            result = std::move(out);
          } else {
            RobimsQueryError e(rc, "invalid result for LTE operator");
            result = e;
          }
        } else if constexpr (std::is_same<R, RobimsField*>::value) {
          CRoaringBitmapPtr out(acquire_bitmap());
          robims::FieldArg arg;
          if constexpr (std::is_same<decltype(left), const int64_t&>::value ||
                        std::is_same<decltype(left), const double&>::value) {
            arg = left;
          } else {
            RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND,
                               "invalid left operand type for LTE operator");
            result = e;
            return result;
          }
          int rc = right->Select(robims::FIELD_OP_GTE, arg, out);
          if (0 == rc) {
            result = std::move(out);
          } else {
            RobimsQueryError e(rc, "invalid result for LTE operator");
            result = e;
          }
        } else {
          RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND, "invalid operand for LTE operator");
          result = e;
        }
        break;
      }
      case op_greater: {
        using LeftA = typename std::remove_reference<decltype(left)>::type;
        using RightA = typename std::remove_reference<decltype(right)>::type;
        if constexpr ((std::is_same<decltype(left), const int64_t&>::value &&
                       (std::is_same<decltype(right), const int64_t&>::value ||
                        std::is_same<decltype(right), const double&>::value)) ||
                      (std::is_same<decltype(left), const double&>::value &&
                       (std::is_same<decltype(right), const double&>::value ||
                        std::is_same<decltype(right), const int64_t&>::value)) ||
                      (std::is_same<decltype(left), const std::string_view&>::value &&
                       std::is_same<decltype(right), const std::string_view&>::value)) {
          result = (left > right);
          break;
        } else if constexpr (std::is_same<T, RobimsField*>::value) {
          CRoaringBitmapPtr out(acquire_bitmap());
          robims::FieldArg arg;
          if constexpr (std::is_same<decltype(right), const int64_t&>::value ||
                        std::is_same<decltype(right), const double&>::value) {
            arg = right;
          } else {
            RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND,
                               "invalid right operand type for GT operator");
            result = e;
            return result;
          }
          int rc = left->Select(robims::FIELD_OP_GT, arg, out);
          if (0 == rc) {
            result = std::move(out);
          } else {
            RobimsQueryError e(rc, "invalid result for GT operator");
            result = e;
          }
          break;
        } else if constexpr (std::is_same<R, RobimsField*>::value) {
          CRoaringBitmapPtr out(acquire_bitmap());
          robims::FieldArg arg;
          if constexpr (std::is_same<decltype(left), const int64_t&>::value ||
                        std::is_same<decltype(left), const double&>::value) {
            arg = left;
          } else {
            RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND,
                               "invalid left operand type for GT operator");
            result = e;
            return result;
          }
          int rc = right->Select(robims::FIELD_OP_LT, arg, out);
          if (0 == rc) {
            result = std::move(out);
          } else {
            RobimsQueryError e(rc, "invalid result for GT operator");
            result = e;
          }
          break;
        } else {
          ROBIMS_ERROR("left type:{}, right type:{}", folly::demangle(typeid(LeftA)),
                       folly::demangle(typeid(RightA)));

          ROBIMS_ERROR("{} {} {} {} {} {}", std::is_const<LeftA>::value,
                       std::is_reference<LeftA>::value, std::is_pointer<LeftA>::value,
                       std::is_const<RightA>::value, std::is_reference<RightA>::value,
                       std::is_pointer<RightA>::value);
          RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND, "invalid operand for GT operator");
          result = e;
          break;
        }
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
        } else if constexpr (std::is_same<T, RobimsField*>::value) {
          CRoaringBitmapPtr out(acquire_bitmap());
          robims::FieldArg arg;
          if constexpr (std::is_same<decltype(right), const int64_t&>::value ||
                        std::is_same<decltype(right), const double&>::value) {
            arg = right;
          } else {
            RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND,
                               "invalid right operand type for GTE operator");
            result = e;
            return result;
          }
          int rc = left->Select(robims::FIELD_OP_GTE, arg, out);
          if (0 == rc) {
            result = std::move(out);
          } else {
            RobimsQueryError e(rc, "invalid result for GTE operator");
            result = e;
          }
        } else if constexpr (std::is_same<R, RobimsField*>::value) {
          CRoaringBitmapPtr out(acquire_bitmap());
          robims::FieldArg arg;
          if constexpr (std::is_same<decltype(left), const int64_t&>::value ||
                        std::is_same<decltype(left), const double&>::value) {
            arg = left;
          } else {
            RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND,
                               "invalid left operand type for GTE operator");
            result = e;
            return result;
          }
          int rc = right->Select(robims::FIELD_OP_LTE, arg, out);
          if (0 == rc) {
            result = std::move(out);
          } else {
            RobimsQueryError e(rc, "invalid result for GTE operator");
            result = e;
          }
        } else {
          RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND, "invalid operand for GTE operator");
          result = e;
        }
        break;
      }
      case op_and: {
        if constexpr (std::is_same<decltype(left), const bool&>::value &&
                      std::is_same<decltype(right), const bool&>::value) {
          result = (left && right);
        } else if constexpr (std::is_same<decltype(left), const CRoaringBitmapPtr&>::value &&
                             std::is_same<decltype(right), const CRoaringBitmapPtr&>::value) {
          CRoaringBitmapPtr& left_bitmap = const_cast<CRoaringBitmapPtr&>(left);
          CRoaringBitmapPtr& right_bitmap = const_cast<CRoaringBitmapPtr&>(right);
          roaring_bitmap_and_inplace(left_bitmap.get(), right_bitmap.get());
          release_bitmap(right_bitmap.release());
          CRoaringBitmapPtr p(left_bitmap.release());
          result = std::move(p);
        } else {
          RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND, "invalid operand for AND operator");
          result = e;
        }
        break;
      }
      case op_or: {
        if constexpr (std::is_same<decltype(left), const bool&>::value &&
                      std::is_same<decltype(right), const bool&>::value) {
          result = (left || right);
        } else if constexpr (std::is_same<decltype(left), const CRoaringBitmapPtr&>::value &&
                             std::is_same<decltype(right), const CRoaringBitmapPtr&>::value) {
          CRoaringBitmapPtr& left_bitmap = const_cast<CRoaringBitmapPtr&>(left);
          CRoaringBitmapPtr& right_bitmap = const_cast<CRoaringBitmapPtr&>(right);
          roaring_bitmap_or_inplace(left_bitmap.get(), right_bitmap.get());
          release_bitmap(right_bitmap.release());
          CRoaringBitmapPtr p(left_bitmap.release());
          result = std::move(p);
        } else {
          RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND, "invalid operand for OR operator");
          result = e;
        }
        break;
      }
      default: {
        RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERATOR, "invalid operator with invalid args");
        result = e;
        break;
      }
    }
    return result;
  }
};

struct Initializer {
  robims::RobimsDBImpl* db;
  Initializer(robims::RobimsDBImpl* d) : db(d) {}
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
    if (n.v.size() != 2) {
      ROBIMS_ERROR("Expected variable with 2 string part but got {}", n.v.size());
      return ROBIMS_QUERY_ERR_INVALID_FIELD_ARGS;
    }
    RobimsTable* table = db->GetTable(n.v[0]);
    if (nullptr == table) {
      ROBIMS_ERROR("Can NOT find table with name:{}", n.v[0]);
      return ROBIMS_QUERY_ERR_INVALID_TABLE_NAME;
    }
    RobimsField* field = table->GetField(n.v[1]);
    if (nullptr == table) {
      ROBIMS_ERROR("Can NOT find field with name:{}", n.v[1]);
      return ROBIMS_QUERY_ERR_INVALID_FIELD_NAME;
    }
    n.field = field;
    return 0;
  }
  int operator()(DynamicVariable& n) const { return 0; }
  int operator()(FuncCall& n) const { return 0; }
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

struct QueryInterpreter {
  //   EvalContext& ctx_;
  //   QueryInterpreter(EvalContext& ctx) : ctx_(ctx) {}
  RobimsQueryValue operator()(Nil) const {
    RobimsQueryValue empty;
    return empty;
  }
  RobimsQueryValue operator()(int64_t n) const {
    RobimsQueryValue v;
    v = (int64_t)n;
    return v;
  }
  RobimsQueryValue operator()(bool n) const {
    RobimsQueryValue v;
    v = n;
    return v;
  }
  RobimsQueryValue operator()(double n) const {
    RobimsQueryValue v;
    v = n;
    return v;
  }
  RobimsQueryValue operator()(std::string const& n) const {
    RobimsQueryValue v;
    v = n;
    return v;
  }
  RobimsQueryValue operator()(CondExpr const& n) const {
    RobimsQueryValue test = boost::apply_visitor(*this, n.lhs);
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
  RobimsQueryValue operator()(Variable const& n) const {
    RobimsQueryValue v;
    if (nullptr == n.field) {
      RobimsQueryError e(ROBIMS_QUERY_ERR_EMPTY_FIELD_INSTANCE, "empty field instance");
      v = e;
      return v;
    }
    v = n.field;
    return v;
  }
  RobimsQueryValue operator()(DynamicVariable const& n) const {
    RobimsQueryValue empty;
    return empty;
  }
  RobimsQueryValue operator()(FuncCall const& n) const {
    RobimsQueryValue empty;
    return empty;
  }
  RobimsQueryValue operator()(Unary const& n) const {
    RobimsQueryValue v = boost::apply_visitor(*this, n.operand_);
    switch (n.operator_) {
      case op_positive: {
        auto pint_val = std::get_if<int64_t>(&v);
        if (nullptr != pint_val) {
          v = *pint_val;
        } else {
          auto pdouble_val = std::get_if<double>(&v);
          if (nullptr != pdouble_val) {
            v = *pdouble_val;
          } else {
            RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND, "invalid operaand type");
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
            RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERAND, "invalid operaand type");
            v = e;
          }
        }
        break;
      }
      default: {
        RobimsQueryError e(ROBIMS_QUERY_ERR_INVALID_OPERATOR, "invalid operator");
        v = e;
        break;
      }
    }
    return v;
  }
  RobimsQueryValue operator()(Operation const& x, RobimsQueryValue& lhs) const {
    RobimsQueryValue rhs = boost::apply_visitor(*this, x.operand_);
    QueryCalc visitor;
    visitor.op = x.operator_;
    ROBIMS_ERROR("visit op:{}, left:{}, right:{}", visitor.op, lhs.index(), rhs.index());
    RobimsQueryValue result = std::visit(visitor, lhs, rhs);
    return result;
  }

  RobimsQueryValue operator()(Expression const& x) const {
    RobimsQueryValue state = boost::apply_visitor(*this, x.first);
    for (Operation const& oper : x.rest) {
      state = (*this)(oper, state);
    }
    return state;
  }
};
}  // namespace ast
}  // namespace robims

namespace robims {
int RobimsQuery::Init(RobimsDBImpl* db, const std::string& query) {
  using robims::parser::expression;                            // Our grammar
  robims::ast::Expression* ast = new robims::ast::Expression;  // Our tree

  using boost::spirit::x3::ascii::space;
  std::string::const_iterator iter = query.begin();
  std::string::const_iterator end = query.end();

  using boost::spirit::x3::with;
  using robims::parser::error_handler_type;
  std::ostringstream err_stream;
  error_handler_type error_handler(iter, end, err_stream);  // Our error handler
  auto const parser =
      // we pass our error handler to the parser so we can access
      // it later in our on_error and on_sucess handlers
      with<boost::spirit::x3::error_handler_tag>(
          std::ref(error_handler))[robims::parser::get_expression()];
  bool r = phrase_parse(iter, end, parser, space, ast->first);
  if (!r) {
    delete ast;
    return -1;
  }
  robims::ast::Initializer init(db);
  int rc = init(*ast);
  if (0 != rc) {
    delete ast;
    return rc;
  }
  expr_.reset(ast);
  return 0;
}
RobimsQueryValue RobimsQuery::Execute(RobimsDBImpl* db) {
  RobimsQueryError err;
  robims::ast::Expression* ast = (robims::ast::Expression*)(expr_.get());
  if (nullptr == ast) {
    err.code = ROBIMS_QUERY_ERR_NIL_INTERPRETER;
    err.reason = "RobimsQuery is not inited success";
    return err;
  }
  robims::ast::QueryInterpreter interpreter;
  return interpreter(*ast);
}
}  // namespace robims