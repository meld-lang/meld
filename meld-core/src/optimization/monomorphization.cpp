#include "meld/optimization/monomorphization.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/meta/type_registry.hpp"
#include <format>
#include <sstream>
#include <algorithm>
#include <queue>

namespace meld::optimization {

// ============================================================================
// MonomorphizationKey Implementation
// ============================================================================

bool MonomorphizationKey::operator<(const MonomorphizationKey& other) const {
    if (base_name != other.base_name) {
        return base_name < other.base_name;
    }
    
    // Compare type arguments
    if (type_args.size() != other.type_args.size()) {
        return type_args.size() < other.type_args.size();
    }
    
    auto it1 = type_args.begin();
    auto it2 = other.type_args.begin();
    
    while (it1 != type_args.end()) {
        if (it1->first != it2->first) {
            return it1->first < it2->first;
        }
        if (it1->second->name() != it2->second->name()) {
            return it1->second->name() < it2->second->name();
        }
        ++it1;
        ++it2;
    }
    
    return false;
}

bool MonomorphizationKey::operator==(const MonomorphizationKey& other) const {
    if (base_name != other.base_name || type_args.size() != other.type_args.size()) {
        return false;
    }
    
    for (const auto& [param, type] : type_args) {
        auto it = other.type_args.find(param);
        if (it == other.type_args.end() || it->second->name() != type->name()) {
            return false;
        }
    }
    
    return true;
}

std::string MonomorphizationKey::to_string() const {
    std::ostringstream oss;
    oss << base_name << "<";
    
    bool first = true;
    for (const auto& [param, type] : type_args) {
        if (!first) oss << ", ";
        oss << type->name();
        first = false;
    }
    
    oss << ">";
    return oss.str();
}

// ============================================================================
// MonomorphizationEngine Implementation
// ============================================================================

std::string MonomorphizationEngine::generate_specialized_name(
    const std::string& base_name,
    const TypeInstantiation& type_args) const {
    
    std::ostringstream oss;
    oss << base_name;
    
    // Sort type args by parameter name for consistent naming
    std::vector<std::pair<std::string, std::shared_ptr<meta::MetaType>>> sorted_args(
        type_args.begin(), type_args.end()
    );
    std::sort(sorted_args.begin(), sorted_args.end());
    
    for (const auto& [param, type] : sorted_args) {
        oss << "_" << type->name();
    }
    
    return oss.str();
}

kernel::Value MonomorphizationEngine::substitute_type_parameters(
    const kernel::Value& ast,
    const TypeInstantiation& type_args) const {
    
    // Recursively substitute type parameters in AST
    if (ast.is<std::shared_ptr<kernel::Symbol>>()) {
        auto sym = ast.as<std::shared_ptr<kernel::Symbol>>();
        auto it = type_args.find(sym->name());
        if (it != type_args.end()) {
            // Replace type parameter with concrete type
            auto type_sym = std::make_shared<kernel::Symbol>(it->second->name());
            return kernel::Value(type_sym);
        }
    }
    
    // For other AST nodes, would recursively process children
    return ast;
}

std::set<std::string> MonomorphizationEngine::extract_dependencies(
    const kernel::Value& ast) const {
    
    std::set<std::string> deps;
    
    // Extract references to other generic types/functions
    // In a full implementation, would traverse AST to find all type references
    
    return deps;
}

std::expected<void, std::string> MonomorphizationEngine::validate_type_instantiation(
    const std::vector<std::string>& type_params,
    const TypeInstantiation& type_args) const {
    
    // Check all type parameters have values
    for (const auto& param : type_params) {
        if (type_args.find(param) == type_args.end()) {
            return std::unexpected(
                std::format("Missing type argument for parameter '{}'", param)
            );
        }
    }
    
    // Check no extra type arguments
    for (const auto& [param, type] : type_args) {
        if (std::find(type_params.begin(), type_params.end(), param) == type_params.end()) {
            return std::unexpected(
                std::format("Unknown type parameter '{}'", param)
            );
        }
    }
    
    return {};
}

std::expected<MonomorphizedInstance, std::string>
MonomorphizationEngine::monomorphize_function(
    const parser::ast::function_declaration& func_def,
    const TypeInstantiation& type_args) {
    
    // Extract type parameters from function
    std::vector<std::string> type_params;  // Would parse from func_def
    
    // Validate type instantiation
    auto validation = validate_type_instantiation(type_params, type_args);
    if (!validation) {
        return std::unexpected(validation.error());
    }
    
    // Generate specialized name
    std::string specialized_name = generate_specialized_name(func_def.name.name, type_args);
    
    // Substitute type parameters in function body
    kernel::Value specialized_ast = substitute_type_parameters(
        kernel::Value(std::make_shared<kernel::Symbol>(func_def.name.name)),
        type_args
    );
    
    // Extract dependencies
    std::set<std::string> deps = extract_dependencies(specialized_ast);
    
    return MonomorphizedInstance{
        specialized_name,
        specialized_ast,
        type_args,
        deps
    };
}

std::expected<MonomorphizedInstance, std::string>
MonomorphizationEngine::monomorphize_type(
    const parser::ast::class_definition& type_def,
    const TypeInstantiation& type_args) {
    
    // Extract type parameters from class
    std::vector<std::string> type_params;  // Would parse from type_def
    
    // Validate type instantiation
    auto validation = validate_type_instantiation(type_params, type_args);
    if (!validation) {
        return std::unexpected(validation.error());
    }
    
    // Generate specialized name
    std::string specialized_name = generate_specialized_name(type_def.name.name, type_args);
    
    // Substitute type parameters in class definition
    kernel::Value specialized_ast = substitute_type_parameters(
        kernel::Value(std::make_shared<kernel::Symbol>(type_def.name.name)),
        type_args
    );
    
    // Extract dependencies
    std::set<std::string> deps = extract_dependencies(specialized_ast);
    
    return MonomorphizedInstance{
        specialized_name,
        specialized_ast,
        type_args,
        deps
    };
}

std::expected<MonomorphizedInstance, std::string>
MonomorphizationEngine::monomorphize_trait_impl(
    const kernel::Value& trait_impl,
    const TypeInstantiation& type_args) {
    
    // Monomorphize trait implementation
    std::string base_name = "trait_impl";  // Would extract from trait_impl
    std::string specialized_name = generate_specialized_name(base_name, type_args);
    
    kernel::Value specialized_ast = substitute_type_parameters(trait_impl, type_args);
    std::set<std::string> deps = extract_dependencies(specialized_ast);
    
    return MonomorphizedInstance{
        specialized_name,
        specialized_ast,
        type_args,
        deps
    };
}

std::expected<MonomorphizedInstance, std::string>
MonomorphizationEngine::get_or_create_instance(
    const MonomorphizationKey& key,
    std::function<std::expected<MonomorphizedInstance, std::string>()> generator) {
    
    // Check cache
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        cache_hits_++;
        return it->second;
    }
    
    cache_misses_++;
    
    // Generate new instance
    auto result = generator();
    if (!result) {
        return result;
    }
    
    // Cache the result
    cache_[key] = *result;
    return *result;
}

bool MonomorphizationEngine::has_instance(const MonomorphizationKey& key) const {
    return cache_.find(key) != cache_.end();
}

std::expected<MonomorphizedInstance, std::string>
MonomorphizationEngine::get_instance(const MonomorphizationKey& key) const {
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        return it->second;
    }
    return std::unexpected(
        std::format("No monomorphized instance found for '{}'", key.to_string())
    );
}

void MonomorphizationEngine::clear_cache() {
    cache_.clear();
    cache_hits_ = 0;
    cache_misses_ = 0;
}

MonomorphizationEngine::Statistics MonomorphizationEngine::get_statistics() const {
    Statistics stats;
    stats.total_instances = cache_.size();
    stats.cache_hits = cache_hits_;
    stats.cache_misses = cache_misses_;
    
    // Count by type (simplified)
    stats.function_instances = 0;
    stats.type_instances = 0;
    stats.trait_impl_instances = 0;
    
    return stats;
}

// ============================================================================
// MonomorphizationAnalyzer Implementation
// ============================================================================

std::vector<MonomorphizationAnalyzer::InstantiationSite>
MonomorphizationAnalyzer::find_instantiations(const kernel::Value& program_ast) {
    std::vector<InstantiationSite> sites;
    
    // Traverse AST to find generic instantiations
    // In a full implementation, would recursively search for type applications
    
    return sites;
}

std::expected<std::vector<MonomorphizationKey>, std::string>
MonomorphizationAnalyzer::determine_monomorphization_order(
    const std::vector<InstantiationSite>& sites,
    const std::map<std::string, std::set<std::string>>& dependencies) {
    
    std::vector<MonomorphizationKey> order;
    std::set<std::string> visited;
    std::set<std::string> in_progress;
    
    // Topological sort using DFS
    std::function<bool(const std::string&)> visit = [&](const std::string& name) -> bool {
        if (visited.count(name)) return true;
        if (in_progress.count(name)) return false;  // Cycle detected
        
        in_progress.insert(name);
        
        auto it = dependencies.find(name);
        if (it != dependencies.end()) {
            for (const auto& dep : it->second) {
                if (!visit(dep)) return false;
            }
        }
        
        in_progress.erase(name);
        visited.insert(name);
        
        return true;
    };
    
    // Visit all sites
    for (const auto& site : sites) {
        if (!visit(site.generic_name)) {
            return std::unexpected("Circular dependency detected in generic instantiations");
        }
    }
    
    return order;
}

MonomorphizationAnalyzer::CodeSizeEstimate
MonomorphizationAnalyzer::estimate_code_size(
    const std::vector<MonomorphizedInstance>& instances) {
    
    CodeSizeEstimate estimate;
    estimate.original_size = 100;  // Placeholder
    estimate.monomorphized_size = instances.size() * 100;  // Placeholder
    estimate.expansion_factor = static_cast<double>(estimate.monomorphized_size) / 
                               estimate.original_size;
    
    return estimate;
}

// ============================================================================
// MonomorphizationOptimizer Implementation
// ============================================================================

kernel::Value MonomorphizationOptimizer::optimize_instance(
    const MonomorphizedInstance& instance) {
    
    // Apply optimizations to monomorphized code
    // - Constant folding
    // - Dead code elimination
    // - Inlining
    
    return instance.specialized_ast;
}

std::vector<MonomorphizedInstance> MonomorphizationOptimizer::deduplicate_instances(
    const std::vector<MonomorphizedInstance>& instances) {
    
    std::vector<MonomorphizedInstance> unique_instances;
    std::set<std::string> seen_names;
    
    for (const auto& instance : instances) {
        if (seen_names.insert(instance.specialized_name).second) {
            unique_instances.push_back(instance);
        }
    }
    
    return unique_instances;
}

kernel::Value MonomorphizationOptimizer::inline_small_functions(
    const kernel::Value& ast,
    size_t size_threshold) {
    
    // Inline functions smaller than threshold
    // In a full implementation, would analyze function size and inline accordingly
    
    return ast;
}

kernel::Value MonomorphizationOptimizer::specialize_for_common_types(
    const MonomorphizedInstance& instance) {
    
    // Apply type-specific optimizations
    // e.g., for int types, use native arithmetic
    // e.g., for string types, use optimized string operations
    
    return instance.specialized_ast;
}

// ============================================================================
// Helper Functions
// ============================================================================

std::vector<std::string> parse_type_parameters(const std::string& definition) {
    std::vector<std::string> params;
    
    // Simple parser for "Array<T>" style definitions
    // In a full implementation, would use the actual parser
    
    size_t start = definition.find('<');
    size_t end = definition.find('>');
    
    if (start != std::string::npos && end != std::string::npos) {
        std::string params_str = definition.substr(start + 1, end - start - 1);
        std::istringstream iss(params_str);
        std::string param;
        
        while (std::getline(iss, param, ',')) {
            // Trim whitespace
            param.erase(0, param.find_first_not_of(" \t"));
            param.erase(param.find_last_not_of(" \t") + 1);
            
            if (!param.empty()) {
                params.push_back(param);
            }
        }
    }
    
    return params;
}

TypeInstantiation create_type_instantiation(
    const std::vector<std::string>& params,
    const std::vector<std::shared_ptr<meta::MetaType>>& types) {
    
    TypeInstantiation instantiation;
    
    if (params.size() != types.size()) {
        return instantiation;  // Error: mismatched sizes
    }
    
    for (size_t i = 0; i < params.size(); ++i) {
        instantiation[params[i]] = types[i];
    }
    
    return instantiation;
}

bool is_generic_type(const kernel::Value& type_def) {
    // Check if type definition contains type parameters
    // In a full implementation, would analyze the AST structure
    
    return false;  // Placeholder
}

std::vector<std::string> extract_type_parameters(const kernel::Value& generic_def) {
    std::vector<std::string> params;
    
    // Extract type parameters from generic definition
    // In a full implementation, would parse the AST
    
    return params;
}

} // namespace meld::optimization
