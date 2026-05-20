#include "meld/kernel/dispatch.hpp"
#include <algorithm>
#include <format>
#include <sstream>

namespace meld::kernel {

// FunctionSignature implementation

bool FunctionSignature::matches(const std::vector<std::shared_ptr<meta::MetaType>>& arg_types) const {
    // Check arity
    if (param_types.size() != arg_types.size()) {
        return false;
    }
    
    // Check if each argument type is assignable to the corresponding parameter type
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (!param_types[i]->is_assignable_from(*arg_types[i])) {
            return false;
        }
    }
    
    return true;
}

int FunctionSignature::specificity_at(size_t position, const meta::MetaType& arg_type) const {
    if (position >= param_types.size()) {
        return -1;
    }
    
    // Use type distance as specificity score (negate so higher = more specific)
    int distance = dispatch_utils::type_distance(*param_types[position], arg_type);
    if (distance < 0) {
        return -1;  // No match
    }
    
    // Convert distance to specificity: exact match = highest score
    // The more specific the type, the lower the distance, so we negate
    return 1000 - distance;
}

int FunctionSignature::total_specificity(const std::vector<std::shared_ptr<meta::MetaType>>& arg_types) const {
    int total = 0;
    for (size_t i = 0; i < arg_types.size(); ++i) {
        int spec = specificity_at(i, *arg_types[i]);
        if (spec < 0) {
            return -1;  // No match
        }
        total += spec;
    }
    return total;
}

std::string FunctionSignature::to_string() const {
    std::ostringstream oss;
    oss << name << "(";
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << param_types[i]->name();
    }
    oss << ") -> " << return_type->name();
    return oss.str();
}

// DispatchRegistry implementation

void DispatchRegistry::register_function(std::shared_ptr<FunctionSignature> signature) {
    std::lock_guard<std::mutex> lock(mutex_);
    signatures_[signature->name].push_back(std::move(signature));
}

std::expected<std::shared_ptr<FunctionSignature>, std::string>
DispatchRegistry::resolve(const std::string& name, 
                         const std::vector<std::shared_ptr<meta::MetaType>>& arg_types) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Find all matching signatures
    auto candidates = find_matching_signatures(name, arg_types);
    
    if (candidates.empty()) {
        // Build error message with available signatures
        std::ostringstream oss;
        oss << "No matching function found for " << name << "(";
        for (size_t i = 0; i < arg_types.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << arg_types[i]->name();
        }
        oss << ")";
        
        // Show available signatures
        auto it = signatures_.find(name);
        if (it != signatures_.end() && !it->second.empty()) {
            oss << "\nAvailable signatures:";
            for (const auto& sig : it->second) {
                oss << "\n  " << sig->to_string();
            }
        }
        
        return std::unexpected(oss.str());
    }
    
    // Select most specific
    return select_most_specific(candidates, arg_types);
}

std::vector<std::shared_ptr<FunctionSignature>> 
DispatchRegistry::get_signatures(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = signatures_.find(name);
    if (it == signatures_.end()) {
        return {};
    }
    return it->second;
}

bool DispatchRegistry::is_ambiguous(const std::string& name,
                                   const std::vector<std::shared_ptr<meta::MetaType>>& arg_types) const {
    auto result = resolve(name, arg_types);
    return !result.has_value() && result.error().find("Ambiguous") != std::string::npos;
}

void DispatchRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    signatures_.clear();
}

std::vector<std::shared_ptr<FunctionSignature>>
DispatchRegistry::find_matching_signatures(const std::string& name,
                                          const std::vector<std::shared_ptr<meta::MetaType>>& arg_types) const {
    std::vector<std::shared_ptr<FunctionSignature>> matches;
    
    auto it = signatures_.find(name);
    if (it == signatures_.end()) {
        return matches;
    }
    
    for (const auto& sig : it->second) {
        if (sig->matches(arg_types)) {
            matches.push_back(sig);
        }
    }
    
    return matches;
}

std::expected<std::shared_ptr<FunctionSignature>, std::string>
DispatchRegistry::select_most_specific(const std::vector<std::shared_ptr<FunctionSignature>>& candidates,
                                      const std::vector<std::shared_ptr<meta::MetaType>>& arg_types) const {
    if (candidates.empty()) {
        return std::unexpected("No candidates to select from");
    }
    
    if (candidates.size() == 1) {
        return candidates[0];
    }
    
    // Find the most specific signature(s)
    std::vector<std::shared_ptr<FunctionSignature>> most_specific;
    most_specific.push_back(candidates[0]);
    
    for (size_t i = 1; i < candidates.size(); ++i) {
        auto comparison = compare_specificity(*candidates[i], *most_specific[0], arg_types);
        
        if (!comparison.has_value()) {
            // Incomparable - could be ambiguous
            most_specific.push_back(candidates[i]);
        } else if (*comparison > 0) {
            // candidates[i] is more specific
            most_specific.clear();
            most_specific.push_back(candidates[i]);
        } else if (*comparison == 0) {
            // Equally specific
            most_specific.push_back(candidates[i]);
        }
        // else: candidates[i] is less specific, skip it
    }
    
    // Check for ambiguity
    if (most_specific.size() > 1) {
        std::ostringstream oss;
        oss << "Ambiguous function call. Multiple equally specific signatures match:\n";
        for (const auto& sig : most_specific) {
            oss << "  " << sig->to_string() << "\n";
        }
        return std::unexpected(oss.str());
    }
    
    return most_specific[0];
}

std::optional<int> DispatchRegistry::compare_specificity(const FunctionSignature& sig1,
                                                        const FunctionSignature& sig2,
                                                        const std::vector<std::shared_ptr<meta::MetaType>>& arg_types) const {
    // Compare parameter by parameter
    int sig1_more_specific = 0;
    int sig2_more_specific = 0;
    
    for (size_t i = 0; i < arg_types.size(); ++i) {
        const auto& param1 = *sig1.param_types[i];
        const auto& param2 = *sig2.param_types[i];
        const auto& arg = *arg_types[i];
        
        if (dispatch_utils::is_more_specific(param1, param2, arg)) {
            sig1_more_specific++;
        } else if (dispatch_utils::is_more_specific(param2, param1, arg)) {
            sig2_more_specific++;
        }
        // else: equally specific at this position
    }
    
    // Determine overall comparison
    if (sig1_more_specific > 0 && sig2_more_specific == 0) {
        return 1;  // sig1 is more specific
    } else if (sig2_more_specific > 0 && sig1_more_specific == 0) {
        return -1;  // sig2 is more specific
    } else if (sig1_more_specific == 0 && sig2_more_specific == 0) {
        return 0;  // equally specific
    } else {
        return std::nullopt;  // incomparable (each is more specific in different positions)
    }
}

// dispatch_utils implementation

namespace dispatch_utils {

int type_distance(const meta::MetaType& param_type, const meta::MetaType& arg_type) {
    // Exact match
    if (param_type.name() == arg_type.name()) {
        return 0;
    }
    
    // Check if arg_type is assignable to param_type
    if (!param_type.is_assignable_from(arg_type)) {
        return -1;  // No match
    }
    
    // Calculate subtype distance
    // For now, use a simple heuristic: if it's a subtype, distance = 1
    // In a full implementation, we'd calculate the actual inheritance depth
    if (arg_type.is_subtype_of(param_type)) {
        return 1;
    }
    
    // Handle union types
    if (auto union_type = dynamic_cast<const meta::UnionMetaType*>(&param_type)) {
        // If param is a union and arg matches one of the union members
        if (union_type->contains_type(arg_type)) {
            return 2;  // Less specific than direct match
        }
    }
    
    // Handle intersection types
    if (auto intersection_type = dynamic_cast<const meta::IntersectionMetaType*>(&param_type)) {
        // If param is an intersection and arg satisfies all members
        if (intersection_type->satisfies_all(arg_type)) {
            return 1;  // More specific than union
        }
    }
    
    // Generic assignability (e.g., through implicit conversions)
    return 3;
}

bool is_more_specific(const meta::MetaType& type1,
                     const meta::MetaType& type2,
                     const meta::MetaType& arg_type) {
    int dist1 = type_distance(type1, arg_type);
    int dist2 = type_distance(type2, arg_type);
    
    // If one doesn't match, it's not more specific
    if (dist1 < 0) return false;
    if (dist2 < 0) return true;
    
    // Lower distance = more specific
    return dist1 < dist2;
}

bool is_equally_specific(const meta::MetaType& type1,
                        const meta::MetaType& type2,
                        const meta::MetaType& arg_type) {
    int dist1 = type_distance(type1, arg_type);
    int dist2 = type_distance(type2, arg_type);
    
    // Both must match
    if (dist1 < 0 || dist2 < 0) return false;
    
    return dist1 == dist2;
}

} // namespace dispatch_utils

} // namespace meld::kernel
