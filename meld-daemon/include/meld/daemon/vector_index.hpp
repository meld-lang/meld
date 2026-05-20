#pragma once

#include "embedding_provider.hpp"
#include "semantic_model.hpp"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace meld::daemon {

/// A single indexed symbol with its embedding and metadata.
struct IndexedSymbol {
    std::string name;
    std::string module_coordinate;   // e.g. "mylib/math:1.0"
    std::string type_signature;
    std::string effect_profile;
    std::string srt_security_profile;
    std::string summary;             // From @blueprint if available
    std::string spec_text;           // Concatenated spec action descriptions
    std::filesystem::path file;
    uint32_t line{0};
    std::vector<float> embedding;
};

/// Result from a nearest-neighbor search.
struct SearchResult {
    IndexedSymbol symbol;
    float score{0.0f};  // Cosine similarity
};

/// Vector-augmented symbol index (AI_DX Req 9).
/// Indexes all exported symbols by semantic embedding alongside name,
/// type signature, and effect profile.  Supports ANN search and
/// incremental updates.
class VectorIndex {
public:
    /// Construct with an embedding provider.  If provider is a
    /// PassthroughEmbeddingProvider, semantic search is disabled and
    /// find_by_intent falls back to keyword matching.
    explicit VectorIndex(std::shared_ptr<EmbeddingProvider> provider);
    ~VectorIndex();

    // Non-copyable
    VectorIndex(const VectorIndex&) = delete;
    VectorIndex& operator=(const VectorIndex&) = delete;

    // --- Indexing ---

    /// Index all exported symbols from a file's semantics.
    /// Replaces any previously indexed symbols for this file.
    void index_file(const std::filesystem::path& path,
                    const FileSemantics& semantics);

    /// Remove all symbols from a file.
    void remove_file(const std::filesystem::path& path);

    /// Rebuild the entire index from a SemanticModel snapshot.
    void rebuild(const SemanticModel& model);

    // --- Search ---

    /// Semantic nearest-neighbor search.  Returns top-K results ranked
    /// by cosine similarity.  Falls back to keyword search when the
    /// embedding provider is passthrough.
    std::vector<SearchResult> find_by_intent(
        const std::string& query,
        size_t max_results = 10,
        const std::string& scope = "all") const;

    /// Keyword-based fallback search (substring match on name/summary).
    std::vector<SearchResult> keyword_search(
        const std::string& query,
        size_t max_results = 10) const;

    // --- Persistence ---

    /// Save index to disk at the given directory (e.g. .meld/).
    /// Writes symbols + embeddings + a config fingerprint.
    bool save(const std::filesystem::path& dir) const;

    /// Load index from disk.  Returns false if file missing or
    /// config fingerprint doesn't match (embedding model changed).
    bool load(const std::filesystem::path& dir);

    // --- Accessors ---

    size_t symbol_count() const;
    bool is_semantic() const;  // true if provider is not passthrough

private:
    float cosine_similarity(const std::vector<float>& a,
                            const std::vector<float>& b) const;

    std::string build_embed_text(const IndexedSymbol& sym) const;

    std::shared_ptr<EmbeddingProvider> provider_;
    mutable std::shared_mutex mutex_;

    // file path string → list of symbols from that file
    std::unordered_map<std::string, std::vector<IndexedSymbol>> file_symbols_;

    // Flat list for ANN scan (rebuilt on mutation)
    std::vector<const IndexedSymbol*> all_symbols_;
    void rebuild_flat_index();

    std::string config_fingerprint() const;
};

}  // namespace meld::daemon
