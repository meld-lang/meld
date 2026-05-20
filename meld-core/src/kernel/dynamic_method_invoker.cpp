#include "meld/kernel/dynamic_method_invoker.hpp"
#include "meld/types/dynamic_property_access.hpp"
#include "meld/meta/type_registration.hpp"
#include <sstream>
#include <algorithm>
#include <set>

namespace meld::kernel {

std::string to_string(MethodInvocationError error) {
    switch (error) {
        case MethodInvocationError::TypeNotFound:
            return "Type not found";
        case MethodInvocationError::MethodNotFound:
            return "Method not found";
        case MethodInvocationError::InvalidInstance:
            return "Invalid instance";
        case MethodInvocationError::ArgumentCountMismatch:
            return "Argument count mismatch";
        case MethodInvocationError::ArgumentTypeMismatch:
            return "Argument type mismatch";
        case MethodInvocationError::ConversionFailed:
            return "Conversion failed";
        case MethodInvocationError::InvocationFailed:
            return "Invocation failed";
        case MethodInvocationError::AmbiguousOverload:
            return "Ambiguous overload";
        default:
            return "Unknown error";
    }
}

std::expected<rttr::variant, MethodInvocationError>
DynamicMethodInvoker::invoke(rttr::instance& instance, const std::string& method_name,
                             const std::vector<rttr::variant>& args) {
#ifdef __APPLE__
    return std::unexpected(MethodInvocationError::InvocationFailed);
#else
    if (!instance.is_valid()) {
        return std::unexpected(MethodInvocationError::InvalidInstance);
    }
    
    rttr::type type = instance.get_type();
    
    // Get all methods with this name
    auto methods = get_overloads(instance, method_name);
    if (methods.empty()) {
        return std::unexpected(MethodInvocationError::MethodNotFound);
    }
    
    // Find best matching overload
    auto best_method = find_best_overload(methods, args);
    if (!best_method) {
        return std::unexpected(MethodInvocationError::ArgumentTypeMismatch);
    }
    
    // Convert arguments if needed
    auto param_infos = best_method->get_parameter_infos();
    std::vector<rttr::type> param_types;
    param_types.reserve(param_infos.size());
    for (const auto& info : param_infos) {
        param_types.push_back(info.get_type());
    }
    
    auto converted_args = convert_arguments(args, param_types);
    if (!converted_args) {
        return std::unexpected(converted_args.error());
    }
    
    // Invoke the method
    rttr::variant result = best_method->invoke(instance, converted_args.value());
    if (!result.is_valid() && best_method->get_return_type() != rttr::type::get<void>()) {
        return std::unexpected(MethodInvocationError::InvocationFailed);
    }
    
    return result;
#endif
}

std::expected<rttr::variant, MethodInvocationError>
DynamicMethodInvoker::invoke(rttr::instance& instance, const std::string& type_name,
                             const std::string& method_name, const std::vector<rttr::variant>& args) {
#ifdef __APPLE__
    return std::unexpected(MethodInvocationError::TypeNotFound);
#else
    rttr::type expected_type = meta::TypeRegistry::instance().get_type(type_name);
    if (!expected_type.is_valid()) {
        return std::unexpected(MethodInvocationError::TypeNotFound);
    }
    
    rttr::type actual_type = instance.get_type();
    if (actual_type != expected_type && !actual_type.is_derived_from(expected_type)) {
        return std::unexpected(MethodInvocationError::InvalidInstance);
    }
    
    return invoke(instance, method_name, args);
#endif
}

bool DynamicMethodInvoker::has_method(const rttr::instance& instance, const std::string& method_name) {
#ifdef __APPLE__
    return false;
#else
    if (!instance.is_valid()) {
        return false;
    }
    
    rttr::type type = instance.get_type();
    rttr::method method = type.get_method(method_name);
    return method.is_valid();
#endif
}

bool DynamicMethodInvoker::has_method(const rttr::instance& instance, const std::string& method_name,
                                      const std::vector<rttr::type>& arg_types) {
#ifdef __APPLE__
    return false;
#else
    if (!instance.is_valid()) {
        return false;
    }
    
    auto methods = get_overloads(instance, method_name);
    
    for (const auto& method : methods) {
        auto param_infos = method.get_parameter_infos();
        if (param_infos.size() != arg_types.size()) {
            continue;
        }
        
        bool matches = true;
        for (size_t i = 0; i < param_infos.size(); ++i) {
            if (param_infos[i].get_type() != arg_types[i]) {
                matches = false;
                break;
            }
        }
        
        if (matches) {
            return true;
        }
    }
    
    return false;
#endif
}

std::optional<rttr::type> DynamicMethodInvoker::get_return_type(const rttr::instance& instance,
                                                                  const std::string& method_name) {
#ifdef __APPLE__
    return std::nullopt;
#else
    if (!instance.is_valid()) {
        return std::nullopt;
    }
    
    rttr::type type = instance.get_type();
    rttr::method method = type.get_method(method_name);
    
    if (!method.is_valid()) {
        return std::nullopt;
    }
    
    return method.get_return_type();
#endif
}

std::vector<rttr::method> DynamicMethodInvoker::get_overloads(const rttr::instance& instance,
                                                                const std::string& method_name) {
    std::vector<rttr::method> result;
#ifdef __APPLE__
    return result;
#else
    if (!instance.is_valid()) {
        return result;
    }
    
    rttr::type type = instance.get_type();
    auto methods = type.get_methods();
    
    for (const auto& method : methods) {
        if (method.get_name().to_string() == method_name) {
            result.push_back(method);
        }
    }
    
    return result;
#endif
}

std::optional<std::vector<rttr::type>> 
DynamicMethodInvoker::get_parameter_types(const rttr::instance& instance, const std::string& method_name) {
#ifdef __APPLE__
    return std::nullopt;
#else
    if (!instance.is_valid()) {
        return std::nullopt;
    }
    
    rttr::type type = instance.get_type();
    rttr::method method = type.get_method(method_name);
    
    if (!method.is_valid()) {
        return std::nullopt;
    }
    
    auto param_infos = method.get_parameter_infos();
    std::vector<rttr::type> param_types;
    param_types.reserve(param_infos.size());
    
    for (const auto& info : param_infos) {
        param_types.push_back(info.get_type());
    }
    
    return param_types;
#endif
}

std::optional<rttr::method> 
DynamicMethodInvoker::find_best_overload(const std::vector<rttr::method>& methods,
                                         const std::vector<rttr::variant>& args) {
#ifdef __APPLE__
    return std::nullopt;
#else
    if (methods.empty()) {
        return std::nullopt;
    }
    
    // If only one method, return it if arguments match
    if (methods.size() == 1) {
        auto param_infos = methods[0].get_parameter_infos();
        if (param_infos.size() == args.size()) {
            return methods[0];
        }
        return std::nullopt;
    }
    
    // Find best match based on argument types
    int best_score = -1;
    std::optional<rttr::method> best_method;
    int match_count = 0;
    
    for (const auto& method : methods) {
        auto param_infos = method.get_parameter_infos();
        
        if (param_infos.size() != args.size()) {
            continue;
        }
        
        std::vector<rttr::type> param_types;
        param_types.reserve(param_infos.size());
        for (const auto& info : param_infos) {
            param_types.push_back(info.get_type());
        }
        
        int score = calculate_match_score(args, param_types);
        if (score > best_score) {
            best_score = score;
            best_method = method;
            match_count = 1;
        } else if (score == best_score && score >= 0) {
            match_count++;
        }
    }
    
    // If multiple methods have the same best score, it's ambiguous
    if (match_count > 1) {
        return std::nullopt;
    }
    
    return best_method;
#endif
}

std::expected<std::vector<rttr::variant>, MethodInvocationError>
DynamicMethodInvoker::convert_arguments(const std::vector<rttr::variant>& args,
                                        const std::vector<rttr::type>& param_types) {
#ifdef __APPLE__
    return std::unexpected(MethodInvocationError::ConversionFailed);
#else
    if (args.size() != param_types.size()) {
        return std::unexpected(MethodInvocationError::ArgumentCountMismatch);
    }
    
    std::vector<rttr::variant> converted_args;
    converted_args.reserve(args.size());
    
    for (size_t i = 0; i < args.size(); ++i) {
        rttr::variant converted = args[i];
        
        // Try to convert to parameter type
        if (converted.get_type() != param_types[i]) {
            if (!converted.convert(param_types[i])) {
                return std::unexpected(MethodInvocationError::ArgumentTypeMismatch);
            }
        }
        
        converted_args.push_back(converted);
    }
    
    return converted_args;
#endif
}

std::expected<Value, MethodInvocationError>
DynamicMethodInvoker::invoke_with_values(rttr::instance& instance, const std::string& method_name,
                                         const std::vector<Value>& args) {
#ifdef __APPLE__
    return std::unexpected(MethodInvocationError::InvocationFailed);
#else
    // Convert Values to variants
    std::vector<rttr::variant> variants;
    variants.reserve(args.size());
    
    for (const auto& arg : args) {
        variants.push_back(types::DynamicPropertyAccess::value_to_variant(arg));
    }
    
    // Invoke method
    auto result = invoke(instance, method_name, variants);
    if (!result) {
        return std::unexpected(result.error());
    }
    
    // Convert result back to Value
    auto value_result = types::DynamicPropertyAccess::variant_to_value(result.value());
    if (!value_result) {
        return std::unexpected(MethodInvocationError::ConversionFailed);
    }
    
    return value_result.value();
#endif
}

std::vector<std::string> DynamicMethodInvoker::get_all_methods(const rttr::instance& instance) {
    std::vector<std::string> result;
#ifdef __APPLE__
    return result;
#else
    if (!instance.is_valid()) {
        return result;
    }
    
    rttr::type type = instance.get_type();
    auto methods = type.get_methods();
    
    // Use a set to avoid duplicates (overloads)
    std::set<std::string> unique_names;
    for (const auto& method : methods) {
        unique_names.insert(method.get_name().to_string());
    }
    
    result.assign(unique_names.begin(), unique_names.end());
    return result;
#endif
}

std::string DynamicMethodInvoker::get_method_signature(const rttr::method& method) {
#ifdef __APPLE__
    return method.get_name() + "(...)";
#else
    std::ostringstream oss;
    
    oss << method.get_name().to_string() << "(";
    
    auto param_infos = method.get_parameter_infos();
    auto param_infos_vec = std::vector<rttr::parameter_info>(param_infos.begin(), param_infos.end());
    for (size_t i = 0; i < param_infos_vec.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << param_infos_vec[i].get_type().get_name().to_string();
    }
    
    oss << ") -> " << method.get_return_type().get_name().to_string();
    
    return oss.str();
#endif
}

bool DynamicMethodInvoker::arguments_match(const std::vector<rttr::variant>& args,
                                           const std::vector<rttr::type>& param_types) {
#ifdef __APPLE__
    return false;
#else
    if (args.size() != param_types.size()) {
        return false;
    }
    
    for (size_t i = 0; i < args.size(); ++i) {
        rttr::variant test = args[i];
        if (!test.can_convert(param_types[i])) {
            return false;
        }
    }
    
    return true;
#endif
}

int DynamicMethodInvoker::calculate_match_score(const std::vector<rttr::variant>& args,
                                                 const std::vector<rttr::type>& param_types) {
#ifdef __APPLE__
    return -1;
#else
    if (args.size() != param_types.size()) {
        return -1;
    }
    
    int score = 0;
    
    for (size_t i = 0; i < args.size(); ++i) {
        rttr::type arg_type = args[i].get_type();
        rttr::type param_type = param_types[i];
        
        // Exact match gets highest score
        if (arg_type == param_type) {
            score += 100;
        }
        // Derived type match gets medium score
        else if (arg_type.is_derived_from(param_type)) {
            score += 50;
        }
        // Convertible gets low score
        else if (args[i].can_convert(param_type)) {
            score += 10;
        }
        // No match
        else {
            return -1;
        }
    }
    
    return score;
#endif
}

} // namespace meld::kernel
