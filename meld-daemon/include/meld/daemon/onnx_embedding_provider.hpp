#pragma once

#include "embedding_provider.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace meld::daemon {

/// ONNX Runtime-based embedding provider (AI_DX Req 9.3).
/// Loads a local ONNX model and runs inference to produce embeddings.
/// Configuration read from meld.toml [daemon.embeddings] section:
///   provider = "onnx"
///   model_path = "path/to/model.onnx"
///   dimensions = 384
class OnnxEmbeddingProvider : public EmbeddingProvider {
public:
    struct Config {
        std::filesystem::path model_path;
        size_t dimensions{384};
    };

    explicit OnnxEmbeddingProvider(Config config);
    ~OnnxEmbeddingProvider() override;

    // Non-copyable
    OnnxEmbeddingProvider(const OnnxEmbeddingProvider&) = delete;
    OnnxEmbeddingProvider& operator=(const OnnxEmbeddingProvider&) = delete;

    std::vector<float> embed(const std::string& text) const override;
    size_t dimensions() const override;

    /// Check if the ONNX model was loaded successfully.
    bool is_loaded() const;

private:
    Config config_;
    struct OrtSession;  // pimpl — hides ONNX Runtime headers
    std::unique_ptr<OrtSession> session_;
};

}  // namespace meld::daemon
