#include "meld/manifest/effect_elision.hpp"

#include <queue>
#include <unordered_map>

namespace meld::manifest {

void EffectElision::set_call_graph(std::vector<CallEdge> edges) {
    call_graph_ = std::move(edges);
}

void EffectElision::set_entry_points(std::vector<std::string> entries) {
    entry_points_ = std::move(entries);
}

Manifest EffectElision::elide(const Manifest& manifest, bool is_release_build) const {
    // Req 4.4: Don't elide in debug builds
    if (!is_release_build) return manifest;

    auto reachable = compute_reachable();

    Manifest elided = manifest;
    // Remove symbols not reachable from any entry point
    std::vector<SymbolEffectEntry> kept;
    for (const auto& sym : elided.symbols) {
        if (reachable.count(sym.symbol_name)) {
            kept.push_back(sym);
        }
    }
    elided.symbols = std::move(kept);
    return elided;
}

std::unordered_set<std::string> EffectElision::compute_reachable() const {
    // Build adjacency list
    std::unordered_map<std::string, std::vector<std::string>> adj;
    for (const auto& edge : call_graph_) {
        adj[edge.caller].push_back(edge.callee);
    }

    // BFS from entry points
    std::unordered_set<std::string> visited;
    std::queue<std::string> queue;
    for (const auto& entry : entry_points_) {
        queue.push(entry);
    }

    while (!queue.empty()) {
        auto current = queue.front();
        queue.pop();
        if (visited.count(current)) continue;
        visited.insert(current);
        auto it = adj.find(current);
        if (it != adj.end()) {
            for (const auto& callee : it->second) {
                if (!visited.count(callee)) queue.push(callee);
            }
        }
    }
    return visited;
}

}  // namespace meld::manifest
