#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/parser/ast.hpp"
#include <string>
#include <memory>
#include <vector>
#include <expected>
#include <cstdint>

namespace meld::optimization {

// ============================================================================
// SIMD OPERATION SUPPORT
// Task 11.5: Add SIMD operation support
// Requirements: 7.5
// ============================================================================

// SIMD vector types
enum class SimdType {
    F32x4,   // 4x float32
    F64x2,   // 2x float64
    I32x4,   // 4x int32
    I64x2,   // 2x int64
    I16x8,   // 8x int16
    I8x16    // 16x int8
};

// SIMD operations
enum class SimdOp {
    Add,
    Sub,
    Mul,
    Div,
    Min,
    Max,
    And,
    Or,
    Xor,
    Sqrt,
    Abs,
    Neg
};

// SIMD vector wrapper
struct SimdVector {
    SimdType type;
    std::vector<double> values;  // Generic storage
    
    size_t lane_count() const;
    size_t element_size() const;
    std::string type_name() const;
};

// Vectorization opportunity
struct VectorizationOpportunity {
    kernel::Value loop;
    std::string operation;
    size_t vector_width;
    double estimated_speedup;
    bool is_safe;  // Safe to vectorize
};

// ============================================================================
// SIMD ANALYZER
// ============================================================================

class SimdAnalyzer {
public:
    // Detect vectorization opportunities in code
    static std::vector<VectorizationOpportunity> detect_opportunities(
        const kernel::Value& ast
    );
    
    // Check if loop can be vectorized
    static bool can_vectorize(const kernel::Value& loop);
    
    // Determine optimal vector width
    static size_t determine_vector_width(const std::string& element_type);
    
    // Estimate vectorization speedup
    static double estimate_speedup(const VectorizationOpportunity& opp);
    
    // Check for vectorization hazards
    static std::vector<std::string> check_hazards(const kernel::Value& loop);
};

// ============================================================================
// SIMD VECTORIZER
// ============================================================================

class SimdVectorizer {
public:
    // Vectorize a loop
    std::expected<kernel::Value, std::string>
    vectorize_loop(const kernel::Value& loop, size_t vector_width);
    
    // Generate SIMD instructions
    kernel::Value generate_simd_code(
        SimdOp operation,
        SimdType vector_type,
        const std::vector<kernel::Value>& operands
    );
    
    // Auto-vectorize eligible code
    std::expected<kernel::Value, std::string>
    auto_vectorize(const kernel::Value& ast);
    
    // Get vectorization statistics
    struct Statistics {
        size_t loops_analyzed;
        size_t loops_vectorized;
        size_t loops_failed;
        double total_estimated_speedup;
    };
    
    Statistics get_statistics() const {
        return stats_;
    }
    
private:
    Statistics stats_;
    
    // Helper: Extract loop body
    kernel::Value extract_loop_body(const kernel::Value& loop);
    
    // Helper: Generate vector load/store
    kernel::Value generate_vector_load(const kernel::Value& address, SimdType type);
    kernel::Value generate_vector_store(const kernel::Value& address, const kernel::Value& vector);
};

// ============================================================================
// SIMD TYPE WRAPPERS
// ============================================================================

class SimdTypeWrapper {
public:
    // Create SIMD vector type
    static SimdVector create_vector(SimdType type, const std::vector<double>& values);
    
    // Get SIMD type from element type and width
    static std::expected<SimdType, std::string> 
    get_simd_type(const std::string& element_type, size_t width);
    
    // Check if type supports SIMD
    static bool supports_simd(const std::string& type_name);
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Convert scalar operation to SIMD operation
SimdOp scalar_to_simd_op(const std::string& scalar_op);

// Get SIMD instruction name
std::string get_simd_instruction(SimdOp op, SimdType type);

// Check if operation is SIMD-compatible
bool is_simd_compatible(const std::string& operation);

} // namespace meld::optimization
