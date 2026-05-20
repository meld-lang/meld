#include "meld/daemon/vector_index.hpp"
#include "meld/daemon/passthrough_embedding_provider.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <numeric>
#include <sstream>

namespace meld::daemon {

// ============================================================================
// Construction
// ============================================================================

VectorIndex::VectorIndex(std::shared_ptr<EmbeddingProvider> provider)
    : provider_(std::move(provider)) {}

VectorIndex::~VectorIndex() = default;

// ============================================================================
// Indexing
// ============================================================================

void VectorIndex::index_file(const std::filesystem::path& path,
                             const FileSemantics& semantics) {
    std::unique_lock lock(mutex_);

    auto key = path.string();
    std::vector<IndexedSymbol> symbols;

    if (!semantics.ast) {
        file_symbols_.erase(key);
        rebuild_flat_index();
        return;
    }

    // Walk the AST top-level children to find exported symbols.
    std::function<void(const std::shared_ptr<ASTNode>&)> walk;
    walk = [&](const std::shared_ptr<ASTNode>& node) {
        if (!node) return;

        bool is_def = (node->kind == "function_definition" ||
                       node->kind == "val_declaration" ||
                       node->kind == "type_definition" ||
                       node->kind == "module_definition");

        if (is_def && !node->name.empty()) {
            IndexedSymbol sym;
            sym.name = node->name;
            sym.type_signature = node->type_info;
            sym.file = path;
            sym.line = node->location.line;

            // Collect effect profile from annotations
            std::ostringstream eff;
            for (size_t i = 0; i < node->effects.size(); ++i) {
                if (i > 0) eff << ", ";
                eff << node->effects[i];
            }
            sym.effect_profile = eff.str();

            // Build embedding text and embed
            auto embed_text = build_embed_text(sym);
            if (provider_ && provider_->dimensions() > 0) {
                sym.embedding = provider_->embed(embed_text);
            }

            symbols.push_back(std::move(sym));
        }

        for (const auto& child : node->children) {
            walk(child);
        }
    };

    walk(semantics.ast);

    file_symbols_[key] = std::move(symbols);
    rebuild_flat_index();
}

void VectorIndex::remove_file(const std::filesystem::path& path) {
    std::unique_lock lock(mutex_);
    file_symbols_.erase(path.string());
    rebuild_flat_index();
}

void VectorIndex::rebuild(const SemanticModel& model) {
    auto files = model.get_indexed_files();
    {
        std::unique_lock lock(mutex_);
        file_symbols_.clear();
        all_symbols_.clear();
    }
    for (const auto& f : files) {
        auto ast = model.get_ast(f);
        if (ast) {
            FileSemantics sem;
            sem.path = f;
            sem.ast = ast;
            index_file(f, sem);
        }
    }
}

// ============================================================================
// Search
// ============================================================================

std::vector<SearchResult> VectorIndex::find_by_intent(
    const std::string& query, size_t max_results,
    const std::string& scope) const {

    // If provider is passthrough (no real embeddings), fall back to keyword.
    if (!is_semantic()) {
        return keyword_search(query, max_results);
    }

    auto query_vec = provider_->embed(query);

    std::shared_lock lock(mutex_);

    // Brute-force ANN scan (sufficient for <100k symbols).
    std::vector<SearchResult> results;
    results.reserve(all_symbols_.size());

    for (const auto* sym : all_symbols_) {
        if (sym->embedding.empty()) continue;

        // Scope filtering
        if (scope == "workspace" && !sym->module_coordinate.empty()) continue;
        if (scope == "dependencies" && sym->module_coordinate.empty()) continue;

        float score = cosine_similarity(query_vec, sym->embedding);
        results.push_back({*sym, score});
    }

    // Sort descending by score
    std::sort(results.begin(), results.end(),
              [](const SearchResult& a, const SearchResult& b) {
                  return a.score > b.score;
              });

    if (results.size() > max_results) {
        results.resize(max_results);
    }
    return results;
}

std::vector<SearchResult> VectorIndex::keyword_search(
    const std::string& query, size_t max_results) const {

    std::shared_lock lock(mutex_);

    // Case-insensitive substring match on name + summary + type_signature
    auto lower = [](const std::string& s) {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return r;
    };

    auto q = lower(query);
    std::vector<SearchResult> results;

    for (const auto* sym : all_symbols_) {
        auto name_l = lower(sym->name);
        auto summary_l = lower(sym->summary);
        auto type_l = lower(sym->type_signature);

        float score = 0.0f;
        if (name_l.find(q) != std::string::npos) score += 1.0f;
        if (summary_l.find(q) != std::string::npos) score += 0.5f;
        if (type_l.find(q) != std::string::npos) score += 0.3f;

        if (score > 0.0f) {
            results.push_back({*sym, score});
        }
    }

    std::sort(results.begin(), results.end(),
              [](const SearchResult& a, const SearchResult& b) {
                  return a.score > b.score;
              });

    if (results.size() > max_results) {
        results.resize(max_results);
    }
    return results;
}

// ============================================================================
// Persistence
// ============================================================================

bool VectorIndex::save(const std::filesystem::path& dir) const {
    std::shared_lock lock(mutex_);

    auto index_path = dir / "vector_index.bin";
    std::ofstream out(index_path, std::ios::binary);
    if (!out) return false;

    // Header: magic + fingerprint
    const char magic[] = "MVIX";
    out.write(magic, 4);

    auto fp = config_fingerprint();
    uint32_t fp_len = static_cast<uint32_t>(fp.size());
    out.write(reinterpret_cast<const char*>(&fp_len), 4);
    out.write(fp.data(), fp_len);

    // Symbol count
    uint32_t count = static_cast<uint32_t>(all_symbols_.size());
    out.write(reinterpret_cast<const char*>(&count), 4);

    for (const auto* sym : all_symbols_) {
        // Write each field as length-prefixed string
        auto write_str = [&](const std::string& s) {
            uint32_t len = static_cast<uint32_t>(s.size());
            out.write(reinterpret_cast<const char*>(&len), 4);
            out.write(s.data(), len);
        };

        write_str(sym->name);
        write_str(sym->module_coordinate);
        write_str(sym->type_signature);
        write_str(sym->effect_profile);
        write_str(sym->summary);
        write_str(sym->file.string());

        out.write(reinterpret_cast<const char*>(&sym->line), 4);

        // Embedding vector
        uint32_t edim = static_cast<uint32_t>(sym->embedding.size());
        out.write(reinterpret_cast<const char*>(&edim), 4);
        if (edim > 0) {
            out.write(reinterpret_cast<const char*>(sym->embedding.data()),
                      edim * sizeof(float));
        }
    }

    return out.good();
}

bool VectorIndex::load(const std::filesystem::path& dir) {
    auto index_path = dir / "vector_index.bin";
    std::ifstream in(index_path, std::ios::binary);
    if (!in) return false;

    char magic[4];
    in.read(magic, 4);
    if (std::string(magic, 4) != "MVIX") return false;

    // Check fingerprint
    uint32_t fp_len = 0;
    in.read(reinterpret_cast<char*>(&fp_len), 4);
    std::string fp(fp_len, '\0');
    in.read(fp.data(), fp_len);
    if (fp != config_fingerprint()) return false;  // Model changed

    uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), 4);

    std::unique_lock lock(mutex_);
    file_symbols_.clear();

    auto read_str = [&]() -> std::string {
        uint32_t len = 0;
        in.read(reinterpret_cast<char*>(&len), 4);
        std::string s(len, '\0');
        in.read(s.data(), len);
        return s;
    };

    for (uint32_t i = 0; i < count; ++i) {
        IndexedSymbol sym;
        sym.name = read_str();
        sym.module_coordinate = read_str();
        sym.type_signature = read_str();
        sym.effect_profile = read_str();
        sym.summary = read_str();
        sym.file = read_str();

        in.read(reinterpret_cast<char*>(&sym.line), 4);

        uint32_t edim = 0;
        in.read(reinterpret_cast<char*>(&edim), 4);
        if (edim > 0) {
            sym.embedding.resize(edim);
            in.read(reinterpret_cast<char*>(sym.embedding.data()),
                    edim * sizeof(float));
        }

        auto key = sym.file.string();
        file_symbols_[key].push_back(std::move(sym));
    }

    rebuild_flat_index();
    return in.good();
}

// ============================================================================
// Accessors
// ============================================================================

size_t VectorIndex::symbol_count() const {
    std::shared_lock lock(mutex_);
    return all_symbols_.size();
}

bool VectorIndex::is_semantic() const {
    if (!provider_ || provider_->dimensions() == 0) return false;
    // Check if it's a PassthroughEmbeddingProvider
    auto* pt = dynamic_cast<const PassthroughEmbeddingProvider*>(provider_.get());
    return (pt == nullptr);
}

// ============================================================================
// Private helpers
// ============================================================================

float VectorIndex::cosine_similarity(const std::vector<float>& a,
                                     const std::vector<float>& b) const {
    if (a.size() != b.size() || a.empty()) return 0.0f;
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    float denom = std::sqrt(na) * std::sqrt(nb);
    return (denom > 0.0f) ? (dot / denom) : 0.0f;
}

std::string VectorIndex::build_embed_text(const IndexedSymbol& sym) const {
    // Concatenate signature + summary + spec descriptions for richer embedding
    std::ostringstream oss;
    oss << sym.name;
    if (!sym.type_signature.empty()) oss << " : " << sym.type_signature;
    if (!sym.effect_profile.empty()) oss << " @uses " << sym.effect_profile;
    if (!sym.summary.empty()) oss << " — " << sym.summary;
    if (!sym.spec_text.empty()) oss << " spec: " << sym.spec_text;
    return oss.str();
}

void VectorIndex::rebuild_flat_index() {
    // Caller must hold write lock
    all_symbols_.clear();
    for (const auto& [_, syms] : file_symbols_) {
        for (const auto& s : syms) {
            all_symbols_.push_back(&s);
        }
    }
}

std::string VectorIndex::config_fingerprint() const {
    // Hash of provider dimensions — if model changes, dims change, index invalidated
    return "dims=" + std::to_string(provider_ ? provider_->dimensions() : 0);
}

}  // namespace meld::daemon
