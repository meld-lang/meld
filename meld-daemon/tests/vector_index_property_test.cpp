#include "meld/daemon/vector_index.hpp"
#include "meld/daemon/embedding_provider.hpp"
#include "meld/daemon/semantic_model.hpp"

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <cmath>
#include <memory>

namespace meld::daemon {
namespace {

// Deterministic test embedding provider.
class PropTestEmbeddingProvider : public EmbeddingProvider {
public:
    std::vector<float> embed(const std::string& text) const override {
        std::vector<float> v(8, 0.0f);
        for (size_t i = 0; i < text.size(); ++i)
            v[i % 8] += static_cast<float>(text[i]) / 128.0f;
        float norm = 0.0f;
        for (float x : v) norm += x * x;
        norm = std::sqrt(norm);
        if (norm > 0.0f) for (float& x : v) x /= norm;
        return v;
    }
    size_t dimensions() const override { return 8; }
};

FileSemantics make_sem(const std::string& path, const std::string& name) {
    auto node = std::make_shared<ASTNode>();
    node->kind = "function_definition";
    node->name = name;
    node->type_info = "() -> Int";
    node->location = {path, 1, 0};
    FileSemantics sem;
    sem.path = path;
    sem.ast = node;
    sem.exports = {name};
    return sem;
}

/// Property 4: Incremental Index Equivalence
/// For any file change, incrementally updating the VectorIndex SHALL produce
/// the same search results as rebuilding the entire index from scratch.
RC_GTEST_PROP(VectorIndexProperty, IncrementalEquivalence, ()) {
    auto provider = std::make_shared<PropTestEmbeddingProvider>();

    // Generate a small set of files with symbols
    auto num_files = *rc::gen::inRange(1, 6);
    std::vector<std::pair<std::string, std::string>> files;
    for (int i = 0; i < num_files; ++i) {
        auto name = "fn_" + std::to_string(i);
        auto path = "src/file_" + std::to_string(i) + ".meld";
        files.emplace_back(path, name);
    }

    // Build incremental index: add all files, then modify one
    VectorIndex incremental(provider);
    for (const auto& [path, name] : files) {
        incremental.index_file(path, make_sem(path, name));
    }

    // Pick a random file to modify
    auto modify_idx = *rc::gen::inRange(0, num_files);
    auto new_name = "modified_fn_" + std::to_string(modify_idx);
    incremental.index_file(files[modify_idx].first,
                           make_sem(files[modify_idx].first, new_name));

    // Build from-scratch index with the same final state
    VectorIndex from_scratch(provider);
    for (int i = 0; i < num_files; ++i) {
        if (i == modify_idx) {
            from_scratch.index_file(files[i].first,
                                    make_sem(files[i].first, new_name));
        } else {
            from_scratch.index_file(files[i].first,
                                    make_sem(files[i].first, files[i].second));
        }
    }

    // Both should have same symbol count
    RC_ASSERT(incremental.symbol_count() == from_scratch.symbol_count());

    // Search results should match for several queries
    for (const auto& query : {"fn", "modified", "file"}) {
        auto r_inc = incremental.find_by_intent(query, 10);
        auto r_scratch = from_scratch.find_by_intent(query, 10);
        RC_ASSERT(r_inc.size() == r_scratch.size());
        for (size_t i = 0; i < r_inc.size(); ++i) {
            RC_ASSERT(r_inc[i].symbol.name == r_scratch[i].symbol.name);
            // Scores should be very close (floating point)
            RC_ASSERT(std::abs(r_inc[i].score - r_scratch[i].score) < 1e-5f);
        }
    }
}

}  // namespace
}  // namespace meld::daemon
