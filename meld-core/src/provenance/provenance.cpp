#include "meld/provenance/provenance.hpp"
#include "meld/kernel/primitives.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <cctype>

namespace meld {
namespace provenance {

// Import kernel types used in this file
using kernel::String;
using kernel::Integer;
using kernel::Boolean;
using kernel::Empty;
using kernel::Symbol;
using kernel::Function;
using kernel::meta_set;
using kernel::meta_get;
using kernel::meta_has;

// Attach provenance metadata to an AST node
Value Provenance::attachProvenance(const Value& node, const ProvenanceMetadata& metadata) {
    Value serialized = serializeMetadata(metadata);
    return meta_set(node, PROVENANCE_KEY, serialized);
}

// Retrieve provenance metadata from an AST node
std::optional<ProvenanceMetadata> Provenance::getProvenance(const Value& node) {
    if (!meta_has(node, PROVENANCE_KEY)) {
        return std::nullopt;
    }
    
    Value serialized = meta_get(node, PROVENANCE_KEY);
    return deserializeMetadata(serialized);
}

// Check if a node has provenance metadata
bool Provenance::hasProvenance(const Value& node) {
    return meta_has(node, PROVENANCE_KEY);
}

// Mark code as verified by a reviewer
Value Provenance::markAsVerified(const Value& node, const std::string& reviewer_email) {
    auto existing = getProvenance(node);
    if (!existing) {
        // Create new metadata if none exists
        ProvenanceMetadata metadata;
        metadata.origin = OriginType::Verified;
        metadata.reviewer_email = reviewer_email;
        metadata.verification_timestamp = std::chrono::system_clock::now();
        metadata.trust_score = TrustLevels::VERIFIED;
        return attachProvenance(node, metadata);
    }
    
    // Update existing metadata
    ProvenanceMetadata metadata = *existing;
    metadata.origin = OriginType::Verified;
    metadata.reviewer_email = reviewer_email;
    metadata.verification_timestamp = std::chrono::system_clock::now();
    metadata.trust_score = TrustLevels::VERIFIED;
    
    return attachProvenance(node, metadata);
}

// Calculate trust score based on origin and confidence
double Provenance::calculateTrustScore(const ProvenanceMetadata& metadata) {
    double base_score = 0.0;
    
    // Calculate base score from origin type
    switch (metadata.origin) {
        case OriginType::Human:
            base_score = TrustLevels::VERIFIED;
            break;
        
        case OriginType::Verified:
            base_score = TrustLevels::VERIFIED;
            break;
        
        case OriginType::Agent:
            if (metadata.confidence_score) {
                base_score = *metadata.confidence_score;
            } else {
                base_score = TrustLevels::MEDIUM_CONFIDENCE; // Default for agents
            }
            break;
        
        default:
            base_score = TrustLevels::UNTRUSTED;
            break;
    }
    
    // Apply time-based decay for AI-generated code
    if (metadata.origin == OriginType::Agent) {
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - metadata.creation_timestamp);
        
        // Apply decay after 7 days (168 hours)
        if (age.count() > 168) {
            // Decay by 10% per week, max 50% reduction
            double weeks = age.count() / 168.0;
            double decay_factor = std::max(0.5, 1.0 - (weeks * 0.1));
            base_score *= decay_factor;
        }
    }
    
    // Apply verification bonus for verified code
    if (metadata.origin == OriginType::Verified && metadata.verification_timestamp) {
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - *metadata.verification_timestamp);
        
        // Verification expires after 30 days (720 hours)
        if (age.count() > 720) {
            // Reduce trust score for expired verification
            double days_expired = (age.count() - 720) / 24.0;
            double expiry_penalty = std::min(0.3, days_expired * 0.01); // Max 30% penalty
            base_score *= (1.0 - expiry_penalty);
        }
    }
    
    // Ensure score stays within valid range [0.0, 1.0]
    return std::clamp(base_score, 0.0, 1.0);
}

// Enhanced trust score calculation with custom parameters
double Provenance::calculateTrustScore(const ProvenanceMetadata& metadata, 
                                     double time_decay_factor,
                                     int decay_threshold_days,
                                     int verification_expiry_days) {
    double base_score = 0.0;
    
    // Calculate base score from origin type
    switch (metadata.origin) {
        case OriginType::Human:
            base_score = TrustLevels::VERIFIED;
            break;
        
        case OriginType::Verified:
            base_score = TrustLevels::VERIFIED;
            break;
        
        case OriginType::Agent:
            if (metadata.confidence_score) {
                base_score = *metadata.confidence_score;
            } else {
                base_score = TrustLevels::MEDIUM_CONFIDENCE; // Default for agents
            }
            break;
        
        default:
            base_score = TrustLevels::UNTRUSTED;
            break;
    }
    
    // Apply time-based decay for AI-generated code
    if (metadata.origin == OriginType::Agent) {
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - metadata.creation_timestamp);
        
        // Apply decay after threshold
        int threshold_hours = decay_threshold_days * 24;
        if (age.count() > threshold_hours) {
            // Apply custom decay factor
            double weeks = age.count() / (7.0 * 24.0);
            double decay_factor = std::max(0.5, 1.0 - (weeks * time_decay_factor));
            base_score *= decay_factor;
        }
    }
    
    // Apply verification expiry for verified code
    if (metadata.origin == OriginType::Verified && metadata.verification_timestamp) {
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - *metadata.verification_timestamp);
        
        // Check verification expiry
        int expiry_hours = verification_expiry_days * 24;
        if (age.count() > expiry_hours) {
            // Reduce trust score for expired verification
            double days_expired = (age.count() - expiry_hours) / 24.0;
            double expiry_penalty = std::min(0.3, days_expired * 0.01); // Max 30% penalty
            base_score *= (1.0 - expiry_penalty);
        }
    }
    
    // Ensure score stays within valid range [0.0, 1.0]
    return std::clamp(base_score, 0.0, 1.0);
}

// Check if trust score meets minimum threshold
bool Provenance::meetsTrustThreshold(const ProvenanceMetadata& metadata, double min_trust_level) {
    if (!isValidTrustThreshold(min_trust_level)) {
        return false; // Invalid threshold means no code can meet it
    }
    
    double actual_trust = calculateTrustScore(metadata);
    return actual_trust >= min_trust_level;
}

// Get trust level category for a given score
std::string Provenance::getTrustLevelCategory(double trust_score) {
    if (trust_score >= TrustLevels::VERIFIED) {
        return "VERIFIED";
    } else if (trust_score >= TrustLevels::HIGH_CONFIDENCE) {
        return "HIGH_CONFIDENCE";
    } else if (trust_score >= TrustLevels::MEDIUM_CONFIDENCE) {
        return "MEDIUM_CONFIDENCE";
    } else if (trust_score >= TrustLevels::LOW_CONFIDENCE) {
        return "LOW_CONFIDENCE";
    } else {
        return "UNTRUSTED";
    }
}

// Validate trust level threshold value
bool Provenance::isValidTrustThreshold(double threshold) {
    return threshold >= 0.0 && threshold <= 1.0;
}

// Query nodes by origin type
std::vector<Value> Provenance::queryByOrigin(const std::vector<Value>& nodes, OriginType origin) {
    std::vector<Value> result;
    
    for (const auto& node : nodes) {
        auto metadata = getProvenance(node);
        if (metadata && metadata->origin == origin) {
            result.push_back(node);
        }
    }
    
    return result;
}

// Query nodes by minimum trust level
std::vector<Value> Provenance::queryByTrustLevel(const std::vector<Value>& nodes, double min_trust) {
    std::vector<Value> result;
    
    for (const auto& node : nodes) {
        auto metadata = getProvenance(node);
        if (metadata && metadata->trust_score >= min_trust) {
            result.push_back(node);
        }
    }
    
    return result;
}

// Main query API - filters a collection of nodes based on provenance criteria
std::vector<Value> Provenance::query(const std::vector<Value>& nodes, const QueryFilter& filter) {
    std::vector<Value> result;
    
    for (const auto& node : nodes) {
        if (matchesQuery(node, filter)) {
            result.push_back(node);
        }
    }
    
    return result;
}

// Convenience function for single node inspection
bool Provenance::matchesQuery(const Value& node, const QueryFilter& filter) {
    auto metadata = getProvenance(node);
    
    // If no provenance metadata exists, only match if no filters are specified
    if (!metadata) {
        return !filter.origin_type && !filter.min_confidence && !filter.max_confidence && 
               !filter.min_trust_level && !filter.agent_model && !filter.author_email && 
               !filter.verified_only;
    }
    
    // Check origin type filter
    if (filter.origin_type && metadata->origin != *filter.origin_type) {
        return false;
    }
    
    // Check confidence filters (only applicable to Agent origin)
    if (filter.min_confidence || filter.max_confidence) {
        if (metadata->origin != OriginType::Agent || !metadata->confidence_score) {
            return false; // No confidence score available
        }
        
        double confidence = *metadata->confidence_score;
        if (filter.min_confidence && confidence < *filter.min_confidence) {
            return false;
        }
        if (filter.max_confidence && confidence > *filter.max_confidence) {
            return false;
        }
    }
    
    // Check trust level filter
    if (filter.min_trust_level) {
        double trust_score = calculateTrustScore(*metadata);
        if (trust_score < *filter.min_trust_level) {
            return false;
        }
    }
    
    // Check agent model filter
    if (filter.agent_model) {
        if (metadata->origin != OriginType::Agent || !metadata->agent_model || 
            *metadata->agent_model != *filter.agent_model) {
            return false;
        }
    }
    
    // Check author filter
    if (filter.author_email) {
        if (!metadata->author_email || *metadata->author_email != *filter.author_email) {
            return false;
        }
    }
    
    // Check verified only filter
    if (filter.verified_only && *filter.verified_only) {
        if (metadata->origin != OriginType::Verified) {
            return false;
        }
    }
    
    return true;
}

// Detect provenance mismatches (legacy interface for backward compatibility)
std::vector<std::string> Provenance::detectMismatches(const Value& node, const std::string& current_blueprint) {
    auto report = analyzeProvenance(node, current_blueprint, "");
    std::vector<std::string> mismatches;
    
    for (const auto& mismatch_type : report.mismatch_types) {
        switch (mismatch_type) {
            case MismatchType::CodeChangedBlueprintStale:
                mismatches.push_back("Code changed but blueprint wasn't updated");
                break;
            case MismatchType::BlueprintChangedCodeStale:
                mismatches.push_back("Blueprint changed but code wasn't regenerated");
                break;
            case MismatchType::BothChanged:
                mismatches.push_back("Both code and blueprint changed independently");
                break;
            case MismatchType::HashMissing:
                mismatches.push_back("Required content hashes are missing");
                break;
            case MismatchType::StaleAICode:
                mismatches.push_back("AI-generated code is stale and needs review");
                break;
            case MismatchType::TrustLevelMismatch:
                mismatches.push_back("Code trust level doesn't meet requirements");
                break;
            case MismatchType::VerificationExpired:
                mismatches.push_back("Code verification has expired");
                break;
        }
    }
    
    if (mismatches.empty() && !report.description.empty()) {
        mismatches.push_back(report.description);
    }
    
    return mismatches;
}

// Enhanced mismatch detection with detailed analysis
ProvenanceMismatchReport Provenance::analyzeProvenance(const Value& node, const std::string& current_blueprint, const std::string& current_code) {
    ProvenanceMismatchReport report(node);
    report.metadata = getProvenance(node);
    
    if (!report.metadata) {
        report.mismatch_types.push_back(MismatchType::HashMissing);
        report.severity = ProvenanceMismatchReport::Severity::Warning;
        report.description = "No provenance metadata found - unable to track changes";
        return report;
    }
    
    // Calculate current content hashes
    report.current_blueprint_hash = hashBlueprint(current_blueprint);
    if (!current_code.empty()) {
        report.current_code_hash = hashCode(current_code);
    }
    
    // Retrieve stored hashes
    if (meta_has(node, BLUEPRINT_HASH_KEY)) {
        Value stored_hash = meta_get(node, BLUEPRINT_HASH_KEY);
        if (stored_hash.is<String>()) {
            report.stored_blueprint_hash = stored_hash.as<String>()->value();
        }
    }
    
    if (meta_has(node, CODE_HASH_KEY)) {
        Value stored_hash = meta_get(node, CODE_HASH_KEY);
        if (stored_hash.is<String>()) {
            report.stored_code_hash = stored_hash.as<String>()->value();
        }
    }
    
    // Analyze blueprint changes
    bool blueprint_changed = false;
    if (!report.stored_blueprint_hash.empty() && !current_blueprint.empty()) {
        blueprint_changed = (report.current_blueprint_hash != report.stored_blueprint_hash);
    }
    
    // Analyze code changes
    bool code_changed = false;
    if (!report.stored_code_hash.empty() && !current_code.empty()) {
        code_changed = (report.current_code_hash != report.stored_code_hash);
    }
    
    // Detect mismatch patterns
    if (blueprint_changed && code_changed) {
        report.mismatch_types.push_back(MismatchType::BothChanged);
        report.severity = ProvenanceMismatchReport::Severity::Warning;
        report.description = "Both blueprint and code have changed independently - review needed";
    } else if (blueprint_changed && !code_changed) {
        report.mismatch_types.push_back(MismatchType::BlueprintChangedCodeStale);
        report.severity = ProvenanceMismatchReport::Severity::Warning;
        report.description = "Blueprint was updated but code wasn't regenerated";
    } else if (!blueprint_changed && code_changed) {
        report.mismatch_types.push_back(MismatchType::CodeChangedBlueprintStale);
        report.severity = ProvenanceMismatchReport::Severity::Warning;
        report.description = "Code was modified but blueprint wasn't updated";
    }
    
    // Check for missing hashes
    if (report.stored_blueprint_hash.empty() || report.stored_code_hash.empty()) {
        report.mismatch_types.push_back(MismatchType::HashMissing);
        if (report.severity < ProvenanceMismatchReport::Severity::Warning) {
            report.severity = ProvenanceMismatchReport::Severity::Info;
        }
    }
    
    // Check for stale AI-generated code
    if (report.metadata->origin == OriginType::Agent) {
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - report.metadata->creation_timestamp);
        
        if (age.count() > 24 * 7) { // Older than a week
            report.mismatch_types.push_back(MismatchType::StaleAICode);
            report.severity = ProvenanceMismatchReport::Severity::Warning;
            if (report.description.empty()) {
                report.description = "AI-generated code is older than 7 days and may need review";
            }
        }
    }
    
    // Check for verification expiration
    if (report.metadata->origin == OriginType::Verified && report.metadata->verification_timestamp) {
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - *report.metadata->verification_timestamp);
        
        if (age.count() > 24 * 30) { // Older than 30 days
            report.mismatch_types.push_back(MismatchType::VerificationExpired);
            report.severity = ProvenanceMismatchReport::Severity::Warning;
        }
    }
    
    // Check trust level requirements
    double required_trust = TrustLevels::MEDIUM_CONFIDENCE; // Default requirement
    if (report.metadata->trust_score < required_trust) {
        report.mismatch_types.push_back(MismatchType::TrustLevelMismatch);
        if (report.severity < ProvenanceMismatchReport::Severity::Error) {
            report.severity = ProvenanceMismatchReport::Severity::Error;
        }
    }
    
    return report;
}

// Generate compiler warnings for mismatches
std::vector<CompilerWarning> Provenance::generateMismatchWarnings(const ProvenanceMismatchReport& report) {
    std::vector<CompilerWarning> warnings;
    
    for (const auto& mismatch_type : report.mismatch_types) {
        CompilerWarning warning(CompilerWarning::Level::Warning, "", "");
        
        switch (mismatch_type) {
            case MismatchType::CodeChangedBlueprintStale:
                warning.code = "PROV001";
                warning.message = "Code was modified but @blueprint annotation wasn't updated";
                warning.suggestions = {
                    "Update the @blueprint annotation to reflect code changes",
                    "Mark the deviation as intentional with @blueprint(ignore_drift: true)",
                    "Regenerate code from the existing blueprint"
                };
                break;
                
            case MismatchType::BlueprintChangedCodeStale:
                warning.code = "PROV002";
                warning.message = "Blueprint was updated but code wasn't regenerated";
                warning.suggestions = {
                    "Regenerate code from the updated blueprint",
                    "Manually update code to match blueprint changes",
                    "Verify that manual changes are intentional"
                };
                break;
                
            case MismatchType::BothChanged:
                warning.code = "PROV003";
                warning.message = "Both blueprint and code changed independently - potential conflict";
                warning.level = CompilerWarning::Level::Error;
                warning.suggestions = {
                    "Review both changes for conflicts",
                    "Choose whether to keep code changes or blueprint changes",
                    "Merge changes manually if both are needed"
                };
                break;
                
            case MismatchType::HashMissing:
                warning.code = "PROV004";
                warning.message = "Content hashes missing - cannot track provenance changes";
                warning.level = CompilerWarning::Level::Info;
                warning.suggestions = {
                    "Run 'meld build --update-hashes' to generate missing hashes",
                    "Enable automatic hash generation in meld.yaml"
                };
                break;
                
            case MismatchType::StaleAICode:
                warning.code = "PROV005";
                warning.message = "AI-generated code is stale and may need review";
                warning.suggestions = {
                    "Review the code for continued relevance",
                    "Regenerate with updated AI model",
                    "Mark as verified if code is still correct"
                };
                break;
                
            case MismatchType::TrustLevelMismatch:
                warning.code = "PROV006";
                warning.message = "Code trust level doesn't meet project requirements";
                warning.level = CompilerWarning::Level::Error;
                warning.suggestions = {
                    "Submit code for human verification",
                    "Improve AI confidence score",
                    "Adjust project trust level requirements"
                };
                break;
                
            case MismatchType::VerificationExpired:
                warning.code = "PROV007";
                warning.message = "Code verification has expired and needs renewal";
                warning.suggestions = {
                    "Submit for re-verification",
                    "Extend verification period if appropriate",
                    "Review code for continued correctness"
                };
                break;
        }
        
        warnings.push_back(warning);
    }
    
    return warnings;
}

// Provide suggestions for resolving mismatches
std::vector<ResolutionSuggestion> Provenance::suggestResolutions(const ProvenanceMismatchReport& report) {
    std::vector<ResolutionSuggestion> suggestions;
    
    for (const auto& mismatch_type : report.mismatch_types) {
        switch (mismatch_type) {
            case MismatchType::CodeChangedBlueprintStale: {
                suggestions.emplace_back(
                    ResolutionSuggestion::ActionType::UpdateBlueprint,
                    "Update @blueprint annotation to match current code implementation",
                    1
                );
                suggestions.back().automated = true;
                suggestions.back().command = "meld update-blueprint --from-code";
                
                suggestions.emplace_back(
                    ResolutionSuggestion::ActionType::MarkAsIntentional,
                    "Mark code deviation as intentional (suppress future warnings)",
                    3
                );
                break;
            }
            
            case MismatchType::BlueprintChangedCodeStale: {
                suggestions.emplace_back(
                    ResolutionSuggestion::ActionType::RegenerateCode,
                    "Regenerate code implementation from updated blueprint",
                    1
                );
                suggestions.back().automated = true;
                suggestions.back().command = "meld generate --from-blueprint";
                break;
            }
            
            case MismatchType::BothChanged: {
                suggestions.emplace_back(
                    ResolutionSuggestion::ActionType::RequestVerification,
                    "Request human review to resolve conflicting changes",
                    1
                );
                break;
            }
            
            case MismatchType::StaleAICode: {
                suggestions.emplace_back(
                    ResolutionSuggestion::ActionType::RequestVerification,
                    "Submit stale AI code for human verification",
                    2
                );
                
                suggestions.emplace_back(
                    ResolutionSuggestion::ActionType::RegenerateCode,
                    "Regenerate with updated AI model",
                    2
                );
                suggestions.back().automated = true;
                break;
            }
            
            case MismatchType::TrustLevelMismatch: {
                suggestions.emplace_back(
                    ResolutionSuggestion::ActionType::RequestVerification,
                    "Submit for human verification to increase trust level",
                    1
                );
                
                suggestions.emplace_back(
                    ResolutionSuggestion::ActionType::UpdateTrustLevel,
                    "Adjust project trust level requirements",
                    4
                );
                break;
            }
            
            case MismatchType::VerificationExpired: {
                suggestions.emplace_back(
                    ResolutionSuggestion::ActionType::RequestVerification,
                    "Submit for re-verification",
                    2
                );
                
                suggestions.emplace_back(
                    ResolutionSuggestion::ActionType::RefreshTimestamp,
                    "Extend verification period (if code unchanged)",
                    3
                );
                break;
            }
            
            case MismatchType::HashMissing: {
                suggestions.emplace_back(
                    ResolutionSuggestion::ActionType::RefreshTimestamp,
                    "Generate missing content hashes",
                    1
                );
                suggestions.back().automated = true;
                suggestions.back().command = "meld build --update-hashes";
                break;
            }
        }
    }
    
    // Sort suggestions by priority
    std::sort(suggestions.begin(), suggestions.end(), 
              [](const ResolutionSuggestion& a, const ResolutionSuggestion& b) {
                  return a.priority < b.priority;
              });
    
    return suggestions;
}

// Hash functions for content comparison
std::string Provenance::hashContent(const std::string& content) {
    std::hash<std::string> hasher;
    return std::to_string(hasher(content));
}

std::string Provenance::hashBlueprint(const std::string& blueprint) {
    // In a real implementation, this would use a cryptographic hash
    // and normalize the blueprint content (remove whitespace, etc.)
    std::string normalized = blueprint;
    // Remove extra whitespace and normalize
    normalized.erase(std::remove_if(normalized.begin(), normalized.end(), ::isspace), normalized.end());
    return hashContent(normalized);
}

std::string Provenance::hashCode(const std::string& code) {
    // In a real implementation, this would use AST-based hashing
    // to ignore formatting changes and focus on semantic content
    std::string normalized = code;
    // Basic normalization - remove comments and extra whitespace
    // This is a simplified version; real implementation would parse AST
    normalized.erase(std::remove_if(normalized.begin(), normalized.end(), 
                                   [](char c) { return c == ' ' || c == '\t' || c == '\n'; }), 
                    normalized.end());
    return hashContent(normalized);
}

// Store content hashes for mismatch detection
Value Provenance::attachContentHashes(const Value& node, const std::string& blueprint, const std::string& code) {
    Value updated_node = node;
    
    if (!blueprint.empty()) {
        std::string blueprint_hash = hashBlueprint(blueprint);
        updated_node = meta_set(updated_node, BLUEPRINT_HASH_KEY, Value(std::make_shared<String>(blueprint_hash)));
    }
    
    if (!code.empty()) {
        std::string code_hash = hashCode(code);
        updated_node = meta_set(updated_node, CODE_HASH_KEY, Value(std::make_shared<String>(code_hash)));
    }
    
    return updated_node;
}

// Check if content has changed since last hash
bool Provenance::hasContentChanged(const Value& node, const std::string& current_content, const std::string& hash_key) {
    if (!meta_has(node, hash_key)) {
        return true; // No stored hash means we can't tell, assume changed
    }
    
    Value stored_hash = meta_get(node, hash_key);
    if (!stored_hash.is<String>()) {
        return true; // Invalid hash format
    }
    
    std::string current_hash;
    if (hash_key == BLUEPRINT_HASH_KEY) {
        current_hash = hashBlueprint(current_content);
    } else if (hash_key == CODE_HASH_KEY) {
        current_hash = hashCode(current_content);
    } else {
        current_hash = hashContent(current_content);
    }
    
    return current_hash != stored_hash.as<String>()->value();
}

// Internal helper to serialize metadata to Value
Value Provenance::serializeMetadata(const ProvenanceMetadata& metadata) {
    // Create a simple serialization using a formatted string
    // In a real implementation, you might use JSON or a binary format
    std::ostringstream oss;
    
    // Serialize origin
    oss << "origin:" << static_cast<int>(metadata.origin) << ";";
    
    // Serialize timestamp
    auto time_t = std::chrono::system_clock::to_time_t(metadata.creation_timestamp);
    oss << "timestamp:" << time_t << ";";
    
    // Serialize optional fields
    if (metadata.author_email) {
        oss << "author:" << *metadata.author_email << ";";
    }
    
    if (metadata.commit_hash) {
        oss << "commit:" << *metadata.commit_hash << ";";
    }
    
    if (metadata.agent_model) {
        oss << "model:" << *metadata.agent_model << ";";
    }
    
    if (metadata.confidence_score) {
        oss << "confidence:" << std::fixed << std::setprecision(3) << *metadata.confidence_score << ";";
    }
    
    if (metadata.blueprint_id) {
        oss << "blueprint:" << *metadata.blueprint_id << ";";
    }
    
    if (metadata.reviewer_email) {
        oss << "reviewer:" << *metadata.reviewer_email << ";";
    }
    
    if (metadata.verification_timestamp) {
        auto verify_time_t = std::chrono::system_clock::to_time_t(*metadata.verification_timestamp);
        oss << "verified:" << verify_time_t << ";";
    }
    
    oss << "trust:" << std::fixed << std::setprecision(3) << metadata.trust_score << ";";
    
    return Value(std::make_shared<String>(oss.str()));
}

// Internal helper to deserialize metadata from Value
std::optional<ProvenanceMetadata> Provenance::deserializeMetadata(const Value& value) {
    if (!value.is<String>()) {
        return std::nullopt;
    }
    
    std::string data = value.as<String>()->value();
    ProvenanceMetadata metadata;
    
    // Parse the serialized string
    std::istringstream iss(data);
    std::string token;
    
    while (std::getline(iss, token, ';')) {
        if (token.empty()) continue;
        
        size_t colon = token.find(':');
        if (colon == std::string::npos) continue;
        
        std::string key = token.substr(0, colon);
        std::string val = token.substr(colon + 1);
        
        if (key == "origin") {
            metadata.origin = static_cast<OriginType>(std::stoi(val));
        } else if (key == "timestamp") {
            std::time_t time_t = std::stoll(val);
            metadata.creation_timestamp = std::chrono::system_clock::from_time_t(time_t);
        } else if (key == "author") {
            metadata.author_email = val;
        } else if (key == "commit") {
            metadata.commit_hash = val;
        } else if (key == "model") {
            metadata.agent_model = val;
        } else if (key == "confidence") {
            metadata.confidence_score = std::stod(val);
        } else if (key == "blueprint") {
            metadata.blueprint_id = val;
        } else if (key == "reviewer") {
            metadata.reviewer_email = val;
        } else if (key == "verified") {
            std::time_t time_t = std::stoll(val);
            metadata.verification_timestamp = std::chrono::system_clock::from_time_t(time_t);
        } else if (key == "trust") {
            metadata.trust_score = std::stod(val);
        }
    }
    
    return metadata;
}

// IDE Integration functions
namespace IDEColors {

TrustColor getColorForTrust(double trust_score) {
    if (trust_score >= TrustLevels::VERIFIED) {
        return HUMAN_VERIFIED;
    } else if (trust_score >= TrustLevels::HIGH_CONFIDENCE) {
        return HIGH_CONFIDENCE_AGENT;
    } else if (trust_score >= TrustLevels::MEDIUM_CONFIDENCE) {
        return MEDIUM_CONFIDENCE_AGENT;
    } else {
        return LOW_CONFIDENCE_AGENT;
    }
}

std::string getGutterIcon(OriginType origin) {
    switch (origin) {
        case OriginType::Human:
            return "👤"; // Human icon
        case OriginType::Agent:
            return "🤖"; // Robot icon
        case OriginType::Verified:
            return "✅"; // Checkmark icon
        default:
            return "❓"; // Question mark for unknown
    }
}

} // namespace IDEColors

// TrustLevelEnforcer implementation
TrustLevelEnforcer::TrustLevelEnforcer(double min_trust_level) 
    : min_trust_level_(min_trust_level) {
    if (!Provenance::isValidTrustThreshold(min_trust_level)) {
        // Default to medium confidence if invalid threshold provided
        min_trust_level_ = TrustLevels::MEDIUM_CONFIDENCE;
    }
}

bool TrustLevelEnforcer::checkNode(const Value& node) const {
    auto metadata = Provenance::getProvenance(node);
    if (!metadata) {
        // Nodes without provenance are considered untrusted
        return TrustLevels::UNTRUSTED >= min_trust_level_;
    }
    
    return Provenance::meetsTrustThreshold(*metadata, min_trust_level_);
}

std::vector<Value> TrustLevelEnforcer::findViolations(const std::vector<Value>& nodes) const {
    std::vector<Value> violations;
    
    for (const auto& node : nodes) {
        if (!checkNode(node)) {
            violations.push_back(node);
        }
    }
    
    return violations;
}

std::vector<CompilerWarning> TrustLevelEnforcer::generateTrustViolationWarnings(const std::vector<Value>& violations) const {
    std::vector<CompilerWarning> warnings;
    
    for (const auto& node : violations) {
        CompilerWarning warning(CompilerWarning::Level::Error, "TRUST001", "");
        
        auto metadata = Provenance::getProvenance(node);
        double actual_trust = metadata ? Provenance::calculateTrustScore(*metadata) : TrustLevels::UNTRUSTED;
        
        std::ostringstream oss;
        oss << "Code does not meet minimum trust level requirement. ";
        oss << "Required: " << std::fixed << std::setprecision(2) << min_trust_level_;
        oss << ", Actual: " << std::fixed << std::setprecision(2) << actual_trust;
        
        if (metadata) {
            oss << " (Origin: ";
            switch (metadata->origin) {
                case OriginType::Human: oss << "Human"; break;
                case OriginType::Agent: oss << "Agent"; break;
                case OriginType::Verified: oss << "Verified"; break;
                default: oss << "Unknown"; break;
            }
            oss << ")";
        }
        
        warning.message = oss.str();
        
        // Provide suggestions based on the violation type
        if (!metadata) {
            warning.suggestions = {
                "Add provenance metadata to track code origin",
                "Mark code as human-authored if written manually",
                "Submit code for verification if AI-generated"
            };
        } else {
            switch (metadata->origin) {
                case OriginType::Agent:
                    warning.suggestions = {
                        "Submit code for human verification",
                        "Improve AI model confidence score",
                        "Regenerate code with higher-confidence model",
                        "Lower project trust level requirements if appropriate"
                    };
                    break;
                case OriginType::Human:
                    warning.suggestions = {
                        "Verify provenance metadata is correctly attached",
                        "Check for time-based trust decay",
                        "Lower project trust level requirements if appropriate"
                    };
                    break;
                case OriginType::Verified:
                    warning.suggestions = {
                        "Check if verification has expired",
                        "Submit for re-verification if needed",
                        "Update verification timestamp if still valid"
                    };
                    break;
            }
        }
        
        warnings.push_back(warning);
    }
    
    return warnings;
}

void TrustLevelEnforcer::setMinimumTrustLevel(double min_trust_level) {
    if (Provenance::isValidTrustThreshold(min_trust_level)) {
        min_trust_level_ = min_trust_level;
    }
}

TrustLevelEnforcer::TrustStats TrustLevelEnforcer::analyzeTrustLevels(const std::vector<Value>& nodes) const {
    TrustStats stats = {};
    stats.total_nodes = static_cast<int>(nodes.size());
    stats.minimum_trust_score = 1.0;
    stats.maximum_trust_score = 0.0;
    
    if (nodes.empty()) {
        return stats;
    }
    
    double total_trust = 0.0;
    
    for (const auto& node : nodes) {
        double trust_score = getNodeTrustScore(node);
        total_trust += trust_score;
        
        // Update min/max
        stats.minimum_trust_score = std::min(stats.minimum_trust_score, trust_score);
        stats.maximum_trust_score = std::max(stats.maximum_trust_score, trust_score);
        
        // Categorize by trust level
        if (trust_score >= TrustLevels::VERIFIED) {
            stats.verified_nodes++;
        } else if (trust_score >= TrustLevels::HIGH_CONFIDENCE) {
            stats.high_confidence_nodes++;
        } else if (trust_score >= TrustLevels::MEDIUM_CONFIDENCE) {
            stats.medium_confidence_nodes++;
        } else if (trust_score >= TrustLevels::LOW_CONFIDENCE) {
            stats.low_confidence_nodes++;
        } else {
            stats.untrusted_nodes++;
        }
        
        // Check for violations
        if (trust_score < min_trust_level_) {
            stats.violations++;
        }
    }
    
    stats.average_trust_score = total_trust / stats.total_nodes;
    
    return stats;
}

double TrustLevelEnforcer::getNodeTrustScore(const Value& node) const {
    auto metadata = Provenance::getProvenance(node);
    if (!metadata) {
        return TrustLevels::UNTRUSTED;
    }
    
    return Provenance::calculateTrustScore(*metadata);
}

} // namespace provenance
} // namespace meld