// x3_grammar.hpp — Boost.Spirit X3 grammar for the Meld language
#pragma once

#include "meld/parser/ast.hpp"
#include <boost/spirit/home/x3.hpp>
#include <string>
#include <vector>

namespace meld::parser::grammar {

namespace x3 = boost::spirit::x3;

// Skipper: whitespace + // comments + /* */ comments
struct skipper_tag;
using skipper_type = x3::rule<skipper_tag>;
extern const skipper_type skipper;

// Entry point
bool parse(const std::string& source, std::vector<ast::expression>& result, std::string& error);

} // namespace meld::parser::grammar
