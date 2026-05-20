#include "meld/macro/attribute.hpp"
#include "meld/kernel/operations.hpp"
#include <format>
#include <sstream>
#include <algorithm>
#include <regex>

namespace meld::macro {

// ============================================================================
// Attribute Implementation
// ============================================================================

std::expected<const AttributeValue*, std::string> 
Attribute::get_positional(size_t index) const {
    size_t pos_count = 0;
    for (const auto& arg : args) {
        if (arg.name.empty()) {
            if (pos_count == index) {
                return &arg.value;
            }
            pos_count++;
        }
    }
    return std::unexpected(std::format("Positional argument {} not found", index));
}

std::expected<const AttributeValue*, std::string> 
Attribute::get_named(const std::string& name) const {
    for (const auto& arg : args) {
        if (arg.name == name) {
            return &arg.value;
        }
    }
    return std::unexpected(std::format("Named argument '{}' not found", name));
}

bool Attribute::has_arg(const std::string& name) const {
    return std::any_of(args.begin(), args.end(),
                      [&name](const AttributeArg& arg) { return arg.name == name; });
}

size_t Attribute::positional_count() const {
    return std::count_if(args.begin(), args.end(),
                        [](const AttributeArg& arg) { return arg.name.empty(); });
}

size_t Attribute::named_count() const {
    return std::count_if(args.begin(), args.end(),
                        [](const AttributeArg& arg) { return !arg.name.empty(); });
}

// ============================================================================
// AttributeMacro Implementation
// ============================================================================

bool AttributeMacro::can_apply_to(AttributeTarget target) const {
    return std::find(targets_.begin(), targets_.end(), target) != targets_.end();
}

std::expected<kernel::Value, std::string> 
AttributeMacro::apply(const kernel::Value& ast_node, 
                     const Attribute& attribute,
                     MacroExpander& expander) const {
    return transformer_(ast_node, attribute, expander);
}

// ============================================================================
// AttributeMacroRegistry Implementation
// ============================================================================

void AttributeMacroRegistry::register_attribute(std::shared_ptr<AttributeMacro> attribute) {
    std::lock_guard<std::mutex> lock(mutex_);
    attributes_[attribute->name()] = std::move(attribute);
}

std::expected<std::shared_ptr<AttributeMacro>, std::string> 
AttributeMacroRegistry::get_attribute(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = attributes_.find(name);
    if (it != attributes_.end()) {
        return it->second;
    }
    return std::unexpected(std::format("Attribute macro '{}' not found", name));
}

bool AttributeMacroRegistry::has_attribute(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return attributes_.contains(name);
}

void AttributeMacroRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    attributes_.clear();
}

// ============================================================================
// AttributeParser Implementation
// ============================================================================

std::expected<Attribute, std::string> 
AttributeParser::parse_attribute(const std::string& attr_str) {
    // Parse @attribute_name or @attribute_name(args...)
    
    // Remove leading @ if present
    std::string str = attr_str;
    if (!str.empty() && str[0] == '@') {
        str = str.substr(1);
    }
    
    // Find opening parenthesis
    size_t paren_pos = str.find('(');
    
    if (paren_pos == std::string::npos) {
        // No arguments
        return Attribute(str);
    }
    
    // Extract name and arguments
    std::string name = str.substr(0, paren_pos);
    
    // Find matching closing parenthesis
    size_t close_paren = str.rfind(')');
    if (close_paren == std::string::npos) {
        return std::unexpected("Missing closing parenthesis in attribute");
    }
    
    std::string args_str = str.substr(paren_pos + 1, close_paren - paren_pos - 1);
    
    // Parse arguments
    auto args_result = parse_args(args_str);
    if (!args_result) {
        return std::unexpected(args_result.error());
    }
    
    return Attribute(name, *args_result);
}

std::expected<std::vector<Attribute>, std::string> 
AttributeParser::parse_attributes(const std::vector<std::string>& attr_strs) {
    std::vector<Attribute> attributes;
    
    for (const auto& attr_str : attr_strs) {
        auto result = parse_attribute(attr_str);
        if (!result) {
            return std::unexpected(result.error());
        }
        attributes.push_back(*result);
    }
    
    return attributes;
}

std::vector<Attribute> 
AttributeParser::extract_attributes(const kernel::Value& ast_node) {
    // Extract attributes from AST node metadata
    // In a full implementation, this would parse actual attribute annotations
    
    std::vector<Attribute> attributes;
    
    // Placeholder implementation
    // TODO: Extract actual attributes from AST metadata
    
    return attributes;
}

std::expected<std::vector<AttributeArg>, std::string> 
AttributeParser::parse_args(const std::string& args_str) {
    std::vector<AttributeArg> args;
    
    if (args_str.empty()) {
        return args;
    }
    
    // Simple argument parser
    // Handles: value, name = value, "string", 123, true/false
    
    std::istringstream iss(args_str);
    std::string token;
    
    while (std::getline(iss, token, ',')) {
        // Trim whitespace
        token.erase(0, token.find_first_not_of(" \t\n\r"));
        token.erase(token.find_last_not_of(" \t\n\r") + 1);
        
        if (token.empty()) continue;
        
        // Check for named argument (name = value)
        size_t eq_pos = token.find('=');
        if (eq_pos != std::string::npos) {
            std::string name = token.substr(0, eq_pos);
            std::string value_str = token.substr(eq_pos + 1);
            
            // Trim whitespace
            name.erase(0, name.find_first_not_of(" \t\n\r"));
            name.erase(name.find_last_not_of(" \t\n\r") + 1);
            value_str.erase(0, value_str.find_first_not_of(" \t\n\r"));
            value_str.erase(value_str.find_last_not_of(" \t\n\r") + 1);
            
            auto value_result = parse_value(value_str);
            if (!value_result) {
                return std::unexpected(value_result.error());
            }
            
            args.push_back(AttributeArg(name, *value_result));
        } else {
            // Positional argument
            auto value_result = parse_value(token);
            if (!value_result) {
                return std::unexpected(value_result.error());
            }
            
            args.push_back(AttributeArg(*value_result));
        }
    }
    
    return args;
}

std::expected<AttributeValue, std::string> 
AttributeParser::parse_value(const std::string& value_str) {
    // Parse attribute value
    
    // String literal
    if (!value_str.empty() && value_str.front() == '"' && value_str.back() == '"') {
        return AttributeValue(value_str.substr(1, value_str.length() - 2));
    }
    
    // Boolean
    if (value_str == "true") {
        return AttributeValue(true);
    }
    if (value_str == "false") {
        return AttributeValue(false);
    }
    
    // Try to parse as integer
    try {
        size_t pos;
        int64_t int_val = std::stoll(value_str, &pos);
        if (pos == value_str.length()) {
            return AttributeValue(int_val);
        }
    } catch (...) {}
    
    // Try to parse as float
    try {
        size_t pos;
        double float_val = std::stod(value_str, &pos);
        if (pos == value_str.length()) {
            return AttributeValue(float_val);
        }
    } catch (...) {}
    
    // Default to string
    return AttributeValue(value_str);
}

// ============================================================================
// AttributeContext Implementation
// ============================================================================

std::expected<kernel::Value, std::string> 
AttributeContext::apply_attributes(
    const kernel::Value& ast_node,
    const std::vector<Attribute>& attributes,
    AttributeTarget target,
    MacroExpander& expander) {
    
    kernel::Value current = ast_node;
    
    // Apply each attribute in sequence
    for (const auto& attr : attributes) {
        auto result = apply_attribute(current, attr, target, expander);
        if (!result) {
            return result;
        }
        current = *result;
    }
    
    return current;
}

std::expected<kernel::Value, std::string> 
AttributeContext::apply_attribute(
    const kernel::Value& ast_node,
    const Attribute& attribute,
    AttributeTarget target,
    MacroExpander& expander) {
    
    // Validate attribute usage
    auto validation = validate_attribute(attribute, target);
    if (!validation) {
        return std::unexpected(validation.error());
    }
    
    // Look up attribute macro
    auto attr_result = AttributeMacroRegistry::instance().get_attribute(attribute.name);
    if (!attr_result) {
        return std::unexpected(attr_result.error());
    }
    
    auto attr_macro = *attr_result;
    
    // Check if attribute can be applied to this target
    if (!attr_macro->can_apply_to(target)) {
        return std::unexpected(
            std::format("Attribute '{}' cannot be applied to this target", attribute.name)
        );
    }
    
    // Apply attribute transformation
    return attr_macro->apply(ast_node, attribute, expander);
}

std::expected<void, std::string> 
AttributeContext::validate_attribute(
    const Attribute& attribute,
    AttributeTarget target) {
    
    // Validate attribute arguments and usage
    // In a full implementation, this would check:
    // - Required arguments are present
    // - Argument types are correct
    // - Attribute is applicable to target
    
    return {};
}

// ============================================================================
// Standard Attribute Macros
// ============================================================================

std::expected<kernel::Value, std::string> 
attr_inline(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander) {
    // Add inline hint to function
    // In a full implementation, this would modify the AST to include inline metadata
    
    auto inline_sym = std::make_shared<kernel::Symbol>("inline");
    auto metadata = kernel::cons(kernel::Value(inline_sym), ast_node);
    return metadata;
}

std::expected<kernel::Value, std::string> 
attr_deprecated(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander) {
    // Mark as deprecated
    // Extract optional "since" and "note" arguments
    
    std::string since = "";
    std::string note = "";
    
    if (auto since_val = attr.get_named("since")) {
        if (auto str = get_attribute_value<std::string>(**since_val)) {
            since = *str;
        }
    }
    
    if (auto note_val = attr.get_named("note")) {
        if (auto str = get_attribute_value<std::string>(**note_val)) {
            note = *str;
        }
    }
    
    // Add deprecation metadata
    auto deprecated_sym = std::make_shared<kernel::Symbol>("deprecated");
    auto since_sym = std::make_shared<kernel::Symbol>(since);
    auto note_sym = std::make_shared<kernel::Symbol>(note);
    
    auto metadata = kernel::cons(
        kernel::Value(deprecated_sym),
        kernel::cons(
            kernel::Value(since_sym),
            kernel::cons(
                kernel::Value(note_sym),
                ast_node
            )
        )
    );
    
    return metadata;
}

std::expected<kernel::Value, std::string> 
attr_test(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander) {
    // Mark as test function
    auto test_sym = std::make_shared<kernel::Symbol>("test");
    auto metadata = kernel::cons(kernel::Value(test_sym), ast_node);
    return metadata;
}

std::expected<kernel::Value, std::string> 
attr_benchmark(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander) {
    // Mark as benchmark function
    auto benchmark_sym = std::make_shared<kernel::Symbol>("benchmark");
    auto metadata = kernel::cons(kernel::Value(benchmark_sym), ast_node);
    return metadata;
}

std::expected<kernel::Value, std::string> 
attr_must_use(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander) {
    // Warn if return value is unused
    auto must_use_sym = std::make_shared<kernel::Symbol>("must_use");
    auto metadata = kernel::cons(kernel::Value(must_use_sym), ast_node);
    return metadata;
}

std::expected<kernel::Value, std::string> 
attr_allow(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander) {
    // Allow specific warnings
    auto allow_sym = std::make_shared<kernel::Symbol>("allow");
    auto metadata = kernel::cons(kernel::Value(allow_sym), ast_node);
    return metadata;
}

std::expected<kernel::Value, std::string> 
attr_deny(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander) {
    // Deny specific warnings (make them errors)
    auto deny_sym = std::make_shared<kernel::Symbol>("deny");
    auto metadata = kernel::cons(kernel::Value(deny_sym), ast_node);
    return metadata;
}

std::expected<kernel::Value, std::string> 
attr_cfg(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander) {
    // Conditional compilation
    // In a full implementation, this would evaluate the condition and
    // either include or exclude the AST node
    
    auto cfg_sym = std::make_shared<kernel::Symbol>("cfg");
    auto metadata = kernel::cons(kernel::Value(cfg_sym), ast_node);
    return metadata;
}

std::expected<kernel::Value, std::string> 
attr_doc(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander) {
    // Documentation comment
    auto doc_sym = std::make_shared<kernel::Symbol>("doc");
    auto metadata = kernel::cons(kernel::Value(doc_sym), ast_node);
    return metadata;
}

std::expected<kernel::Value, std::string> 
attr_repr(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander) {
    // Control memory representation
    auto repr_sym = std::make_shared<kernel::Symbol>("repr");
    auto metadata = kernel::cons(kernel::Value(repr_sym), ast_node);
    return metadata;
}

std::expected<kernel::Value, std::string> 
attr_align(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander) {
    // Control alignment
    auto align_sym = std::make_shared<kernel::Symbol>("align");
    auto metadata = kernel::cons(kernel::Value(align_sym), ast_node);
    return metadata;
}

std::expected<kernel::Value, std::string> 
attr_packed(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander) {
    // Pack struct fields
    auto packed_sym = std::make_shared<kernel::Symbol>("packed");
    auto metadata = kernel::cons(kernel::Value(packed_sym), ast_node);
    return metadata;
}

std::expected<kernel::Value, std::string> 
attr_private(const kernel::Value& ast_node, const Attribute& attr, MacroExpander& expander) {
    // Mark a declaration as module-private (not exported)
    auto private_sym = std::make_shared<kernel::Symbol>("private");
    auto metadata = kernel::cons(kernel::Value(private_sym), ast_node);
    return metadata;
}

// ============================================================================
// Helper Functions
// ============================================================================

std::shared_ptr<AttributeMacro> make_attribute_macro(
    std::string name,
    std::vector<AttributeTarget> targets,
    AttributeTransformer transformer) {
    
    return std::make_shared<AttributeMacro>(
        std::move(name),
        std::move(targets),
        std::move(transformer)
    );
}

void register_standard_attributes() {
    auto& registry = AttributeMacroRegistry::instance();
    
    // Register standard attribute macros
    registry.register_attribute(make_attribute_macro(
        "inline",
        {AttributeTarget::Function},
        attr_inline
    ));
    
    registry.register_attribute(make_attribute_macro(
        "deprecated",
        {AttributeTarget::Function, AttributeTarget::Struct, AttributeTarget::Enum},
        attr_deprecated
    ));
    
    registry.register_attribute(make_attribute_macro(
        "test",
        {AttributeTarget::Function},
        attr_test
    ));
    
    registry.register_attribute(make_attribute_macro(
        "benchmark",
        {AttributeTarget::Function},
        attr_benchmark
    ));
    
    registry.register_attribute(make_attribute_macro(
        "must_use",
        {AttributeTarget::Function},
        attr_must_use
    ));
    
    registry.register_attribute(make_attribute_macro(
        "allow",
        {AttributeTarget::Function, AttributeTarget::Struct, AttributeTarget::Module},
        attr_allow
    ));
    
    registry.register_attribute(make_attribute_macro(
        "deny",
        {AttributeTarget::Function, AttributeTarget::Struct, AttributeTarget::Module},
        attr_deny
    ));
    
    registry.register_attribute(make_attribute_macro(
        "cfg",
        {AttributeTarget::Function, AttributeTarget::Struct, AttributeTarget::Module},
        attr_cfg
    ));
    
    registry.register_attribute(make_attribute_macro(
        "doc",
        {AttributeTarget::Function, AttributeTarget::Struct, AttributeTarget::Enum, AttributeTarget::Field},
        attr_doc
    ));
    
    registry.register_attribute(make_attribute_macro(
        "repr",
        {AttributeTarget::Struct, AttributeTarget::Enum},
        attr_repr
    ));
    
    registry.register_attribute(make_attribute_macro(
        "align",
        {AttributeTarget::Struct, AttributeTarget::Field},
        attr_align
    ));
    
    registry.register_attribute(make_attribute_macro(
        "packed",
        {AttributeTarget::Struct},
        attr_packed
    ));
    
    // Module visibility: @private restricts a declaration to its defining module
    registry.register_attribute(make_attribute_macro(
        "private",
        {AttributeTarget::Function, AttributeTarget::Struct, AttributeTarget::Enum,
         AttributeTarget::Field, AttributeTarget::Module},
        attr_private
    ));
}

std::string attribute_value_to_string(const AttributeValue& value) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return "\"" + arg + "\"";
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::shared_ptr<AttributeValueList>>) {
            std::ostringstream oss;
            oss << "[";
            for (size_t i = 0; i < arg->values.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << attribute_value_to_string(arg->values[i]);
            }
            oss << "]";
            return oss.str();
        }
        return "<unknown>";
    }, value);
}

} // namespace meld::macro
