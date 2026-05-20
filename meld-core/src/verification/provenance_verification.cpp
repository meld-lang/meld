#include "meld/verification/provenance_verification.hpp"
#include "meld/kernel/primitives.hpp"
#include "meld/provenance/provenance.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace meld {
namespace verification {

// Static member initialization
std::vector<AuditEntry> VerificationAudit::audit_entries_;
std::vector<VerificationRequest> VerificationWorkflow::pending_requests_;
int VerificationWorkflow::next_request_id_ = 1;
std::map<std::string, std::string> VerificationPolicy::policies_;

// ProvenanceVerifier implementation
ProvenanceVerifier::ProvenanceVerifier(const std::string& reviewer_email, const std::string& reviewer_name)
    : reviewer_email_(reviewer_email), reviewer_name_(reviewer_name) {
}

Value ProvenanceVerifier::markAsVerified(const Value& node, const std::string& verification_notes) {
    // Use the core provenance system to mark as verified
    Value verified_node = provenance::Provenance::markAsVerified(node, reviewer_email_);
    
    // Create detailed verification info
    VerificationInfo info = createVerificationInfo(verification_notes);
    
    // Record in audit trail
    VerificationAudit::recordVerification(verified_node, info);
    
    return verified_node;
}

std::vector<Value> ProvenanceVerifier::markAsVerified(const std::vector<Value>& nodes, 
                                                    const std::string& verification_notes) {
    std::vector<Value> verified_nodes;
    verified_nodes.reserve(nodes.size());
    
    for (const auto& node : nodes) {
        verified_nodes.push_back(markAsVerified(node, verification_notes));
    }
    
    return verified_nodes;
}

Value ProvenanceVerifier::verifyWithDetails(const Value& node, const VerificationDetails& details) {
    // Use the core provenance system to mark as verified
    Value verified_node = provenance::Provenance::markAsVerified(node, reviewer_email_);
    
    // Create verification info with detailed information
    VerificationInfo info;
    info.reviewer_email = reviewer_email_;
    info.reviewer_name = reviewer_name_;
    info.verification_timestamp = std::chrono::system_clock::now();
    info.details = details;
    info.is_active = true;
    
    // Record in audit trail
    VerificationAudit::recordVerification(verified_node, info);
    
    return verified_node;
}

bool ProvenanceVerifier::isVerified(const Value& node) const {
    auto metadata = provenance::Provenance::getProvenance(node);
    return metadata && metadata->origin == provenance::OriginType::Verified;
}

std::optional<VerificationInfo> ProvenanceVerifier::getVerificationInfo(const Value& node) const {
    auto metadata = provenance::Provenance::getProvenance(node);
    if (!metadata || metadata->origin != provenance::OriginType::Verified) {
        return std::nullopt;
    }
    
    VerificationInfo info;
    info.reviewer_email = metadata->reviewer_email.value_or("");
    info.reviewer_name = reviewer_name_;
    info.verification_timestamp = metadata->verification_timestamp.value_or(std::chrono::system_clock::now());
    info.is_active = true;
    
    return info;
}

Value ProvenanceVerifier::revokeVerification(const Value& node, const std::string& reason) {
    auto metadata = provenance::Provenance::getProvenance(node);
    if (!metadata) {
        return node; // No metadata to revoke
    }
    
    // Reset to original origin (Human or Agent)
    provenance::ProvenanceMetadata new_metadata = *metadata;
    if (metadata->agent_model) {
        new_metadata.origin = provenance::OriginType::Agent;
        new_metadata.trust_score = metadata->confidence_score.value_or(0.7);
    } else {
        new_metadata.origin = provenance::OriginType::Human;
        new_metadata.trust_score = 1.0;
    }
    
    // Clear verification fields
    new_metadata.reviewer_email = std::nullopt;
    new_metadata.verification_timestamp = std::nullopt;
    
    // Record revocation in audit trail
    VerificationAudit::recordRevocation(node, reason, reviewer_email_);
    
    return provenance::Provenance::attachProvenance(node, new_metadata);
}

void ProvenanceVerifier::setReviewer(const std::string& email, const std::string& name) {
    reviewer_email_ = email;
    reviewer_name_ = name;
}

VerificationInfo ProvenanceVerifier::createVerificationInfo(const std::string& notes) const {
    VerificationInfo info;
    info.reviewer_email = reviewer_email_;
    info.reviewer_name = reviewer_name_;
    info.verification_timestamp = std::chrono::system_clock::now();
    info.details = VerificationDetails("manual", "basic", notes);
    info.is_active = true;
    return info;
}

// VerificationAudit implementation
void VerificationAudit::recordVerification(const Value& node, const VerificationInfo& info) {
    // Generate a simple node ID (in production, use proper AST node IDs)
    std::ostringstream oss;
    oss << "node_" << std::hex << reinterpret_cast<uintptr_t>(&node);
    std::string node_id = oss.str();
    
    AuditEntry entry(node_id, "verified", info.reviewer_email);
    entry.details = info.details;
    audit_entries_.push_back(entry);
}

void VerificationAudit::recordRevocation(const Value& node, const std::string& reason, 
                                       const std::string& revoker_email) {
    // Generate a simple node ID
    std::ostringstream oss;
    oss << "node_" << std::hex << reinterpret_cast<uintptr_t>(&node);
    std::string node_id = oss.str();
    
    AuditEntry entry(node_id, "revoked", revoker_email);
    entry.revocation_reason = reason;
    audit_entries_.push_back(entry);
}

std::vector<AuditEntry> VerificationAudit::getAuditTrail(const Value& node) {
    // Generate node ID
    std::ostringstream oss;
    oss << "node_" << std::hex << reinterpret_cast<uintptr_t>(&node);
    std::string node_id = oss.str();
    
    std::vector<AuditEntry> trail;
    for (const auto& entry : audit_entries_) {
        if (entry.node_id == node_id) {
            trail.push_back(entry);
        }
    }
    return trail;
}

std::vector<AuditEntry> VerificationAudit::getVerificationsByReviewer(const std::string& reviewer_email) {
    std::vector<AuditEntry> entries;
    for (const auto& entry : audit_entries_) {
        if (entry.reviewer_email == reviewer_email) {
            entries.push_back(entry);
        }
    }
    return entries;
}

VerificationStats VerificationAudit::getStats() {
    VerificationStats stats;
    stats.total_verifications = 0;
    stats.active_verifications = 0;
    stats.revoked_verifications = 0;
    
    std::map<std::string, int> reviewer_counts;
    
    for (const auto& entry : audit_entries_) {
        if (entry.event_type == "verified") {
            stats.total_verifications++;
            stats.active_verifications++;
            reviewer_counts[entry.reviewer_email]++;
            
            stats.verification_types[entry.details.verification_type]++;
            stats.verification_levels[entry.details.verification_level]++;
        } else if (entry.event_type == "revoked") {
            stats.revoked_verifications++;
            if (stats.active_verifications > 0) {
                stats.active_verifications--;
            }
        }
    }
    
    // Find top reviewers
    std::vector<std::pair<std::string, int>> reviewer_pairs(reviewer_counts.begin(), reviewer_counts.end());
    std::sort(reviewer_pairs.begin(), reviewer_pairs.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (size_t i = 0; i < std::min(size_t(5), reviewer_pairs.size()); ++i) {
        stats.top_reviewers.push_back(reviewer_pairs[i].first);
    }
    
    // Calculate average verification time (simplified)
    stats.average_verification_time_hours = 2.5; // Placeholder
    
    return stats;
}

std::string VerificationAudit::exportAuditTrail(const std::string& format) {
    if (format == "json") {
        std::ostringstream oss;
        oss << "{\n  \"audit_entries\": [\n";
        
        for (size_t i = 0; i < audit_entries_.size(); ++i) {
            const auto& entry = audit_entries_[i];
            oss << "    {\n";
            oss << "      \"node_id\": \"" << entry.node_id << "\",\n";
            oss << "      \"event_type\": \"" << entry.event_type << "\",\n";
            oss << "      \"reviewer_email\": \"" << entry.reviewer_email << "\",\n";
            
            auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
            oss << "      \"timestamp\": " << time_t << ",\n";
            oss << "      \"verification_type\": \"" << entry.details.verification_type << "\",\n";
            oss << "      \"verification_level\": \"" << entry.details.verification_level << "\"";
            
            if (entry.revocation_reason) {
                oss << ",\n      \"revocation_reason\": \"" << *entry.revocation_reason << "\"";
            }
            
            oss << "\n    }";
            if (i < audit_entries_.size() - 1) {
                oss << ",";
            }
            oss << "\n";
        }
        
        oss << "  ]\n}";
        return oss.str();
    }
    
    // Default to simple text format
    std::ostringstream oss;
    oss << "Verification Audit Trail\n";
    oss << "========================\n\n";
    
    for (const auto& entry : audit_entries_) {
        auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
        oss << "Node: " << entry.node_id << "\n";
        oss << "Event: " << entry.event_type << "\n";
        oss << "Reviewer: " << entry.reviewer_email << "\n";
        oss << "Timestamp: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "\n";
        
        if (entry.revocation_reason) {
            oss << "Reason: " << *entry.revocation_reason << "\n";
        }
        
        oss << "\n";
    }
    
    return oss.str();
}

// VerificationPolicy implementation
bool VerificationPolicy::isVerificationRequired(const Value& node) {
    auto metadata = provenance::Provenance::getProvenance(node);
    if (!metadata) {
        return false; // No metadata means no verification required
    }
    
    // Require verification for AI-generated code with low confidence
    if (metadata->origin == provenance::OriginType::Agent) {
        double confidence = metadata->confidence_score.value_or(0.5);
        double threshold = std::stod(policies_.count("ai_verification_threshold") ? 
                                   policies_["ai_verification_threshold"] : "0.8");
        return confidence < threshold;
    }
    
    return false;
}

std::string VerificationPolicy::getRequiredVerificationLevel(const Value& node) {
    auto metadata = provenance::Provenance::getProvenance(node);
    if (!metadata) {
        return "none";
    }
    
    if (metadata->origin == provenance::OriginType::Agent) {
        double confidence = metadata->confidence_score.value_or(0.5);
        if (confidence < 0.5) {
            return "thorough";
        } else if (confidence < 0.8) {
            return "basic";
        }
    }
    
    return "none";
}

bool VerificationPolicy::meetsVerificationPolicy(const Value& node) {
    if (!isVerificationRequired(node)) {
        return true; // No verification required
    }
    
    auto metadata = provenance::Provenance::getProvenance(node);
    return metadata && metadata->origin == provenance::OriginType::Verified;
}

std::vector<std::string> VerificationPolicy::getPolicyViolations(const Value& node) {
    std::vector<std::string> violations;
    
    if (isVerificationRequired(node) && !meetsVerificationPolicy(node)) {
        std::string required_level = getRequiredVerificationLevel(node);
        violations.push_back("Code requires " + required_level + " verification but is not verified");
    }
    
    return violations;
}

void VerificationPolicy::setPolicy(const std::string& policy_name, const std::string& policy_value) {
    policies_[policy_name] = policy_value;
}

std::map<std::string, std::string> VerificationPolicy::getCurrentPolicies() {
    if (policies_.empty()) {
        initializeDefaultPolicies();
    }
    return policies_;
}

void VerificationPolicy::initializeDefaultPolicies() {
    policies_["ai_verification_threshold"] = "0.8";
    policies_["require_security_audit"] = "false";
    policies_["max_unverified_age_days"] = "7";
}

// VerificationWorkflow implementation
std::string VerificationWorkflow::submitForVerification(const Value& node, 
                                                      const std::string& submitter_email,
                                                      const std::string& priority) {
    std::string request_id = "VR" + std::to_string(next_request_id_++);
    
    VerificationRequest request(request_id, node, submitter_email);
    request.priority = priority;
    request.description = "Code verification request";
    
    pending_requests_.push_back(request);
    
    return request_id;
}

bool VerificationWorkflow::assignReviewer(const std::string& request_id, 
                                        const std::string& reviewer_email) {
    for (auto& request : pending_requests_) {
        if (request.request_id == request_id && request.status == "pending") {
            request.assigned_reviewer = reviewer_email;
            request.status = "assigned";
            request.assigned_at = std::chrono::system_clock::now();
            return true;
        }
    }
    return false;
}

std::vector<VerificationRequest> VerificationWorkflow::getPendingRequests() {
    std::vector<VerificationRequest> pending;
    for (const auto& request : pending_requests_) {
        if (request.status == "pending") {
            pending.push_back(request);
        }
    }
    return pending;
}

std::vector<VerificationRequest> VerificationWorkflow::getAssignedRequests(const std::string& reviewer_email) {
    std::vector<VerificationRequest> assigned;
    for (const auto& request : pending_requests_) {
        if (request.assigned_reviewer == reviewer_email && 
            (request.status == "assigned" || request.status == "in-progress")) {
            assigned.push_back(request);
        }
    }
    return assigned;
}

bool VerificationWorkflow::completeVerification(const std::string& request_id, 
                                              const VerificationDetails& details) {
    for (auto& request : pending_requests_) {
        if (request.request_id == request_id) {
            request.status = "completed";
            request.completed_at = std::chrono::system_clock::now();
            
            // Mark the node as verified
            ProvenanceVerifier verifier(request.assigned_reviewer);
            verifier.verifyWithDetails(request.code_node, details);
            
            return true;
        }
    }
    return false;
}

bool VerificationWorkflow::escalateRequest(const std::string& request_id, 
                                         const std::string& escalation_reason) {
    for (auto& request : pending_requests_) {
        if (request.request_id == request_id) {
            request.priority = "critical";
            request.description += " [ESCALATED: " + escalation_reason + "]";
            return true;
        }
    }
    return false;
}

} // namespace verification
} // namespace meld