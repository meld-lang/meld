#pragma once

/**
 * @file parser_api.hpp
 * @brief Public API for Meld parser functionality
 * 
 * This header provides the main interface for other packages to interact
 * with the Meld parser system.
 */

#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"

namespace meld::api {

/**
 * @brief Main parser interface for external packages
 */
class ParserAPI {
public:
    /**
     * @brief Parse Meld source code into AST
     * @param source_code The Meld source code to parse
     * @return Parsed AST or nullptr on error
     */
    static std::unique_ptr<ast::Node> parse(const std::string& source_code);
    
    /**
     * @brief Parse and validate Meld source code
     * @param source_code The source code to parse and validate
     * @return True if parsing succeeded, false otherwise
     */
    static bool validate_syntax(const std::string& source_code);
    
    /**
     * @brief Get parser errors from last operation
     * @return Vector of error messages
     */
    static std::vector<std::string> get_errors();
    
    /**
     * @brief Format Meld source code
     * @param source_code The source code to format
     * @return Formatted source code
     */
    static std::string format(const std::string& source_code);
};

} // namespace meld::api