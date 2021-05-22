#include <boost/fusion/adapted/struct.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
namespace x3 = boost::spirit::x3;

namespace ast {

struct nil {};
struct unary_op;
struct binary_op;
struct conditional_op;
struct op_chain;

using UnFunc = std::function<double(double)>;
using BinFunc = std::function<double(double, double)>;

struct expression
    : x3::variant<nil, double, std::string, x3::forward_ast<unary_op>, x3::forward_ast<binary_op>,
                  x3::forward_ast<conditional_op>, x3::forward_ast<op_chain> > {
  using base_type::base_type;
  using base_type::operator=;
};

struct unary_op {
  UnFunc op;
  expression rhs;
};

struct binary_op {
  BinFunc op;
  expression lhs;
  expression rhs;
};

struct conditional_op {
  expression lhs;
  expression rhs_true;
  expression rhs_false;
};

struct operation {
  BinFunc op;
  expression rhs;
};

struct op_chain {
  expression lhs;
  std::list<operation> rhs;
};

struct debug_printer {
  std::ostream& _os;

  // delegate
  template <typename... Ts>
  std::ostream& operator()(Ts&&... args) const {
    return call(std::forward<Ts>(args)...);
  }

  // visit/forward
  std::ostream& call(expression const& v) const { return boost::apply_visitor(*this, v); }
  template <typename T>
  std::ostream& call(x3::forward_ast<T> const& v) const {
    return call(v.get());
  }

  // work horses
  std::ostream& call(nil) const { return _os << "nil"; }
  std::ostream& call(double v) const { return _os << v; }
  std::ostream& call(std::string const& v) const { return _os << v; }
  std::ostream& call(unary_op const& e) const {
    _os << hacky_classify(e.op) << "(";
    return call(e.rhs) << ")";
  }
  std::ostream& call(binary_op const& e) const {
    _os << hacky_classify(e.op, false) << "(";
    call(e.lhs) << ", ";
    return call(e.rhs) << ")";
  }
  std::ostream& call(conditional_op const& e) const {
    call(e.lhs) << "?";
    call(e.rhs_true) << " : ";
    return call(e.rhs_false);
  }
  std::ostream& call(op_chain const& e) const {
    _os << "(";
    call(e.lhs);
    for (auto& op : e.rhs) {
      _os << " " << hacky_classify(op.op) << " ";
      call(op.rhs);
    }
    return _os << ")";
  }

  static std::string hacky_classify(UnFunc const& f) {
    if (f(+42) == -42) return "-";
    if (f(-42) == +42) return "abs";
    return "?";
  }

  static std::string hacky_classify(BinFunc const& f, bool infix = true) {
    if (f(6, 4) == 1296) return infix ? "**" : "pow";
    if (f(36, 36) == 1296) return "*";
    if (f(36, 35) == 35) return "min";
    if (f(35, 36) == 36) return "max";
    if (f(1296, 36) == 36) return "/";
    if (f(1296, 36) == 1260) return "-";
    if (f(1296, 36) == 1332) return "+";
    if (f(4, 6) == 1) return f(4, 4) == 1 ? "<=" : "<";
    if (f(6, 4) == 1) return f(4, 4) == 1 ? ">=" : ">";
    return "?";  // not all functions detected, this is just debug
  }
};

template <typename... Ts>
static inline std::ostream& operator<<(std::ostream& os, x3::variant<Ts...> const& v) {
  return debug_printer{os}(v);
}

template <typename T>
static inline std::ostream& operator<<(std::ostream& os, T const& v) {
  return debug_printer{os}(v);
}
}  // namespace ast

BOOST_FUSION_ADAPT_STRUCT(ast::op_chain, lhs, rhs)
BOOST_FUSION_ADAPT_STRUCT(ast::operation, op, rhs)
BOOST_FUSION_ADAPT_STRUCT(ast::conditional_op, lhs, rhs_true, rhs_false)
BOOST_FUSION_ADAPT_STRUCT(ast::binary_op, op, lhs, rhs)
BOOST_FUSION_ADAPT_STRUCT(ast::unary_op, op, rhs)

namespace P {

struct ehbase {
  template <typename It, typename Ctx>
  x3::error_handler_result on_error(It f, It l, x3::expectation_failure<It> const& e,
                                    Ctx const& /*ctx*/) const {
    std::cout << std::string(f, l) << "\n"
              << std::setw(1 + std::distance(f, e.where())) << "^"
              << "-- expected: " << e.which() << "\n";
    return x3::error_handler_result::fail;
  }
};

struct expression_class : ehbase {};
struct logical_class : ehbase {};
struct equality_class : ehbase {};
struct relational_class : ehbase {};
struct additive_class : ehbase {};
struct multiplicative_class : ehbase {};
struct factor_class : ehbase {};
struct primary_class : ehbase {};
struct unary_class : ehbase {};
struct binary_class : ehbase {};
struct conditional_class : ehbase {};
struct variable_class : ehbase {};

// Rule declarations
auto const op_chain = x3::rule<expression_class, ast::expression>{"op_chain"};
auto const conditional = x3::rule<conditional_class, ast::expression>{"conditional"};
auto const primary = x3::rule<primary_class, ast::expression>{"primary"};
auto const logical = x3::rule<logical_class, ast::op_chain>{"logical"};
auto const equality = x3::rule<equality_class, ast::op_chain>{"equality"};
auto const relational = x3::rule<relational_class, ast::op_chain>{"relational"};
auto const additive = x3::rule<additive_class, ast::op_chain>{"additive"};
auto const multiplicative = x3::rule<multiplicative_class, ast::op_chain>{"multiplicative"};
auto const factor = x3::rule<factor_class, ast::op_chain>{"factor"};
auto const unary = x3::rule<unary_class, ast::unary_op>{"unary"};
auto const binary = x3::rule<binary_class, ast::binary_op>{"binary"};
auto const variable = x3::rule<variable_class, std::string>{"variable"};

struct constant_ : x3::symbols<double> {
  constant_() {
    this->add("e", boost::math::constants::e<double>())("pi", boost::math::constants::pi<double>());
  }
} constant;

struct ufunc_ : x3::symbols<ast::UnFunc> {
  ufunc_() { this->add("abs", &std::abs<double>); }
} ufunc;

struct bfunc_ : x3::symbols<ast::BinFunc> {
  bfunc_() {
    this->add("max", [](double a, double b) { return std::fmax(a, b); })(
        "min", [](double a, double b) { return std::fmin(a, b); })(
        "pow", [](double a, double b) { return std::pow(a, b); });
  }
} bfunc;

struct unary_op_ : x3::symbols<ast::UnFunc> {
  unary_op_() {
    this->add("+", [](double v) { return +v; })("-", std::negate{})("!",
                                                                    [](double v) { return !v; });
  }
} unary_op;

struct additive_op_ : x3::symbols<ast::BinFunc> {
  additive_op_() { this->add("+", std::plus{})("-", std::minus{}); }
} additive_op;

struct multiplicative_op_ : x3::symbols<ast::BinFunc> {
  multiplicative_op_() {
    this->add("*", std::multiplies<>{})("/", std::divides<>{})(
        "%", [](double a, double b) { return std::fmod(a, b); });
  }
} multiplicative_op;

struct logical_op_ : x3::symbols<ast::BinFunc> {
  logical_op_() { this->add("&&", std::logical_and{})("||", std::logical_or{}); }
} logical_op;

struct relational_op_ : x3::symbols<ast::BinFunc> {
  relational_op_() {
    this->add("<", std::less{})("<=", std::less_equal{})(">", std::greater{})(">=",
                                                                              std::greater_equal{});
  }
} relational_op;

struct equality_op_ : x3::symbols<ast::BinFunc> {
  equality_op_() { this->add("==", std::equal_to{})("!=", std::not_equal_to{}); }
} equality_op;

struct power_ : x3::symbols<ast::BinFunc> {
  power_() {
    this->add("**", [](double v, double exp) { return std::pow(v, exp); });
  }
} power;

auto const variable_def = x3::lexeme[x3::alpha >> *x3::alnum];

// Rule defintions
auto const op_chain_def = conditional;

auto make_conditional_op = [](auto& ctx) {
  using boost::fusion::at_c;
  x3::_val(ctx) =
      ast::conditional_op{x3::_val(ctx), at_c<0>(x3::_attr(ctx)), at_c<1>(x3::_attr(ctx))};
};

auto const conditional_def = logical[([](auto& ctx) { _val(ctx) = _attr(ctx); })] >>
                             -('?' > op_chain > ':' > op_chain)[make_conditional_op];

auto const logical_def = equality >> *(logical_op > equality);

auto const equality_def = relational >> *(equality_op > relational);

auto const relational_def = additive >> *(relational_op > additive);

auto const additive_def = multiplicative >> *(additive_op > multiplicative);

auto const multiplicative_def = factor >> *(multiplicative_op > factor);

auto const factor_def = primary >> *(power > factor);

auto const unary_def = (unary_op > primary) | (ufunc > '(' > op_chain > ')');

auto const binary_def = bfunc > '(' > op_chain > ',' > op_chain > ')';

auto const primary_def = x3::double_ |
                         ('(' > op_chain > ')')
                         //| (unary_op > primary)
                         | binary | unary | constant | variable;

BOOST_SPIRIT_DEFINE(op_chain)
BOOST_SPIRIT_DEFINE(logical)
BOOST_SPIRIT_DEFINE(equality)
BOOST_SPIRIT_DEFINE(relational)
BOOST_SPIRIT_DEFINE(additive)
BOOST_SPIRIT_DEFINE(multiplicative)
BOOST_SPIRIT_DEFINE(factor)
BOOST_SPIRIT_DEFINE(primary)
BOOST_SPIRIT_DEFINE(unary)
BOOST_SPIRIT_DEFINE(binary)
BOOST_SPIRIT_DEFINE(conditional)
BOOST_SPIRIT_DEFINE(variable)
}  // namespace P

int main() {
  for (std::string const input : {
           "x+(3**pow(2,8))",
           "1 + (2 + abs(x))",
           "min(x,1+y)",
           "(x > y ? 1 : 0) * (y - z)",
           "min(3**4,7))",
           "3***4",
           "(3,4)",
       }) {
    std::cout << " ===== " << std::quoted(input) << " =====\n";
    auto f = begin(input), l = end(input);
    ast::expression e;
    if (phrase_parse(f, l, P::op_chain, x3::space, e)) {
      std::cout << "Success: " << e << "\n";
    } else {
      std::cout << "Failed\n";
    }
    if (f != l) {
      std::cout << "Unparsed: " << std::quoted(std::string(f, l)) << "\n";
    }
  }
}
