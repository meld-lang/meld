#pragma once

/**
 * @file holographic_view.hpp
 * @brief Holographic View API for semantic compression
 * 
 * This header provides the Holographic View system that enables semantic compression
 * of Meld code by stripping function bodies while preserving signatures, contracts,
 * and type definitions. This achieves ~95% token reduction for AI context loading.
 */

#include "meld/parser/ast.hpp"
#include "meld/compiler/ir.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <cstdint>

namespace meld::api {

// Forward declarations
class HolographicView;
class ModuleHologram;
class ClassHologram;

/**
 * @brief Represents a holographic view of a function signature
 */
struct FunctionSignatureHologram {
    std::string name;
    std::vector<std::string> parameters;
    std::string return_type;
    std::vector<std::string> named_returns;
    std::vector<std::string> effects;  // Effect annotations
    std::string require_contract;      // Precondition contract
    std::string ensure_contract;       // Postcondition contract
    bool is_public = true;
    bool is_extension = false;
    std::string extension_target;      // For extension methods
    
    std::string to_string() const;
};

/**
 * @brief Represents a holographic view of a type definition
 */
struct TypeDefinitionHologram {
    enum class Kind {
        STRUCT,
        CLASS,
        TRAIT,
        ENUM,
        TYPE_ALIAS,
        REFINEMENT_TYPE
    };
    
    Kind kind;
    std::string name;
    std::vector<std::string> type_parameters;
    std::vector<std::string> fields;
    std::vector<std::string> properties;
    std::vector<FunctionSignatureHologram> methods;
    std::vector<std::string> traits;  // Implemented traits
    std::string base_type;            // For type aliases and refinement types
    std::string refinement_predicate; // For refinement types
    bool is_public = true;
    
    std::string to_string() const;
};

/**
 * @brief Represents a holographic view of an import/export relationship
 */
struct ImportExportHologram {
    enum class Kind {
        IMPORT_SPECIFIC,   // import com.example.User
        IMPORT_WILDCARD,   // import com.example.*
        IMPORT_ALIASED,    // import com.example.models as Models
        EXPORT_SPECIFIC,   // export User
        EXPORT_WILDCARD    // export *
    };
    
    Kind kind;
    std::vector<std::string> namespace_path;
    std::string alias;
    std::vector<std::string> exported_symbols;
    
    std::string to_string() const;
};

/**
 * @brief Represents a holographic view of a class
 */
class ClassHologram {
public:
    ClassHologram(const parser::ast::class_definition& class_def);
    
    // Getters
    const std::string& getName() const { return name_; }
    const std::vector<std::string>& getTypeParameters() const { return type_parameters_; }
    const std::vector<std::string>& getFields() const { return fields_; }
    const std::vector<std::string>& getProperties() const { return properties_; }
    const std::vector<FunctionSignatureHologram>& getMethods() const { return methods_; }
    const std::vector<std::string>& getTraits() const { return traits_; }
    
    // Generate holographic representation
    std::string toHologram() const;
    std::string toToon() const;  // Generate TOON representation
    std::vector<uint8_t> toMeldB() const;  // Generate MELD-B binary format
    
    // Calculate token reduction
    size_t getOriginalTokenCount() const { return original_token_count_; }
    size_t getHologramTokenCount() const;
    double getCompressionRatio() const;
    
private:
    std::string name_;
    std::vector<std::string> type_parameters_;
    std::vector<std::string> fields_;
    std::vector<std::string> properties_;
    std::vector<FunctionSignatureHologram> methods_;
    std::vector<std::string> traits_;
    size_t original_token_count_;
    
    void extractFields(const parser::ast::class_definition& class_def);
    void extractProperties(const parser::ast::class_definition& class_def);
    void extractMethods(const parser::ast::class_definition& class_def);
    FunctionSignatureHologram extractFunctionSignature(const parser::ast::function_definition& func_def);
    size_t calculateOriginalTokens(const parser::ast::class_definition& class_def);
};

/**
 * @brief Represents a holographic view of a module
 */
class ModuleHologram {
    friend class HolographicView;
    friend class ClassHologram;
public:
    ModuleHologram(const std::string& module_name);
    ModuleHologram(const std::vector<parser::ast::expression>& module_ast);
    
    // Add components to the hologram
    void addTypeDefinition(const TypeDefinitionHologram& type_def);
    void addFunctionSignature(const FunctionSignatureHologram& func_sig);
    void addImportExport(const ImportExportHologram& import_export);
    void addClass(const ClassHologram& class_hologram);
    
    // Getters
    const std::string& getName() const { return name_; }
    const std::vector<TypeDefinitionHologram>& getTypeDefinitions() const { return type_definitions_; }
    const std::vector<FunctionSignatureHologram>& getFunctionSignatures() const { return function_signatures_; }
    const std::vector<ImportExportHologram>& getImportExports() const { return import_exports_; }
    const std::vector<ClassHologram>& getClasses() const { return classes_; }
    
    // Generate holographic representation
    std::string toHologram() const;
    
    // Calculate token reduction
    size_t getOriginalTokenCount() const { return original_token_count_; }
    size_t getHologramTokenCount() const;
    double getCompressionRatio() const;
    
    // Export to different formats
    std::string toMeldSource() const;  // Generate Meld source code
    std::string toJson() const;        // Generate JSON representation
    std::string toToon() const;        // Generate TOON representation
    std::vector<uint8_t> toMeldB() const;  // Generate MELD-B binary format (Requirement 40.1)
    
private:
    std::string name_;
    std::vector<TypeDefinitionHologram> type_definitions_;
    std::vector<FunctionSignatureHologram> function_signatures_;
    std::vector<ImportExportHologram> import_exports_;
    std::vector<ClassHologram> classes_;
    size_t original_token_count_;
    
    void processModuleAST(const std::vector<parser::ast::expression>& module_ast);
    void processExpression(const parser::ast::expression& expr);
    void processNamespaceDeclaration(const parser::ast::namespace_declaration& ns_decl);
    void processImportDeclaration(const parser::ast::import_declaration& import_decl);
    void processStructDefinition(const parser::ast::struct_definition& struct_def);
    void processClassDefinition(const parser::ast::class_definition& class_def);
    void processFunctionDefinition(const parser::ast::function_definition& func_def);
    void processTypeAliasDeclaration(const parser::ast::typealias_declaration& alias_decl);
    void processRefinementTypeDefinition(const parser::ast::refinement_type_definition& refinement_def);
    
    FunctionSignatureHologram extractFunctionSignature(const parser::ast::function_definition& func_def);
    TypeDefinitionHologram extractTypeDefinition(const parser::ast::struct_definition& struct_def);
    TypeDefinitionHologram extractTypeDefinition(const parser::ast::class_definition& class_def);
    ImportExportHologram extractImportExport(const parser::ast::import_declaration& import_decl);
    
    size_t calculateTokenCount(const std::string& text) const;
};

/**
 * @brief Main Holographic View API
 */
class HolographicView {
public:
    /**
     * @brief Create a holographic view from Meld source code
     * @param source_code The Meld source code to compress
     * @param module_name Optional module name
     * @return ModuleHologram representing the compressed view
     */
    static ModuleHologram fromSource(const std::string& source_code, 
                                   const std::string& module_name = "");
    
    /**
     * @brief Create a holographic view from parsed AST
     * @param module_ast The parsed AST expressions
     * @param module_name Optional module name
     * @return ModuleHologram representing the compressed view
     */
    static ModuleHologram fromAST(const std::vector<parser::ast::expression>& module_ast,
                                const std::string& module_name = "");
    
    /**
     * @brief Create a holographic view from IR module
     * @param ir_module The IR module to compress
     * @return ModuleHologram representing the compressed view
     */
    static ModuleHologram fromIR(const compiler::ir::Module& ir_module);
    
    /**
     * @brief Create a holographic view of a single class
     * @param class_def The class definition to compress
     * @return ClassHologram representing the compressed class
     */
    static ClassHologram fromClass(const parser::ast::class_definition& class_def);
    
    /**
     * @brief Validate that a hologram achieves the required compression ratio
     * @param hologram The hologram to validate
     * @param min_compression_ratio Minimum required compression ratio (default 0.90 for 90%)
     * @return True if compression ratio is achieved
     */
    static bool validateCompressionRatio(const ModuleHologram& hologram, 
                                       double min_compression_ratio = 0.90);
    
    /**
     * @brief Get compression statistics for a hologram
     * @param hologram The hologram to analyze
     * @return Map of statistics (original_tokens, hologram_tokens, compression_ratio, etc.)
     */
    static std::unordered_map<std::string, std::string> getCompressionStats(const ModuleHologram& hologram);
    
private:
    HolographicView() = delete; // Static class
};

} // namespace meld::api
