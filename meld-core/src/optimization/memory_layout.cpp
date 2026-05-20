#include "meld/optimization/memory_layout.hpp"
#include <format>
#include <algorithm>
#include <iostream>

namespace meld::optimization {

// ============================================================================
// LayoutAnalyzer Implementation
// ============================================================================

StructLayout LayoutAnalyzer::analyze_layout(
    const parser::ast::class_definition& struct_def,
    LayoutRepr repr) {
    
    StructLayout layout;
    layout.name = struct_def.name.name;
    layout.repr = repr;
    
    // Extract field information
    std::vector<std::pair<std::string, size_t>> field_info;
    for (const auto& field : struct_def.fields) {
        std::string field_name = field.name.name;
        size_t field_size = estimate_type_size(field.type.type_name.name);
        field_info.push_back({field_name, field_size});
    }
    
    // Calculate field offsets based on repr
    layout.fields = calculate_field_offsets(field_info, repr);
    
    // Calculate total size and alignment
    if (!layout.fields.empty()) {
        const auto& last_field = layout.fields.back();
        layout.total_size = last_field.offset + last_field.size;
        
        // Struct alignment is max of field alignments
        layout.alignment = 1;
        for (const auto& field : layout.fields) {
            layout.alignment = std::max(layout.alignment, field.alignment);
        }
        
        // Align total size to struct alignment
        if (repr != LayoutRepr::Packed) {
            layout.total_size = align_to(layout.total_size, layout.alignment);
        }
    } else {
        layout.total_size = 0;
        layout.alignment = 1;
    }
    
    // Calculate padding
    size_t fields_size = 0;
    for (const auto& field : layout.fields) {
        fields_size += field.size;
    }
    layout.padding_bytes = layout.total_size - fields_size;
    
    return layout;
}

std::vector<FieldLayout> LayoutAnalyzer::calculate_field_offsets(
    const std::vector<std::pair<std::string, size_t>>& fields,
    LayoutRepr repr,
    size_t struct_alignment) {
    
    std::vector<FieldLayout> layouts;
    size_t current_offset = 0;
    
    for (const auto& [name, size] : fields) {
        FieldLayout field;
        field.name = name;
        field.size = size;
        
        // Determine alignment based on repr
        if (repr == LayoutRepr::Packed) {
            field.alignment = 1;  // No alignment for packed
        } else {
            field.alignment = estimate_type_alignment(name);
        }
        
        // Calculate offset with padding
        if (repr != LayoutRepr::Packed) {
            current_offset = align_to(current_offset, field.alignment);
        }
        
        field.offset = current_offset;
        current_offset += size;
        
        layouts.push_back(field);
    }
    
    return layouts;
}

size_t LayoutAnalyzer::estimate_type_size(const std::string& type_name) {
    // Estimate size based on type name
    if (type_name == "bool") return 1;
    if (type_name == "i8" || type_name == "u8") return 1;
    if (type_name == "i16" || type_name == "u16") return 2;
    if (type_name == "i32" || type_name == "u32" || type_name == "f32") return 4;
    if (type_name == "i64" || type_name == "u64" || type_name == "f64") return 8;
    if (type_name == "Int") return 8;
    if (type_name == "Float") return 8;
    if (type_name == "String") return 16;  // Pointer + length
    
    // Default: pointer size
    return 8;
}

size_t LayoutAnalyzer::estimate_type_alignment(const std::string& type_name) {
    // Alignment typically matches size for primitive types
    size_t size = estimate_type_size(type_name);
    
    // Cap alignment at 8 bytes for most platforms
    return std::min(size, size_t(8));
}

void LayoutAnalyzer::compare_layouts(
    const StructLayout& layout1,
    const StructLayout& layout2) {
    
    std::cout << "Layout Comparison:" << std::endl;
    std::cout << "  Layout 1 (" << static_cast<int>(layout1.repr) << "):" << std::endl;
    std::cout << "    Size: " << layout1.total_size << " bytes" << std::endl;
    std::cout << "    Padding: " << layout1.padding_bytes << " bytes" << std::endl;
    
    std::cout << "  Layout 2 (" << static_cast<int>(layout2.repr) << "):" << std::endl;
    std::cout << "    Size: " << layout2.total_size << " bytes" << std::endl;
    std::cout << "    Padding: " << layout2.padding_bytes << " bytes" << std::endl;
    
    if (layout1.total_size < layout2.total_size) {
        std::cout << "  Layout 1 is more compact" << std::endl;
    } else if (layout2.total_size < layout1.total_size) {
        std::cout << "  Layout 2 is more compact" << std::endl;
    } else {
        std::cout << "  Layouts have same size" << std::endl;
    }
}

// ============================================================================
// LayoutOptimizer Implementation
// ============================================================================

std::vector<FieldLayout> LayoutOptimizer::optimize_field_order(
    const std::vector<FieldLayout>& fields) {
    
    // Sort fields by alignment (descending) to minimize padding
    std::vector<FieldLayout> optimized = fields;
    
    std::sort(optimized.begin(), optimized.end(),
        [](const FieldLayout& a, const FieldLayout& b) {
            return a.alignment > b.alignment;
        });
    
    // Recalculate offsets
    size_t offset = 0;
    for (auto& field : optimized) {
        offset = align_to(offset, field.alignment);
        field.offset = offset;
        offset += field.size;
    }
    
    return optimized;
}

LayoutRepr LayoutOptimizer::suggest_layout(
    const parser::ast::class_definition& struct_def) {
    
    // Analyze struct to suggest optimal layout
    // - Use packed for small structs with no alignment requirements
    // - Use C for FFI compatibility
    // - Use default for most cases
    
    size_t field_count = struct_def.fields.size();
    
    if (field_count <= 2) {
        return LayoutRepr::Packed;  // Small structs benefit from packing
    }
    
    return LayoutRepr::Default;
}

size_t LayoutOptimizer::calculate_padding_savings(
    const StructLayout& original,
    const StructLayout& optimized) {
    
    if (original.padding_bytes > optimized.padding_bytes) {
        return original.padding_bytes - optimized.padding_bytes;
    }
    return 0;
}

// ============================================================================
// ReprAttributeHandler Implementation
// ============================================================================

std::expected<LayoutRepr, std::string>
ReprAttributeHandler::parse_repr_attribute(const std::string& attr) {
    
    if (attr == "C" || attr == "c") {
        return LayoutRepr::C;
    } else if (attr == "packed") {
        return LayoutRepr::Packed;
    } else if (attr.starts_with("align")) {
        return LayoutRepr::Aligned;
    } else if (attr == "transparent") {
        return LayoutRepr::Transparent;
    }
    
    return std::unexpected("Unknown repr attribute: " + attr);
}

std::expected<Alignment, std::string>
ReprAttributeHandler::parse_alignment(const std::string& attr) {
    
    // Parse "align(N)" format
    if (!attr.starts_with("align(") || !attr.ends_with(")")) {
        return std::unexpected("Invalid alignment format");
    }
    
    std::string num_str = attr.substr(6, attr.length() - 7);
    
    try {
        size_t bytes = std::stoull(num_str);
        Alignment align{bytes};
        
        if (!align.is_valid()) {
            return std::unexpected("Alignment must be a power of 2");
        }
        
        return align;
    } catch (...) {
        return std::unexpected("Invalid alignment value");
    }
}

std::expected<StructLayout, std::string>
ReprAttributeHandler::apply_repr(
    const parser::ast::class_definition& struct_def,
    const std::string& repr_attr) {
    
    auto repr_result = parse_repr_attribute(repr_attr);
    if (!repr_result) {
        return std::unexpected(repr_result.error());
    }
    
    LayoutRepr repr = *repr_result;
    
    // Analyze layout with specified repr
    StructLayout layout = LayoutAnalyzer::analyze_layout(struct_def, repr);
    
    return layout;
}

// ============================================================================
// Helper Functions
// ============================================================================

size_t calculate_padding(size_t offset, size_t alignment) {
    if (alignment == 0) return 0;
    
    size_t remainder = offset % alignment;
    if (remainder == 0) return 0;
    
    return alignment - remainder;
}

size_t align_to(size_t value, size_t alignment) {
    if (alignment == 0) return value;
    
    size_t padding = calculate_padding(value, alignment);
    return value + padding;
}

bool is_c_compatible(const StructLayout& layout) {
    // Check if layout follows C struct rules
    return layout.repr == LayoutRepr::C || layout.repr == LayoutRepr::Default;
}

} // namespace meld::optimization
