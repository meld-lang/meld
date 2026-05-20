#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <memory>
#include <vector>
#include <expected>
#include <optional>

namespace meld::optimization {

// ============================================================================
// ITERATOR FUSION OPTIMIZATION
// Task 11.2: Implement iterator optimization
// Requirements: 7.2
// ============================================================================

// Iterator operation types
enum class IteratorOp {
    Map,        // Transform elements
    Filter,     // Select elements
    FlatMap,    // Transform and flatten
    Take,       // Take first N elements
    Skip,       // Skip first N elements
    Zip,        // Combine two iterators
    Enumerate,  // Add indices
    Collect     // Materialize results
};

// Iterator operation node
struct IteratorOperation {
    IteratorOp op_type;
    kernel::Value function;  // For map, filter, flatMap
    std::optional<int64_t> count;  // For take, skip
    std::optional<kernel::Value> other_iterator;  // For zip
    
    std::string to_string() const;
};

// Iterator chain
// Represents a sequence of iterator operations
struct IteratorChain {
    kernel::Value source;  // Source collection
    std::vector<IteratorOperation> operations;
    
    bool is_fusible() const;
    size_t complexity() const;
};

// ============================================================================
// ITERATOR FUSION ENGINE
// ============================================================================

class IteratorFusionEngine {
public:
    // Fuse iterator chain into optimized loop
    static std::expected<kernel::Value, std::string>
    fuse_chain(const IteratorChain& chain);
    
    // Detect iterator chains in AST
    static std::vector<IteratorChain> detect_chains(const kernel::Value& ast);
    
    // Optimize iterator chain
    static std::expected<kernel::Value, std::string>
    optimize_chain(const IteratorChain& chain);
    
    // Generate efficient loop from chain
    static kernel::Value generate_loop(const IteratorChain& chain);
    
    // Check if operations can be fused
    static bool can_fuse(const IteratorOperation& op1, const IteratorOperation& op2);
};

// ============================================================================
// LAZY EVALUATION SUPPORT
// ============================================================================

class LazyIterator {
public:
    // Create lazy iterator from source
    static kernel::Value create_lazy(const kernel::Value& source);
    
    // Add lazy operation
    static kernel::Value add_operation(
        const kernel::Value& iterator,
        const IteratorOperation& op
    );
    
    // Force evaluation
    static kernel::Value force_evaluation(const kernel::Value& iterator);
    
    // Check if iterator is lazy
    static bool is_lazy(const kernel::Value& iterator);
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Parse iterator chain from method calls
// e.g., "list.map(f).filter(g).collect()" -> IteratorChain
std::expected<IteratorChain, std::string> parse_iterator_chain(
    const kernel::Value& expr
);

// Estimate performance improvement from fusion
double estimate_fusion_benefit(const IteratorChain& chain);

} // namespace meld::optimization
