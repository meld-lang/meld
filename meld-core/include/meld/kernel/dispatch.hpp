#pragma once

#include "primitives.hpp"
#include "meld/meta/metatype.hpp"
#include <vector>
#include <string>
#include <memory>
#include <expected>
#include <optional>
#include <map>
#include <functional>

namespace meld::kernel {

// Function signature for multiple dispatch
struct FunctionSignature {
    std::string name;
    std::vector<std::shared_ptr<meta::MetaType>> param_types;
    std::shared_ptr<meta::MetaType> return_type;
    Value implementation;  // The actual function implementation
    
    FunctionSignature(std::string n,
                     std::vector<std::shared_ptr<meta::MetaType>> params,
                     std::shared_ptr<meta::MetaType> ret,
                     Value impl)
        : name(std::move(n))
        , param_types(std::move(params))
        , return_type(std::move(ret))
        , implementation(std::move(impl)) {}
    
    // Check if this signature matches the given argument types
    bool matches(const std::vector<std::shared_ptr<meta::MetaType>>& arg_types) const;
    
    // Get specificity score for a given argument position
    // Higher score = more specific
    int specificity_at(size_t position, const meta::MetaType& arg_type) const;
    
    // Get total specificity score for all arguments
    int total_specificity(const std::vector<std::shared_ptr<meta::MetaType>>& arg_types) const;
    
    std::string to_string() const;
};

// Result of dispatch resolution
struct DispatchResult {
    std::shared_ptr<FunctionSignature> signature;
    bool is_ambiguous;
    std::vector<std::shared_ptr<FunctionSignature>> ambiguous_candidates;
    
    DispatchResult(std::shared_ptr<FunctionSignature> sig)
        : signature(std::move(sig)), is_ambiguous(false) {}
    
    DispatchResult(std::vector<std::shared_ptr<FunctionSignature>> candidates)
        : signature(nullptr), is_ambiguous(true), ambiguous_candidates(std::move(candidates)) {}
};

// Multiple dispatch registry
class DispatchRegistry {
public:
    static DispatchRegistry& instance() {
        static DispatchRegistry registry;
        return registry;
    }
    
    // Register a function signature
    void register_function(std::shared_ptr<FunctionSignature> signature);
    
    // Resolve dispatch for a function call
    // Returns the most specific matching signature, or error if ambiguous/not found
    std::expected<std::shared_ptr<FunctionSignature>, std::string>
    resolve(const std::string& name, const std::vector<std::shared_ptr<meta::MetaType>>& arg_types) const;
    
    // Get all signatures for a function name
    std::vector<std::shared_ptr<FunctionSignature>> get_signatures(const std::string& name) const;
    
    // Check if a call would be ambiguous
    bool is_ambiguous(const std::string& name, const std::vector<std::shared_ptr<meta::MetaType>>& arg_types) const;
    
    // Clear all registered functions (useful for testing)
    void clear();
    
    DispatchRegistry() = default;

private:
    
    // Find all matching signatures
    std::vector<std::shared_ptr<FunctionSignature>>
    find_matching_signatures(const std::string& name, 
                            const std::vector<std::shared_ptr<meta::MetaType>>& arg_types) const;
    
    // Select most specific signature from candidates
    std::expected<std::shared_ptr<FunctionSignature>, std::string>
    select_most_specific(const std::vector<std::shared_ptr<FunctionSignature>>& candidates,
                        const std::vector<std::shared_ptr<meta::MetaType>>& arg_types) const;
    
    // Compare two signatures for specificity
    // Returns: -1 if sig1 < sig2, 0 if equal, 1 if sig1 > sig2, nullopt if incomparable
    std::optional<int> compare_specificity(const FunctionSignature& sig1,
                                          const FunctionSignature& sig2,
                                          const std::vector<std::shared_ptr<meta::MetaType>>& arg_types) const;
    
    // Map from function name to list of signatures
    std::map<std::string, std::vector<std::shared_ptr<FunctionSignature>>> signatures_;
    mutable std::mutex mutex_;
};

// Type specificity ranking utilities
namespace dispatch_utils {
    
    // Calculate type distance (lower = more specific)
    // Returns: 0 for exact match, positive for subtype distance, -1 for no match
    int type_distance(const meta::MetaType& param_type, const meta::MetaType& arg_type);
    
    // Check if one type is more specific than another for a given argument
    bool is_more_specific(const meta::MetaType& type1, 
                         const meta::MetaType& type2,
                         const meta::MetaType& arg_type);
    
    // Check if two types are equally specific for a given argument
    bool is_equally_specific(const meta::MetaType& type1,
                            const meta::MetaType& type2,
                            const meta::MetaType& arg_type);
    
} // namespace dispatch_utils

} // namespace meld::kernel
