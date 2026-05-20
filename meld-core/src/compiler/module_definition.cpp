#include "meld/compiler/module_definition.hpp"
#include "meld/kernel/primitives.hpp"
#include "meld/compat/visit.hpp"
#include <algorithm>
#include <sstream>

namespace meld::compiler {

// ============================================================================
// ModuleDefinition
// ============================================================================

ModuleDefinition::ModuleDefinition(const std::string& file_path)
    : file_path_(file_path)
    , module_name_(derive_module_name(file_path))
    , module_path_(derive_module_path(file_path))
{}

std::string ModuleDefinition::derive_module_name(const std::string& file_path) {
    auto parts = derive_module_path(file_path);
    if (parts.empty()) return "";
    
    std::ostringstream oss;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) oss << ".";
        oss << parts[i];
    }
    return oss.str();
}

std::vector<std::string> ModuleDefinition::derive_module_path(const std::string& file_path) {
    std::string path = file_path;
    
    // Normalize separators to '/'
    std::replace(path.begin(), path.end(), '\\', '/');
    
    // Strip .meld extension
    const std::string ext = ".meld";
    if (path.size() >= ext.size() && 
        path.compare(path.size() - ext.size(), ext.size(), ext) == 0) {
        path = path.substr(0, path.size() - ext.size());
    }
    
    // Strip leading "./" if present
    if (path.starts_with("./")) {
        path = path.substr(2);
    }
    
    // Split on '/'
    std::vector<std::string> parts;
    std::istringstream iss(path);
    std::string segment;
    while (std::getline(iss, segment, '/')) {
        if (!segment.empty()) {
            parts.push_back(segment);
        }
    }
    
    return parts;
}

void ModuleDefinition::build_from_expressions(
    const std::vector<parser::ast::expression>& expressions,
    const std::unordered_set<std::string>& private_syms)
{
    namespace x3 = boost::spirit::x3;
    
    for (const auto& expr : expressions) {
        meld::compat::visit([&](auto&& node) {
            using T = std::decay_t<decltype(node)>;
            
            // val declaration
            if constexpr (std::is_same_v<T, x3::forward_ast<parser::ast::val_declaration>>) {
                const auto& decl = node.get();
                auto vis = private_syms.contains(decl.name.name)
                    ? SymbolVisibility::PRIVATE : SymbolVisibility::PUBLIC;
                add_symbol(decl.name.name, ModuleSymbol::Kind::VAL, vis);
            }
            // var declaration
            else if constexpr (std::is_same_v<T, x3::forward_ast<parser::ast::var_declaration>>) {
                const auto& decl = node.get();
                auto vis = private_syms.contains(decl.name.name)
                    ? SymbolVisibility::PRIVATE : SymbolVisibility::PUBLIC;
                add_symbol(decl.name.name, ModuleSymbol::Kind::VAR, vis);
            }
            // function definition
            else if constexpr (std::is_same_v<T, x3::forward_ast<parser::ast::function_definition>>) {
                const auto& def = node.get();
                auto vis = private_syms.contains(def.name.name)
                    ? SymbolVisibility::PRIVATE : SymbolVisibility::PUBLIC;
                add_symbol(def.name.name, ModuleSymbol::Kind::FUNCTION, vis);
            }
            // struct definition
            else if constexpr (std::is_same_v<T, x3::forward_ast<parser::ast::struct_definition>>) {
                const auto& def = node.get();
                auto vis = private_syms.contains(def.name.name)
                    ? SymbolVisibility::PRIVATE : SymbolVisibility::PUBLIC;
                add_symbol(def.name.name, ModuleSymbol::Kind::STRUCT, vis);
            }
            // class definition
            else if constexpr (std::is_same_v<T, x3::forward_ast<parser::ast::class_definition>>) {
                const auto& def = node.get();
                auto vis = private_syms.contains(def.name.name)
                    ? SymbolVisibility::PRIVATE : SymbolVisibility::PUBLIC;
                add_symbol(def.name.name, ModuleSymbol::Kind::CLASS, vis);
            }
            // enum definition
            else if constexpr (std::is_same_v<T, x3::forward_ast<parser::ast::enum_definition>>) {
                const auto& def = node.get();
                auto vis = private_syms.contains(def.name.name)
                    ? SymbolVisibility::PRIVATE : SymbolVisibility::PUBLIC;
                add_symbol(def.name.name, ModuleSymbol::Kind::ENUM, vis);
            }
            // Import declarations and other expressions are not exported symbols
    // However, imp declarations at top level constitute re-exports:
    // symbols imported via imp become part of this module's public API
    // (per Requirement 31F.20)
    else if constexpr (std::is_same_v<T, x3::forward_ast<parser::ast::import_declaration>>) {
        const auto& imp_decl = node.get();
        if (imp_decl.is_imp && imp_decl.import_type == parser::ast::ImportType::IMP_DESTRUCTURED) {
            // Destructured imports re-export the named symbols
            for (const auto& sym : imp_decl.symbols) {
                auto vis = private_syms.contains(sym.local_name)
                    ? SymbolVisibility::PRIVATE : SymbolVisibility::PUBLIC;
                add_symbol(sym.local_name, ModuleSymbol::Kind::VAL, vis);
            }
        }
    }
    // Other expression types are not exported symbols
        }, expr);
    }
}

void ModuleDefinition::add_symbol(const std::string& name, ModuleSymbol::Kind kind,
                                   SymbolVisibility visibility) {
    // Avoid duplicates
    if (symbol_index_.contains(name)) return;
    
    ModuleSymbol sym;
    sym.name = name;
    sym.kind = kind;
    sym.visibility = visibility;
    
    symbol_index_[name] = symbols_.size();
    symbols_.push_back(std::move(sym));
}

std::vector<const ModuleSymbol*> ModuleDefinition::public_symbols() const {
    std::vector<const ModuleSymbol*> result;
    for (const auto& sym : symbols_) {
        if (sym.visibility == SymbolVisibility::PUBLIC) {
            result.push_back(&sym);
        }
    }
    return result;
}

std::vector<const ModuleSymbol*> ModuleDefinition::private_symbols() const {
    std::vector<const ModuleSymbol*> result;
    for (const auto& sym : symbols_) {
        if (sym.visibility == SymbolVisibility::PRIVATE) {
            result.push_back(&sym);
        }
    }
    return result;
}

bool ModuleDefinition::is_exported(const std::string& name) const {
    auto it = symbol_index_.find(name);
    if (it == symbol_index_.end()) return false;
    return symbols_[it->second].visibility == SymbolVisibility::PUBLIC;
}

bool ModuleDefinition::has_symbol(const std::string& name) const {
    return symbol_index_.contains(name);
}

const ModuleSymbol* ModuleDefinition::find_symbol(const std::string& name) const {
    auto it = symbol_index_.find(name);
    if (it == symbol_index_.end()) return nullptr;
    return &symbols_[it->second];
}

void ModuleDefinition::register_in_namespace(
    std::shared_ptr<kernel::NamespaceScope> scope) const
{
    for (const auto& sym : symbols_) {
        if (sym.visibility == SymbolVisibility::PUBLIC) {
            // Create a placeholder symbol in the namespace
            auto kernel_sym = std::make_shared<kernel::Symbol>(sym.name);
            scope->register_symbol(sym.name, kernel_sym);
        }
    }
}

// ============================================================================
// ModuleRegistry
// ============================================================================

void ModuleRegistry::register_module(std::shared_ptr<ModuleDefinition> module) {
    std::lock_guard<std::mutex> lock(mutex_);
    modules_[module->module_name()] = module;
    path_to_module_[module->file_path()] = module;
}

std::shared_ptr<ModuleDefinition> ModuleRegistry::find_module(
    const std::string& module_name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = modules_.find(module_name);
    if (it != modules_.end()) return it->second;
    return nullptr;
}

std::shared_ptr<ModuleDefinition> ModuleRegistry::find_module_by_path(
    const std::string& file_path) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = path_to_module_.find(file_path);
    if (it != path_to_module_.end()) return it->second;
    return nullptr;
}

void ModuleRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    modules_.clear();
    path_to_module_.clear();
}

} // namespace meld::compiler
