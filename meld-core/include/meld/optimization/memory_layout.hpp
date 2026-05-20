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
// MEMORY LAYOUT CONTROL
// Task 11.4: Implement memory layout control
// Requirements: 7.4
// ============================================================================

// Memory layout representation
enum class LayoutRepr {
    Default,    // Default Meld layout
    C,          // C-compatible layout
    Packed,     // Packed layout (no padding)
    Aligned,    // Specific alignment
    Transparent // Transparent wrapper (same layout as inner type)
};

// Alignment specification
struct Alignment {
    size_t bytes;  // Alignment in bytes (must be power of 2)
    
    bool is_valid() const {
        return bytes > 0 && (bytes & (bytes - 1)) == 0;  // Power of 2 check
    }
};

// Field layout information
struct FieldLayout {
    std::string name;
    size_t offset;      // Offset from struct start
    size_t size;        // Size in bytes
    size_t alignment;   // Alignment requirement
};

// Struct layout information
struct StructLayout {
    std::string name;
    LayoutRepr repr;
    size_t total_size;
    size_t alignment;
    std::vector<FieldLayout> fields;
    size_t padding_bytes;  // Total padding added
    
    double padding_ratio() const {
        return total_size > 0 ? 
            static_cast<double>(padding_bytes) / total_size : 0.0;
    }
};

// ============================================================================
// LAYOUT ANALYZER
// ============================================================================

class LayoutAnalyzer {
public:
    // Analyze struct layout
    static StructLayout analyze_layout(
        const parser::ast::class_definition& struct_def,
        LayoutRepr repr = LayoutRepr::Default
    );
    
    // Calculate field offsets
    static std::vector<FieldLayout> calculate_field_offsets(
        const std::vector<std::pair<std::string, size_t>>& fields,
        LayoutRepr repr,
        size_t struct_alignment = 0
    );
    
    // Estimate size of type
    static size_t estimate_type_size(const std::string& type_name);
    
    // Estimate alignment of type
    static size_t estimate_type_alignment(const std::string& type_name);
    
    // Compare layouts
    static void compare_layouts(const StructLayout& layout1, const StructLayout& layout2);
};

// ============================================================================
// LAYOUT OPTIMIZER
// ============================================================================

class LayoutOptimizer {
public:
    // Optimize field ordering to minimize padding
    static std::vector<FieldLayout> optimize_field_order(
        const std::vector<FieldLayout>& fields
    );
    
    // Suggest optimal layout representation
    static LayoutRepr suggest_layout(const parser::ast::class_definition& struct_def);
    
    // Calculate padding savings from optimization
    static size_t calculate_padding_savings(
        const StructLayout& original,
        const StructLayout& optimized
    );
};

// ============================================================================
// REPR ATTRIBUTE HANDLER
// ============================================================================

class ReprAttributeHandler {
public:
    // Parse repr attribute
    // e.g., @repr(C), @repr(packed), @repr(align(16))
    static std::expected<LayoutRepr, std::string> parse_repr_attribute(
        const std::string& attr
    );
    
    // Parse alignment from repr attribute
    static std::expected<Alignment, std::string> parse_alignment(
        const std::string& attr
    );
    
    // Apply repr attribute to struct
    static std::expected<StructLayout, std::string> apply_repr(
        const parser::ast::class_definition& struct_def,
        const std::string& repr_attr
    );
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Calculate padding needed for alignment
size_t calculate_padding(size_t offset, size_t alignment);

// Round up to next multiple of alignment
size_t align_to(size_t value, size_t alignment);

// Check if layout is C-compatible
bool is_c_compatible(const StructLayout& layout);

} // namespace meld::optimization
