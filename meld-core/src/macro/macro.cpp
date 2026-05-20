#include "meld/macro/macro.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/kernel/symbol_table.hpp"
#include <format>

namespace meld::macro {

// MacroScope implementation
void MacroScope::push_scope() {
    scopes_.emplace_back();
}

void MacroScope::pop_scope() {
    if (!scopes_.empty()) {
        scopes_.pop_back();
    }
}

void MacroScope::register_symbol(const std::string& name, std::shared_ptr<kernel::Symbol> symbol) {
    if (scopes_.empty()) {
        push_scope();
    }
    scopes_.back()[name] = symbol;
}

std::shared_ptr<kernel::Symbol> MacroScope::lookup_symbol(const std::string& name) const {
    // Search from innermost to outermost scope
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second;
        }
    }
    return nullptr;
}

bool MacroScope::has_symbol(const std::string& name) const {
    return lookup_symbol(name) != nullptr;
}

// Macro implementation
std::expected<kernel::Value, std::string> 
Macro::apply(const kernel::Value& ast_node, MacroExpander& expander) const {
    return transformer_(ast_node, expander);
}

// MacroRegistry implementation
void MacroRegistry::register_macro(std::shared_ptr<Macro> macro) {
    std::lock_guard<std::mutex> lock(mutex_);
    macros_[macro->name()] = std::move(macro);
}

std::expected<std::shared_ptr<Macro>, std::string> 
MacroRegistry::get_macro(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = macros_.find(name);
    if (it != macros_.end()) {
        return it->second;
    }
    return std::unexpected(std::format("Macro '{}' not found", name));
}

bool MacroRegistry::has_macro(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return macros_.contains(name);
}

void MacroRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    macros_.clear();
}

// MacroExpander implementation
std::expected<kernel::Value, std::string> 
MacroExpander::expand(const kernel::Value& ast_node) {
    // Check recursion depth
    if (expansion_depth_ >= MAX_EXPANSION_DEPTH) {
        return std::unexpected("Macro expansion depth exceeded (possible infinite recursion)");
    }
    
    // If this is a macro call, expand it
    if (is_macro_call(ast_node)) {
        return expand_macro_call(ast_node);
    }
    
    // Otherwise, return as-is
    return ast_node;
}

std::expected<kernel::Value, std::string> 
MacroExpander::expand_recursive(const kernel::Value& ast_node) {
    // First, try to expand this node
    auto expanded = expand(ast_node);
    if (!expanded) {
        return expanded;
    }
    
    // If it's a list, recursively expand all elements
    if (expanded->is<kernel::Cons>()) {
        auto list_result = kernel::list_to_array(*expanded);
        if (!list_result) {
            return std::unexpected(list_result.error());
        }
        
        std::vector<kernel::Value> expanded_elements;
        for (const auto& elem : *list_result) {
            auto expanded_elem = expand_recursive(elem);
            if (!expanded_elem) {
                return expanded_elem;
            }
            expanded_elements.push_back(*expanded_elem);
        }
        
        return kernel::list(expanded_elements);
    }
    
    return *expanded;
}

bool MacroExpander::is_macro_call(const kernel::Value& ast_node) const {
    // A macro call is a list where the first element is a symbol
    // that names a registered macro
    if (!ast_node.is<kernel::Cons>()) {
        return false;
    }
    
    auto car_result = kernel::car(ast_node);
    if (!car_result || !car_result->is<kernel::Symbol>()) {
        return false;
    }
    
    auto sym = car_result->as<kernel::Symbol>();
    return MacroRegistry::instance().has_macro(sym->name());
}

std::expected<std::string, std::string> 
MacroExpander::get_macro_name(const kernel::Value& ast_node) const {
    if (!ast_node.is<kernel::Cons>()) {
        return std::unexpected("Expected list for macro call");
    }
    
    auto car_result = kernel::car(ast_node);
    if (!car_result) {
        return std::unexpected(car_result.error());
    }
    
    if (!car_result->is<kernel::Symbol>()) {
        return std::unexpected("Expected symbol as first element of macro call");
    }
    
    return car_result->as<kernel::Symbol>()->name();
}

std::expected<std::vector<kernel::Value>, std::string> 
MacroExpander::get_macro_args(const kernel::Value& ast_node) const {
    if (!ast_node.is<kernel::Cons>()) {
        return std::unexpected("Expected list for macro call");
    }
    
    // Get the cdr (rest of the list after the macro name)
    auto cdr_result = kernel::cdr(ast_node);
    if (!cdr_result) {
        return std::unexpected(cdr_result.error());
    }
    
    // Convert to array
    return kernel::list_to_array(*cdr_result);
}

std::expected<kernel::Value, std::string> 
MacroExpander::expand_macro_call(const kernel::Value& ast_node) {
    // Increment depth
    ++expansion_depth_;
    
    // Get macro name
    auto name_result = get_macro_name(ast_node);
    if (!name_result) {
        --expansion_depth_;
        return std::unexpected(name_result.error());
    }
    
    // Look up macro
    auto macro_result = MacroRegistry::instance().get_macro(*name_result);
    if (!macro_result) {
        --expansion_depth_;
        return std::unexpected(macro_result.error());
    }
    
    // Push a new scope for hygienic expansion
    scope_.push_scope();
    
    // Apply macro transformation
    auto result = (*macro_result)->apply(ast_node, *this);
    
    // Pop scope
    scope_.pop_scope();
    
    // Decrement depth
    --expansion_depth_;
    
    if (!result) {
        return result;
    }
    
    // Recursively expand the result
    return expand(*result);
}

// Hygienic macro expansion support
std::shared_ptr<kernel::Symbol> MacroExpander::gensym(const std::string& prefix) {
    return kernel::gensym(prefix);
}

kernel::Value MacroExpander::rename_symbols(
    const kernel::Value& ast_node,
    const std::map<std::string, std::shared_ptr<kernel::Symbol>>& renames) {
    
    // If it's a symbol, check if it needs renaming
    if (ast_node.is<kernel::Symbol>()) {
        auto sym = ast_node.as<kernel::Symbol>();
        
        // Don't rename unhygienic symbols
        if (is_unhygienic(sym->name())) {
            return ast_node;
        }
        
        // Check if this symbol should be renamed
        auto it = renames.find(sym->name());
        if (it != renames.end()) {
            return kernel::Value(it->second);
        }
        return ast_node;
    }
    
    // If it's a list, recursively rename all elements
    if (ast_node.is<kernel::Cons>()) {
        auto list_result = kernel::list_to_array(ast_node);
        if (!list_result) {
            return ast_node;
        }
        
        std::vector<kernel::Value> renamed_elements;
        for (const auto& elem : *list_result) {
            renamed_elements.push_back(rename_symbols(elem, renames));
        }
        
        return kernel::list(renamed_elements);
    }
    
    // Other types don't need renaming
    return ast_node;
}

void MacroExpander::mark_unhygienic(const std::string& symbol_name) {
    unhygienic_symbols_.insert(symbol_name);
}

bool MacroExpander::is_unhygienic(const std::string& symbol_name) const {
    return unhygienic_symbols_.contains(symbol_name);
}

// Helper functions
std::shared_ptr<Macro> make_macro(
    std::string name,
    std::vector<std::string> params,
    MacroTransformer transformer) {
    
    return std::make_shared<Macro>(
        std::move(name),
        std::move(params),
        std::move(transformer)
    );
}

std::shared_ptr<Macro> make_simple_macro(
    std::string name,
    std::vector<std::string> params,
    std::function<kernel::Value(const std::vector<kernel::Value>&)> body) {
    
    auto transformer = [body = std::move(body), param_names = params](
        const kernel::Value& ast_node,
        MacroExpander& expander) -> std::expected<kernel::Value, std::string> {
        
        // Extract arguments
        auto args_result = expander.get_macro_args(ast_node);
        if (!args_result) {
            return std::unexpected(args_result.error());
        }
        
        // Check argument count
        if (args_result->size() != param_names.size()) {
            return std::unexpected(std::format(
                "Macro expects {} arguments, got {}",
                param_names.size(),
                args_result->size()
            ));
        }
        
        // Apply transformation
        return body(*args_result);
    };
    
    return make_macro(std::move(name), std::move(params), std::move(transformer));
}

} // namespace meld::macro
