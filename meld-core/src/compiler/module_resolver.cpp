#include "meld/compiler/module_resolver.hpp"
#include "meld/parser/parser.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace meld::compiler {

// ============================================================================
// ModuleResolutionError
// ============================================================================

std::string ModuleResolutionError::to_cap_json() const {
    nlohmann::json j;
    j["kind"] = [&]() -> std::string {
        switch (kind) {
            case Kind::NOT_FOUND:          return "module_not_found";
            case Kind::CIRCULAR_IMPORT:    return "circular_import";
            case Kind::PRIVATE_SYMBOL:     return "private_symbol";
            case Kind::SYMBOL_NOT_EXPORTED: return "symbol_not_exported";
            case Kind::AMBIGUOUS_MODULE:   return "ambiguous_module";
            case Kind::PARSE_ERROR:        return "parse_error";
        }
        return "unknown";
    }();
    j["message"] = message;
    j["module_path"] = module_path;
    if (!cycle.empty()) {
        j["cycle"] = cycle;
    }
    return j.dump(2);
}

// ============================================================================
// ModuleResolver
// ============================================================================

ModuleResolver::ModuleResolver(const ModuleResolverConfig& config)
    : config_(config)
{}

std::vector<std::string> ModuleResolver::split_module_path(const std::string& module_path) {
    std::vector<std::string> segments;
    std::istringstream iss(module_path);
    std::string segment;
    while (std::getline(iss, segment, '.')) {
        if (!segment.empty()) {
            segments.push_back(segment);
        }
    }
    return segments;
}

std::filesystem::path ModuleResolver::build_file_path(
    const std::vector<std::string>& segments,
    const std::filesystem::path& root)
{
    auto path = root;
    for (const auto& seg : segments) {
        path /= seg;
    }
    path += ".meld";
    return path;
}

bool ModuleResolver::is_stdlib_path(const std::string& module_path) const {
    return module_path.starts_with("std.");
}

bool ModuleResolver::is_package_path(const std::string& module_path) const {
    return get_package_prefix(module_path).has_value();
}

std::optional<std::string> ModuleResolver::get_package_prefix(
    const std::string& module_path) const
{
    for (const auto& [prefix, _] : config_.package_roots) {
        if (module_path == prefix || module_path.starts_with(prefix + ".")) {
            return prefix;
        }
    }
    return std::nullopt;
}

void ModuleResolver::register_package(
    const std::string& prefix,
    const std::filesystem::path& root)
{
    config_.package_roots[prefix] = root;
}

std::variant<std::filesystem::path, ModuleResolutionError>
ModuleResolver::resolve(const std::string& module_path) const
{
    auto segments = split_module_path(module_path);
    if (segments.empty()) {
        return ModuleResolutionError{
            ModuleResolutionError::Kind::NOT_FOUND,
            "Empty module path",
            module_path, {}
        };
    }
    
    // 1. Standard library: std.* prefix
    if (segments[0] == "std" && !config_.stdlib_root.empty()) {
        // Strip the "std" prefix for filesystem lookup
        std::vector<std::string> stdlib_segments(segments.begin() + 1, segments.end());
        auto path = build_file_path(stdlib_segments, config_.stdlib_root);
        if (std::filesystem::exists(path)) {
            return path;
        }
        // Also try without stripping (std/math.meld)
        auto alt_path = build_file_path(segments, config_.stdlib_root);
        if (std::filesystem::exists(alt_path)) {
            return alt_path;
        }
    }
    
    // 2. Project-relative paths
    if (!config_.project_root.empty()) {
        auto path = build_file_path(segments, config_.project_root);
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    
    // 3. Registered package prefix
    auto pkg_prefix = get_package_prefix(module_path);
    if (pkg_prefix) {
        const auto& pkg_root = config_.package_roots.at(*pkg_prefix);
        // Strip the package prefix from segments
        auto prefix_segments = split_module_path(*pkg_prefix);
        std::vector<std::string> remaining(
            segments.begin() + prefix_segments.size(), segments.end());
        auto path = build_file_path(remaining, pkg_root);
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    
    // 4. Additional search roots
    for (const auto& root : config_.additional_roots) {
        auto path = build_file_path(segments, root);
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    
    return ModuleResolutionError{
        ModuleResolutionError::Kind::NOT_FOUND,
        "Module '" + module_path + "' not found in any search path",
        module_path, {}
    };
}

std::variant<std::shared_ptr<ModuleDefinition>, ModuleResolutionError>
ModuleResolver::resolve_and_load(const std::string& module_path)
{
    // Check if already loaded
    auto& registry = ModuleRegistry::instance();
    auto existing = registry.find_module(module_path);
    if (existing) {
        return existing;
    }
    
    // Resolve to filesystem path
    auto resolve_result = resolve(module_path);
    if (auto* error = std::get_if<ModuleResolutionError>(&resolve_result)) {
        return *error;
    }
    
    auto file_path = std::get<std::filesystem::path>(resolve_result);
    
    // Read and parse the file
    std::ifstream file(file_path);
    if (!file) {
        return ModuleResolutionError{
            ModuleResolutionError::Kind::NOT_FOUND,
            "Could not read module file: " + file_path.string(),
            module_path, {}
        };
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    
    parser::Parser parser;
    std::vector<parser::ast::expression> expressions;
    if (!parser.parse_file(source, expressions)) {
        return ModuleResolutionError{
            ModuleResolutionError::Kind::PARSE_ERROR,
            "Parse error in module '" + module_path + "': " + parser.error_message(),
            module_path, {}
        };
    }
    
    // Build module definition
    auto module_def = std::make_shared<ModuleDefinition>(file_path.string());
    module_def->build_from_expressions(expressions);
    
    // Register in the global registry
    registry.register_module(module_def);
    
    return module_def;
}

// ============================================================================
// ModuleDependencyGraph
// ============================================================================

void ModuleDependencyGraph::add_dependency(
    const std::string& importer, const std::string& imported)
{
    edges_[importer].insert(imported);
    reverse_edges_[imported].insert(importer);
    // Ensure both nodes exist in the graph
    if (!edges_.contains(imported)) edges_[imported];
    if (!reverse_edges_.contains(importer)) reverse_edges_[importer];
}

std::vector<std::string> ModuleDependencyGraph::would_create_cycle(
    const std::string& importer, const std::string& imported) const
{
    // If imported can reach importer through existing edges, adding
    // importer→imported would create a cycle.
    // Do a DFS from 'imported' to see if we can reach 'importer'.
    if (importer == imported) {
        return {importer, imported};
    }
    
    std::unordered_set<std::string> visited;
    std::vector<std::string> path;
    
    std::function<bool(const std::string&)> dfs = [&](const std::string& node) -> bool {
        if (node == importer) {
            path.push_back(node);
            return true;
        }
        if (visited.contains(node)) return false;
        visited.insert(node);
        path.push_back(node);
        
        auto it = edges_.find(node);
        if (it != edges_.end()) {
            for (const auto& neighbor : it->second) {
                if (dfs(neighbor)) return true;
            }
        }
        
        path.pop_back();
        return false;
    };
    
    if (dfs(imported)) {
        // Build the full cycle: importer → imported → ... → importer
        std::vector<std::string> cycle;
        cycle.push_back(importer);
        cycle.insert(cycle.end(), path.begin(), path.end());
        return cycle;
    }
    
    return {};
}

bool ModuleDependencyGraph::dfs_has_cycle(
    const std::string& node,
    std::unordered_set<std::string>& visiting,
    std::unordered_set<std::string>& visited,
    std::vector<std::string>& path) const
{
    visiting.insert(node);
    path.push_back(node);
    
    auto it = edges_.find(node);
    if (it != edges_.end()) {
        for (const auto& neighbor : it->second) {
            if (visiting.contains(neighbor)) {
                path.push_back(neighbor);
                return true;
            }
            if (!visited.contains(neighbor)) {
                if (dfs_has_cycle(neighbor, visiting, visited, path)) {
                    return true;
                }
            }
        }
    }
    
    visiting.erase(node);
    visited.insert(node);
    path.pop_back();
    return false;
}

std::vector<std::string> ModuleDependencyGraph::find_cycle() const {
    std::unordered_set<std::string> visiting;
    std::unordered_set<std::string> visited;
    std::vector<std::string> path;
    
    for (const auto& [node, _] : edges_) {
        if (!visited.contains(node)) {
            if (dfs_has_cycle(node, visiting, visited, path)) {
                // Trim path to just the cycle portion
                auto cycle_start = std::find(path.begin(), path.end(), path.back());
                if (cycle_start != path.end()) {
                    return std::vector<std::string>(cycle_start, path.end());
                }
                return path;
            }
        }
    }
    
    return {};
}

std::vector<std::string> ModuleDependencyGraph::dependencies_of(
    const std::string& module) const
{
    auto it = edges_.find(module);
    if (it == edges_.end()) return {};
    return std::vector<std::string>(it->second.begin(), it->second.end());
}

std::vector<std::string> ModuleDependencyGraph::dependents_of(
    const std::string& module) const
{
    auto it = reverse_edges_.find(module);
    if (it == reverse_edges_.end()) return {};
    return std::vector<std::string>(it->second.begin(), it->second.end());
}

std::vector<std::string> ModuleDependencyGraph::topological_order() const {
    // Kahn's algorithm
    std::unordered_map<std::string, size_t> in_degree;
    for (const auto& [node, _] : edges_) {
        if (!in_degree.contains(node)) in_degree[node] = 0;
    }
    for (const auto& [_, neighbors] : edges_) {
        for (const auto& n : neighbors) {
            in_degree[n]++;
        }
    }
    
    std::vector<std::string> queue;
    for (const auto& [node, deg] : in_degree) {
        if (deg == 0) queue.push_back(node);
    }
    
    std::vector<std::string> order;
    while (!queue.empty()) {
        auto node = queue.back();
        queue.pop_back();
        order.push_back(node);
        
        auto it = edges_.find(node);
        if (it != edges_.end()) {
            for (const auto& neighbor : it->second) {
                if (--in_degree[neighbor] == 0) {
                    queue.push_back(neighbor);
                }
            }
        }
    }
    
    // If order doesn't contain all nodes, there's a cycle
    if (order.size() != in_degree.size()) {
        return {};
    }
    
    return order;
}

void ModuleDependencyGraph::clear() {
    edges_.clear();
    reverse_edges_.clear();
}

// ============================================================================
// ReExportManager
// ============================================================================

void ReExportManager::add_re_export(
    const std::string& re_exporting_module,
    const std::string& symbol_name,
    const std::string& origin_module)
{
    re_exports_[re_exporting_module].push_back({symbol_name, origin_module});
}

std::vector<ReExportManager::ReExportedSymbol>
ReExportManager::get_re_exports(const std::string& module) const
{
    auto it = re_exports_.find(module);
    if (it == re_exports_.end()) return {};
    return it->second;
}

bool ReExportManager::is_re_exported(
    const std::string& module, const std::string& symbol_name) const
{
    auto it = re_exports_.find(module);
    if (it == re_exports_.end()) return false;
    return std::any_of(it->second.begin(), it->second.end(),
        [&](const auto& sym) { return sym.symbol_name == symbol_name; });
}

void ReExportManager::clear() {
    re_exports_.clear();
}

} // namespace meld::compiler
