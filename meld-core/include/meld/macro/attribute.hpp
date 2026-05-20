#pragma once

#include "macro.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <expected>
#include <functional>
#include <variant>

namespace meld::macro {

// ============================================================================
// ATTRIBUTE MACRO SYSTEM
// Task 9.3: Implement attribute macros
// Requirements: 6.4
// ============================================================================

// Forward declaration for recursive variant
struct AttributeValueList;

// Attribute argument value types
using AttributeValue = std::variant<
    std::string,                    // String literal
    int64_t,                        // Integer literal
    double,                         // Float literal
    bool,                           // Boolean literal
    std::shared_ptr<AttributeValueList>  // List of values (recursive via indirection)
>;

// Wrapper for recursive list
struct AttributeValueList {
    std::vector<AttributeValue> values;
};

// Attribute argument
struct AttributeArg {
    std::string name;               // Argument name (empty for positional args)
    AttributeValue value;           // Argument value
    
    AttributeArg(std::string n, AttributeValue v)
        : name(std::move(n)), value(std::move(v)) {}
    
    // Constructor for positional arguments
    explicit AttributeArg(AttributeValue v)
        : name(""), value(std::move(v)) {}
};

// Attribute definition
// Represents an attribute annotation like @attribute_name(args...)
struct Attribute {
    std::string name;               // Attribute name
    std::vector<AttributeArg> args; // Attribute arguments
    
    Attribute(std::string n, std::vector<AttributeArg> a = {})
        : name(std::move(n)), args(std::move(a)) {}
    
    // Get positional argument by index
    std::expected<const AttributeValue*, std::string> 
    get_positional(size_t index) const;
    
    // Get named argument by name
    std::expected<const AttributeValue*, std::string> 
    get_named(const std::string& name) const;
    
    // Check if attribute has a specific argument
    bool has_arg(const std::string& name) const;
    
    // Get number of positional arguments
    size_t positional_count() const;
    
    // Get number of named arguments
    size_t named_count() const;
};

// Attribute target types
enum class AttributeTarget {
    Function,       // Function/method
    Struct,         // Struct/class
    Enum,           // Enum
    Field,          // Struct field
    Parameter,      // Function parameter
    Module,         // Module
    Expression,     // Expression
    Statement,      // Statement
    Type            // Type annotation
};

// Attribute transformer function type
// Takes AST node and attribute, returns transformed AST
using AttributeTransformer = std::function<std::expected<kernel::Value, std::string>(
    const kernel::Value& ast_node,
    const Attribute& attribute,
    MacroExpander& expander
)>;

// Attribute macro definition
class AttributeMacro {
public:
    AttributeMacro(std::string name, 
                   std::vector<AttributeTarget> targets,
                   AttributeTransformer transformer)
        : name_(std::move(name))
        , targets_(std::move(targets))
        , transformer_(std::move(transformer)) {}
    
    const std::string& name() const { return name_; }
    const std::vector<AttributeTarget>& targets() const { return targets_; }
    
    // Check if attribute can be applied to a target
    bool can_apply_to(AttributeTarget target) const;
    
    // Apply attribute transformation
    std::expected<kernel::Value, std::string> 
    apply(const kernel::Value& ast_node, 
          const Attribute& attribute,
          MacroExpander& expander) const;
    
private:
    std::string name_;
    std::vector<AttributeTarget> targets_;
    AttributeTransformer transformer_;
};

// Attribute macro registry
class AttributeMacroRegistry {
public:
    static AttributeMacroRegistry& instance() {
        static AttributeMacroRegistry registry;
        return registry;
    }
    
    // Register an attribute macro
    void register_attribute(std::shared_ptr<AttributeMacro> attribute);
    
    // Look up an attribute macro by name
    std::expected<std::shared_ptr<AttributeMacro>, std::string> 
    get_attribute(const std::string& name) const;
    
    // Check if an attribute macro exists
    bool has_attribute(const std::string& name) const;
    
    // Get all registered attribute macros
    const std::map<std::string, std::shared_ptr<AttributeMacro>>& attributes() const {
        return attributes_;
    }
    
    // Clear all attributes (useful for testing)
    void clear();
    
private:
    AttributeMacroRegistry() = default;
    
    std::map<std::string, std::shared_ptr<AttributeMacro>> attributes_;
    mutable std::mutex mutex_;
};

// Attribute parser
// Parses attribute annotations from source code
class AttributeParser {
public:
    // Parse a single attribute from string
    // e.g., "@inline" or "@deprecated(since = "1.0", note = "Use new_func instead")"
    static std::expected<Attribute, std::string> 
    parse_attribute(const std::string& attr_str);
    
    // Parse multiple attributes from a list
    static std::expected<std::vector<Attribute>, std::string> 
    parse_attributes(const std::vector<std::string>& attr_strs);
    
    // Extract attributes from AST node metadata
    static std::vector<Attribute> 
    extract_attributes(const kernel::Value& ast_node);
    
private:
    // Parse attribute arguments
    static std::expected<std::vector<AttributeArg>, std::string> 
    parse_args(const std::string& args_str);
    
    // Parse a single attribute value
    static std::expected<AttributeValue, std::string> 
    parse_value(const std::string& value_str);
};

// Attribute application context
// Manages attribute application to AST nodes
class AttributeContext {
public:
    // Apply all attributes to an AST node
    static std::expected<kernel::Value, std::string> 
    apply_attributes(const kernel::Value& ast_node,
                    const std::vector<Attribute>& attributes,
                    AttributeTarget target,
                    MacroExpander& expander);
    
    // Apply a single attribute to an AST node
    static std::expected<kernel::Value, std::string> 
    apply_attribute(const kernel::Value& ast_node,
                   const Attribute& attribute,
                   AttributeTarget target,
                   MacroExpander& expander);
    
    // Validate attribute usage
    static std::expected<void, std::string> 
    validate_attribute(const Attribute& attribute,
                      AttributeTarget target);
};

// ============================================================================
// STANDARD ATTRIBUTE MACROS
// ============================================================================

// @inline - Hint to inline function
std::expected<kernel::Value, std::string> 
attr_inline(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander);

// @deprecated - Mark as deprecated
std::expected<kernel::Value, std::string> 
attr_deprecated(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander);

// @test - Mark as test function
std::expected<kernel::Value, std::string> 
attr_test(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander);

// @benchmark - Mark as benchmark function
std::expected<kernel::Value, std::string> 
attr_benchmark(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander);

// @must_use - Warn if return value is unused
std::expected<kernel::Value, std::string> 
attr_must_use(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander);

// @allow - Allow specific warnings
std::expected<kernel::Value, std::string> 
attr_allow(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander);

// @deny - Deny specific warnings (make them errors)
std::expected<kernel::Value, std::string> 
attr_deny(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander);

// @cfg - Conditional compilation
std::expected<kernel::Value, std::string> 
attr_cfg(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander);

// @doc - Documentation comment
std::expected<kernel::Value, std::string> 
attr_doc(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander);

// @repr - Control memory representation
std::expected<kernel::Value, std::string> 
attr_repr(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander);

// @align - Control alignment
std::expected<kernel::Value, std::string> 
attr_align(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander);

// @packed - Pack struct fields
std::expected<kernel::Value, std::string> 
attr_packed(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander);

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Create an attribute macro
std::shared_ptr<AttributeMacro> make_attribute_macro(
    std::string name,
    std::vector<AttributeTarget> targets,
    AttributeTransformer transformer
);

// Register all standard attribute macros
void register_standard_attributes();

// Convert attribute value to string
std::string attribute_value_to_string(const AttributeValue& value);

// Check if attribute value is of a specific type
template<typename T>
bool is_attribute_value_type(const AttributeValue& value) {
    return std::holds_alternative<T>(value);
}

// Get attribute value as a specific type
template<typename T>
std::expected<T, std::string> get_attribute_value(const AttributeValue& value) {
    if (auto* val = std::get_if<T>(&value)) {
        return *val;
    }
    return std::unexpected("Attribute value type mismatch");
}

} // namespace meld::macro
