#pragma once

#include "meld/provenance/provenance.hpp"
#include "meld/kernel/primitives.hpp"
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <optional>

namespace meld {
namespace verification {

// Import kernel::Value for convenience
using kernel::Value;

/**
 * VerificationDetails - Detailed information about code verification
 */
struct VerificationDetails {
    std::string verification_type;    // "manual", "automated", "peer-review"
    std::string verification_level;   // "basic", "thorough", "security-audit"
    std::string verification_notes;   // Free-form notes about the verification
    std::vector<std::string> checks_performed; // List of specific checks
    std::optional<std::string> security_clearance; // Security level if applicable
    std::optional<std::string> compliance_standard; // Compliance standard met
    
    VerificationDetails(const std::string& type = "manual", 
                       const std::string& level = "basic",
                       const std::string& notes = "")
        : verification_type(type), verification_level(level), verification_notes(notes) {}
};

/**
 * VerificationInfo - Information about a verification event
 */
struct VerificationInfo {
    std::string reviewer_email;
    std::string reviewer_name;
    std::chrono::system_clock::time_point verification_timestamp;
    VerificationDetails details;
    bool is_active;  // false if verification was revoked
    
    VerificationInfo() : is_active(true) {}
};

/**
 * AuditEntry - Single entry in the verification audit trail
 */
struct AuditEntry {
    std::string node_id;              // Unique identifier for the node
    std::string event_type;           // "verified", "revoked"
    std::string reviewer_email;
    std::chrono::system_clock::time_point timestamp;
    VerificationDetails details;
    std::optional<std::string> revocation_reason;
    
    AuditEntry(const std::string& id, const std::string& type, const std::string& email)
        : node_id(id), event_type(type), reviewer_email(email), 
          timestamp(std::chrono::system_clock::now()) {}
};

/**
 * VerificationStats - Statistics about verification activities
 */
struct VerificationStats {
    int total_verifications;
    int active_verifications;
    int revoked_verifications;
    std::vector<std::string> top_reviewers;
    double average_verification_time_hours;
    std::map<std::string, int> verification_types;
    std::map<std::string, int> verification_levels;
};

/**
 * VerificationRequest - A request for code verification
 */
struct VerificationRequest {
    std::string request_id;
    Value code_node;
    std::string submitter_email;
    std::string assigned_reviewer;
    std::string priority;           // "low", "normal", "high", "critical"
    std::string status;            // "pending", "assigned", "in-progress", "completed"
    std::chrono::system_clock::time_point submitted_at;
    std::optional<std::chrono::system_clock::time_point> assigned_at;
    std::optional<std::chrono::system_clock::time_point> completed_at;
    std::string description;
    
    VerificationRequest(const std::string& id, const Value& node, const std::string& submitter)
        : request_id(id), code_node(node), submitter_email(submitter), 
          priority("normal"), status("pending"), 
          submitted_at(std::chrono::system_clock::now()) {}
};

/**
 * ProvenanceVerifier - Handles verification of code provenance
 * 
 * This class provides functionality to verify AI-generated or human-written
 * code, marking it as verified and tracking reviewer information.
 */
class ProvenanceVerifier {
public:
    // Constructor with reviewer information
    ProvenanceVerifier(const std::string& reviewer_email, const std::string& reviewer_name = "");
    
    // Mark a single node as verified
    Value markAsVerified(const Value& node, const std::string& verification_notes = "");
    
    // Mark multiple nodes as verified (batch operation)
    std::vector<Value> markAsVerified(const std::vector<Value>& nodes, 
                                    const std::string& verification_notes = "");
    
    // Verify with detailed review information
    Value verifyWithDetails(const Value& node, 
                          const VerificationDetails& details);
    
    // Check if a node is already verified
    bool isVerified(const Value& node) const;
    
    // Get verification information for a node
    std::optional<VerificationInfo> getVerificationInfo(const Value& node) const;
    
    // Revoke verification (mark as unverified)
    Value revokeVerification(const Value& node, const std::string& reason = "");
    
    // Get reviewer information
    std::string getReviewerEmail() const { return reviewer_email_; }
    std::string getReviewerName() const { return reviewer_name_; }
    
    // Set reviewer information
    void setReviewer(const std::string& email, const std::string& name = "");
    
private:
    std::string reviewer_email_;
    std::string reviewer_name_;
    
    // Create verification metadata
    VerificationInfo createVerificationInfo(const std::string& notes) const;
};

/**
 * VerificationAudit - Audit trail for verification events
 * 
 * This class maintains a complete audit trail of all verification
 * events for compliance and security purposes.
 */
class VerificationAudit {
public:
    // Record a verification event
    static void recordVerification(const Value& node, const VerificationInfo& info);
    
    // Record a verification revocation
    static void recordRevocation(const Value& node, const std::string& reason, 
                               const std::string& revoker_email);
    
    // Get complete audit trail for a node
    static std::vector<AuditEntry> getAuditTrail(const Value& node);
    
    // Get all verifications by a specific reviewer
    static std::vector<AuditEntry> getVerificationsByReviewer(const std::string& reviewer_email);
    
    // Get verification statistics
    static VerificationStats getStats();
    
    // Export audit trail for compliance reporting
    static std::string exportAuditTrail(const std::string& format = "json");
    
private:
    // Internal audit storage (in production, this would be persistent)
    static std::vector<AuditEntry> audit_entries_;
};

/**
 * VerificationPolicy - Configurable policies for verification requirements
 * 
 * This class defines policies for when verification is required and
 * what level of verification is needed for different types of code.
 */
class VerificationPolicy {
public:
    // Check if verification is required for a node
    static bool isVerificationRequired(const Value& node);
    
    // Get required verification level for a node
    static std::string getRequiredVerificationLevel(const Value& node);
    
    // Check if current verification meets policy requirements
    static bool meetsVerificationPolicy(const Value& node);
    
    // Get policy violations for a node
    static std::vector<std::string> getPolicyViolations(const Value& node);
    
    // Configure verification policies
    static void setPolicy(const std::string& policy_name, const std::string& policy_value);
    
    // Get current policy configuration
    static std::map<std::string, std::string> getCurrentPolicies();
    
private:
    static std::map<std::string, std::string> policies_;
    static void initializeDefaultPolicies();
};

/**
 * VerificationWorkflow - Workflow management for verification processes
 * 
 * This class manages verification workflows, including assignment of
 * reviewers, tracking of review progress, and escalation procedures.
 */
class VerificationWorkflow {
public:
    // Submit code for verification
    static std::string submitForVerification(const Value& node, 
                                           const std::string& submitter_email,
                                           const std::string& priority = "normal");
    
    // Assign reviewer to a verification request
    static bool assignReviewer(const std::string& request_id, 
                             const std::string& reviewer_email);
    
    // Get pending verification requests
    static std::vector<VerificationRequest> getPendingRequests();
    
    // Get verification requests assigned to a reviewer
    static std::vector<VerificationRequest> getAssignedRequests(const std::string& reviewer_email);
    
    // Complete a verification request
    static bool completeVerification(const std::string& request_id, 
                                   const VerificationDetails& details);
    
    // Escalate a verification request
    static bool escalateRequest(const std::string& request_id, 
                              const std::string& escalation_reason);
    
private:
    static std::vector<VerificationRequest> pending_requests_;
    static int next_request_id_;
};

} // namespace verification
} // namespace meld