#pragma once

/**
 * @file compiler_api.hpp
 * @brief Public API for Meld compiler functionality
 * 
 * This header provides the main interface for other packages to interact
 * with the Meld compiler system.
 */

#include "meld/compiler/type_checker.hpp"
#include "meld/compiler/ir.hpp"
#include "meld/compiler/codegen.hpp"

namespace meld::api {

/**
 * @brief Main compiler interface for external packages
 */
class CompilerAPI {
public:
    /**
     * @brief Compile Meld source code to target format
     * @param source_code The Meld source code to compile
     * @param options Compilation options
     * @return Compilation result
     */
    static bool compile(const std::string& source_code, 
                       const std::string& output_path = "");
    
    /**
     * @brief Type check Meld source code
     * @param source_code The source code to type check
     * @return Type checking result with diagnostics
     */
    static bool type_check(const std::string& source_code);
    
    /**
     * @brief Get compiler diagnostics from last operation
     * @return Vector of diagnostic messages
     */
    static std::vector<std::string> get_diagnostics();
};

} // namespace meld::api