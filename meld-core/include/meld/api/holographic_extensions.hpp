#pragma once

/**
 * @file holographic_extensions.hpp
 * @brief Extension methods to add toHologram() functionality to existing types
 * 
 * This header provides extension methods that add holographic view capabilities
 * to existing Meld types like modules, classes, and IR structures.
 */

#include "meld/api/holographic_view.hpp"
#include "meld/parser/ast.hpp"
#include "meld/compiler/ir.hpp"

namespace meld::api::extensions {

/**
 * @brief Extension methods for AST class definitions
 */
class ClassDefinitionExtensions {
public:
    /**
     * @brief Create a holographic view of a class definition
     * @param class_def The class definition to create a hologram from
     * @return ClassHologram representing the compressed class
     */
    static ClassHologram toHologram(const parser::ast::class_definition& class_def);
    
    /**
     * @brief Get compression statistics for a class definition
     * @param class_def The class definition to analyze
     * @return Map of compression statistics
     */
    static std::unordered_map<std::string, std::string> getCompressionStats(const parser::ast::class_definition& class_def);
    
    /**
     * @brief Create a TOON representation of a class definition
     * @param class_def The class definition to create a TOON representation from
     * @return TOON string representation
     */
    static std::string toToon(const parser::ast::class_definition& class_def);
};

/**
 * @brief Extension methods for AST struct definitions
 */
class StructDefinitionExtensions {
public:
    /**
     * @brief Create a holographic view of a struct definition
     * @param struct_def The struct definition to create a hologram from
     * @return TypeDefinitionHologram representing the compressed struct
     */
    static TypeDefinitionHologram toHologram(const parser::ast::struct_definition& struct_def);
};

/**
 * @brief Extension methods for AST function definitions
 */
class FunctionDefinitionExtensions {
public:
    /**
     * @brief Create a holographic view of a function definition
     * @param func_def The function definition to create a hologram from
     * @return FunctionSignatureHologram representing the compressed function
     */
    static FunctionSignatureHologram toHologram(const parser::ast::function_definition& func_def);
};

/**
 * @brief Extension methods for IR modules
 */
class IRModuleExtensions {
public:
    /**
     * @brief Create a holographic view of an IR module
     * @param ir_module The IR module to create a hologram from
     * @return ModuleHologram representing the compressed module
     */
    static ModuleHologram toHologram(const compiler::ir::Module& ir_module);
    
    /**
     * @brief Get compression statistics for an IR module
     * @param ir_module The IR module to analyze
     * @return Map of compression statistics
     */
    static std::unordered_map<std::string, std::string> getCompressionStats(const compiler::ir::Module& ir_module);
    
    /**
     * @brief Create a TOON representation of an IR module
     * @param ir_module The IR module to create a TOON representation from
     * @return TOON string representation
     */
    static std::string toToon(const compiler::ir::Module& ir_module);
};

/**
 * @brief Extension methods for collections of AST expressions (representing modules)
 */
class ModuleASTExtensions {
public:
    /**
     * @brief Create a holographic view of a module from AST expressions
     * @param module_ast The AST expressions representing the module
     * @param module_name Optional module name
     * @return ModuleHologram representing the compressed module
     */
    static ModuleHologram toHologram(const std::vector<parser::ast::expression>& module_ast,
                                   const std::string& module_name = "");
    
    /**
     * @brief Get compression statistics for a module AST
     * @param module_ast The AST expressions to analyze
     * @param module_name Optional module name
     * @return Map of compression statistics
     */
    static std::unordered_map<std::string, std::string> getCompressionStats(const std::vector<parser::ast::expression>& module_ast,
                                                                           const std::string& module_name = "");
    
    /**
     * @brief Create a TOON representation of a module from AST expressions
     * @param module_ast The AST expressions representing the module
     * @param module_name Optional module name
     * @return TOON string representation
     */
    static std::string toToon(const std::vector<parser::ast::expression>& module_ast,
                            const std::string& module_name = "");
};

/**
 * @brief Utility functions for holographic view operations
 */
class HolographicUtils {
public:
    /**
     * @brief Validate that a hologram meets compression requirements
     * @param original_source The original source code
     * @param hologram_source The holographic representation
     * @param min_compression_ratio Minimum required compression ratio (default 0.90)
     * @return True if compression requirements are met
     */
    static bool validateCompression(const std::string& original_source,
                                  const std::string& hologram_source,
                                  double min_compression_ratio = 0.90);
    
    /**
     * @brief Calculate compression statistics between two text representations
     * @param original_text The original text
     * @param compressed_text The compressed text
     * @return Map of compression statistics
     */
    static std::unordered_map<std::string, std::string> calculateCompressionStats(const std::string& original_text,
                                                                                 const std::string& compressed_text);
    
    /**
     * @brief Count tokens in a text string
     * @param text The text to count tokens in
     * @return Number of tokens
     */
    static size_t countTokens(const std::string& text);
    
    /**
     * @brief Extract contracts from function body (require/ensure blocks)
     * @param func_def The function definition to extract contracts from
     * @return Pair of (require_contract, ensure_contract)
     */
    static std::pair<std::string, std::string> extractContracts(const parser::ast::function_definition& func_def);
    
    /**
     * @brief Extract effect annotations from function
     * @param func_def The function definition to extract effects from
     * @return Vector of effect names
     */
    static std::vector<std::string> extractEffects(const parser::ast::function_definition& func_def);
};

} // namespace meld::api::extensions

// Convenience macros for adding toHologram() methods to existing types
#define MELD_ADD_HOLOGRAM_METHOD(ClassName) \
    inline auto toHologram() const -> decltype(meld::api::extensions::ClassName##Extensions::toHologram(*this)) { \
        return meld::api::extensions::ClassName##Extensions::toHologram(*this); \
    }

#define MELD_ADD_HOLOGRAM_STATS_METHOD(ClassName) \
    inline auto getCompressionStats() const -> decltype(meld::api::extensions::ClassName##Extensions::getCompressionStats(*this)) { \
        return meld::api::extensions::ClassName##Extensions::getCompressionStats(*this); \
    }