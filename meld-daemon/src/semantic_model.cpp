#include "meld/daemon/semantic_model.hpp"
#include <algorithm>
#include <functional>

namespace meld::daemon {

void SemanticModel::update_file(const std::filesystem::path& path, FileSemantics semantics) {
    std::unique_lock lock(mutex_);

    // Remove old index entries for this file
    for (auto& [name, locs] : symbol_index_) {
        std::erase_if(locs, [&](const SymbolLocation& loc) {
            return loc.file == path;
        });
    }

    // Add new index entries from the AST
    if (semantics.ast) {
        std::function<void(const std::shared_ptr<ASTNode>&)> index_node;
        index_node = [&](const std::shared_ptr<ASTNode>& node) {
            if (!node) return;
            if (!node->name.empty()) {
                symbol_index_[node->name].push_back({
                    path, node->location.line, node->location.column,
                    node->kind, node->name
                });
            }
            for (const auto& child : node->children) {
                index_node(child);
            }
        };
        index_node(semantics.ast);
    }

    files_[path.string()] = std::move(semantics);
}

void SemanticModel::remove_file(const std::filesystem::path& path) {
    std::unique_lock lock(mutex_);

    // Remove index entries for this file
    for (auto& [name, locs] : symbol_index_) {
        std::erase_if(locs, [&](const SymbolLocation& loc) {
            return loc.file == path;
        });
    }

    files_.erase(path.string());
}

void SemanticModel::clear() {
    std::unique_lock lock(mutex_);
    files_.clear();
    symbol_index_.clear();
}

std::shared_ptr<ASTNode> SemanticModel::get_ast(const std::filesystem::path& path) const {
    std::shared_lock lock(mutex_);
    auto it = files_.find(path.string());
    if (it == files_.end()) return nullptr;
    return it->second.ast;
}

std::vector<Diagnostic> SemanticModel::get_diagnostics(const std::filesystem::path& path) const {
    std::shared_lock lock(mutex_);
    auto it = files_.find(path.string());
    if (it == files_.end()) return {};
    return it->second.diagnostics;
}

std::vector<Diagnostic> SemanticModel::get_all_diagnostics() const {
    std::shared_lock lock(mutex_);
    std::vector<Diagnostic> all;
    for (const auto& [_, semantics] : files_) {
        all.insert(all.end(), semantics.diagnostics.begin(), semantics.diagnostics.end());
    }
    return all;
}

std::vector<std::filesystem::path> SemanticModel::get_indexed_files() const {
    std::shared_lock lock(mutex_);
    std::vector<std::filesystem::path> paths;
    paths.reserve(files_.size());
    for (const auto& [path_str, _] : files_) {
        paths.emplace_back(path_str);
    }
    return paths;
}


bool SemanticModel::has_file(const std::filesystem::path& path) const {
    std::shared_lock lock(mutex_);
    return files_.count(path.string()) > 0;
}

size_t SemanticModel::file_count() const {
    std::shared_lock lock(mutex_);
    return files_.size();
}

std::optional<TypeInfo> SemanticModel::query_type(const std::filesystem::path& path,
                                                   const std::string& symbol) const {
    std::shared_lock lock(mutex_);
    auto it = files_.find(path.string());
    if (it == files_.end()) return std::nullopt;

    // Search AST for the symbol and return its type info
    std::function<std::optional<TypeInfo>(const std::shared_ptr<ASTNode>&)> find_symbol;
    find_symbol = [&](const std::shared_ptr<ASTNode>& node) -> std::optional<TypeInfo> {
        if (!node) return std::nullopt;
        if (node->name == symbol && !node->type_info.empty()) {
            return TypeInfo{node->name, node->type_info, {}};
        }
        for (const auto& child : node->children) {
            if (auto result = find_symbol(child)) return result;
        }
        return std::nullopt;
    };

    return find_symbol(it->second.ast);
}

std::optional<EffectInfo> SemanticModel::query_effects(const std::filesystem::path& path,
                                                        uint32_t line) const {
    std::shared_lock lock(mutex_);
    auto it = files_.find(path.string());
    if (it == files_.end()) return std::nullopt;

    // Search AST for a node at the given line and return its effects
    std::function<std::optional<EffectInfo>(const std::shared_ptr<ASTNode>&)> find_at_line;
    find_at_line = [&](const std::shared_ptr<ASTNode>& node) -> std::optional<EffectInfo> {
        if (!node) return std::nullopt;
        if (node->location.line == line && !node->effects.empty()) {
            return EffectInfo{node->effects, {}};
        }
        for (const auto& child : node->children) {
            if (auto result = find_at_line(child)) return result;
        }
        return std::nullopt;
    };

    return find_at_line(it->second.ast);
}

std::optional<OwnershipInfo> SemanticModel::query_ownership(const std::filesystem::path& path,
                                                             const std::string& symbol) const {
    std::shared_lock lock(mutex_);
    auto it = files_.find(path.string());
    if (it == files_.end()) return std::nullopt;

    // Search AST for the symbol and infer ownership from type info
    std::function<std::optional<OwnershipInfo>(const std::shared_ptr<ASTNode>&)> find_ownership;
    find_ownership = [&](const std::shared_ptr<ASTNode>& node) -> std::optional<OwnershipInfo> {
        if (!node) return std::nullopt;
        if (node->name == symbol && !node->type_info.empty()) {
            OwnershipKind kind = OwnershipKind::Unknown;
            if (node->type_info.find("Own[") != std::string::npos) {
                kind = OwnershipKind::Own;
            } else if (node->type_info.find("Link[") != std::string::npos) {
                kind = OwnershipKind::Link;
            }
            return OwnershipInfo{kind, LifecycleState::Valid, "local"};
        }
        for (const auto& child : node->children) {
            if (auto result = find_ownership(child)) return result;
        }
        return std::nullopt;
    };

    return find_ownership(it->second.ast);
}

std::string SemanticModel::get_symbol_at(const std::filesystem::path& path,
                                          uint32_t line, uint32_t column) const {
    std::shared_lock lock(mutex_);
    auto it = files_.find(path.string());
    if (it == files_.end()) return "";

    // Walk the AST to find the deepest node whose location matches the position.
    // We look for the node closest to (line, column) that has a non-empty name.
    // Prefer declarations over references when multiple nodes match.
    std::string best_name;
    bool best_is_declaration = false;
    std::function<void(const std::shared_ptr<ASTNode>&)> find_at;
    find_at = [&](const std::shared_ptr<ASTNode>& node) {
        if (!node) return;
        if (node->location.line == line && !node->name.empty()) {
            // Accept if column is within the name span (half-open interval)
            if (column >= node->location.column &&
                column < node->location.column + node->name.size()) {
                bool is_decl = (node->kind != "reference");
                // Prefer declarations over references
                if (best_name.empty() || (is_decl && !best_is_declaration)) {
                    best_name = node->name;
                    best_is_declaration = is_decl;
                }
            }
        }
        for (const auto& child : node->children) {
            find_at(child);
        }
    };
    find_at(it->second.ast);
    return best_name;
}

std::optional<std::string> SemanticModel::format_file(const std::filesystem::path& path) const {
    std::shared_lock lock(mutex_);
    auto it = files_.find(path.string());
    if (it == files_.end()) return std::nullopt;

    // TODO: Delegate to meld fmt when the formatter subsystem is wired.
    // For now, return nullopt to indicate no formatting available.
    return std::nullopt;
}

std::vector<SemanticModel::SymbolLocation> SemanticModel::get_symbol_locations(
    const std::string& name) const {
    std::shared_lock lock(mutex_);
    auto it = symbol_index_.find(name);
    if (it == symbol_index_.end()) return {};
    return it->second;
}

std::optional<SemanticModel::SymbolLocation> SemanticModel::get_definition_location(
    const std::string& name) const {
    std::shared_lock lock(mutex_);
    auto it = symbol_index_.find(name);
    if (it == symbol_index_.end()) return std::nullopt;
    // Prefer declaration kinds over references
    for (const auto& loc : it->second) {
        if (loc.kind != "reference") return loc;
    }
    return std::nullopt;
}

}  // namespace meld::daemon
