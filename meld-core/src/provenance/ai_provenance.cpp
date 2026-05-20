#include "meld/provenance/ai_provenance.hpp"
#include "meld/kernel/primitives.hpp"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <numeric>

namespace meld {
namespace ai {

// Import kernel types used in this file
using kernel::Value;
using kernel::String;
using kernel::Integer;
using kernel::Boolean;
using kernel::Empty;
using kernel::meta_set;
using kernel::meta_get;
using kernel::meta_has;

// AIProvenanceTracker implementation
AIProvenanceTracker::AIProvenanceTracker(const std::string& model_name, const std::string& model_version)
    : model_name_(model_name), model_version_(model_version) {
    
    // Set default generation timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    generation_timestamp_ = oss.str();
}

Value AIProvenanceTracker::markAsAIGenerated(const Value& node, double confidence_score, 
                                           const std::string& blueprint_id) {
    if (!isValidConfidence(confidence_score)) {
        // Log warning or throw exception
        confidence_score = std::clamp(confidence_score, 0.0, 1.0);
    }
    
    provenance::ProvenanceMetadata metadata = createAIMetadata(confidence_score, blueprint_id);
    Value result = provenance::Provenance::attachProvenance(node, metadata);
    
    // Store additional AI-specific metadata
    meta_set(result, "ai_model", Value(std::make_shared<String>(model_name_)));
    meta_set(result, "ai_version", Value(std::make_shared<String>(model_version_)));
    meta_set(result, "generation_timestamp", Value(std::make_shared<String>(generation_timestamp_)));
    
    if (!conversation_id_.empty()) {
        meta_set(result, "conversation_id", Value(std::make_shared<String>(conversation_id_)));
    }
    
    if (!prompt_hash_.empty()) {
        meta_set(result, "prompt_hash", Value(std::make_shared<String>(prompt_hash_)));
    }
    
    // Link to blueprint if provided
    if (!blueprint_id.empty()) {
        result = BlueprintLinker::linkCodeToBlueprint(result, blueprint_id, "");
    }
    
    return result;
}

std::vector<Value> AIProvenanceTracker::markAsAIGenerated(const std::vector<Value>& nodes, 
                                                        double confidence_score,
                                                        const std::string& blueprint_id) {
    std::vector<Value> result;
    result.reserve(nodes.size());
    
    for (const auto& node : nodes) {
        result.push_back(markAsAIGenerated(node, confidence_score, blueprint_id));
    }
    
    return result;
}

Value AIProvenanceTracker::updateConfidence(const Value& node, double new_confidence) {
    if (!isValidConfidence(new_confidence)) {
        new_confidence = std::clamp(new_confidence, 0.0, 1.0);
    }
    
    auto existing = provenance::Provenance::getProvenance(node);
    if (!existing || existing->origin != provenance::OriginType::Agent) {
        // Cannot update confidence for non-AI code
        return node;
    }
    
    // Update the confidence score
    provenance::ProvenanceMetadata updated = *existing;
    updated.confidence_score = new_confidence;
    updated.trust_score = calculateAITrustScore(model_name_, new_confidence);
    
    return provenance::Provenance::attachProvenance(node, updated);
}

Value AIProvenanceTracker::linkToBlueprint(const Value& node, const std::string& blueprint_id) {
    return BlueprintLinker::linkCodeToBlueprint(node, blueprint_id, "");
}

void AIProvenanceTracker::setGenerationContext(const std::string& conversation_id, 
                                             const std::string& prompt_hash,
                                             const std::string& generation_timestamp) {
    conversation_id_ = conversation_id;
    prompt_hash_ = prompt_hash;
    if (!generation_timestamp.empty()) {
        generation_timestamp_ = generation_timestamp;
    }
}

bool AIProvenanceTracker::isValidConfidence(double confidence) {
    return confidence >= 0.0 && confidence <= 1.0;
}

double AIProvenanceTracker::calculateAITrustScore(const std::string& model_name, double confidence) {
    // Get model-specific trust multiplier
    auto model_info = ModelRegistry::getModelInfo(model_name);
    double base_multiplier = model_info.has_value() ? model_info->base_trust_multiplier : 0.8;
    
    return confidence * base_multiplier;
}

provenance::ProvenanceMetadata AIProvenanceTracker::createAIMetadata(double confidence, 
                                                                    const std::string& blueprint_id) const {
    provenance::ProvenanceMetadata metadata(model_name_, confidence, blueprint_id);
    
    // Calculate trust score using model registry
    metadata.trust_score = calculateAITrustScore(model_name_, confidence);
    
    return metadata;
}

// BlueprintLinker implementation
Value BlueprintLinker::linkCodeToBlueprint(const Value& code_node, 
                                         const std::string& blueprint_id,
                                         const std::string& blueprint_content) {
    Value result = code_node;
    
    // Store blueprint ID
    meta_set(result, "blueprint_id", Value(std::make_shared<String>(blueprint_id)));
    
    // Store blueprint hash if content provided
    if (!blueprint_content.empty()) {
        std::string hash = calculateBlueprintHash(blueprint_content);
        meta_set(result, "blueprint_hash", Value(std::make_shared<String>(hash)));
    }
    
    // Store link timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    meta_set(result, "blueprint_link_time", Value(std::make_shared<Integer>(time_t)));
    
    return result;
}

std::vector<std::string> BlueprintLinker::verifyBlueprintConsistency(const Value& code_node,
                                                                   const std::string& current_blueprint) {
    std::vector<std::string> issues;
    
    // Check if blueprint ID exists
    auto blueprint_id = getBlueprintId(code_node);
    if (!blueprint_id) {
        issues.push_back("No blueprint ID found for AI-generated code");
        return issues;
    }
    
    // Check if blueprint hash exists
    auto stored_hash = getBlueprintHash(code_node);
    if (!stored_hash) {
        issues.push_back("No blueprint hash found - cannot verify consistency");
        return issues;
    }
    
    // Calculate current blueprint hash
    std::string current_hash = calculateBlueprintHash(current_blueprint);
    
    // Compare hashes
    if (*stored_hash != current_hash) {
        issues.push_back("Blueprint content has changed since code generation");
        issues.push_back("Stored hash: " + *stored_hash);
        issues.push_back("Current hash: " + current_hash);
    }
    
    return issues;
}

Value BlueprintLinker::updateBlueprintHash(const Value& code_node, 
                                         const std::string& new_blueprint_content) {
    std::string new_hash = calculateBlueprintHash(new_blueprint_content);
    return meta_set(code_node, "blueprint_hash", Value(std::make_shared<String>(new_hash)));
}

std::optional<std::string> BlueprintLinker::getBlueprintId(const Value& code_node) {
    if (!meta_has(code_node, "blueprint_id")) {
        return std::nullopt;
    }
    
    Value id_value = meta_get(code_node, "blueprint_id");
    if (id_value.is<String>()) {
        return id_value.as<String>()->value();
    }
    
    return std::nullopt;
}

std::optional<std::string> BlueprintLinker::getBlueprintHash(const Value& code_node) {
    if (!meta_has(code_node, "blueprint_hash")) {
        return std::nullopt;
    }
    
    Value hash_value = meta_get(code_node, "blueprint_hash");
    if (hash_value.is<String>()) {
        return hash_value.as<String>()->value();
    }
    
    return std::nullopt;
}

std::string BlueprintLinker::calculateBlueprintHash(const std::string& blueprint_content) {
    // Simple hash implementation (in production, use SHA-256 or similar)
    std::hash<std::string> hasher;
    size_t hash_value = hasher(blueprint_content);
    
    std::ostringstream oss;
    oss << std::hex << hash_value;
    return oss.str();
}

// ModelRegistry implementation
std::vector<ModelRegistry::ModelInfo> ModelRegistry::models_;

void ModelRegistry::registerModel(const ModelInfo& model_info) {
    // Remove existing model with same name
    models_.erase(
        std::remove_if(models_.begin(), models_.end(),
                      [&](const ModelInfo& m) { return m.name == model_info.name; }),
        models_.end()
    );
    
    models_.push_back(model_info);
}

std::optional<ModelRegistry::ModelInfo> ModelRegistry::getModelInfo(const std::string& model_name) {
    if (models_.empty()) {
        initializeDefaultModels();
    }
    
    auto it = std::find_if(models_.begin(), models_.end(),
                          [&](const ModelInfo& m) { return m.name == model_name; });
    
    if (it != models_.end()) {
        return *it;
    }
    
    return std::nullopt;
}

double ModelRegistry::calculateTrustScore(const std::string& model_name, double confidence) {
    auto model_info = getModelInfo(model_name);
    double multiplier = model_info ? model_info->base_trust_multiplier : 0.8;
    return confidence * multiplier;
}

std::vector<ModelRegistry::ModelInfo> ModelRegistry::getAllModels() {
    if (models_.empty()) {
        initializeDefaultModels();
    }
    return models_;
}

bool ModelRegistry::isVerifiedModel(const std::string& model_name) {
    auto model_info = getModelInfo(model_name);
    return model_info && model_info->is_verified;
}

void ModelRegistry::initializeDefaultModels() {
    // Register common AI models with their trust characteristics
    registerModel({"gpt-4", "latest", 0.95, "code-generation,refactoring,analysis", true});
    registerModel({"gpt-3.5-turbo", "latest", 0.85, "code-generation,refactoring", true});
    registerModel({"claude-3", "latest", 0.90, "code-generation,analysis", true});
    registerModel({"codex", "latest", 0.88, "code-generation", true});
    registerModel({"copilot", "latest", 0.80, "code-completion", true});
    registerModel({"unknown", "unknown", 0.70, "unknown", false});
}

// AIGenerationSession implementation
AIGenerationSession::AIGenerationSession(const std::string& session_id, const std::string& model_name)
    : session_id_(session_id), model_name_(model_name), current_iteration_(0), tracker_(model_name) {
}

void AIGenerationSession::startIteration(const std::string& prompt, const std::string& blueprint_id) {
    current_iteration_++;
    
    // Store iteration information
    std::ostringstream oss;
    oss << "Iteration " << current_iteration_ << ": " << prompt;
    if (!blueprint_id.empty()) {
        oss << " (Blueprint: " << blueprint_id << ")";
    }
    iteration_history_.push_back(oss.str());
    
    // Update tracker context
    std::hash<std::string> hasher;
    std::string prompt_hash = std::to_string(hasher(prompt));
    tracker_.setGenerationContext(session_id_, prompt_hash);
}

std::vector<Value> AIGenerationSession::markGenerated(const std::vector<Value>& nodes, double confidence) {
    confidence_scores_.push_back(confidence);
    return tracker_.markAsAIGenerated(nodes, confidence);
}

void AIGenerationSession::recordFeedback(const Value& node, const std::string& feedback_type, 
                                       const std::string& feedback_text) {
    // Store feedback as metadata
    meta_set(node, "feedback_type", Value(std::make_shared<String>(feedback_type)));
    meta_set(node, "feedback_text", Value(std::make_shared<String>(feedback_text)));
    meta_set(node, "feedback_iteration", Value(std::make_shared<Integer>(current_iteration_)));
}

void AIGenerationSession::endIteration(bool accepted) {
    iteration_results_.push_back(accepted);
}

AIGenerationSession::SessionStats AIGenerationSession::getStats() const {
    SessionStats stats;
    stats.total_iterations = current_iteration_;
    stats.accepted_iterations = std::count(iteration_results_.begin(), iteration_results_.end(), true);
    
    if (!confidence_scores_.empty()) {
        double sum = std::accumulate(confidence_scores_.begin(), confidence_scores_.end(), 0.0);
        stats.average_confidence = sum / confidence_scores_.size();
    } else {
        stats.average_confidence = 0.0;
    }
    
    // Extract unique feedback types (would need to implement based on stored feedback)
    stats.feedback_types = {"accepted", "rejected", "modified"};
    
    return stats;
}

std::string AIGenerationSession::exportSessionHistory() const {
    std::ostringstream oss;
    oss << "AI Generation Session: " << session_id_ << "\n";
    oss << "Model: " << model_name_ << "\n";
    oss << "Total Iterations: " << current_iteration_ << "\n\n";
    
    for (size_t i = 0; i < iteration_history_.size(); ++i) {
        oss << iteration_history_[i] << "\n";
        if (i < confidence_scores_.size()) {
            oss << "Confidence: " << confidence_scores_[i] << "\n";
        }
        if (i < iteration_results_.size()) {
            oss << "Result: " << (iteration_results_[i] ? "Accepted" : "Rejected") << "\n";
        }
        oss << "\n";
    }
    
    return oss.str();
}

} // namespace ai
} // namespace meld