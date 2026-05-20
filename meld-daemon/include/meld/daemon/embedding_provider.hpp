#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace meld::daemon {

/// Abstract interface for embedding providers (AI_DX Req 9).
/// Stateless per call — each embed() invocation is independent.
class EmbeddingProvider {
public:
    virtual ~EmbeddingProvider() = default;

    /// Embed a text string into a dense vector.
    virtual std::vector<float> embed(const std::string& text) const = 0;

    /// Return the dimensionality of the embedding vectors.
    virtual size_t dimensions() const = 0;
};

}  // namespace meld::daemon
