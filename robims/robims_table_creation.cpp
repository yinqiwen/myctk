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
#include "robims_table_creation.h"
#include <iostream>
#include <list>
#include <string>
#include <vector>

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include "robims_log.h"

namespace robims {

namespace ast {
typedef boost::spirit::x3::variant<nullptr_t, int64_t, double> IntOrDouble;
struct Field {
  std::string name;
  std::string ztype;
  IntOrDouble min;
  IntOrDouble max;
};

struct Table {
  std::string name;
  std::vector<Field> fields;
};
using boost::fusion::operator<<;
}  // namespace ast
}  // namespace robims

// We need to tell fusion about our employee struct
// to make it a first-class fusion citizen. This has to
// be in global scope.
BOOST_FUSION_ADAPT_STRUCT(robims::ast::Field, name, ztype, min, max)
BOOST_FUSION_ADAPT_STRUCT(robims::ast::Table, name, fields)

namespace robims {
///////////////////////////////////////////////////////////////////////////////
//  Our employee parser
///////////////////////////////////////////////////////////////////////////////
namespace parser {
namespace x3 = boost::spirit::x3;
namespace ascii = boost::spirit::x3::ascii;

using ascii::char_;
using x3::double_;
using x3::int_;
using x3::lexeme;
using x3::lit;

x3::rule<class Table, ast::Table> const table = "table";
x3::rule<class Field, ast::Field> const field = "field";

auto const quoted_string = lexeme['"' >> +(char_ - '"') >> '"'];
x3::rule<class identifier_rule_, std::string> const identifier_rule = "identifier_rule";
auto const identifier_rule_def =
    x3::lexeme[(x3::alpha | x3::char_('_')) >> *(x3::alnum | x3::char_('_'))];
boost::spirit::x3::real_parser<double, boost::spirit::x3::strict_real_policies<double> > const
    strict_double_ = {};
const auto int_or_double = strict_double_ | boost::spirit::x3::int64;
auto const field_def = identifier_rule >> identifier_rule >>
                       -('[' >> int_or_double >> ',' >> int_or_double >> ']');
// auto const field_def = identifier_rule >> identifier_rule;
// auto const employee_def = identifier_rule >> '(' >> int_ >> ',' >> quoted_string >> ',' >>
//                           quoted_string >> ',' >> double_ >> ')';
auto const table_def = identifier_rule >> '(' >> (field % ',') >> ')';

BOOST_SPIRIT_DEFINE(table, field, identifier_rule)
}  // namespace parser
}  // namespace robims

namespace robims {
using robims::parser::table;
int parse_talbe_creation_desc(const std::string& desc, TableSchema& schema) {
  using boost::spirit::x3::ascii::space;
  std::string::const_iterator iter = desc.begin();
  std::string::const_iterator end = desc.end();
  robims::ast::Table table_ast;
  bool r = phrase_parse(iter, end, table, space, table_ast);
  if (!r) {
    ROBIMS_ERROR("Failed to parse table creation:{}", desc);
    return -1;
  }
  schema.set_name(table_ast.name);
  for (const auto& field_def : table_ast.fields) {
    auto field_name = field_def.name;
    auto filed_type = field_def.ztype;
    FieldIndexType ftype = UNKNOWN_INDEX;
    bool is_id_field = false;
    if (filed_type == "id" || filed_type == "ID") {
      is_id_field = true;
      if (!schema.id_field().empty()) {
        ROBIMS_ERROR("More than ONE ID field:{}/{} in {}", field_name, schema.id_field(), desc);
        return -1;
      }
    } else if (filed_type == "set" || filed_type == "SET") {
      ftype = SET_INDEX;
    } else if (filed_type == "weight_set" || filed_type == "WEIGHT_SET") {
      ftype = WEIGHT_SET_INDEX;
    } else if (filed_type == "mutex" || filed_type == "MUTEX") {
      ftype = MUTEX_INDEX;
    } else if (filed_type == "int" || filed_type == "INT") {
      ftype = INT_INDEX;
    } else if (filed_type == "float" || filed_type == "FLOAT") {
      ftype = FLOAT_INDEX;
    } else if (filed_type == "bool" || filed_type == "BOOL") {
      ftype = BOOL_INDEX;
    } else {
      ROBIMS_ERROR("Invalid field type:{} in {}", filed_type, desc);
      return -1;
    }
    if (is_id_field) {
      schema.set_id_field(field_name);
    } else {
      auto index_field = schema.add_index_field();
      index_field->set_index_type(ftype);
      index_field->set_name(field_name);
      int min_idx = field_def.min.get().which();
      if (min_idx > 0) {
        if (min_idx == 1) {
          index_field->set_min(boost::get<int64_t>(field_def.min.get()));
        } else {
          index_field->set_min(boost::get<double>(field_def.min.get()));
        }
      }
      int max_idx = field_def.max.get().which();
      if (max_idx > 0) {
        if (max_idx == 1) {
          index_field->set_max(boost::get<int64_t>(field_def.max.get()));
        } else {
          index_field->set_max(boost::get<double>(field_def.max.get()));
        }
      }
      // ROBIMS_INFO("filed:{} {} {}", field_name, min_idx, max_idx);
    }
  }
  return 0;
}
}  // namespace robims
