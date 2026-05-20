#include "meld/daemon/workspace_provider.hpp"

#include <algorithm>
#include <chrono>
#include <functional>

namespace meld::daemon {

WorkspaceProvider::WorkspaceProvider(SemanticModel& model,
                                     DependencyGraph& dep_graph)
    : model_(model), dep_graph_(dep_graph) {}

// ---------------------------------------------------------------------------
// File discovery (Req 20.1)
// ---------------------------------------------------------------------------

std::vector<std::filesystem::path> WorkspaceProvider::discover_meld_files(
    const std::filesystem::path& root) const {
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
        return files;
    }
    std::error_code ec;
    for (auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".meld") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::vector<std::filesystem::path> WorkspaceProvider::discover_and_index(
    const std::filesystem::path& workspace_root) {
    auto start = std::chrono::steady_clock::now();

    auto files = discover_meld_files(workspace_root);
    for (const auto& file : files) {
        auto sem = build_semantics(file);
        model_.update_file(file, std::move(sem));
    }

    auto end = std::chrono::steady_clock::now();
    stats_.total_files = files.size();
    stats_.indexed_files = files.size();
    stats_.last_index_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    stats_.incremental = false;

    return files;
}

// ---------------------------------------------------------------------------
// Incremental index updates (Req 20.2)
// ---------------------------------------------------------------------------

void WorkspaceProvider::handle_file_change(const std::filesystem::path& file,
                                            FileChangeType change_type) {
    auto start = std::chrono::steady_clock::now();

    switch (change_type) {
        case FileChangeType::Created:
        case FileChangeType::Modified: {
            auto sem = build_semantics(file);
            model_.update_file(file, std::move(sem));
            if (change_type == FileChangeType::Created) {
                ++stats_.total_files;
            }
            break;
        }
        case FileChangeType::Deleted:
            model_.remove_file(file);
            if (stats_.total_files > 0) --stats_.total_files;
            break;
    }

    auto end = std::chrono::steady_clock::now();
    stats_.indexed_files = model_.file_count();
    stats_.last_index_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    stats_.incremental = true;
}

// ---------------------------------------------------------------------------
// Cross-file resolution (Req 20.3)
// ---------------------------------------------------------------------------

std::optional<std::pair<std::filesystem::path, SourceLocation>>
WorkspaceProvider::find_symbol_definition(const std::string& symbol_name) const {
    for (const auto& f : model_.get_indexed_files()) {
        auto ast = model_.get_ast(f);
        if (!ast) continue;

        std::function<std::optional<SourceLocation>(const std::shared_ptr<ASTNode>&)> search;
        search = [&](const std::shared_ptr<ASTNode>& node)
            -> std::optional<SourceLocation> {
            if (!node) return std::nullopt;
            if (node->name == symbol_name && !node->name.empty()) {
                const auto& k = node->kind;
                if (k == "function_definition" || k == "val_declaration" ||
                    k == "struct" || k == "enum" || k == "trait" ||
                    k == "type_alias" || k == "module") {
                    return node->location;
                }
            }
            for (const auto& child : node->children) {
                auto result = search(child);
                if (result) return result;
            }
            return std::nullopt;
        };

        auto loc = search(ast);
        if (loc) return std::make_pair(f, *loc);
    }
    return std::nullopt;
}

CrossFileReference WorkspaceProvider::resolve_cross_file_reference(
    const std::filesystem::path& from_file,
    const std::string& symbol_name) const {
    CrossFileReference ref;
    ref.symbol_name = symbol_name;
    ref.source_file = from_file;

    auto def = find_symbol_definition(symbol_name);
    if (def) {
        ref.target_file = def->first;
        ref.target_line = def->second.line;
        ref.target_column = def->second.column;
        ref.resolved = true;
    }
    return ref;
}

std::vector<CrossFileReference> WorkspaceProvider::resolve_all_imports(
    const std::filesystem::path& file) const {
    std::vector<CrossFileReference> results;

    auto ast = model_.get_ast(file);
    if (!ast) return results;

    // Walk AST looking for import references
    std::function<void(const std::shared_ptr<ASTNode>&)> walk;
    walk = [&](const std::shared_ptr<ASTNode>& node) {
        if (!node) return;
        if (node->kind == "import" && !node->name.empty()) {
            results.push_back(resolve_cross_file_reference(file, node->name));
        }
        for (const auto& child : node->children) {
            walk(child);
        }
    };
    walk(ast);

    return results;
}

// ---------------------------------------------------------------------------
// Performance (Req 20.4)
// ---------------------------------------------------------------------------

WorkspaceStats WorkspaceProvider::get_stats() const {
    return stats_;
}

bool WorkspaceProvider::is_within_performance_budget(
    std::chrono::milliseconds budget) const {
    return stats_.last_index_duration <= budget;
}

// ---------------------------------------------------------------------------
// Configuration change handling (Req 20.5)
// ---------------------------------------------------------------------------

std::vector<std::filesystem::path> WorkspaceProvider::find_affected_files(
    const std::filesystem::path& /*config_file*/) const {
    // When config changes, all indexed files are potentially affected
    return model_.get_indexed_files();
}

std::vector<std::filesystem::path> WorkspaceProvider::handle_config_change(
    const std::filesystem::path& config_file) {
    auto affected = find_affected_files(config_file);

    for (const auto& file : affected) {
        auto sem = build_semantics(file);
        model_.update_file(file, std::move(sem));
    }

    stats_.indexed_files = model_.file_count();
    stats_.incremental = true;
    return affected;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

std::vector<std::filesystem::path> WorkspaceProvider::get_indexed_files() const {
    return model_.get_indexed_files();
}

bool WorkspaceProvider::is_indexed(const std::filesystem::path& file) const {
    return model_.has_file(file);
}

size_t WorkspaceProvider::indexed_file_count() const {
    return model_.file_count();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

FileSemantics WorkspaceProvider::build_semantics(
    const std::filesystem::path& file) const {
    FileSemantics sem;
    sem.path = file;

    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = file.stem().string();
    root->location = SourceLocation{file, 1, 0};

    sem.ast = root;
    return sem;
}

}  // namespace meld::daemon
