#include "meld/parser/parser.hpp"
#include "meld/parser/parser_context.hpp"
#include "meld/parser/query_desugar.hpp"
#include "meld/parser/x3_grammar.hpp"
#include <boost/spirit/home/x3.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <queue>
#include <type_traits>

// Windows headers may redefine CONST macro, conflicting with ParameterDecorator::CONST
#ifdef CONST
#undef CONST
#endif

namespace meld::parser {

namespace x3 = boost::spirit::x3;

// Helper: convert vector<expression> to vector<forward_ast<expression>> for MSVC compatibility
static std::vector<x3::forward_ast<ast::expression>> to_forward_ast_vec(std::vector<ast::expression>& src) {
    std::vector<x3::forward_ast<ast::expression>> dst;
    dst.reserve(src.size());
    for (auto& e : src) {
        dst.push_back(x3::forward_ast<ast::expression>(std::move(e)));
    }
    return dst;
}

bool Parser::parse_file(const std::string& input, std::vector<ast::expression>& result) {
    // Try X3 grammar first (authoritative parser)
    std::string x3_error;
    if (grammar::parse(input, result, x3_error)) {
        // Validate: remove any empty-identifier nodes that X3 may produce
        result.erase(
            std::remove_if(result.begin(), result.end(), [](const ast::expression& e) {
                auto* id = boost::get<ast::identifier>(&e);
                return id && id->name.empty();
            }),
            result.end());
        return true;
    }
    
    error_message_ = x3_error.empty() ? "Parse failed" : x3_error;
    return false;
}

// Contract parsing methods
bool Parser::parse_expression(const std::string& input, ast::expression& result) {
    std::vector<ast::expression> results;
    if (parse_file(input, results) && !results.empty()) {
        result = results[0];
        return true;
    }
    return false;
}

} // namespace meld::parser
