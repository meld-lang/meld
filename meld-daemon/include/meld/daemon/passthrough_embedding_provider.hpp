#pragma once

#include "embedding_provider.hpp"
#include <string>
#include <vector>

namespace meld::daemon {

/// Fallback provider when no embedding model is configured (AI_DX Req 9.4).
/// Returns zero-vectors — callers detect this and fall back to keyword search.
class PassthroughEmbeddingProvider : public EmbeddingProvider {
public:
    explicit PassthroughEmbeddingProvider(size_t dims = 0) : dims_(dims) {}

    std::vector<float> embed(const std::string& /*text*/) const override {
        return std::vector<float>(dims_, 0.0f);
    }

    size_t dimensions() const override { return dims_; }

    /// Passthrough always reports as "not real" so callers can branch.
    bool is_semantic() const { return false; }

private:
    size_t dims_;
};

}  // namespace meld::daemon
