#pragma once

#include "meld/provenance/provenance.hpp"
#include "meld/kernel/primitives.hpp"
#include <string>
#include <optional>
#include <vector>

namespace meld {
namespace ai {

// Import kernel::Value for convenience
using kernel::Value;

/**
 * AIProvenanceTracker - Tracks provenance for AI-generated code
 * 
 * This class provides functionality to mark code as AI-generated,
 * store agent model information, confidence scores, and link to
 * @blueprint annotations for full traceability.
 */
class AIProvenanceTracker {
public:
    // Constructor with agent model information
    AIProvenanceTracker(const std::string& model_name, const std::string& model_version = "");
    
    // Mark a node as AI-generated with confidence score
    Value markAsAIGenerated(const Value& node, double confidence_score, 
                           const std::string& blueprint_id = "");
    
    // Mark multiple nodes as AI-generated (batch operation)
    std::vector<Value> markAsAIGenerated(const std::vector<Value>& nodes, 
                                       double confidence_score,
                                       const std::string& blueprint_id = "");
    
    // Update confidence score for existing AI-generated node
    Value updateConfidence(const Value& node, double new_confidence);
    
    // Link node to @blueprint annotation
    Value linkToBlueprint(const Value& node, const std::string& blueprint_id);
    
    // Set generation context (conversation ID, prompt, etc.)
    void setGenerationContext(const std::string& conversation_id, 
                            const std::string& prompt_hash = "",
                            const std::string& generation_timestamp = "");
    
    // Get current model information
    std::string getModelName() const { return model_name_; }
    std::string getModelVersion() const { return model_version_; }
    
    // Validate confidence score (0.0 to 1.0)
    static bool isValidConfidence(double confidence);
    
    // Calculate trust score based on model and confidence
    static double calculateAITrustScore(const std::string& model_name, double confidence);
    
private:
    std::string model_name_;
    std::string model_version_;
    std::string conversation_id_;
    std::string prompt_hash_;
    std::string generation_timestamp_;
    
    // Create AI provenance metadata
    provenance::ProvenanceMetadata createAIMetadata(double confidence, 
                                                   const std::string& blueprint_id) const;
};

/**
 * BlueprintLinker - Links AI-generated code to @blueprint annotations
 * 
 * This class manages the relationship between AI-generated code and
 * the @blueprint annotations that guided their generation.
 */
class BlueprintLinker {
public:
    // Link code to blueprint with hash verification
    static Value linkCodeToBlueprint(const Value& code_node, 
                                   const std::string& blueprint_id,
                                   const std::string& blueprint_content);
    
    // Verify blueprint-code consistency
    static std::vector<std::string> verifyBlueprintConsistency(const Value& code_node,
                                                             const std::string& current_blueprint);
    
    // Update blueprint hash when blueprint changes
    static Value updateBlueprintHash(const Value& code_node, 
                                   const std::string& new_blueprint_content);
    
    // Get blueprint ID from code node
    static std::optional<std::string> getBlueprintId(const Value& code_node);
    
    // Get blueprint hash from code node
    static std::optional<std::string> getBlueprintHash(const Value& code_node);
    
private:
    // Calculate hash of blueprint content
    static std::string calculateBlueprintHash(const std::string& blueprint_content);
};

/**
 * ModelRegistry - Registry of known AI models and their trust characteristics
 * 
 * This class maintains information about different AI models and their
 * typical performance characteristics for trust score calculation.
 */
class ModelRegistry {
public:
    struct ModelInfo {
        std::string name;
        std::string version;
        double base_trust_multiplier;  // 0.5 to 1.0
        std::string capabilities;      // "code-generation", "refactoring", etc.
        bool is_verified;             // Whether this model has been validated
    };
    
    // Register a new model
    static void registerModel(const ModelInfo& model_info);
    
    // Get model information
    static std::optional<ModelInfo> getModelInfo(const std::string& model_name);
    
    // Calculate trust score for model + confidence combination
    static double calculateTrustScore(const std::string& model_name, double confidence);
    
    // Get all registered models
    static std::vector<ModelInfo> getAllModels();
    
    // Check if model is known and verified
    static bool isVerifiedModel(const std::string& model_name);
    
private:
    static std::vector<ModelInfo> models_;
    static void initializeDefaultModels();
};

/**
 * AIGenerationSession - Tracks a complete AI code generation session
 * 
 * This class manages the provenance tracking for an entire AI generation
 * session, including multiple iterations, refinements, and user feedback.
 */
class AIGenerationSession {
public:
    AIGenerationSession(const std::string& session_id, const std::string& model_name);
    
    // Start a new generation iteration
    void startIteration(const std::string& prompt, const std::string& blueprint_id = "");
    
    // Mark nodes as generated in current iteration
    std::vector<Value> markGenerated(const std::vector<Value>& nodes, double confidence);
    
    // Record user feedback on generated code
    void recordFeedback(const Value& node, const std::string& feedback_type, 
                       const std::string& feedback_text);
    
    // End current iteration
    void endIteration(bool accepted);
    
    // Get session statistics
    struct SessionStats {
        int total_iterations;
        int accepted_iterations;
        double average_confidence;
        std::vector<std::string> feedback_types;
    };
    SessionStats getStats() const;
    
    // Export session history for shadow provenance
    std::string exportSessionHistory() const;
    
private:
    std::string session_id_;
    std::string model_name_;
    int current_iteration_;
    std::vector<std::string> iteration_history_;
    std::vector<double> confidence_scores_;
    std::vector<bool> iteration_results_;
    
    AIProvenanceTracker tracker_;
};

} // namespace ai
} // namespace meld