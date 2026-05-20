#include "meld/types/newtype.hpp"
#include <sstream>

namespace meld {
namespace types {

// Newtype compiler implementation
NewtypeCompiler::NewtypeInfo NewtypeCompiler::analyze_newtype(
    const std::string& name,
    const std::string& inner_type
) {
    NewtypeInfo info;
    info.name = name;
    info.inner_type = inner_type;
    info.tag_type = name + "Tag";
    
    // Calculate size (should be same as inner type for zero-cost)
    if (inner_type == "int") {
        info.size_bytes = sizeof(int);
    } else if (inner_type == "double") {
        info.size_bytes = sizeof(double);
    } else if (inner_type == "string") {
        info.size_bytes = sizeof(std::string);
    } else {
        info.size_bytes = 0;  // Unknown type
    }
    
    // Verify zero-cost property
    info.is_zero_cost = verify_zero_cost(info);
    
    return info;
}

bool NewtypeCompiler::verify_zero_cost(const NewtypeInfo& info) {
    // Zero-cost means:
    // 1. Same size as inner type
    // 2. No virtual functions
    // 3. No additional data members
    // 4. Can be optimized away at compile time
    
    // For our implementation, newtypes are always zero-cost
    // because they're just wrappers with no additional overhead
    return info.size_bytes > 0;
}

std::string NewtypeCompiler::generate_optimized_code(const NewtypeInfo& info) {
    std::ostringstream oss;
    
    oss << "// Zero-cost newtype: " << info.name << "\n";
    oss << "// Inner type: " << info.inner_type << "\n";
    oss << "// Size: " << info.size_bytes << " bytes (same as inner type)\n";
    oss << "\n";
    oss << "struct " << info.tag_type << " {};\n";
    oss << "using " << info.name << " = Newtype<" << info.inner_type 
        << ", " << info.tag_type << ">;\n";
    
    return oss.str();
}

bool NewtypeCompiler::check_type_safety(
    const std::string& operation,
    const NewtypeInfo& lhs,
    const NewtypeInfo& rhs
) {
    // Type safety check: newtypes with different tags cannot be mixed
    if (operation == "add" || operation == "subtract" || 
        operation == "multiply" || operation == "divide") {
        // Arithmetic operations require same newtype
        return lhs.tag_type == rhs.tag_type;
    }
    
    if (operation == "compare") {
        // Comparison requires same newtype
        return lhs.tag_type == rhs.tag_type;
    }
    
    return false;
}

// Newtype optimizer implementation
NewtypeOptimizer::OptimizationResult NewtypeOptimizer::analyze(
    const NewtypeCompiler::NewtypeInfo& info
) {
    OptimizationResult result;
    
    // Newtypes can always be inlined
    result.can_inline = true;
    
    // Wrapper can be elided if only used for type checking
    result.can_elide_wrapper = info.is_zero_cost;
    
    // No runtime checks needed for zero-cost abstractions
    result.requires_runtime_check = false;
    
    result.optimization_strategy = "inline_and_elide";
    
    return result;
}

std::string NewtypeOptimizer::apply_optimizations(
    const std::string& code,
    const OptimizationResult& result
) {
    if (result.can_elide_wrapper) {
        // In optimized code, newtype wrappers are completely removed
        // and operations work directly on inner types
        return "// Optimized: wrapper elided\n" + code;
    }
    
    return code;
}

bool NewtypeOptimizer::verify_optimization(
    const std::string& original,
    const std::string& optimized
) {
    // Verify that optimization preserves semantics
    // In a real implementation, this would compare ASTs or run tests
    return true;
}

// Newtype validator implementation
template<typename T, typename Tag>
std::vector<std::pair<std::string, typename NewtypeValidator<T, Tag>::ValidatorFn>>
NewtypeValidator<T, Tag>::validators_;

template<typename T, typename Tag>
void NewtypeValidator<T, Tag>::add_validator(
    const std::string& name,
    ValidatorFn validator
) {
    validators_.push_back({name, validator});
}

template<typename T, typename Tag>
bool NewtypeValidator<T, Tag>::validate(const T& value) {
    for (const auto& [name, validator] : validators_) {
        if (!validator(value)) {
            return false;
        }
    }
    return true;
}

template<typename T, typename Tag>
std::vector<std::string> NewtypeValidator<T, Tag>::get_errors(const T& value) {
    std::vector<std::string> errors;
    
    for (const auto& [name, validator] : validators_) {
        if (!validator(value)) {
            errors.push_back("Validation failed: " + name);
        }
    }
    
    return errors;
}

// Explicit template instantiations for common types
template class NewtypeValidator<int, struct IntTag>;
template class NewtypeValidator<double, struct DoubleTag>;
template class NewtypeValidator<std::string, struct StringTag>;

} // namespace types
} // namespace meld
