#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <memory>
#include <vector>
#include <set>
#include <expected>

namespace meld::optimization {

// ============================================================================
// CLOSURE INLINING OPTIMIZATION
// Task 11.3: Add closure inlining
// Requirements: 7.3
// ============================================================================

// Closure capture analysis
struct CaptureInfo {
    std::set<std::string> captured_vars;  // Variables captured from environment
    std::set<std::string> by_value;       // Captured by value
    std::set<std::string> by_reference;   // Captured by reference
    bool captures_self;                    // Captures 'self'
    
    size_t total_captures() const {
        return captured_vars.size();
    }
    
    bool is_trivial() const {
        return captured_vars.empty();
    }
};

// Closure call site information
struct ClosureCallSite {
    kernel::Value closure;
    kernel::Value call_expr;
    std::string location;
    size_t call_count;  // How many times this closure is called
    
    bool is_hot() const {
        return call_count > 10;  // Threshold for hot path
    }
};

// Inlining decision
struct InliningDecision {
    bool should_inline;
    std::string reason;
    size_t estimated_size;
    double estimated_benefit;
};

// ============================================================================
// CLOSURE ANALYZER
// ============================================================================

class ClosureAnalyzer {
public:
    // Analyze closure captures
    static CaptureInfo analyze_captures(const kernel::Value& closure);
    
    // Estimate closure size
    static size_t estimate_size(const kernel::Value& closure);
    
    // Find all closure call sites in AST
    static std::vector<ClosureCallSite> find_call_sites(const kernel::Value& ast);
    
    // Check if closure can be safely inlined
    static bool can_inline(const kernel::Value& closure, const ClosureCallSite& site);
    
    // Estimate inlining benefit
    static double estimate_benefit(const kernel::Value& closure, const ClosureCallSite& site);
};

// ============================================================================
// CLOSURE INLINER
// ============================================================================

class ClosureInliner {
public:
    ClosureInliner() = default;
    
    // Inline closure at call site
    std::expected<kernel::Value, std::string>
    inline_closure(const kernel::Value& closure, const ClosureCallSite& site);
    
    // Inline all eligible closures in AST
    std::expected<kernel::Value, std::string>
    inline_all_closures(const kernel::Value& ast, size_t size_threshold = 20);
    
    // Make inlining decision
    InliningDecision decide_inlining(
        const kernel::Value& closure,
        const ClosureCallSite& site,
        size_t size_threshold
    );
    
    // Get inlining statistics
    struct Statistics {
        size_t total_closures;
        size_t inlined_closures;
        size_t skipped_closures;
        double total_benefit;
    };
    
    Statistics get_statistics() const {
        return stats_;
    }
    
private:
    Statistics stats_;
    
    // Helper: Substitute closure parameters with arguments
    kernel::Value substitute_parameters(
        const kernel::Value& body,
        const std::vector<std::string>& params,
        const std::vector<kernel::Value>& args
    );
    
    // Helper: Substitute captured variables
    kernel::Value substitute_captures(
        const kernel::Value& body,
        const CaptureInfo& captures
    );
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Check if closure is small enough to inline
bool is_small_closure(const kernel::Value& closure, size_t threshold);

// Extract closure body
kernel::Value extract_closure_body(const kernel::Value& closure);

// Extract closure parameters
std::vector<std::string> extract_closure_parameters(const kernel::Value& closure);

} // namespace meld::optimization
