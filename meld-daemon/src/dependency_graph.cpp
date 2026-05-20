#include "meld/daemon/dependency_graph.hpp"

#include <algorithm>
#include <fstream>
#include <queue>
#include <sstream>

namespace meld::daemon {

void DependencyGraph::upsert(DependencyNode node) {
    nodes_[node.name] = std::move(node);
}

bool DependencyGraph::remove(const std::string& name) {
    return nodes_.erase(name) > 0;
}

std::optional<DependencyNode> DependencyGraph::get(const std::string& name) const {
    auto it = nodes_.find(name);
    if (it == nodes_.end()) return std::nullopt;
    return it->second;
}

std::vector<std::string> DependencyGraph::all_names() const {
    std::vector<std::string> names;
    names.reserve(nodes_.size());
    for (const auto& [name, _] : nodes_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::unordered_set<std::string> DependencyGraph::transitive_closure(
    const std::string& name) const {
    std::unordered_set<std::string> visited;
    std::queue<std::string> queue;
    queue.push(name);

    while (!queue.empty()) {
        auto current = queue.front();
        queue.pop();
        if (visited.count(current)) continue;
        visited.insert(current);

        auto it = nodes_.find(current);
        if (it != nodes_.end()) {
            for (const auto& dep : it->second.transitive_deps) {
                if (!visited.count(dep)) {
                    queue.push(dep);
                }
            }
        }
    }

    visited.erase(name);  // Don't include self
    return visited;
}

DependencyDiff DependencyGraph::diff(const std::vector<DependencyNode>& new_deps) const {
    DependencyDiff result;

    std::unordered_set<std::string> new_names;
    for (const auto& dep : new_deps) {
        new_names.insert(dep.name);
        auto it = nodes_.find(dep.name);
        if (it == nodes_.end()) {
            result.added.push_back(dep.name);
        } else if (it->second.version != dep.version || it->second.source != dep.source) {
            result.changed.push_back(dep.name);
        }
    }

    for (const auto& [name, _] : nodes_) {
        if (!new_names.count(name)) {
            result.removed.push_back(name);
        }
    }

    std::sort(result.added.begin(), result.added.end());
    std::sort(result.removed.begin(), result.removed.end());
    std::sort(result.changed.begin(), result.changed.end());
    return result;
}

void DependencyGraph::replace_all(std::vector<DependencyNode> deps) {
    nodes_.clear();
    for (auto& dep : deps) {
        nodes_[dep.name] = std::move(dep);
    }
}

std::vector<DependencyNode> DependencyGraph::parse_meld_toml(const std::string& content) {
    std::vector<DependencyNode> deps;

    // Simplified TOML parser: look for [dependencies] section
    // Format: name = { version = "x.y.z", source = "registry" }
    // or:     name = "x.y.z"
    std::istringstream stream(content);
    std::string line;
    bool in_deps = false;

    while (std::getline(stream, line)) {
        // Trim
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
            line.erase(line.begin());
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();

        if (line == "[dependencies]") {
            in_deps = true;
            continue;
        }
        if (!line.empty() && line.front() == '[') {
            in_deps = false;
            continue;
        }
        if (!in_deps || line.empty() || line.front() == '#') continue;

        // Parse "name = ..."
        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        DependencyNode node;
        node.name = line.substr(0, eq_pos);
        // Trim name
        while (!node.name.empty() && node.name.back() == ' ') node.name.pop_back();

        auto value = line.substr(eq_pos + 1);
        while (!value.empty() && value.front() == ' ') value.erase(value.begin());

        // Simple version string: name = "x.y.z"
        if (value.front() == '"') {
            node.version = value.substr(1, value.size() - 2);
            node.source = "registry";
        }
        deps.push_back(std::move(node));
    }

    return deps;
}

std::vector<Diagnostic> DependencyGraph::sync_with_bazel(
    const std::filesystem::path& workspace) {
    std::vector<Diagnostic> diags;

    // Read meld.toml
    auto toml_path = workspace / "meld.toml";
    std::ifstream ifs(toml_path);
    if (!ifs.is_open()) {
        Diagnostic d;
        d.location = {toml_path, 0, 0};
        d.severity = DiagnosticSeverity::Warning;
        d.message = "meld.toml not found in workspace";
        d.rule_id = "W0010-no-manifest";
        diags.push_back(std::move(d));
        return diags;
    }

    std::ostringstream oss;
    oss << ifs.rdbuf();
    auto content = oss.str();

    auto new_deps = parse_meld_toml(content);
    auto changes = diff(new_deps);

    if (!changes.empty()) {
        // In production: trigger `bazel sync` and `bazel query`
        // For now, just update the in-memory graph
        replace_all(std::move(new_deps));
    }

    return diags;
}

}  // namespace meld::daemon
