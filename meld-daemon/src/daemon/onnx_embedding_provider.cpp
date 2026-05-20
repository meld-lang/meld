#include "meld/daemon/onnx_embedding_provider.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

// NOTE: In a real build this would #include <onnxruntime_cxx_api.h> and link
// against the ONNX Runtime shared library.  The implementation below provides
// the structural skeleton; the actual ORT calls are guarded behind
// MELD_HAS_ONNXRUNTIME so the code compiles without the dependency.

namespace meld::daemon {

// --------------------------------------------------------------------------
// pimpl for ONNX Runtime session state
// --------------------------------------------------------------------------
struct OnnxEmbeddingProvider::OrtSession {
    bool loaded{false};
    // In production: Ort::Env env; Ort::Session session; Ort::SessionOptions opts;
};

// --------------------------------------------------------------------------
// Construction / destruction
// --------------------------------------------------------------------------
OnnxEmbeddingProvider::OnnxEmbeddingProvider(Config config)
    : config_(std::move(config)), session_(std::make_unique<OrtSession>()) {
#ifdef MELD_HAS_ONNXRUNTIME
    // Production path: initialise ORT env, load model, validate output dims.
    // Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "meld-daemon");
    // Ort::SessionOptions opts;
    // opts.SetIntraOpNumThreads(1);
    // session_->session = Ort::Session(env, config_.model_path.c_str(), opts);
    // session_->loaded = true;
#else
    // Fallback: mark as not loaded — callers should check is_loaded().
    if (std::filesystem::exists(config_.model_path)) {
        session_->loaded = true;  // Structural placeholder
    }
#endif
}

OnnxEmbeddingProvider::~OnnxEmbeddingProvider() = default;

// --------------------------------------------------------------------------
// EmbeddingProvider interface
// --------------------------------------------------------------------------
std::vector<float> OnnxEmbeddingProvider::embed(const std::string& text) const {
    if (!is_loaded()) {
        throw std::runtime_error(
            "OnnxEmbeddingProvider: model not loaded from " +
            config_.model_path.string());
    }

#ifdef MELD_HAS_ONNXRUNTIME
    // Production: tokenize `text`, run session_->session.Run(), extract output
    // tensor, L2-normalise, return vector.
    (void)text;
    return std::vector<float>(config_.dimensions, 0.0f);
#else
    // Deterministic hash-based pseudo-embedding for testing.
    // NOT suitable for real semantic search — just keeps the API exercisable.
    std::vector<float> vec(config_.dimensions, 0.0f);
    std::hash<std::string> hasher;
    auto h = hasher(text);
    for (size_t i = 0; i < config_.dimensions; ++i) {
        h ^= (h << 13) ^ (i * 0x9e3779b97f4a7c15ULL);
        vec[i] = static_cast<float>(static_cast<int64_t>(h) % 1000) / 1000.0f;
    }
    // L2-normalise
    float norm = 0.0f;
    for (auto v : vec) norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 0.0f) {
        for (auto& v : vec) v /= norm;
    }
    return vec;
#endif
}

size_t OnnxEmbeddingProvider::dimensions() const {
    return config_.dimensions;
}

bool OnnxEmbeddingProvider::is_loaded() const {
    return session_ && session_->loaded;
}

}  // namespace meld::daemon
