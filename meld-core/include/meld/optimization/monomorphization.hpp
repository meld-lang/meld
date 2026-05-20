#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/parser/ast.hpp"
#include "meld/meta/metatype.hpp"
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <set>
#include <expected>
#include <functional>

namespace meld::optimization {

// ============================================================================
// GENERIC TYPE MONOMORPHIZATION
// Task 11.1: Add monomorphization for generics
// Requirements: 7.1
// ============================================================================

// Type parameter instantiation
// Maps type parameter names to concrete types
using TypeInstantiation = std::map<std::string, std::shared_ptr<meta::MetaType>>;

// Monomorphization key for caching
// Uniquely identifies a monomorphized instance
struct MonomorphizationKey {
    std::string base_name;
    TypeInstantiation type_args;
    
    bool operator<(const MonomorphizationKey& other) const;
    bool operator==(const MonomorphizationKey& other) const;
    std::string to_string() const;
};

// Monomorphization result
// Contains the specialized code and metadata
struct MonomorphizedInstance {
    std::string specialized_name;
    kernel::Value specialized_ast;
    TypeInstantiation type_args;
    std::set<std::string> dependencies;  // Other monomorphized instances this depends on
};

// ============================================================================
// MONOMORPHIZATION ENGINE
// ============================================================================

class MonomorphizationEngine {
public:
    MonomorphizationEngine() = default;
    
    // Monomorphize a generic function
    std::expected<MonomorphizedInstance, std::string>
    monomorphize_function(
        const parser::ast::function_declaration& func_def,
        const TypeInstantiation& type_args
    );
    
    // Monomorphize a generic type/class
    std::expected<MonomorphizedInstance, std::string>
    monomorphize_type(
        const parser::ast::class_definition& type_def,
        const TypeInstantiation& type_args
    );
    
    // Monomorphize a generic trait implementation
    std::expected<MonomorphizedInstance, std::string>
    monomorphize_trait_impl(
        const kernel::Value& trait_impl,
        const TypeInstantiation& type_args
    );
    
    // Get or create a monomorphized instance (with caching)
    std::expected<MonomorphizedInstance, std::string>
    get_or_create_instance(
        const MonomorphizationKey& key,
        std::function<std::expected<MonomorphizedInstance, std::string>()> generator
    );
    
    // Check if an instance is already monomorphized
    bool has_instance(const MonomorphizationKey& key) const;
    
    // Get a cached instance
    std::expected<MonomorphizedInstance, std::string>
    get_instance(const MonomorphizationKey& key) const;
    
    // Clear the monomorphization cache
    void clear_cache();
    
    // Get all monomorphized instances
    const std::map<MonomorphizationKey, MonomorphizedInstance>& instances() const {
        return cache_;
    }
    
    // Generate statistics about monomorphization
    struct Statistics {
        size_t total_instances;
        size_t function_instances;
        size_t type_instances;
        size_t trait_impl_instances;
        size_t cache_hits;
        size_t cache_misses;
    };
    
    Statistics get_statistics() const;
    
private:
    // Cache of monomorphized instances
    std::map<MonomorphizationKey, MonomorphizedInstance> cache_;
    
    // Statistics tracking
    mutable size_t cache_hits_ = 0;
    mutable size_t cache_misses_ = 0;
    
    // Helper: Generate specialized name
    std::string generate_specialized_name(
        const std::string& base_name,
        const TypeInstantiation& type_args
    ) const;
    
    // Helper: Substitute type parameters in AST
    kernel::Value substitute_type_parameters(
        const kernel::Value& ast,
        const TypeInstantiation& type_args
    ) const;
    
    // Helper: Extract dependencies from AST
    std::set<std::string> extract_dependencies(
        const kernel::Value& ast
    ) const;
    
    // Helper: Validate type instantiation
    std::expected<void, std::string> validate_type_instantiation(
        const std::vector<std::string>& type_params,
        const TypeInstantiation& type_args
    ) const;
};

// ============================================================================
// MONOMORPHIZATION ANALYZER
// ============================================================================

class MonomorphizationAnalyzer {
public:
    // Analyze a program to find all generic instantiations
    struct InstantiationSite {
        std::string generic_name;
        TypeInstantiation type_args;
        std::string location;  // Source location
    };
    
    static std::vector<InstantiationSite> find_instantiations(
        const kernel::Value& program_ast
    );
    
    // Determine monomorphization order (topological sort)
    static std::expected<std::vector<MonomorphizationKey>, std::string>
    determine_monomorphization_order(
        const std::vector<InstantiationSite>& sites,
        const std::map<std::string, std::set<std::string>>& dependencies
    );
    
    // Estimate code size impact of monomorphization
    struct CodeSizeEstimate {
        size_t original_size;
        size_t monomorphized_size;
        double expansion_factor;
    };
    
    static CodeSizeEstimate estimate_code_size(
        const std::vector<MonomorphizedInstance>& instances
    );
};

// ============================================================================
// MONOMORPHIZATION OPTIMIZER
// ============================================================================

class MonomorphizationOptimizer {
public:
    // Optimize monomorphized code
    static kernel::Value optimize_instance(
        const MonomorphizedInstance& instance
    );
    
    // Detect and merge duplicate monomorphizations
    static std::vector<MonomorphizedInstance> deduplicate_instances(
        const std::vector<MonomorphizedInstance>& instances
    );
    
    // Inline small monomorphized functions
    static kernel::Value inline_small_functions(
        const kernel::Value& ast,
        size_t size_threshold = 10
    );
    
    // Specialize for common types (e.g., int, string)
    static kernel::Value specialize_for_common_types(
        const MonomorphizedInstance& instance
    );
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Parse type parameters from generic definition
// e.g., "Array<T>" -> ["T"]
std::vector<std::string> parse_type_parameters(
    const std::string& definition
);

// Create type instantiation from concrete types
// e.g., ["T", "U"] + [Int, String] -> {"T": Int, "U": String}
TypeInstantiation create_type_instantiation(
    const std::vector<std::string>& params,
    const std::vector<std::shared_ptr<meta::MetaType>>& types
);

// Check if a type is generic (has type parameters)
bool is_generic_type(const kernel::Value& type_def);

// Extract type parameters from generic definition
std::vector<std::string> extract_type_parameters(
    const kernel::Value& generic_def
);

} // namespace meld::optimization
