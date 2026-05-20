#pragma once

#include "meld/manifest/manifest.hpp"

#include <string>
#include <unordered_set>
#include <vector>

namespace meld::manifest {

/// Call graph edge: caller → callee
struct CallEdge {
    std::string caller;
    std::string callee;
};

/// Global effect elision pass (Req 4).
/// Performs whole-binary call-graph analysis to strip unreachable effects.
class EffectElision {
public:
    /// Set the call graph edges
    void set_call_graph(std::vector<CallEdge> edges);

    /// Set the entry point symbols
    void set_entry_points(std::vector<std::string> entries);

    /// Run elision on a manifest. Returns the elided manifest.
    /// Only runs when is_release_build is true.
    Manifest elide(const Manifest& manifest, bool is_release_build) const;

private:
    /// Compute the set of symbols reachable from entry points
    std::unordered_set<std::string> compute_reachable() const;

    std::vector<CallEdge> call_graph_;
    std::vector<std::string> entry_points_;
};

}  // namespace meld::manifest
