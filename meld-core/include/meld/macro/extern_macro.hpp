#pragma once

#include "decorator.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <expected>
#include <functional>

namespace meld::macro {

// Supported target languages for FFI
enum class TargetLanguage {
    Java,
    Go,
    Cpp,
    C
};

// Convert string to TargetLanguage enum
std::expected<TargetLanguage, std::string> parse_target_language(const std::string& lang);

// Convert TargetLanguage enum to string
std::string target_language_to_string(TargetLanguage lang);

// FFI binding configuration
struct FFIBinding {
    TargetLanguage target_lang;
    std::string class_name;      // For Java/C++: full class name
    std::string package_name;    // For Go: package name
    std::string header_name;     // For C/C++: header file
    std::string library_name;    // For dynamic libraries
    std::map<std::string, std::string> type_mappings; // Meld type -> target type
    std::map<std::string, std::string> extra_params;  // Additional parameters
};

// Type mapping utilities
class TypeMapper {
public:
    // Get default type mapping for a target language
    static std::map<std::string, std::string> get_default_mappings(TargetLanguage target);
    
    // Map a Meld type to target language type
    static std::expected<std::string, std::string> 
    map_type(const std::string& meld_type, TargetLanguage target, 
             const std::map<std::string, std::string>& custom_mappings = {});
    
    // Helper methods for complex type mapping
    static std::expected<std::string, std::string>
    map_generic_type(const std::string& meld_type, TargetLanguage target,
                    const std::map<std::string, std::string>& custom_mappings = {});
    
    static std::expected<std::string, std::string>
    map_array_type(const std::string& meld_type, TargetLanguage target,
                  const std::map<std::string, std::string>& custom_mappings = {});
    
    static std::expected<std::string, std::string>
    map_nullable_type(const std::string& meld_type, TargetLanguage target,
                     const std::map<std::string, std::string>& custom_mappings = {});
    
    static std::expected<std::string, std::string>
    map_function_type(const std::string& meld_type, TargetLanguage target,
                     const std::map<std::string, std::string>& custom_mappings = {});
    
    static std::expected<std::string, std::string>
    map_union_type(const std::string& meld_type, TargetLanguage target,
                  const std::map<std::string, std::string>& custom_mappings = {});
    
    static std::expected<std::string, std::string>
    map_tuple_type(const std::string& meld_type, TargetLanguage target,
                  const std::map<std::string, std::string>& custom_mappings = {});
    
    // Map function signature from Meld to target language
    static std::expected<std::string, std::string>
    map_function_signature(const parser::ast::function_definition& func_decl,
                          TargetLanguage target,
                          const std::map<std::string, std::string>& type_mappings = {});
    
    // Map class definition from Meld to target language
    static std::expected<std::string, std::string>
    map_class_definition(const parser::ast::class_definition& class_def,
                        TargetLanguage target,
                        const std::map<std::string, std::string>& type_mappings = {});
};

// FFI binding generator
class FFIBindingGenerator {
public:
    // Generate binding code for a function
    static std::expected<std::string, std::string>
    generate_function_binding(const parser::ast::function_definition& func_decl,
                             const FFIBinding& binding);
    
    // Generate binding code for a class
    static std::expected<std::string, std::string>
    generate_class_binding(const parser::ast::class_definition& class_def,
                          const FFIBinding& binding);
    
    // Generate import/include statements
    static std::string generate_imports(const FFIBinding& binding);
    
    // Generate wrapper code for native calls
    static std::expected<std::string, std::string>
    generate_native_wrapper(const parser::ast::function_definition& func_decl,
                           const FFIBinding& binding);
    
    // Generate ecosystem integration helpers
    static std::expected<std::string, std::string>
    generate_ecosystem_integration(const FFIBinding& binding);
    
    // Generate error handling wrappers
    static std::expected<std::string, std::string>
    generate_error_handling_wrapper(const parser::ast::function_definition& func_decl,
                                   const FFIBinding& binding);
    
    // Generate async/await wrappers
    static std::expected<std::string, std::string>
    generate_async_wrapper(const parser::ast::function_definition& func_decl,
                          const FFIBinding& binding);
    
    // Generate memory management helpers
    static std::expected<std::string, std::string>
    generate_memory_management(const FFIBinding& binding);

private:
    // Language-specific generators
    static std::expected<std::string, std::string>
    generate_java_binding(const parser::ast::function_definition& func_decl,
                         const FFIBinding& binding);
    
    static std::expected<std::string, std::string>
    generate_go_binding(const parser::ast::function_definition& func_decl,
                       const FFIBinding& binding);
    
    static std::expected<std::string, std::string>
    generate_cpp_binding(const parser::ast::function_definition& func_decl,
                        const FFIBinding& binding);
    
    static std::expected<std::string, std::string>
    generate_c_binding(const parser::ast::function_definition& func_decl,
                      const FFIBinding& binding);
};

// @extern macro implementation
class ExternMacro {
public:
    // Parse @extern annotation parameters
    static std::expected<FFIBinding, std::string>
    parse_extern_annotation(const std::string& annotation_text);
    
    // Apply @extern macro to function declaration
    static std::expected<kernel::Value, std::string>
    apply_to_function(const parser::ast::function_definition& func_decl,
                     const FFIBinding& binding,
                     MacroExpander& expander);
    
    // Apply @extern macro to class definition
    static std::expected<kernel::Value, std::string>
    apply_to_class(const parser::ast::class_definition& class_def,
                  const FFIBinding& binding,
                  MacroExpander& expander);
    
    // Generate native_call invocation
    static kernel::Value generate_native_call(
        const std::string& function_name,
        const std::vector<std::string>& param_names,
        const FFIBinding& binding);
    
    // Generate native_load invocation for dynamic libraries
    static kernel::Value generate_native_load(const FFIBinding& binding);
};

// @extern decorator for classes and functions
class ExternDecorator : public Decorator {
public:
    ExternDecorator();
    
    // Apply @extern decorator transformation
    std::expected<kernel::Value, std::string> 
    apply(const parser::ast::class_definition& class_def, MacroExpander& expander) const;

private:
    // Extract @extern annotation from class definition
    std::expected<FFIBinding, std::string>
    extract_extern_binding(const parser::ast::class_definition& class_def) const;
};

// Registration functions
void register_extern_macro();
void register_extern_decorator();

// Helper functions for annotation parsing
namespace extern_parser {
    // Parse lang parameter: @extern(lang: "java")
    std::expected<TargetLanguage, std::string> parse_lang_param(const std::string& value);
    
    // Parse class parameter: @extern(class: "java.util.ArrayList")
    std::expected<std::string, std::string> parse_class_param(const std::string& value);
    
    // Parse package parameter: @extern(package: "fmt")
    std::expected<std::string, std::string> parse_package_param(const std::string& value);
    
    // Parse header parameter: @extern(header: "<vector>")
    std::expected<std::string, std::string> parse_header_param(const std::string& value);
    
    // Parse library parameter: @extern(library: "libmath.so")
    std::expected<std::string, std::string> parse_library_param(const std::string& value);
    
    // Parse type mapping parameter: @extern(types: {"int": "int32", "string": "std::string"})
    std::expected<std::map<std::string, std::string>, std::string> 
    parse_type_mappings(const std::string& value);
}

} // namespace meld::macro
