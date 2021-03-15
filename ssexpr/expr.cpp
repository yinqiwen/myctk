#include <boost/config/warning_disable.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>

#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <string>
namespace client {
namespace ast {
namespace fusion = boost::fusion;
namespace x3 = boost::spirit::x3;

struct nil {};
struct unary;
struct expression;

struct variable : x3::position_tagged {
  variable(std::string const& name = "") : v(name) {}
  std::string v;
};
// struct variable : x3::position_tagged {
//   variable() {}
//   variable(std::vector<std::string> const& v) : name(v) {}
//   std::vector<std::string> name;
// };

struct const_string : x3::position_tagged {
  const_string(std::string const& name = "") : v(name) {}
  std::string v;
};

struct operand : x3::variant<nil, unsigned int, double, bool, std::string, variable,
                             x3::forward_ast<unary>, x3::forward_ast<expression> > {
  using base_type::base_type;
  using base_type::operator=;
};

enum optoken {
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

struct unary {
  optoken operator_;
  operand operand_;
};

struct operation : x3::position_tagged {
  optoken operator_;
  operand operand_;
};

struct expression : x3::position_tagged {
  operand first;
  std::list<operation> rest;
};
}  // namespace ast
}  // namespace client

// We need to tell fusion about our rexpr struct
// to make it a first-class fusion citizen
BOOST_FUSION_ADAPT_STRUCT(client::ast::unary, operator_, operand_)

BOOST_FUSION_ADAPT_STRUCT(client::ast::operation, operator_, operand_)

BOOST_FUSION_ADAPT_STRUCT(client::ast::expression, first, rest)
BOOST_FUSION_ADAPT_STRUCT(client::ast::variable, v)

namespace client {
namespace parser {
namespace x3 = boost::spirit::x3;
using x3::alnum;
using x3::alpha;
using x3::bool_;
using x3::char_;
using x3::lexeme;
using x3::raw;
using x3::uint_;
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

x3::symbols<ast::optoken> equality_op;
x3::symbols<ast::optoken> relational_op;
x3::symbols<ast::optoken> logical_op;
x3::symbols<ast::optoken> additive_op;
x3::symbols<ast::optoken> multiplicative_op;
x3::symbols<ast::optoken> unary_op;
x3::symbols<> keywords;

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

  keywords.add("var")("true")("false")("if")("else")("while");
}

////////////////////////////////////////////////////////////////////////////
// Main expression grammar
////////////////////////////////////////////////////////////////////////////
struct expression_class;
typedef x3::rule<expression_class, ast::expression> expression_type;
BOOST_SPIRIT_DECLARE(expression_type);

struct equality_expr_class;
struct relational_expr_class;
struct logical_expr_class;
struct additive_expr_class;
struct multiplicative_expr_class;
struct unary_expr_class;
struct primary_expr_class;
struct var_class;

typedef x3::rule<equality_expr_class, ast::expression> equality_expr_type;
typedef x3::rule<relational_expr_class, ast::expression> relational_expr_type;
typedef x3::rule<logical_expr_class, ast::expression> logical_expr_type;
typedef x3::rule<additive_expr_class, ast::expression> additive_expr_type;
typedef x3::rule<multiplicative_expr_class, ast::expression> multiplicative_expr_type;
typedef x3::rule<unary_expr_class, ast::operand> unary_expr_type;
typedef x3::rule<primary_expr_class, ast::operand> primary_expr_type;
typedef x3::rule<var_class, ast::variable> var_type;

// struct var_class;
// typedef x3::rule<var_class, std::string> var_type;
var_type const var = "var";
// auto const var_def = raw['$' >> lexeme[(alpha | '_') >> *(alnum | '_')]];
auto const var_unit = lexeme[(alpha | '_') >> *(alnum | '_' | '.')];
auto const var_def = raw['$' >> var_unit];
BOOST_SPIRIT_DEFINE(var);

struct const_string_class;
typedef x3::rule<const_string_class, std::string> const_string_type;
const_string_type const const_string = "const_string";
// auto const var_def = raw['$' >> lexeme[(alpha | '_') >> *(alnum | '_')]];
auto const const_string_def = lexeme['"' >> *(char_ - '"') >> '"'];
// BOOST_SPIRIT_DEFINE(const_string);

expression_type const expression = "expression";
equality_expr_type const equality_expr = "equality_expr";
relational_expr_type const relational_expr = "relational_expr";
logical_expr_type const logical_expr = "logical_expr";
additive_expr_type const additive_expr = "additive_expr";
multiplicative_expr_type const multiplicative_expr = "multiplicative_expr";
unary_expr_type const unary_expr = "unary_expr";
primary_expr_type const primary_expr = "primary_expr";

auto const logical_expr_def = equality_expr >> *(logical_op > equality_expr);

auto const equality_expr_def = relational_expr >> *(equality_op > relational_expr);

auto const relational_expr_def = additive_expr >> *(relational_op > additive_expr);

auto const additive_expr_def = multiplicative_expr >> *(additive_op > multiplicative_expr);

auto const multiplicative_expr_def = unary_expr >> *(multiplicative_op > unary_expr);

auto const unary_expr_def = primary_expr | (unary_op > primary_expr);
auto const quoted_string = lexeme['"' >> *(char_ - '"') >> '"'];

auto const primary_expr_def = uint_ | bool_ | quoted_string | var | '(' > expression > ')';

auto const expression_def = logical_expr;

BOOST_SPIRIT_DEFINE(expression, logical_expr, equality_expr, relational_expr, additive_expr,
                    multiplicative_expr, unary_expr, primary_expr);

struct unary_expr_class : x3::annotate_on_success {};
struct primary_expr_class : x3::annotate_on_success {};
// struct var_class : x3::annotate_on_success {};

BOOST_SPIRIT_INSTANTIATE(expression_type, iterator_type, context_type);

expression_type const& get_expression() {
  add_keywords();
  return expression;
}

}  // namespace parser
}  // namespace client

namespace client {
namespace ast {
///////////////////////////////////////////////////////////////////////////
//  Print out the rexpr tree
///////////////////////////////////////////////////////////////////////////
int const tabsize = 4;

struct expr_printer {
  typedef void result_type;

  expr_printer(int indent = 0) : indent(indent) {}

  void operator()(expression const& ast) const {
    boost::apply_visitor(expr_printer(indent + tabsize), ast.first);
    for (auto& v : ast.rest) {
      expr_printer s;
      s(v);
    }
  }
  void operator()(variable const& v) const { std::cout << "$" << v.v << " "; }
  void operator()(unsigned int const& v) const { std::cout << v << " "; }
  void operator()(unary const& v) const {
    switch (v.operator_) {
      case op_plus: {
        std::cout << "+";
        break;
      }
      case op_minus: {
        std::cout << "-";
        break;
      }
      case op_times: {
        std::cout << "*";
        break;
      }
      case op_divide: {
        std::cout << "/";
        break;
      }
      case op_positive: {
        std::cout << "+";
        break;
      }
      case op_negative: {
        std::cout << "-";
        break;
      }
      case op_not: {
        std::cout << "!";
        break;
      }
      case op_equal: {
        std::cout << "==";
        break;
      }
      case op_not_equal: {
        std::cout << "!=";
        break;
      }
      case op_less: {
        std::cout << "<";
        break;
      }
      case op_less_equal: {
        std::cout << "<=";
        break;
      }
      case op_greater: {
        std::cout << ">";
        break;
      }
      case op_greater_equal: {
        std::cout << ">=";
        break;
      }
      case op_and: {
        std::cout << "&&";
        break;
      }
      case op_or: {
        std::cout << "!!";
        break;
      }
      default: {
        break;
      }
    }
    boost::apply_visitor(expr_printer(indent + tabsize), v.operand_);
  }
  void operator()(operation const& v) const {
    switch (v.operator_) {
      case op_plus: {
        std::cout << "+";
        break;
      }
      case op_minus: {
        std::cout << "-";
        break;
      }
      case op_times: {
        std::cout << "*";
        break;
      }
      case op_divide: {
        std::cout << "/";
        break;
      }
      case op_positive: {
        std::cout << "+";
        break;
      }
      case op_negative: {
        std::cout << "-";
        break;
      }
      case op_not: {
        std::cout << "!";
        break;
      }
      case op_equal: {
        std::cout << "==";
        break;
      }
      case op_not_equal: {
        std::cout << "!=";
        break;
      }
      case op_less: {
        std::cout << "<";
        break;
      }
      case op_less_equal: {
        std::cout << "<=";
        break;
      }
      case op_greater: {
        std::cout << ">";
        break;
      }
      case op_greater_equal: {
        std::cout << ">=";
        break;
      }
      case op_and: {
        std::cout << "&&";
        break;
      }
      case op_or: {
        std::cout << "!!";
        break;
      }
      default: {
        break;
      }
    }
    boost::apply_visitor(expr_printer(indent + tabsize), v.operand_);
  }
  void operator()(nil const& v) const {}
  void operator()(std::string const& text) const { std::cout << '"' << text << '"'; }

  void tab(int spaces) const {
    for (int i = 0; i < spaces; ++i) std::cout << ' ';
  }

  int indent;
};
}  // namespace ast
}  // namespace client

int main(int argc, char** argv) {
  char const* filename;
  if (argc > 1) {
    filename = argv[1];
  } else {
    std::cerr << "Error: No input file provided." << std::endl;
    return 1;
  }

  std::ifstream in(filename, std::ios_base::in);

  if (!in) {
    std::cerr << "Error: Could not open input file: " << filename << std::endl;
    return 1;
  }

  std::string storage;          // We will read the contents here.
  in.unsetf(std::ios::skipws);  // No white space skipping!
  std::copy(std::istream_iterator<char>(in), std::istream_iterator<char>(),
            std::back_inserter(storage));

  using client::parser::expression;  // Our grammar
  client::ast::expression ast;       // Our tree

  using boost::spirit::x3::ascii::space;
  std::string::const_iterator iter = storage.begin();
  std::string::const_iterator end = storage.end();

  using boost::spirit::x3::with;
  using client::parser::error_handler_type;
  error_handler_type error_handler(iter, end, std::cerr);  // Our error handler
  auto const parser =
      // we pass our error handler to the parser so we can access
      // it later in our on_error and on_sucess handlers
      with<boost::spirit::x3::error_handler_tag>(
          std::ref(error_handler))[client::parser::get_expression()];
  bool r = phrase_parse(iter, end, parser, space, ast);

  if (r && iter == end) {
    std::cout << "-------------------------\n";
    std::cout << "Parsing succeeded\n";
    std::cout << "-------------------------\n";
    client::ast::expr_printer printer;
    printer(ast);
    return 0;
  } else {
    std::string::const_iterator some = iter + 30;
    std::string context(iter, (some > end) ? end : some);
    std::cout << "-------------------------\n";
    std::cout << "Parsing failed\n";
    std::cout << "stopped at: \": " << context << "...\"\n";
    std::cout << "-------------------------\n";
    return 1;
  }
}
