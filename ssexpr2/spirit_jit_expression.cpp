// Copyright (c) 2020, Tencent Inc.
// All rights reserved.
#include "spirit_jit_expression.h"
#include <boost/config/warning_disable.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/numeric/int.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>

#include <xbyak/xbyak_util.h>
#include <list>
#include <sstream>

#define DEBUG_ASM_OP(op)        \
  do {                          \
    BOOST_PP_REMOVE_PARENS(op); \
  } while (0)

namespace ssexpr2 {

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

  ValueAccessor accessor_;
};

struct Operand
    : x3::variant<Nil, int64_t, double, bool, std::string, Variable, x3::forward_ast<CondExpr>,
                  x3::forward_ast<FuncCall>, x3::forward_ast<Unary>, x3::forward_ast<Expression> > {
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

struct CondExpr {
  Operand lhs;
  Operand rhs_true;
  Operand rhs_false;
};

struct Unary {
  Optoken operator_;
  Operand operand_;
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
}  // namespace ssexpr2
BOOST_FUSION_ADAPT_STRUCT(ssexpr2::ast::Unary, operator_, operand_)
BOOST_FUSION_ADAPT_STRUCT(ssexpr2::ast::Operation, operator_, operand_)
BOOST_FUSION_ADAPT_STRUCT(ssexpr2::ast::Expression, first, rest)
BOOST_FUSION_ADAPT_STRUCT(ssexpr2::ast::Variable, v)
BOOST_FUSION_ADAPT_STRUCT(ssexpr2::ast::FuncCall, func, args)
BOOST_FUSION_ADAPT_STRUCT(ssexpr2::ast::CondExpr, lhs, rhs_true, rhs_false)

namespace ssexpr2 {
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

  relational_op.add("<", ast::op_less)("<=", ast::op_less_equal)(
      ">", ast::op_greater)(">=", ast::op_greater_equal);

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
struct func_class;
struct cond_expr_class;

typedef x3::rule<cond_expr_class, ast::Operand> cond_expr_type;
typedef x3::rule<equality_expr_class, ast::Expression> equality_expr_type;
typedef x3::rule<relational_expr_class, ast::Expression> relational_expr_type;
typedef x3::rule<logical_expr_class, ast::Expression> logical_expr_type;
typedef x3::rule<additive_expr_class, ast::Expression> additive_expr_type;
typedef x3::rule<multiplicative_expr_class, ast::Expression> multiplicative_expr_type;
typedef x3::rule<unary_expr_class, ast::Operand> unary_expr_type;
typedef x3::rule<primary_expr_class, ast::Operand> primary_expr_type;
typedef x3::rule<var_class, ast::Variable> var_type;
typedef x3::rule<func_class, ast::FuncCall> func_type;

x3::rule<class identifier_rule_, std::string> const identifier_rule = "identifier_rule";
auto const identifier_rule_def =
    x3::lexeme[(x3::alpha | x3::char_('_')) >> *(x3::alnum | x3::char_('_'))];
BOOST_SPIRIT_DEFINE(identifier_rule)

// struct var_class;
// typedef x3::rule<var_class, std::string> var_type;
var_type const var = "var";
// auto const var_def = raw['$' >> lexeme[(alpha | '_') >> *(alnum | '_')]];
// auto const var_unit = lexeme[(alpha | '_') >> *(alnum | '_' | '.')];
// auto const var_def = raw['$' >> var_unit];
auto const var_def = identifier_rule_def % '.';
BOOST_SPIRIT_DEFINE(var);

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
    int_or_double | bool_ | quoted_string | func | var | '(' > expression > ')';

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
}  // namespace ssexpr2

namespace ssexpr2 {
namespace ast {

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
    n.accessor_ = opt_.get_member_access(n.v, nullptr);
    if (!n.accessor_.Valid()) {
      return ERR_INVALID_STRUCT_MEMBER;
    }
    return 0;
  }
  int operator()(FuncCall& n) const {
    auto found = opt_.functions.find(n.func);
    if (found == opt_.functions.end()) {
      return ERR_INVALID_FUNCTION;
    }
    n.functor_ = found->second;
    if (n.args.size() > 3) {
      return ERR_TOO_MANY_ARGS;
    }
    for (auto& operand : n.args) {
      int rc = boost::apply_visitor(*this, operand);
      if (0 != rc) {
        return rc;
      }
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

static Value notValue(Value v) {
  if (v.type != V_BOOL_VALUE || v.type != V_BOOL) {
    v.type = 0;
    v.val = ERR_INVALID_OPERAND_TYPE;
    return v;
  }
  if (v.type == V_BOOL_VALUE) {
    v.val = 1 - v.val;
  } else {
    bool rv = v.Get<bool>();
    v.type = V_BOOL_VALUE;
    v.val = 1 - rv;
  }
  return v;
}
// static bool getBool(Value v) { return v.Get<bool>(); }
static bool fastAndOr(Value v, size_t op) {
  // printf("####Enter fastAndOr %d %d %d\n", op, v.type, v.val);
  bool bv = v.Get<bool>();
  bool rv = false;
  if (op == op_and) {
    if (!bv) {
      rv = true;
    }
  } else if (op == op_or) {
    if (bv) {
      rv = true;
    }
  }
  // printf("####Exit fastAndOr %d \n", rv);
  return rv;
}
static Value negValue(Value v) { return v; }
static inline Value normValue(Value v) {
  switch (v.type) {
    case V_CHAR: {
      char c = v.Get<char>();
      v.type = V_INT64_VALUE;
      v.val = c;
      break;
    }
    case V_UINT8:
    case V_UINT8_VALUE: {
      uint8_t c = v.Get<uint8_t>();
      v.type = V_INT64_VALUE;
      v.val = c;
      break;
    }
    case V_UINT16:
    case V_UINT16_VALUE: {
      uint16_t c = v.Get<uint16_t>();
      v.type = V_INT64_VALUE;
      v.val = c;
      break;
    }
    case V_INT16:
    case V_INT16_VALUE: {
      int16_t c = v.Get<int16_t>();
      v.type = V_INT64_VALUE;
      v.val = c;
      break;
    }
    case V_INT32:
    case V_INT32_VALUE: {
      int32_t c = v.Get<int32_t>();
      v.type = V_INT64_VALUE;
      v.val = c;
      break;
    }
    case V_UINT32:
    case V_UINT32_VALUE: {
      uint32_t c = v.Get<uint32_t>();
      v.type = V_INT64_VALUE;
      v.val = c;
      break;
    }
    case V_UINT64:
    case V_UINT64_VALUE: {
      uint64_t c = v.Get<uint64_t>();
      v.type = V_INT64_VALUE;
      v.val = (int64_t)c;
      break;
    }
    case V_INT64: {
      int64_t c = v.Get<int64_t>();
      v.type = V_INT64_VALUE;
      v.val = (int64_t)c;
      break;
    }
    case V_FLOAT:
    case V_FLOAT_VALUE: {
      float c = v.Get<float>();
      v.type = V_DOUBLE_VALUE;
      double d = (double)c;
      memcpy(&v.val, &d, sizeof(d));
      break;
    }
    case V_DOUBLE: {
      v.type = V_DOUBLE_VALUE;
      memcpy(&v.val, (void*)(v.val), sizeof(double));
      break;
    }
    default: {
      break;
    }
  }

  return v;
}
static inline Value doAdd(Value left, Value right) {
  // printf("####add left:%d, rightr:%d\n", left.type, right.type);
  switch (left.type) {
    case V_DOUBLE_VALUE: {
      double ld = left.Get<double>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          // printf("####doAdd %f %f\n", ld, rd);
          left.Set<double>(ld + rd);
          return left;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          left.Set<double>(ld + rd);
          return left;
        }
        default: {
          Value err;
          err.type = 0;
          err.val = ERR_INVALID_OPERAND_TYPE;
          return err;
        }
      }
    }
    case V_INT64_VALUE: {
      int64_t ld = left.Get<int64_t>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          left.Set<double>(ld + rd);
          return left;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          left.Set<int64_t>(ld + rd);
          return left;
        }
        default: {
          Value err;
          err.type = 0;
          err.val = ERR_INVALID_OPERAND_TYPE;
          return err;
        }
      }
    }
    default: {
      Value err;
      err.type = 0;
      err.val = ERR_INVALID_OPERAND_TYPE;
      return err;
    }
  }
}
static inline Value doMinus(Value left, Value right) {
  switch (left.type) {
    case V_DOUBLE_VALUE: {
      double ld = left.Get<double>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          left.Set<double>(ld - rd);
          return left;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          left.Set<double>(ld - rd);
          return left;
        }
        default: {
          Value err;
          err.type = 0;
          err.val = ERR_INVALID_OPERAND_TYPE;
          return err;
        }
      }
    }
    case V_INT64_VALUE: {
      int64_t ld = left.Get<int64_t>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          left.Set<double>(ld - rd);
          return left;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          left.Set<int64_t>(ld - rd);
          return left;
        }
        default: {
          Value err;
          err.type = 0;
          err.val = ERR_INVALID_OPERAND_TYPE;
          return err;
        }
      }
    }
    default: {
      Value err;
      err.type = 0;
      err.val = ERR_INVALID_OPERAND_TYPE;
      return err;
    }
  }
}
static inline Value doMultiply(Value left, Value right) {
  switch (left.type) {
    case V_DOUBLE_VALUE: {
      double ld = left.Get<double>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          left.Set<double>(ld * rd);
          return left;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          left.Set<double>(ld * rd);
          return left;
        }
        default: {
          Value err;
          err.type = 0;
          err.val = ERR_INVALID_OPERAND_TYPE;
          return err;
        }
      }
    }
    case V_INT64_VALUE: {
      int64_t ld = left.Get<int64_t>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          left.Set<double>(ld * rd);
          return left;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          left.Set<int64_t>(ld * rd);
          return left;
        }
        default: {
          Value err;
          err.type = 0;
          err.val = ERR_INVALID_OPERAND_TYPE;
          return err;
        }
      }
    }
    default: {
      Value err;
      err.type = 0;
      err.val = ERR_INVALID_OPERAND_TYPE;
      return err;
    }
  }
}
static inline Value doDivide(Value left, Value right) {
  switch (left.type) {
    case V_DOUBLE_VALUE: {
      double ld = left.Get<double>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          left.Set<double>(ld / rd);
          return left;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          left.Set<double>(ld / rd);
          return left;
        }
        default: {
          Value err;
          err.type = 0;
          err.val = ERR_INVALID_OPERAND_TYPE;
          return err;
        }
      }
    }
    case V_INT64_VALUE: {
      int64_t ld = left.Get<int64_t>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          left.Set<double>(ld / rd);
          return left;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          left.Set<int64_t>(ld / rd);
          return left;
        }
        default: {
          Value err;
          err.type = 0;
          err.val = ERR_INVALID_OPERAND_TYPE;
          return err;
        }
      }
    }
    default: {
      Value err;
      err.type = 0;
      err.val = ERR_INVALID_OPERAND_TYPE;
      return err;
    }
  }
}
static inline Value doAnd(Value left, Value right) {
  switch (left.type) {
    case V_BOOL_VALUE: {
      bool lv = left.Get<bool>();
      switch (right.type) {
        case V_BOOL_VALUE: {
          bool rv = right.Get<bool>();
          Value vv;
          vv.Set<bool>(lv && rv);
          return vv;
        }
        default: {
          break;
        }
      }
    }
    default: {
      break;
    }
  }
  Value err;
  err.type = 0;
  err.val = ERR_INVALID_OPERAND_TYPE;
  return err;
}
static inline Value doOr(Value left, Value right) {
  if (left.type == V_BOOL_VALUE && right.type == V_BOOL_VALUE) {
    bool lv = left.Get<bool>();
    bool rv = right.Get<bool>();
    Value v;
    v.Set<bool>(lv || rv);
    return v;
  }
  Value err;
  err.type = 0;
  err.val = ERR_INVALID_OPERAND_TYPE;
  return err;
}

static inline Value doEq(Value left, Value right) {
  Value vv;
  switch (left.type) {
    case V_BOOL_VALUE: {
      bool lb = left.Get<bool>();
      if (V_BOOL_VALUE == right.type) {
        bool rb = right.Get<bool>();
        vv.Set<bool>(lb == rb);
        return vv;
      }
      break;
    }
    case V_STD_STRING:
    case V_STD_STRING_VIEW:
    case V_FLATBUFFERS_STRING:
    case V_CSTRING: {
      std::string_view lstr = left.Get<std::string_view>();
      switch (right.type) {
        case V_STD_STRING:
        case V_STD_STRING_VIEW:
        case V_FLATBUFFERS_STRING:
        case V_CSTRING: {
          std::string_view rstr = right.Get<std::string_view>();
          vv.Set<bool>(lstr == rstr);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    case V_DOUBLE_VALUE: {
      double ld = left.Get<double>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          vv.Set<bool>(ld == rd);
          return vv;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          vv.Set<bool>(ld == rd);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    case V_INT64_VALUE: {
      int64_t ld = left.Get<int64_t>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          vv.Set<bool>(ld == rd);
          return vv;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          vv.Set<bool>(ld == rd);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    default: {
      break;
    }
  }
  Value err;
  err.type = 0;
  err.val = ERR_INVALID_OPERAND_TYPE;
  return err;
}
static inline Value doNotEq(Value left, Value right) {
  Value vv;
  switch (left.type) {
    case V_BOOL_VALUE: {
      bool lb = left.Get<bool>();
      if (V_BOOL_VALUE == right.type) {
        bool rb = right.Get<bool>();
        vv.Set<bool>(lb != rb);
        return vv;
      }
      break;
    }
    case V_STD_STRING:
    case V_STD_STRING_VIEW:
    case V_FLATBUFFERS_STRING:
    case V_CSTRING: {
      std::string_view lstr = left.Get<std::string_view>();
      switch (right.type) {
        case V_STD_STRING:
        case V_STD_STRING_VIEW:
        case V_FLATBUFFERS_STRING:
        case V_CSTRING: {
          std::string_view rstr = right.Get<std::string_view>();
          vv.Set<bool>(lstr != rstr);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    case V_DOUBLE_VALUE: {
      double ld = left.Get<double>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          vv.Set<bool>(ld != rd);
          return vv;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          vv.Set<bool>(ld != rd);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    case V_INT64_VALUE: {
      int64_t ld = left.Get<int64_t>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          vv.Set<bool>(ld != rd);
          return vv;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          vv.Set<bool>(ld != rd);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    default: {
      break;
    }
  }
  Value err;
  err.type = 0;
  err.val = ERR_INVALID_OPERAND_TYPE;
  return err;
}
static inline Value doLess(Value left, Value right) {
  Value vv;
  switch (left.type) {
    case V_BOOL_VALUE: {
      bool lb = left.Get<bool>();
      if (V_BOOL_VALUE == right.type) {
        bool rb = right.Get<bool>();
        vv.Set<bool>(lb < rb);
        return vv;
      }
      break;
    }
    case V_STD_STRING:
    case V_STD_STRING_VIEW:
    case V_FLATBUFFERS_STRING:
    case V_CSTRING: {
      std::string_view lstr = left.Get<std::string_view>();
      switch (right.type) {
        case V_STD_STRING:
        case V_STD_STRING_VIEW:
        case V_FLATBUFFERS_STRING:
        case V_CSTRING: {
          std::string_view rstr = right.Get<std::string_view>();
          vv.Set<bool>(lstr < rstr);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    case V_DOUBLE_VALUE: {
      double ld = left.Get<double>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          vv.Set<bool>(ld < rd);
          return vv;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          vv.Set<bool>(ld < rd);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    case V_INT64_VALUE: {
      int64_t ld = left.Get<int64_t>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          vv.Set<bool>(ld < rd);
          return vv;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          vv.Set<bool>(ld < rd);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    default: {
      break;
    }
  }
  Value err;
  err.type = 0;
  err.val = ERR_INVALID_OPERAND_TYPE;
  return err;
}
static inline Value doLessEq(Value left, Value right) {
  Value vv;
  switch (left.type) {
    case V_BOOL_VALUE: {
      bool lb = left.Get<bool>();
      if (V_BOOL_VALUE == right.type) {
        bool rb = right.Get<bool>();
        vv.Set<bool>(lb <= rb);
        return vv;
      }
      break;
    }
    case V_STD_STRING:
    case V_STD_STRING_VIEW:
    case V_FLATBUFFERS_STRING:
    case V_CSTRING: {
      std::string_view lstr = left.Get<std::string_view>();
      switch (right.type) {
        case V_STD_STRING:
        case V_STD_STRING_VIEW:
        case V_FLATBUFFERS_STRING:
        case V_CSTRING: {
          std::string_view rstr = right.Get<std::string_view>();
          vv.Set<bool>(lstr <= rstr);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    case V_DOUBLE_VALUE: {
      double ld = left.Get<double>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          vv.Set<bool>(ld <= rd);
          return vv;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          vv.Set<bool>(ld <= rd);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    case V_INT64_VALUE: {
      int64_t ld = left.Get<int64_t>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          vv.Set<bool>(ld <= rd);
          return vv;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          vv.Set<bool>(ld <= rd);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    default: {
      break;
    }
  }
  Value err;
  err.type = 0;
  err.val = ERR_INVALID_OPERAND_TYPE;
  return err;
}
static inline Value doGt(Value left, Value right) {
  Value vv;
  switch (left.type) {
    case V_BOOL_VALUE: {
      bool lb = left.Get<bool>();
      if (V_BOOL_VALUE == right.type) {
        bool rb = right.Get<bool>();
        vv.Set<bool>(lb > rb);
        return vv;
      }
      break;
    }
    case V_STD_STRING:
    case V_FLATBUFFERS_STRING:
    case V_STD_STRING_VIEW:
    case V_CSTRING: {
      std::string_view lstr = left.Get<std::string_view>();
      switch (right.type) {
        case V_STD_STRING:
        case V_FLATBUFFERS_STRING:
        case V_STD_STRING_VIEW:
        case V_CSTRING: {
          std::string_view rstr = right.Get<std::string_view>();
          vv.Set<bool>(lstr > rstr);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    case V_DOUBLE_VALUE: {
      double ld = left.Get<double>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          vv.Set<bool>(ld > rd);
          return vv;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          vv.Set<bool>(ld > rd);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    case V_INT64_VALUE: {
      int64_t ld = left.Get<int64_t>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          vv.Set<bool>(ld > rd);
          return vv;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          vv.Set<bool>(ld > rd);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    default: {
      break;
    }
  }
  Value err;
  err.type = 0;
  err.val = ERR_INVALID_OPERAND_TYPE;
  return err;
}
static inline Value doGe(Value left, Value right) {
  Value vv;
  switch (left.type) {
    case V_BOOL_VALUE: {
      bool lb = left.Get<bool>();
      if (V_BOOL_VALUE == right.type) {
        bool rb = right.Get<bool>();
        vv.Set<bool>(lb >= rb);
        return vv;
      }
      break;
    }
    case V_STD_STRING:
    case V_FLATBUFFERS_STRING:
    case V_STD_STRING_VIEW:
    case V_CSTRING: {
      std::string_view lstr = left.Get<std::string_view>();
      switch (right.type) {
        case V_STD_STRING:
        case V_FLATBUFFERS_STRING:
        case V_STD_STRING_VIEW:
        case V_CSTRING: {
          std::string_view rstr = right.Get<std::string_view>();
          vv.Set<bool>(lstr >= rstr);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    case V_DOUBLE_VALUE: {
      double ld = left.Get<double>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          vv.Set<bool>(ld >= rd);
          return vv;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          vv.Set<bool>(ld >= rd);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    case V_INT64_VALUE: {
      int64_t ld = left.Get<int64_t>();
      switch (right.type) {
        case V_DOUBLE_VALUE: {
          double rd = right.Get<double>();
          vv.Set<bool>(ld >= rd);
          return vv;
        }
        case V_INT64_VALUE: {
          int64_t rd = right.Get<int64_t>();
          vv.Set<bool>(ld >= rd);
          return vv;
        }
        default: {
          break;
        }
      }
      break;
    }
    default: {
      break;
    }
  }
  Value err;
  err.type = 0;
  err.val = ERR_INVALID_OPERAND_TYPE;
  return err;
}
static Value calcPairValue(Value left, Value right, size_t op) {
  // printf("####Enter calcPairValue %lld %d %d\n", op, left.type, right.type);
  if (left.type == 0) {
    return left;
  }
  if (right.type == 0) {
    return right;
  }
  left = normValue(left);
  right = normValue(right);
  switch (op) {
    case op_plus: {
      return doAdd(left, right);
    }
    case op_minus: {
      return doMinus(left, right);
    }
    case op_times: {
      return doMultiply(left, right);
    }
    case op_divide: {
      return doDivide(left, right);
    }
    case op_equal: {
      Value rv = doEq(left, right);
      // printf("####Exit eq calcPairValue  %d %d\n", rv.type, rv.val);
      return rv;
    }
    case op_not_equal: {
      Value rv = doNotEq(left, right);
      // printf("####Exit neq calcPairValue %d %d \n", rv.type, rv.val);
      return rv;
    }
    case op_less: {
      return doLess(left, right);
    }
    case op_less_equal: {
      return doLessEq(left, right);
    }
    case op_greater: {
      Value rv = doGt(left, right);
      // printf("####Exit GT %llu %llu\n", rv.type, rv.val);
      return rv;
    }
    case op_greater_equal: {
      return doGe(left, right);
    }
    case op_and: {
      Value rv = doAnd(left, right);
      return rv;
    }
    case op_or: {
      // printf("####Enter op_or %d %d\n", left.type, right.type);
      Value rv = doOr(left, right);
      // printf("####Exit op_or calcPairValue %d %d \n", rv.type, rv.val);
      return rv;
    }
    default: {
      break;
    }
  }
  Value err;
  err.type = 0;
  err.val = ERR_INVALID_OPERATOR;
  return err;
}
static Value printValue(Value v) {
  printf("##print value: %llu %llu\n", v.type, v.val);
  return v;
}

struct CodeGenerator {
  const ExprOptions& opt_;
  std::shared_ptr<Xbyak::CodeGenerator> _jit_ptr;
  Xbyak::CodeGenerator& jit_;
  std::string debug_asm_;
  size_t cursor = 0;
  /**
   * r12: save eval obj
   * r13: save pushed counter
   *
   * @param jit
   */
  CodeGenerator(const ExprOptions& opt, std::shared_ptr<Xbyak::CodeGenerator> jit)
      : opt_(opt), _jit_ptr(jit), jit_(*jit) {}
  // void PushValue(const Value& v) {
  //   DEBUG_ASM_OP((jit_.push(v.val)));
  //   DEBUG_ASM_OP((jit_.push(v.type)));
  //   pushed_registers_num_++;
  // }
  void PushRegisters() {
    DEBUG_ASM_OP((jit_.push(jit_.rax)));
    DEBUG_ASM_OP((jit_.push(jit_.rdx)));
    jit_.inc(jit_.r13);
  }
  void SaveRegisterValue(const Value& v) {
    DEBUG_ASM_OP((jit_.mov(jit_.rdx, v.type)));
    DEBUG_ASM_OP((jit_.mov(jit_.rax, v.val)));
  }
  void PopValue() {
    DEBUG_ASM_OP((jit_.pop(jit_.rdx)));
    DEBUG_ASM_OP((jit_.pop(jit_.rax)));
    jit_.dec(jit_.r13);
  }

  void operator()(Nil) {}
  void operator()(int64_t n) {
    Value v;
    v.Set<int64_t>(n);
    SaveRegisterValue(v);
  }
  void operator()(bool n) {
    Value v;
    v.Set<bool>(n);
    SaveRegisterValue(v);
  }
  void operator()(double n) {
    Value v;
    v.Set<double>(n);
    SaveRegisterValue(v);
  }
  void operator()(std::string const& n) {
    Value v;
    v.Set<std::string>(n);
    SaveRegisterValue(v);
  }
  void operator()(Variable const& n) {
    DEBUG_ASM_OP((jit_.mov(jit_.rdi, jit_.r12)));
    // const GetValue* func = n.accessor_.GetFunc();
    // DEBUG_ASM_OP((jit_.mov(jit_.rax, (size_t)func)));
    // DEBUG_ASM_OP((jit_.call(jit_.rax)));
    // jit_.push(jit_.rbp);
    opt_.get_member_access(n.v, _jit_ptr);
    // jit_.pop(jit_.rbp);
  }
  void operator()(CondExpr const& n) {
    size_t current_cursor = cursor;
    cursor++;
    boost::apply_visitor(*this, n.lhs);
    jit_.cmp(jit_.edx, V_BOOL);
    jit_.je(".cond_test_ptr" + std::to_string(current_cursor));
    jit_.cmp(jit_.edx, V_BOOL_VALUE);
    jit_.je(".cond_test_value" + std::to_string(current_cursor));
    jit_.mov(jit_.r10, ERR_INVALID_OPERAND_TYPE);
    jit_.jmp(".err_exit", jit_.T_NEAR);
    jit_.L(".cond_test_ptr" + std::to_string(current_cursor));
    jit_.mov(jit_.rdx, V_BOOL_VALUE);
    jit_.mov(jit_.rax, jit_.ptr[jit_.rax]);
    jit_.L(".cond_test_value" + std::to_string(current_cursor));
    jit_.cmp(jit_.rax, 0);
    jit_.je(".cond_test_false" + std::to_string(current_cursor), jit_.T_NEAR);
    jit_.L(".cond_test_true" + std::to_string(current_cursor));
    boost::apply_visitor(*this, n.rhs_true);
    jit_.jmp(".cond_test_exit" + std::to_string(current_cursor), jit_.T_NEAR);
    jit_.L(".cond_test_false" + std::to_string(current_cursor));
    boost::apply_visitor(*this, n.rhs_false);
    jit_.jmp(".cond_test_exit" + std::to_string(current_cursor));
    jit_.L(".cond_test_exit" + std::to_string(current_cursor));
  }
  void operator()(FuncCall const& n) {
    for (size_t i = 0; i < n.args.size(); i++) {
      boost::apply_visitor(*this, n.args[i]);
      if (i != (n.args.size() - 1)) {
        PushRegisters();
      }
    }
    if (3 == n.args.size()) {
      DEBUG_ASM_OP((jit_.mov(jit_.r8, jit_.rax)));
      DEBUG_ASM_OP((jit_.mov(jit_.r9, jit_.rdx)));
      PopValue();
      DEBUG_ASM_OP((jit_.mov(jit_.r10, jit_.rax)));
      DEBUG_ASM_OP((jit_.mov(jit_.r11, jit_.rdx)));
      // DEBUG_ASM_OP((jit_.mov(jit_.rdx, jit_.rax)));
      // DEBUG_ASM_OP((jit_.mov(jit_.rcx, jit_.r11)));
      PopValue();
      DEBUG_ASM_OP((jit_.mov(jit_.rdi, jit_.rax)));
      DEBUG_ASM_OP((jit_.mov(jit_.rsi, jit_.rdx)));
      DEBUG_ASM_OP((jit_.mov(jit_.rdx, jit_.r10)));
      DEBUG_ASM_OP((jit_.mov(jit_.rcx, jit_.r11)));
    } else if (2 == n.args.size()) {
      DEBUG_ASM_OP((jit_.mov(jit_.r10, jit_.rax)));
      DEBUG_ASM_OP((jit_.mov(jit_.r11, jit_.rdx)));
      PopValue();
      DEBUG_ASM_OP((jit_.mov(jit_.rdi, jit_.rax)));
      DEBUG_ASM_OP((jit_.mov(jit_.rsi, jit_.rdx)));
      DEBUG_ASM_OP((jit_.mov(jit_.rdx, jit_.r10)));
      DEBUG_ASM_OP((jit_.mov(jit_.rcx, jit_.r11)));
    } else if (1 == n.args.size()) {
      DEBUG_ASM_OP((jit_.mov(jit_.rdi, jit_.rax)));
      DEBUG_ASM_OP((jit_.mov(jit_.rsi, jit_.rdx)));
    }
    DEBUG_ASM_OP((jit_.mov(jit_.rax, (size_t)(n.functor_))));
    // jit_.push(jit_.rbp);
    DEBUG_ASM_OP((jit_.call(jit_.rax)));
    // jit_.pop(jit_.rbp);
  }
  void operator()(Unary const& n) {
    boost::apply_visitor(*this, n.operand_);
    if (n.operator_ == op_positive) {
      return;
    }
    if (n.operator_ == op_not) {
      DEBUG_ASM_OP((jit_.mov(jit_.rdi, jit_.rax)));
      DEBUG_ASM_OP((jit_.mov(jit_.rsi, jit_.rdx)));
      DEBUG_ASM_OP((jit_.mov(jit_.rax, (size_t)notValue)));
      // jit_.push(jit_.rbp);
      DEBUG_ASM_OP((jit_.call(jit_.rax)));
      // jit_.pop(jit_.rbp);
      return;
    } else if (n.operator_ == op_negative) {
      DEBUG_ASM_OP((jit_.mov(jit_.rdi, jit_.rax)));
      DEBUG_ASM_OP((jit_.mov(jit_.rsi, jit_.rdx)));
      DEBUG_ASM_OP((jit_.mov(jit_.rax, (size_t)negValue)));
      // jit_.push(jit_.rbp);
      DEBUG_ASM_OP((jit_.call(jit_.rax)));
      // jit_.pop(jit_.rbp);
      return;
    }
  }
  void operator()(Operation const& x) {
    // printf("builder op:%d\n", x.operator_);
    size_t current_cursor = cursor;
    cursor++;
    if (x.operator_ == op_and || x.operator_ == op_or) {
      DEBUG_ASM_OP((jit_.cmp(jit_.edx, V_BOOL)));
      DEBUG_ASM_OP((jit_.je(".and_or_ptr" + std::to_string(current_cursor))));
      DEBUG_ASM_OP((jit_.cmp(jit_.edx, V_BOOL_VALUE)));
      DEBUG_ASM_OP((jit_.je(".and_or" + std::to_string(current_cursor))));
      DEBUG_ASM_OP((jit_.mov(jit_.r10, ERR_INVALID_OPERAND_TYPE)));
      // jit_.cmp(jit_.r13, 0);
      // jit_.je(".err_exit", jit_.T_NEAR);
      // jit_.L(".loop" + std::to_string(current_cursor));
      // PopValue();
      // jit_.jne(".loop" + std::to_string(current_cursor));
      DEBUG_ASM_OP((jit_.jmp(".err_exit", jit_.T_NEAR)));
      jit_.L(".and_or_ptr" + std::to_string(current_cursor));
      jit_.mov(jit_.edx, V_BOOL_VALUE);
      jit_.mov(jit_.rax, jit_.ptr[jit_.rax]);
      DEBUG_ASM_OP((jit_.L(".and_or" + std::to_string(current_cursor))));
      if (x.operator_ == op_and) {
        jit_.cmp(jit_.rax, 0);
        jit_.je(".fast_ret" + std::to_string(current_cursor), jit_.T_NEAR);
      } else {
        jit_.cmp(jit_.rax, 1);
        jit_.je(".fast_ret" + std::to_string(current_cursor), jit_.T_NEAR);
      }
    }
    // rhs
    PushRegisters();
    boost::apply_visitor(*this, x.operand_);
    DEBUG_ASM_OP((jit_.mov(jit_.r11, jit_.rdx)));
    DEBUG_ASM_OP((jit_.mov(jit_.r10, jit_.rax)));
    PopValue();
    DEBUG_ASM_OP((jit_.mov(jit_.rdi, jit_.rax)));
    DEBUG_ASM_OP((jit_.mov(jit_.rsi, jit_.rdx)));
    DEBUG_ASM_OP((jit_.mov(jit_.rdx, jit_.r10)));
    DEBUG_ASM_OP((jit_.mov(jit_.rcx, jit_.r11)));
    DEBUG_ASM_OP((jit_.mov(jit_.r8, (size_t)(x.operator_))));
    DEBUG_ASM_OP((jit_.mov(jit_.rax, (size_t)calcPairValue)));
    // jit_.push(jit_.rbp);
    DEBUG_ASM_OP((jit_.call(jit_.rax)));
    // jit_.pop(jit_.rbp);
    DEBUG_ASM_OP((jit_.jmp(".ret" + std::to_string(current_cursor))));

    DEBUG_ASM_OP((jit_.L(".fast_ret" + std::to_string(current_cursor))));
    if (x.operator_ == op_and) {
      DEBUG_ASM_OP((jit_.mov(jit_.rdx, V_BOOL_VALUE)));
      DEBUG_ASM_OP((jit_.mov(jit_.rax, 0)));
    } else if (x.operator_ == op_or) {
      DEBUG_ASM_OP((jit_.mov(jit_.rdx, V_BOOL_VALUE)));
      DEBUG_ASM_OP((jit_.mov(jit_.rax, 1)));
    }
    DEBUG_ASM_OP((jit_.L(".ret" + std::to_string(current_cursor))));
    // DEBUG_ASM_OP((jit_.nop()));
  }

  void operator()(Expression const& x) {
    boost::apply_visitor(*this, x.first);
    for (Operation const& oper : x.rest) {
      (*this)(oper);
    }
  }
};
}  // namespace ast
}  // namespace ssexpr2

namespace ssexpr2 {
int SpiritExpression::Init(const std::string& expr, const ExprOptions& options) {
  using ssexpr2::parser::expression;                             // Our grammar
  ssexpr2::ast::Expression* ast = new ssexpr2::ast::Expression;  // Our tree

  using boost::spirit::x3::ascii::space;
  std::string::const_iterator iter = expr.begin();
  std::string::const_iterator end = expr.end();

  using boost::spirit::x3::with;
  using ssexpr2::parser::error_handler_type;
  std::ostringstream err_stream;
  error_handler_type error_handler(iter, end, err_stream);  // Our error handler
  auto const parser =
      // we pass our error handler to the parser so we can access
      // it later in our on_error and on_sucess handlers
      with<boost::spirit::x3::error_handler_tag>(
          std::ref(error_handler))[ssexpr2::parser::get_expression()];
  bool r = phrase_parse(iter, end, parser, space, ast->first);
  if (!r) {
    delete ast;
    return -1;
  }
  ssexpr2::ast::Initializer init(options);
  int rc = init(*ast);
  if (0 != rc) {
    delete ast;
    return rc;
  }
  jit_.reset(new Xbyak::CodeGenerator);
  ssexpr2::ast::CodeGenerator gen(options, jit_);
  jit_->inLocalLabel();
  // jit_->push(jit_->rbx);
  jit_->push(jit_->rbp);
  jit_->push(jit_->r12);
  jit_->push(jit_->r13);
  jit_->push(jit_->r14);
  jit_->push(jit_->r15);
  jit_->mov(jit_->r13, 0);
  jit_->mov(jit_->r12, jit_->rdi);
  gen(*ast);
  DEBUG_ASM_OP((jit_->jmp(".ok_exit")));
  DEBUG_ASM_OP((jit_->L(".err_exit")));
  jit_->cmp(jit_->r13, 0);
  jit_->je(".final_err_exit");
  jit_->L(".err_exit_loop");
  gen.PopValue();
  jit_->jne(".err_exit_loop");
  jit_->L(".final_err_exit");
  DEBUG_ASM_OP((jit_->mov(jit_->rax, jit_->r10)));
  DEBUG_ASM_OP((jit_->mov(jit_->rdx, 0)));
  DEBUG_ASM_OP((jit_->L(".ok_exit")));
  jit_->pop(jit_->r15);
  jit_->pop(jit_->r14);
  jit_->pop(jit_->r13);
  jit_->pop(jit_->r12);
  jit_->pop(jit_->rbp);
  // jit_->pop(jit_->rbx);
  DEBUG_ASM_OP((jit_->ret()));
  DEBUG_ASM_OP((jit_->outLocalLabel()));
  _eval = jit_->getCode<EvalFunc>();
  expr_.reset(ast);
  return 0;
}
Value SpiritExpression::doEval(const void* obj) {
  if (_eval) {
    return _eval(obj);
  }
  Value err;
  err.type = 0;
  err.val = ERR_NIL_EVAL_FUNC;
  return err;
}

/**
 * @brief
 * objdump -d -M intel:x86-64  -D -b binary -m i386:x86-64:intel ./asm.bin
 * @param file
 * @return int
 */
int SpiritExpression::DumpAsmCode(const std::string& file) {
  FILE* f = fopen(file.c_str(), "wb");
  if (nullptr == f) {
    return -1;
  }
  size_t n = fwrite(jit_->getCode(), jit_->getSize(), 1, f);
  fclose(f);
  if (n != jit_->getSize()) {
    return -1;
  }
  return 0;
}
}  // namespace ssexpr2
