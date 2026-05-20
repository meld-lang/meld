#include "meld/macro/extern_macro.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/macro/macro.hpp"
#include <format>
#include <sstream>
#include <regex>
#include <algorithm>

namespace meld::macro {

// TargetLanguage utilities
std::expected<TargetLanguage, std::string> parse_target_language(const std::string& lang) {
    std::string lower_lang = lang;
    std::transform(lower_lang.begin(), lower_lang.end(), lower_lang.begin(), ::tolower);
    
    if (lower_lang == "java") return TargetLanguage::Java;
    if (lower_lang == "go") return TargetLanguage::Go;
    if (lower_lang == "cpp" || lower_lang == "c++") return TargetLanguage::Cpp;
    if (lower_lang == "c") return TargetLanguage::C;
    
    return std::unexpected(std::format("Unsupported target language: {}", lang));
}

std::string target_language_to_string(TargetLanguage lang) {
    switch (lang) {
        case TargetLanguage::Java: return "java";
        case TargetLanguage::Go: return "go";
        case TargetLanguage::Cpp: return "cpp";
        case TargetLanguage::C: return "c";
    }
    return "unknown";
}

// TypeMapper implementation
std::map<std::string, std::string> TypeMapper::get_default_mappings(TargetLanguage target) {
    switch (target) {
        case TargetLanguage::Java:
            return {
                // Primitive types
                {"int", "int"},
                {"float", "double"},
                {"bool", "boolean"},
                {"string", "String"},
                {"nil", "null"},
                
                // Collection types
                {"list", "List"},
                {"map", "Map"},
                {"set", "Set"},
                {"array", "Object[]"},
                
                // Meld-specific types
                {"Result", "Optional"},
                {"Task", "CompletableFuture"},
                {"Option", "Optional"},
                
                // Common Java ecosystem types
                {"BigInteger", "java.math.BigInteger"},
                {"BigDecimal", "java.math.BigDecimal"},
                {"Date", "java.time.LocalDateTime"},
                {"UUID", "java.util.UUID"},
                {"Path", "java.nio.file.Path"},
                {"URI", "java.net.URI"},
                {"URL", "java.net.URL"},
                
                // Stream and functional types
                {"Stream", "java.util.stream.Stream"},
                {"Function", "java.util.function.Function"},
                {"Predicate", "java.util.function.Predicate"},
                {"Consumer", "java.util.function.Consumer"},
                {"Supplier", "java.util.function.Supplier"},
                
                // Concurrency types
                {"Future", "java.util.concurrent.Future"},
                {"ExecutorService", "java.util.concurrent.ExecutorService"},
                {"CompletableFuture", "java.util.concurrent.CompletableFuture"},
                
                // IO types
                {"InputStream", "java.io.InputStream"},
                {"OutputStream", "java.io.OutputStream"},
                {"Reader", "java.io.Reader"},
                {"Writer", "java.io.Writer"},
                {"File", "java.io.File"},
                
                // JSON and serialization
                {"JsonObject", "com.fasterxml.jackson.databind.JsonNode"},
                {"JsonArray", "com.fasterxml.jackson.databind.JsonNode"}
            };
        
        case TargetLanguage::Go:
            return {
                // Primitive types
                {"int", "int64"},
                {"float", "float64"},
                {"bool", "bool"},
                {"string", "string"},
                {"nil", "nil"},
                {"byte", "byte"},
                
                // Collection types
                {"list", "[]interface{}"},
                {"map", "map[string]interface{}"},
                {"set", "map[interface{}]bool"},
                {"array", "[]interface{}"},
                
                // Meld-specific types
                {"Result", "interface{}"},
                {"Task", "chan interface{}"},
                {"Option", "interface{}"},
                
                // Common Go ecosystem types
                {"Context", "context.Context"},
                {"Error", "error"},
                {"Time", "time.Time"},
                {"Duration", "time.Duration"},
                {"UUID", "uuid.UUID"},
                {"URL", "url.URL"},
                {"Mutex", "sync.Mutex"},
                {"WaitGroup", "sync.WaitGroup"},
                
                // IO types
                {"Reader", "io.Reader"},
                {"Writer", "io.Writer"},
                {"File", "os.File"},
                {"Buffer", "bytes.Buffer"},
                
                // Network types
                {"Conn", "net.Conn"},
                {"Listener", "net.Listener"},
                {"Request", "http.Request"},
                {"Response", "http.Response"},
                
                // JSON types
                {"JsonObject", "map[string]interface{}"},
                {"JsonArray", "[]interface{}"},
                
                // Database types
                {"DB", "sql.DB"},
                {"Rows", "sql.Rows"},
                {"Result", "sql.Result"}
            };
        
        case TargetLanguage::Cpp:
            return {
                // Primitive types
                {"int", "int64_t"},
                {"float", "double"},
                {"bool", "bool"},
                {"string", "std::string"},
                {"nil", "nullptr"},
                {"byte", "uint8_t"},
                
                // Collection types
                {"list", "std::vector"},
                {"map", "std::map"},
                {"set", "std::set"},
                {"array", "std::array"},
                
                // Meld-specific types
                {"Result", "std::expected"},
                {"Task", "std::future"},
                {"Option", "std::optional"},
                
                // Smart pointers
                {"unique_ptr", "std::unique_ptr"},
                {"shared_ptr", "std::shared_ptr"},
                {"weak_ptr", "std::weak_ptr"},
                
                // Functional types
                {"Function", "std::function"},
                {"Predicate", "std::function<bool(T)>"},
                
                // Concurrency types
                {"Thread", "std::thread"},
                {"Mutex", "std::mutex"},
                {"Future", "std::future"},
                {"Promise", "std::promise"},
                {"Atomic", "std::atomic"},
                
                // IO types
                {"InputStream", "std::istream"},
                {"OutputStream", "std::ostream"},
                {"FileStream", "std::fstream"},
                {"StringStream", "std::stringstream"},
                
                // Time types
                {"TimePoint", "std::chrono::time_point"},
                {"Duration", "std::chrono::duration"},
                
                // Container types
                {"Vector", "std::vector"},
                {"Map", "std::map"},
                {"UnorderedMap", "std::unordered_map"},
                {"Set", "std::set"},
                {"UnorderedSet", "std::unordered_set"},
                {"Queue", "std::queue"},
                {"Stack", "std::stack"},
                {"Deque", "std::deque"},
                
                // Utility types
                {"Pair", "std::pair"},
                {"Tuple", "std::tuple"},
                {"Variant", "std::variant"},
                {"Any", "std::any"}
            };
        
        case TargetLanguage::C:
            return {
                // Primitive types
                {"int", "int64_t"},
                {"float", "double"},
                {"bool", "bool"},
                {"string", "char*"},
                {"nil", "NULL"},
                {"byte", "uint8_t"},
                
                // Collection types (simplified for C)
                {"list", "void**"},
                {"map", "void*"},
                {"set", "void*"},
                {"array", "void*"},
                
                // Meld-specific types (as structs)
                {"Result", "result_t"},
                {"Task", "task_t"},
                {"Option", "option_t"},
                
                // Standard C types
                {"size_t", "size_t"},
                {"ptrdiff_t", "ptrdiff_t"},
                {"FILE", "FILE*"},
                
                // Fixed-width integer types
                {"int8", "int8_t"},
                {"int16", "int16_t"},
                {"int32", "int32_t"},
                {"int64", "int64_t"},
                {"uint8", "uint8_t"},
                {"uint16", "uint16_t"},
                {"uint32", "uint32_t"},
                {"uint64", "uint64_t"},
                
                // Function pointer types
                {"Function", "void*"},
                {"Callback", "void(*)(void*)"},
                
                // Memory types
                {"Buffer", "void*"},
                {"Pointer", "void*"},
                
                // Time types
                {"Time", "time_t"},
                {"Clock", "clock_t"}
            };
    }
    return {};
}

std::expected<std::string, std::string> 
TypeMapper::map_type(const std::string& meld_type, TargetLanguage target, 
                     const std::map<std::string, std::string>& custom_mappings) {
    
    // Check custom mappings first
    if (auto it = custom_mappings.find(meld_type); it != custom_mappings.end()) {
        return it->second;
    }
    
    // Check default mappings
    auto default_mappings = get_default_mappings(target);
    if (auto it = default_mappings.find(meld_type); it != default_mappings.end()) {
        return it->second;
    }
    
    // Handle generic types like List<T>, Map<K,V>, etc.
    if (meld_type.find('<') != std::string::npos) {
        return map_generic_type(meld_type, target, custom_mappings);
    }
    
    // Handle array types like int[], string[]
    if (meld_type.ends_with("[]")) {
        return map_array_type(meld_type, target, custom_mappings);
    }
    
    // Handle nullable types like int?, string?
    if (meld_type.ends_with("?")) {
        return map_nullable_type(meld_type, target, custom_mappings);
    }
    
    // Handle function types like (int, string) -> bool
    if (meld_type.find("->") != std::string::npos) {
        return map_function_type(meld_type, target, custom_mappings);
    }
    
    // Handle union types like int | string
    if (meld_type.find(" | ") != std::string::npos) {
        return map_union_type(meld_type, target, custom_mappings);
    }
    
    // Handle tuple types like (int, string, bool)
    if (meld_type.starts_with("(") && meld_type.ends_with(")") && meld_type.find("->") == std::string::npos) {
        return map_tuple_type(meld_type, target, custom_mappings);
    }
    
    // If no mapping found, return the original type (assume it's already in target language)
    return meld_type;
}

// Helper method for generic types
std::expected<std::string, std::string>
TypeMapper::map_generic_type(const std::string& meld_type, TargetLanguage target,
                            const std::map<std::string, std::string>& custom_mappings) {
    
    std::regex generic_regex(R"((\w+)<(.+)>)");
    std::smatch match;
    
    if (std::regex_match(meld_type, match, generic_regex)) {
        std::string base_type = match[1].str();
        std::string type_params = match[2].str();
        
        // Map the base type
        auto mapped_base = map_type(base_type, target, custom_mappings);
        if (!mapped_base) return mapped_base;
        
        // Parse and map type parameters
        std::vector<std::string> params;
        std::stringstream ss(type_params);
        std::string param;
        
        while (std::getline(ss, param, ',')) {
            // Trim whitespace
            param.erase(0, param.find_first_not_of(" \t"));
            param.erase(param.find_last_not_of(" \t") + 1);
            
            auto mapped_param = map_type(param, target, custom_mappings);
            if (!mapped_param) return mapped_param;
            params.push_back(*mapped_param);
        }
        
        // Reconstruct generic type with mapped parameters
        std::ostringstream result;
        result << *mapped_base;
        
        switch (target) {
            case TargetLanguage::Java:
            case TargetLanguage::Cpp:
                result << "<";
                for (size_t i = 0; i < params.size(); ++i) {
                    if (i > 0) result << ", ";
                    result << params[i];
                }
                result << ">";
                break;
            case TargetLanguage::Go:
                // Go doesn't have generics in older versions, use interface{}
                if (base_type == "list") {
                    result.str("");
                    result << "[]" << (params.empty() ? "interface{}" : params[0]);
                } else if (base_type == "map") {
                    result.str("");
                    result << "map[" << (params.size() < 2 ? "string" : params[0]) 
                           << "]" << (params.size() < 2 ? "interface{}" : params[1]);
                }
                break;
            case TargetLanguage::C:
                // C doesn't have generics, use void* or specific types
                result.str("");
                if (base_type == "list") {
                    result << "void**";
                } else if (base_type == "map") {
                    result << "void*";
                } else {
                    result << "void*";
                }
                break;
        }
        
        return result.str();
    }
    
    return std::unexpected("Invalid generic type format: " + meld_type);
}

// Helper method for array types
std::expected<std::string, std::string>
TypeMapper::map_array_type(const std::string& meld_type, TargetLanguage target,
                          const std::map<std::string, std::string>& custom_mappings) {
    
    std::string element_type = meld_type.substr(0, meld_type.length() - 2);
    auto mapped_element = map_type(element_type, target, custom_mappings);
    if (!mapped_element) return mapped_element;
    
    switch (target) {
        case TargetLanguage::Java:
            return *mapped_element + "[]";
        case TargetLanguage::Go:
            return "[]" + *mapped_element;
        case TargetLanguage::Cpp:
            return "std::vector<" + *mapped_element + ">";
        case TargetLanguage::C:
            return *mapped_element + "*";
    }
    
    return meld_type;
}

// Helper method for nullable types
std::expected<std::string, std::string>
TypeMapper::map_nullable_type(const std::string& meld_type, TargetLanguage target,
                             const std::map<std::string, std::string>& custom_mappings) {
    
    std::string base_type = meld_type.substr(0, meld_type.length() - 1);
    auto mapped_base = map_type(base_type, target, custom_mappings);
    if (!mapped_base) return mapped_base;
    
    switch (target) {
        case TargetLanguage::Java:
            // Java uses wrapper types for nullability
            if (*mapped_base == "int") return "Integer";
            if (*mapped_base == "double") return "Double";
            if (*mapped_base == "boolean") return "Boolean";
            return *mapped_base; // Reference types are already nullable
        case TargetLanguage::Go:
            return "*" + *mapped_base; // Pointer for nullability
        case TargetLanguage::Cpp:
            return "std::optional<" + *mapped_base + ">";
        case TargetLanguage::C:
            return *mapped_base + "*"; // Pointer for nullability
    }
    
    return meld_type;
}

// Helper method for function types
std::expected<std::string, std::string>
TypeMapper::map_function_type(const std::string& meld_type, TargetLanguage target,
                             const std::map<std::string, std::string>& custom_mappings) {
    
    // Parse function type: (param_types) -> return_type
    std::regex func_regex(R"(\(([^)]*)\)\s*->\s*(.+))");
    std::smatch match;
    
    if (std::regex_match(meld_type, match, func_regex)) {
        std::string param_types_str = match[1].str();
        std::string return_type = match[2].str();
        
        // Parse parameter types
        std::vector<std::string> param_types;
        if (!param_types_str.empty()) {
            std::stringstream ss(param_types_str);
            std::string param;
            while (std::getline(ss, param, ',')) {
                param.erase(0, param.find_first_not_of(" \t"));
                param.erase(param.find_last_not_of(" \t") + 1);
                auto mapped_param = map_type(param, target, custom_mappings);
                if (!mapped_param) return mapped_param;
                param_types.push_back(*mapped_param);
            }
        }
        
        // Map return type
        auto mapped_return = map_type(return_type, target, custom_mappings);
        if (!mapped_return) return mapped_return;
        
        // Generate function type for target language
        switch (target) {
            case TargetLanguage::Java: {
                std::ostringstream result;
                result << "java.util.function.Function<";
                if (param_types.size() == 1) {
                    result << param_types[0] << ", " << *mapped_return;
                } else {
                    result << "Object[], " << *mapped_return;
                }
                result << ">";
                return result.str();
            }
            case TargetLanguage::Go: {
                std::ostringstream result;
                result << "func(";
                for (size_t i = 0; i < param_types.size(); ++i) {
                    if (i > 0) result << ", ";
                    result << param_types[i];
                }
                result << ") " << *mapped_return;
                return result.str();
            }
            case TargetLanguage::Cpp: {
                std::ostringstream result;
                result << "std::function<" << *mapped_return << "(";
                for (size_t i = 0; i < param_types.size(); ++i) {
                    if (i > 0) result << ", ";
                    result << param_types[i];
                }
                result << ")>";
                return result.str();
            }
            case TargetLanguage::C: {
                std::ostringstream result;
                result << *mapped_return << " (*)(";
                for (size_t i = 0; i < param_types.size(); ++i) {
                    if (i > 0) result << ", ";
                    result << param_types[i];
                }
                if (param_types.empty()) result << "void";
                result << ")";
                return result.str();
            }
        }
    }
    
    return std::unexpected("Invalid function type format: " + meld_type);
}

// Helper method for union types
std::expected<std::string, std::string>
TypeMapper::map_union_type(const std::string& meld_type, TargetLanguage target,
                          const std::map<std::string, std::string>& custom_mappings) {
    
    // Parse union type: type1 | type2 | type3
    std::vector<std::string> union_types;
    std::stringstream ss(meld_type);
    std::string type;
    
    while (std::getline(ss, type, '|')) {
        type.erase(0, type.find_first_not_of(" \t"));
        type.erase(type.find_last_not_of(" \t") + 1);
        union_types.push_back(type);
    }
    
    switch (target) {
        case TargetLanguage::Java:
            // Java doesn't have union types, use Object or common supertype
            return "Object";
        case TargetLanguage::Go:
            // Go uses interface{} for union types
            return "interface{}";
        case TargetLanguage::Cpp:
            // C++ can use std::variant
            {
                std::ostringstream result;
                result << "std::variant<";
                for (size_t i = 0; i < union_types.size(); ++i) {
                    if (i > 0) result << ", ";
                    auto mapped_type = map_type(union_types[i], target, custom_mappings);
                    if (!mapped_type) return mapped_type;
                    result << *mapped_type;
                }
                result << ">";
                return result.str();
            }
        case TargetLanguage::C:
            // C uses void* or union
            return "void*";
    }
    
    return meld_type;
}

// Helper method for tuple types
std::expected<std::string, std::string>
TypeMapper::map_tuple_type(const std::string& meld_type, TargetLanguage target,
                          const std::map<std::string, std::string>& custom_mappings) {
    
    // Parse tuple type: (type1, type2, type3)
    std::string inner = meld_type.substr(1, meld_type.length() - 2);
    std::vector<std::string> tuple_types;
    std::stringstream ss(inner);
    std::string type;
    
    while (std::getline(ss, type, ',')) {
        type.erase(0, type.find_first_not_of(" \t"));
        type.erase(type.find_last_not_of(" \t") + 1);
        tuple_types.push_back(type);
    }
    
    switch (target) {
        case TargetLanguage::Java:
            // Java doesn't have tuples, use custom Tuple class or Object[]
            return "Object[]";
        case TargetLanguage::Go:
            // Go uses structs for tuples
            {
                std::ostringstream result;
                result << "struct { ";
                for (size_t i = 0; i < tuple_types.size(); ++i) {
                    auto mapped_type = map_type(tuple_types[i], target, custom_mappings);
                    if (!mapped_type) return mapped_type;
                    result << "Field" << i << " " << *mapped_type << "; ";
                }
                result << "}";
                return result.str();
            }
        case TargetLanguage::Cpp:
            // C++ can use std::tuple
            {
                std::ostringstream result;
                result << "std::tuple<";
                for (size_t i = 0; i < tuple_types.size(); ++i) {
                    if (i > 0) result << ", ";
                    auto mapped_type = map_type(tuple_types[i], target, custom_mappings);
                    if (!mapped_type) return mapped_type;
                    result << *mapped_type;
                }
                result << ">";
                return result.str();
            }
        case TargetLanguage::C:
            // C uses structs for tuples
            return "void*"; // Simplified for C
    }
    
    return meld_type;
}

std::expected<std::string, std::string>
TypeMapper::map_function_signature(const parser::ast::function_definition& func_decl,
                                  TargetLanguage target,
                                  const std::map<std::string, std::string>& type_mappings) {
    
    std::ostringstream signature;
    
    // Map return type
    std::string return_type = "void"; // Default if no return type specified
    if (!func_decl.return_type.type_name.name.empty()) {
        auto mapped_return = map_type(func_decl.return_type.type_name.name, target, type_mappings);
        if (!mapped_return) return mapped_return;
        return_type = *mapped_return;
    }
    
    // Generate signature based on target language
    switch (target) {
        case TargetLanguage::Java:
            signature << "public " << return_type << " " << func_decl.name.name << "(";
            break;
        case TargetLanguage::Go:
            signature << "func " << func_decl.name.name << "(";
            break;
        case TargetLanguage::Cpp:
            signature << return_type << " " << func_decl.name.name << "(";
            break;
        case TargetLanguage::C:
            signature << return_type << " " << func_decl.name.name << "(";
            break;
    }
    
    // Map parameters
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) signature << ", ";
        
        const auto& param = func_decl.parameters[i];
        auto mapped_type = map_type(param.type.type_name.name, target, type_mappings);
        if (!mapped_type) return mapped_type;
        
        switch (target) {
            case TargetLanguage::Java:
            case TargetLanguage::Cpp:
            case TargetLanguage::C:
                signature << *mapped_type << " " << param.name.name;
                break;
            case TargetLanguage::Go:
                signature << param.name.name << " " << *mapped_type;
                break;
        }
    }
    
    signature << ")";
    
    // Add return type for Go (comes after parameters)
    if (target == TargetLanguage::Go && return_type != "void") {
        signature << " " << return_type;
    }
    
    return signature.str();
}

std::expected<std::string, std::string>
TypeMapper::map_class_definition(const parser::ast::class_definition& class_def,
                                TargetLanguage target,
                                const std::map<std::string, std::string>& type_mappings) {
    
    std::ostringstream class_code;
    
    switch (target) {
        case TargetLanguage::Java:
            class_code << "public class " << class_def.name.name << " {\n";
            break;
        case TargetLanguage::Cpp:
            class_code << "class " << class_def.name.name << " {\npublic:\n";
            break;
        case TargetLanguage::Go:
            class_code << "type " << class_def.name.name << " struct {\n";
            break;
        case TargetLanguage::C:
            class_code << "typedef struct " << class_def.name.name << " {\n";
            break;
    }
    
    // Map fields (simplified - full implementation would handle all field types)
    for (const auto& field : class_def.fields) {
        auto mapped_type = map_type(field.type.type_name.name, target, type_mappings);
        if (!mapped_type) return mapped_type;
        
        switch (target) {
            case TargetLanguage::Java:
                class_code << "    public " << *mapped_type << " " << field.name.name << ";\n";
                break;
            case TargetLanguage::Cpp:
                class_code << "    " << *mapped_type << " " << field.name.name << ";\n";
                break;
            case TargetLanguage::Go:
                class_code << "    " << field.name.name << " " << *mapped_type << "\n";
                break;
            case TargetLanguage::C:
                class_code << "    " << *mapped_type << " " << field.name.name << ";\n";
                break;
        }
    }
    
    // Close class definition
    switch (target) {
        case TargetLanguage::Java:
        case TargetLanguage::Cpp:
            class_code << "};\n";
            break;
        case TargetLanguage::Go:
            class_code << "}\n";
            break;
        case TargetLanguage::C:
            class_code << "} " << class_def.name.name << ";\n";
            break;
    }
    
    return class_code.str();
}

// FFIBindingGenerator implementation
std::expected<std::string, std::string>
FFIBindingGenerator::generate_function_binding(const parser::ast::function_definition& func_decl,
                                              const FFIBinding& binding) {
    
    switch (binding.target_lang) {
        case TargetLanguage::Java:
            return generate_java_binding(func_decl, binding);
        case TargetLanguage::Go:
            return generate_go_binding(func_decl, binding);
        case TargetLanguage::Cpp:
            return generate_cpp_binding(func_decl, binding);
        case TargetLanguage::C:
            return generate_c_binding(func_decl, binding);
    }
    
    return std::unexpected("Unsupported target language");
}

std::expected<std::string, std::string>
FFIBindingGenerator::generate_class_binding(const parser::ast::class_definition& class_def,
                                           const FFIBinding& binding) {
    
    auto mapped_class = TypeMapper::map_class_definition(class_def, binding.target_lang, binding.type_mappings);
    if (!mapped_class) return mapped_class;
    
    std::ostringstream binding_code;
    binding_code << generate_imports(binding) << "\n\n";
    binding_code << *mapped_class;
    
    return binding_code.str();
}

std::string FFIBindingGenerator::generate_imports(const FFIBinding& binding) {
    std::ostringstream imports;
    
    switch (binding.target_lang) {
        case TargetLanguage::Java:
            if (!binding.class_name.empty()) {
                imports << "import " << binding.class_name << ";";
            }
            break;
        case TargetLanguage::Go:
            if (!binding.package_name.empty()) {
                imports << "import \"" << binding.package_name << "\"";
            }
            break;
        case TargetLanguage::Cpp:
            if (!binding.header_name.empty()) {
                imports << "#include " << binding.header_name;
            }
            break;
        case TargetLanguage::C:
            if (!binding.header_name.empty()) {
                imports << "#include " << binding.header_name;
            }
            break;
    }
    
    return imports.str();
}

std::expected<std::string, std::string>
FFIBindingGenerator::generate_native_wrapper(const parser::ast::function_definition& func_decl,
                                             const FFIBinding& binding) {
    
    std::ostringstream wrapper;
    
    // Generate comprehensive Meld function that calls native_call
    wrapper << "// Generated FFI wrapper for " << target_language_to_string(binding.target_lang) << " function\n";
    wrapper << "fnc " << func_decl.name.name << "(";
    
    // Parameters with type annotations
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) wrapper << ", ";
        wrapper << func_decl.parameters[i].name.name << ": " << func_decl.parameters[i].type.type_name.name;
    }
    
    wrapper << ")";
    
    // Return type
    if (!func_decl.return_type.type_name.name.empty()) {
        wrapper << " -> " << func_decl.return_type.type_name.name;
    }
    
    wrapper << " {\n";
    
    // Add parameter validation
    for (const auto& param : func_decl.parameters) {
        if (param.type.type_name.name == "string") {
            wrapper << "    // Validate string parameter\n";
            wrapper << "    if (" << param.name.name << " == nil) {\n";
            wrapper << "        throw ArgumentError(\"Parameter " << param.name.name << " cannot be nil\")\n";
            wrapper << "    }\n";
        }
    }
    
    // Generate library loading if needed
    if (!binding.library_name.empty()) {
        wrapper << "    // Load native library if not already loaded\n";
        wrapper << "    val library_handle = native_load(\"" << binding.library_name << "\")\n";
        wrapper << "    if (library_handle == nil) {\n";
        wrapper << "        throw LibraryError(\"Failed to load library: " << binding.library_name << "\")\n";
        wrapper << "    }\n\n";
    }
    
    // Generate native_call invocation with comprehensive signature
    wrapper << "    // Call native function with type-safe marshalling\n";
    wrapper << "    ";
    if (!func_decl.return_type.type_name.name.empty()) {
        wrapper << "val result = ";
    }
    
    wrapper << "native_call(\n";
    wrapper << "        function_name: \"" << func_decl.name.name << "\",\n";
    
    // Generate detailed function signature for native call
    auto signature = TypeMapper::map_function_signature(func_decl, binding.target_lang, binding.type_mappings);
    if (!signature) return signature;
    
    wrapper << "        signature: \"" << target_language_to_string(binding.target_lang) << ":" << *signature << "\",\n";
    
    // Add binding metadata
    wrapper << "        binding: {\n";
    wrapper << "            target_lang: \"" << target_language_to_string(binding.target_lang) << "\",\n";
    
    if (!binding.class_name.empty()) {
        wrapper << "            class_name: \"" << binding.class_name << "\",\n";
    }
    if (!binding.package_name.empty()) {
        wrapper << "            package_name: \"" << binding.package_name << "\",\n";
    }
    if (!binding.header_name.empty()) {
        wrapper << "            header_name: \"" << binding.header_name << "\",\n";
    }
    if (!binding.library_name.empty()) {
        wrapper << "            library_name: \"" << binding.library_name << "\",\n";
    }
    
    // Add type mappings
    if (!binding.type_mappings.empty()) {
        wrapper << "            type_mappings: {\n";
        for (const auto& [meld_type, target_type] : binding.type_mappings) {
            wrapper << "                \"" << meld_type << "\": \"" << target_type << "\",\n";
        }
        wrapper << "            },\n";
    }
    
    wrapper << "        },\n";
    
    // Arguments array with type conversion
    wrapper << "        arguments: [\n";
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        wrapper << "            {\n";
        wrapper << "                name: \"" << func_decl.parameters[i].name.name << "\",\n";
        wrapper << "                meld_type: \"" << func_decl.parameters[i].type.type_name.name << "\",\n";
        
        auto target_type = TypeMapper::map_type(func_decl.parameters[i].type.type_name.name, binding.target_lang, binding.type_mappings);
        if (target_type) {
            wrapper << "                target_type: \"" << *target_type << "\",\n";
        }
        
        wrapper << "                value: " << func_decl.parameters[i].name.name << "\n";
        wrapper << "            }";
        if (i < func_decl.parameters.size() - 1) wrapper << ",";
        wrapper << "\n";
    }
    wrapper << "        ]\n";
    wrapper << "    )\n\n";
    
    // Add error handling
    wrapper << "    // Handle potential FFI errors\n";
    wrapper << "    if (native_call_failed()) {\n";
    wrapper << "        val error_msg = native_get_last_error()\n";
    wrapper << "        throw FFIError(`Failed to call " << func_decl.name.name << ": ${error_msg}`)\n";
    wrapper << "    }\n\n";
    
    // Return result with type conversion if needed
    if (!func_decl.return_type.type_name.name.empty()) {
        wrapper << "    // Convert result from target language type to Meld type\n";
        auto target_return_type = TypeMapper::map_type(func_decl.return_type.type_name.name, binding.target_lang, binding.type_mappings);
        if (target_return_type && *target_return_type != func_decl.return_type.type_name.name) {
            wrapper << "    val converted_result = convert_from_" << target_language_to_string(binding.target_lang) 
                   << "_type(result, \"" << *target_return_type << "\", \"" << func_decl.return_type.type_name.name << "\")\n";
            wrapper << "    rtn converted_result\n";
        } else {
            wrapper << "    rtn result\n";
        }
    }
    
    wrapper << "}\n\n";
    
    // Generate convenience overloads for optional parameters
    if (func_decl.parameters.size() > 1) {
        wrapper << "// Convenience overloads\n";
        
        // Generate overload with default parameters
        wrapper << "fnc " << func_decl.name.name << "_simple(";
        
        // Only include required parameters (first parameter)
        if (!func_decl.parameters.empty()) {
            wrapper << func_decl.parameters[0].name.name << ": " << func_decl.parameters[0].type.type_name.name;
        }
        
        wrapper << ")";
        if (!func_decl.return_type.type_name.name.empty()) {
            wrapper << " -> " << func_decl.return_type.type_name.name;
        }
        wrapper << " {\n";
        
        wrapper << "    rtn " << func_decl.name.name << "(";
        for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
            if (i > 0) wrapper << ", ";
            if (i == 0) {
                wrapper << func_decl.parameters[i].name.name;
            } else {
                // Provide default values for other parameters
                if (func_decl.parameters[i].type.type_name.name == "string") {
                    wrapper << "\"\"";
                } else if (func_decl.parameters[i].type.type_name.name == "int") {
                    wrapper << "0";
                } else if (func_decl.parameters[i].type.type_name.name == "float") {
                    wrapper << "0.0";
                } else if (func_decl.parameters[i].type.type_name.name == "bool") {
                    wrapper << "false";
                } else {
                    wrapper << "nil";
                }
            }
        }
        wrapper << ")\n";
        wrapper << "}\n\n";
    }
    
    // Generate async version if applicable
    wrapper << "// Async version\n";
    wrapper << "fnc " << func_decl.name.name << "_async(";
    
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) wrapper << ", ";
        wrapper << func_decl.parameters[i].name.name << ": " << func_decl.parameters[i].type.type_name.name;
    }
    
    wrapper << ") -> Task<" << (func_decl.return_type.type_name.name.empty() ? "Unit" : func_decl.return_type.type_name.name) << "> {\n";
    wrapper << "    rtn Task.create {\n";
    wrapper << "        " << func_decl.name.name << "(";
    
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) wrapper << ", ";
        wrapper << func_decl.parameters[i].name.name;
    }
    
    wrapper << ")\n";
    wrapper << "    }\n";
    wrapper << "}\n";
    
    return wrapper.str();
}

// Generate ecosystem integration helpers
std::expected<std::string, std::string>
FFIBindingGenerator::generate_ecosystem_integration(const FFIBinding& binding) {
    
    std::ostringstream integration;
    
    switch (binding.target_lang) {
        case TargetLanguage::Java:
            integration << "// Java ecosystem integration\n";
            integration << "import java.util.concurrent.CompletableFuture;\n";
            integration << "import java.util.stream.Stream;\n";
            integration << "import java.util.Optional;\n";
            integration << "import java.time.LocalDateTime;\n";
            integration << "import com.fasterxml.jackson.databind.ObjectMapper;\n\n";
            
            integration << "public class MeldJavaIntegration {\n";
            integration << "    private static final ObjectMapper objectMapper = new ObjectMapper();\n\n";
            
            integration << "    // Convert Meld Result to Java Optional\n";
            integration << "    public static <T> Optional<T> resultToOptional(Object meldResult) {\n";
            integration << "        // Implementation would check Result type and extract value\n";
            integration << "        return Optional.ofNullable((T) meldResult);\n";
            integration << "    }\n\n";
            
            integration << "    // Convert Meld Task to CompletableFuture\n";
            integration << "    public static <T> CompletableFuture<T> taskToFuture(Object meldTask) {\n";
            integration << "        return CompletableFuture.supplyAsync(() -> {\n";
            integration << "            // Implementation would await Meld Task\n";
            integration << "            return (T) meldTask;\n";
            integration << "        });\n";
            integration << "    }\n\n";
            
            integration << "    // JSON serialization helpers\n";
            integration << "    public static String toJson(Object obj) throws Exception {\n";
            integration << "        return objectMapper.writeValueAsString(obj);\n";
            integration << "    }\n\n";
            
            integration << "    public static <T> T fromJson(String json, Class<T> clazz) throws Exception {\n";
            integration << "        return objectMapper.readValue(json, clazz);\n";
            integration << "    }\n";
            integration << "}\n";
            break;
            
        case TargetLanguage::Go:
            integration << "// Go ecosystem integration\n";
            integration << "package meld\n\n";
            integration << "import (\n";
            integration << "    \"context\"\n";
            integration << "    \"encoding/json\"\n";
            integration << "    \"fmt\"\n";
            integration << "    \"sync\"\n";
            integration << "    \"time\"\n";
            integration << ")\n\n";
            
            integration << "// MeldGoIntegration provides ecosystem integration utilities\n";
            integration << "type MeldGoIntegration struct {\n";
            integration << "    ctx context.Context\n";
            integration << "    mu  sync.RWMutex\n";
            integration << "}\n\n";
            
            integration << "// NewIntegration creates a new integration instance\n";
            integration << "func NewIntegration(ctx context.Context) *MeldGoIntegration {\n";
            integration << "    return &MeldGoIntegration{ctx: ctx}\n";
            integration << "}\n\n";
            
            integration << "// ResultToChannel converts Meld Result to Go channel\n";
            integration << "func (m *MeldGoIntegration) ResultToChannel(result interface{}) <-chan interface{} {\n";
            integration << "    ch := make(chan interface{}, 1)\n";
            integration << "    go func() {\n";
            integration << "        defer close(ch)\n";
            integration << "        ch <- result\n";
            integration << "    }()\n";
            integration << "    return ch\n";
            integration << "}\n\n";
            
            integration << "// TaskToContext converts Meld Task to Go context\n";
            integration << "func (m *MeldGoIntegration) TaskToContext(task interface{}) (context.Context, context.CancelFunc) {\n";
            integration << "    return context.WithTimeout(m.ctx, 30*time.Second)\n";
            integration << "}\n\n";
            
            integration << "// JSON helpers\n";
            integration << "func ToJSON(v interface{}) ([]byte, error) {\n";
            integration << "    return json.Marshal(v)\n";
            integration << "}\n\n";
            
            integration << "func FromJSON(data []byte, v interface{}) error {\n";
            integration << "    return json.Unmarshal(data, v)\n";
            integration << "}\n";
            break;
            
        case TargetLanguage::Cpp:
            integration << "// C++ ecosystem integration\n";
            integration << "#pragma once\n\n";
            integration << "#include <memory>\n";
            integration << "#include <future>\n";
            integration << "#include <optional>\n";
            integration << "#include <expected>\n";
            integration << "#include <chrono>\n";
            integration << "#include <string>\n";
            integration << "#include <nlohmann/json.hpp>\n\n";
            
            integration << "namespace meld {\n\n";
            
            integration << "class EcosystemIntegration {\n";
            integration << "public:\n";
            integration << "    // Convert Meld Result to std::expected\n";
            integration << "    template<typename T, typename E>\n";
            integration << "    static std::expected<T, E> result_to_expected(const void* meld_result) {\n";
            integration << "        // Implementation would check Result type and extract value\n";
            integration << "        return std::unexpected(E{});\n";
            integration << "    }\n\n";
            
            integration << "    // Convert Meld Task to std::future\n";
            integration << "    template<typename T>\n";
            integration << "    static std::future<T> task_to_future(const void* meld_task) {\n";
            integration << "        return std::async(std::launch::async, [meld_task]() -> T {\n";
            integration << "            // Implementation would await Meld Task\n";
            integration << "            return T{};\n";
            integration << "        });\n";
            integration << "    }\n\n";
            
            integration << "    // JSON serialization helpers\n";
            integration << "    template<typename T>\n";
            integration << "    static std::string to_json(const T& obj) {\n";
            integration << "        nlohmann::json j = obj;\n";
            integration << "        return j.dump();\n";
            integration << "    }\n\n";
            
            integration << "    template<typename T>\n";
            integration << "    static T from_json(const std::string& json_str) {\n";
            integration << "        auto j = nlohmann::json::parse(json_str);\n";
            integration << "        return j.get<T>();\n";
            integration << "    }\n\n";
            
            integration << "    // Memory management helpers\n";
            integration << "    template<typename T>\n";
            integration << "    static std::unique_ptr<T> make_unique_from_meld(const void* meld_obj) {\n";
            integration << "        // Implementation would convert Meld object to C++ object\n";
            integration << "        return std::make_unique<T>();\n";
            integration << "    }\n";
            integration << "};\n\n";
            
            integration << "} // namespace meld\n";
            break;
            
        case TargetLanguage::C:
            integration << "// C ecosystem integration\n";
            integration << "#ifndef MELD_C_INTEGRATION_H\n";
            integration << "#define MELD_C_INTEGRATION_H\n\n";
            integration << "#include <stdio.h>\n";
            integration << "#include <stdlib.h>\n";
            integration << "#include <string.h>\n";
            integration << "#include <stdint.h>\n";
            integration << "#include <stdbool.h>\n\n";
            
            integration << "// Meld type wrappers for C\n";
            integration << "typedef struct {\n";
            integration << "    bool is_success;\n";
            integration << "    void* value;\n";
            integration << "    void* error;\n";
            integration << "} meld_result_t;\n\n";
            
            integration << "typedef struct {\n";
            integration << "    void* data;\n";
            integration << "    size_t size;\n";
            integration << "    size_t capacity;\n";
            integration << "} meld_list_t;\n\n";
            
            integration << "typedef struct {\n";
            integration << "    char* key;\n";
            integration << "    void* value;\n";
            integration << "} meld_map_entry_t;\n\n";
            
            integration << "typedef struct {\n";
            integration << "    meld_map_entry_t* entries;\n";
            integration << "    size_t size;\n";
            integration << "    size_t capacity;\n";
            integration << "} meld_map_t;\n\n";
            
            integration << "// Ecosystem integration functions\n";
            integration << "meld_result_t* meld_result_create_success(void* value);\n";
            integration << "meld_result_t* meld_result_create_error(void* error);\n";
            integration << "void meld_result_free(meld_result_t* result);\n";
            integration << "bool meld_result_is_success(const meld_result_t* result);\n";
            integration << "void* meld_result_get_value(const meld_result_t* result);\n";
            integration << "void* meld_result_get_error(const meld_result_t* result);\n\n";
            
            integration << "meld_list_t* meld_list_create(size_t initial_capacity);\n";
            integration << "void meld_list_free(meld_list_t* list);\n";
            integration << "int meld_list_add(meld_list_t* list, void* item);\n";
            integration << "void* meld_list_get(const meld_list_t* list, size_t index);\n";
            integration << "size_t meld_list_size(const meld_list_t* list);\n\n";
            
            integration << "meld_map_t* meld_map_create(size_t initial_capacity);\n";
            integration << "void meld_map_free(meld_map_t* map);\n";
            integration << "int meld_map_put(meld_map_t* map, const char* key, void* value);\n";
            integration << "void* meld_map_get(const meld_map_t* map, const char* key);\n";
            integration << "size_t meld_map_size(const meld_map_t* map);\n\n";
            
            integration << "// JSON helpers (requires cJSON library)\n";
            integration << "char* meld_to_json_string(const void* obj);\n";
            integration << "void* meld_from_json_string(const char* json_str);\n";
            integration << "void meld_free_json_string(char* json_str);\n\n";
            
            integration << "#endif // MELD_C_INTEGRATION_H\n";
            break;
    }
    
    return integration.str();
}

// Generate error handling wrappers
std::expected<std::string, std::string>
FFIBindingGenerator::generate_error_handling_wrapper(const parser::ast::function_definition& func_decl,
                                                    const FFIBinding& binding) {
    
    std::ostringstream wrapper;
    
    wrapper << "// Error handling wrapper for " << func_decl.name.name << "\n";
    wrapper << "fnc " << func_decl.name.name << "_safe(";
    
    // Parameters
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) wrapper << ", ";
        wrapper << func_decl.parameters[i].name.name << ": " << func_decl.parameters[i].type.type_name.name;
    }
    
    wrapper << ") -> Result<" << (func_decl.return_type.type_name.name.empty() ? "Unit" : func_decl.return_type.type_name.name) << ", FFIError> {\n";
    
    wrapper << "    try {\n";
    wrapper << "        val result = " << func_decl.name.name << "(";
    
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) wrapper << ", ";
        wrapper << func_decl.parameters[i].name.name;
    }
    
    wrapper << ")\n";
    
    if (!func_decl.return_type.type_name.name.empty()) {
        wrapper << "        rtn Success(result)\n";
    } else {
        wrapper << "        rtn Success(Unit)\n";
    }
    
    wrapper << "    } catch (e: Exception) {\n";
    wrapper << "        rtn Error(FFIError(\n";
    wrapper << "            message: `FFI call to " << func_decl.name.name << " failed: ${e.message}`,\n";
    wrapper << "            function_name: \"" << func_decl.name.name << "\",\n";
    wrapper << "            target_language: \"" << target_language_to_string(binding.target_lang) << "\",\n";
    wrapper << "            original_error: e\n";
    wrapper << "        ))\n";
    wrapper << "    }\n";
    wrapper << "}\n";
    
    return wrapper.str();
}

// Generate async/await wrappers
std::expected<std::string, std::string>
FFIBindingGenerator::generate_async_wrapper(const parser::ast::function_definition& func_decl,
                                           const FFIBinding& binding) {
    
    std::ostringstream wrapper;
    
    wrapper << "// Async wrapper for " << func_decl.name.name << "\n";
    wrapper << "fnc " << func_decl.name.name << "_async(";
    
    // Parameters
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) wrapper << ", ";
        wrapper << func_decl.parameters[i].name.name << ": " << func_decl.parameters[i].type.type_name.name;
    }
    
    wrapper << ") -> Task<" << (func_decl.return_type.type_name.name.empty() ? "Unit" : func_decl.return_type.type_name.name) << "> {\n";
    
    wrapper << "    rtn Task.create {\n";
    wrapper << "        // Execute FFI call on background thread\n";
    wrapper << "        val result = " << func_decl.name.name << "_safe(";
    
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) wrapper << ", ";
        wrapper << func_decl.parameters[i].name.name;
    }
    
    wrapper << ")\n";
    wrapper << "        \n";
    wrapper << "        result.match {\n";
    wrapper << "            Success(value) -> value\n";
    wrapper << "            Error(error) -> throw error\n";
    wrapper << "        }\n";
    wrapper << "    }\n";
    wrapper << "}\n\n";
    
    // Generate cancellable version
    wrapper << "// Cancellable async wrapper\n";
    wrapper << "fnc " << func_decl.name.name << "_async_cancellable(";
    
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) wrapper << ", ";
        wrapper << func_decl.parameters[i].name.name << ": " << func_decl.parameters[i].type.type_name.name;
    }
    
    if (!func_decl.parameters.empty()) wrapper << ", ";
    wrapper << "cancellation_token: CancellationToken";
    
    wrapper << ") -> Task<" << (func_decl.return_type.type_name.name.empty() ? "Unit" : func_decl.return_type.type_name.name) << "> {\n";
    
    wrapper << "    rtn Task.create {\n";
    wrapper << "        // Check cancellation before starting\n";
    wrapper << "        cancellation_token.throwIfCancelled()\n";
    wrapper << "        \n";
    wrapper << "        val result = " << func_decl.name.name << "_safe(";
    
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) wrapper << ", ";
        wrapper << func_decl.parameters[i].name.name;
    }
    
    wrapper << ")\n";
    wrapper << "        \n";
    wrapper << "        // Check cancellation after completion\n";
    wrapper << "        cancellation_token.throwIfCancelled()\n";
    wrapper << "        \n";
    wrapper << "        result.match {\n";
    wrapper << "            Success(value) -> value\n";
    wrapper << "            Error(error) -> throw error\n";
    wrapper << "        }\n";
    wrapper << "    }\n";
    wrapper << "}\n";
    
    return wrapper.str();
}

// Generate memory management helpers
std::expected<std::string, std::string>
FFIBindingGenerator::generate_memory_management(const FFIBinding& binding) {
    
    std::ostringstream memory;
    
    memory << "// Memory management helpers for " << target_language_to_string(binding.target_lang) << " FFI\n";
    
    switch (binding.target_lang) {
        case TargetLanguage::Java:
            memory << "class MeldJavaMemoryManager {\n";
            memory << "    // Java uses garbage collection, minimal memory management needed\n";
            memory << "    \n";
            memory << "    fnc cleanup_native_resources(resource: NativeResource) {\n";
            memory << "        // Call native cleanup if needed\n";
            memory << "        if (resource.needs_cleanup()) {\n";
            memory << "            native_call(\"cleanup_resource\", \"java:void cleanup(long)\", [resource.handle])\n";
            memory << "        }\n";
            memory << "    }\n";
            memory << "}\n";
            break;
            
        case TargetLanguage::Go:
            memory << "class MeldGoMemoryManager {\n";
            memory << "    // Go has garbage collection but may need to manage C resources\n";
            memory << "    \n";
            memory << "    fnc cleanup_cgo_resources(resource: CGoResource) {\n";
            memory << "        // Free C memory allocated in CGO calls\n";
            memory << "        if (resource.c_pointer != nil) {\n";
            memory << "            native_call(\"free\", \"c:void free(void*)\", [resource.c_pointer])\n";
            memory << "            resource.c_pointer = nil\n";
            memory << "        }\n";
            memory << "    }\n";
            memory << "}\n";
            break;
            
        case TargetLanguage::Cpp:
            memory << "class MeldCppMemoryManager {\n";
            memory << "    // C++ requires careful memory management\n";
            memory << "    \n";
            memory << "    fnc cleanup_cpp_resources(resource: CppResource) {\n";
            memory << "        // Call C++ destructor or delete\n";
            memory << "        if (resource.cpp_pointer != nil) {\n";
            memory << "            native_call(\"delete_cpp_object\", \"cpp:void delete_object(void*)\", [resource.cpp_pointer])\n";
            memory << "            resource.cpp_pointer = nil\n";
            memory << "        }\n";
            memory << "    }\n";
            memory << "    \n";
            memory << "    fnc create_smart_pointer<T>(raw_pointer: RawPointer<T>) -> UniquePtr<T> {\n";
            memory << "        // Wrap raw pointer in smart pointer for RAII\n";
            memory << "        rtn UniquePtr.from_raw(raw_pointer)\n";
            memory << "    }\n";
            memory << "}\n";
            break;
            
        case TargetLanguage::C:
            memory << "class MeldCMemoryManager {\n";
            memory << "    // C requires manual memory management\n";
            memory << "    \n";
            memory << "    fnc cleanup_c_resources(resource: CResource) {\n";
            memory << "        // Free C memory\n";
            memory << "        if (resource.c_pointer != nil) {\n";
            memory << "            native_call(\"free\", \"c:void free(void*)\", [resource.c_pointer])\n";
            memory << "            resource.c_pointer = nil\n";
            memory << "        }\n";
            memory << "    }\n";
            memory << "    \n";
            memory << "    fnc allocate_c_memory(size: int) -> CPointer {\n";
            memory << "        val pointer = native_call(\"malloc\", \"c:void* malloc(size_t)\", [size])\n";
            memory << "        if (pointer == nil) {\n";
            memory << "            throw OutOfMemoryError(`Failed to allocate ${size} bytes`)\n";
            memory << "        }\n";
            memory << "        rtn CPointer(pointer)\n";
            memory << "    }\n";
            memory << "    \n";
            memory << "    fnc reallocate_c_memory(pointer: CPointer, new_size: int) -> CPointer {\n";
            memory << "        val new_pointer = native_call(\"realloc\", \"c:void* realloc(void*, size_t)\", [pointer.raw, new_size])\n";
            memory << "        if (new_pointer == nil && new_size > 0) {\n";
            memory << "            throw OutOfMemoryError(`Failed to reallocate to ${new_size} bytes`)\n";
            memory << "        }\n";
            memory << "        rtn CPointer(new_pointer)\n";
            memory << "    }\n";
            memory << "}\n";
            break;
    }
    
    return memory.str();
}

// Language-specific generators
std::expected<std::string, std::string>
FFIBindingGenerator::generate_java_binding(const parser::ast::function_definition& func_decl,
                                          const FFIBinding& binding) {
    
    std::ostringstream java_code;
    
    // Generate package declaration if specified
    if (!binding.package_name.empty()) {
        java_code << "package " << binding.package_name << ";\n\n";
    }
    
    // Generate imports
    java_code << generate_imports(binding) << "\n";
    
    // Add standard JNI imports
    java_code << "import java.nio.ByteBuffer;\n";
    java_code << "import java.util.*;\n\n";
    
    // Generate JNI wrapper class
    std::string class_name = func_decl.name.name + "Wrapper";
    java_code << "public class " << class_name << " {\n";
    
    // Static initializer for native library loading
    std::string library_name = binding.library_name.empty() ? "meld_jni" : binding.library_name;
    java_code << "    static {\n";
    java_code << "        try {\n";
    java_code << "            System.loadLibrary(\"" << library_name << "\");\n";
    java_code << "        } catch (UnsatisfiedLinkError e) {\n";
    java_code << "            System.err.println(\"Failed to load native library: " << library_name << "\");\n";
    java_code << "            throw e;\n";
    java_code << "        }\n";
    java_code << "    }\n\n";
    
    // Generate native method declaration
    auto signature = TypeMapper::map_function_signature(func_decl, TargetLanguage::Java, binding.type_mappings);
    if (!signature) return signature;
    
    java_code << "    public static native " << *signature << ";\n\n";
    
    // Generate convenience wrapper method
    java_code << "    // Convenience wrapper method\n";
    java_code << "    " << *signature << " {\n";
    java_code << "        return " << func_decl.name.name << "(";
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) java_code << ", ";
        java_code << func_decl.parameters[i].name.name;
    }
    java_code << ");\n";
    java_code << "    }\n\n";
    
    // Generate JNI method signature comment for C implementation
    java_code << "    /*\n";
    java_code << "     * JNI C function signature:\n";
    java_code << "     * JNIEXPORT ";
    
    // Map return type to JNI type
    std::string jni_return_type = "void";
    if (!func_decl.return_type.type_name.name.empty()) {
        auto mapped_return = TypeMapper::map_type(func_decl.return_type.type_name.name, TargetLanguage::Java, binding.type_mappings);
        if (mapped_return) {
            if (*mapped_return == "int") jni_return_type = "jint";
            else if (*mapped_return == "double") jni_return_type = "jdouble";
            else if (*mapped_return == "boolean") jni_return_type = "jboolean";
            else if (*mapped_return == "String") jni_return_type = "jstring";
            else jni_return_type = "jobject";
        }
    }
    
    java_code << jni_return_type << " JNICALL\n";
    java_code << "     * Java_" << (binding.package_name.empty() ? "" : binding.package_name + "_") 
              << class_name << "_" << func_decl.name.name << "\n";
    java_code << "     *   (JNIEnv *env, jclass cls";
    
    for (const auto& param : func_decl.parameters) {
        java_code << ", ";
        auto mapped_type = TypeMapper::map_type(param.type.type_name.name, TargetLanguage::Java, binding.type_mappings);
        if (mapped_type) {
            if (*mapped_type == "int") java_code << "jint";
            else if (*mapped_type == "double") java_code << "jdouble";
            else if (*mapped_type == "boolean") java_code << "jboolean";
            else if (*mapped_type == "String") java_code << "jstring";
            else java_code << "jobject";
        } else {
            java_code << "jobject";
        }
        java_code << " " << param.name.name;
    }
    
    java_code << ");\n";
    java_code << "     */\n";
    
    java_code << "}\n";
    
    return java_code.str();
}

std::expected<std::string, std::string>
FFIBindingGenerator::generate_go_binding(const parser::ast::function_definition& func_decl,
                                        const FFIBinding& binding) {
    
    std::ostringstream go_code;
    
    // Generate package declaration
    std::string package_name = binding.package_name.empty() ? "main" : binding.package_name;
    go_code << "package " << package_name << "\n\n";
    
    // Generate imports
    if (!binding.package_name.empty() && binding.package_name != "main") {
        go_code << generate_imports(binding) << "\n";
    }
    go_code << "import \"C\"\n";
    go_code << "import (\n";
    go_code << "    \"unsafe\"\n";
    go_code << "    \"fmt\"\n";
    go_code << ")\n\n";
    
    // Generate CGO comment with C function declaration
    go_code << "/*\n";
    auto c_signature = TypeMapper::map_function_signature(func_decl, TargetLanguage::C, binding.type_mappings);
    if (c_signature) {
        go_code << *c_signature << ";\n";
    }
    
    // Add any required C headers
    if (!binding.header_name.empty()) {
        go_code << "#include " << binding.header_name << "\n";
    }
    
    // Add library linking if specified
    if (!binding.library_name.empty()) {
        go_code << "#cgo LDFLAGS: -l" << binding.library_name << "\n";
    }
    
    go_code << "*/\n";
    go_code << "import \"C\"\n\n";
    
    // Generate Go wrapper function
    auto go_signature = TypeMapper::map_function_signature(func_decl, TargetLanguage::Go, binding.type_mappings);
    if (!go_signature) return go_signature;
    
    go_code << "// " << func_decl.name.name << " wraps the C function\n";
    go_code << *go_signature << " {\n";
    
    // Generate parameter conversion from Go to C types
    for (const auto& param : func_decl.parameters) {
        auto go_type = TypeMapper::map_type(param.type.type_name.name, TargetLanguage::Go, binding.type_mappings);
        if (go_type && *go_type == "string") {
            go_code << "    c_" << param.name.name << " := C.CString(" << param.name.name << ")\n";
            go_code << "    defer C.free(unsafe.Pointer(c_" << param.name.name << "))\n";
        }
    }
    
    // Generate C function call
    go_code << "    ";
    if (!func_decl.return_type.type_name.name.empty()) {
        go_code << "result := ";
    }
    go_code << "C." << func_decl.name.name << "(";
    
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) go_code << ", ";
        
        auto go_type = TypeMapper::map_type(func_decl.parameters[i].type.type_name.name, TargetLanguage::Go, binding.type_mappings);
        if (go_type && *go_type == "string") {
            go_code << "c_" << func_decl.parameters[i].name.name;
        } else {
            go_code << "C." << func_decl.parameters[i].type.type_name.name << "(" << func_decl.parameters[i].name.name << ")";
        }
    }
    
    go_code << ")\n";
    
    // Generate return value conversion
    if (!func_decl.return_type.type_name.name.empty()) {
        auto return_type = TypeMapper::map_type(func_decl.return_type.type_name.name, TargetLanguage::Go, binding.type_mappings);
        if (return_type && *return_type == "string") {
            go_code << "    return C.GoString(result)\n";
        } else {
            go_code << "    return " << *return_type << "(result)\n";
        }
    }
    
    go_code << "}\n\n";
    
    // Generate export comment for reverse calls (Go -> C)
    go_code << "//export " << func_decl.name.name << "_export\n";
    go_code << "func " << func_decl.name.name << "_export(";
    
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) go_code << ", ";
        go_code << func_decl.parameters[i].name.name << " C." << func_decl.parameters[i].type.type_name.name;
    }
    
    go_code << ") ";
    if (!func_decl.return_type.type_name.name.empty()) {
        go_code << "C." << func_decl.return_type.type_name.name << " ";
    }
    go_code << "{\n";
    go_code << "    // Export function for C to call Go implementation\n";
    go_code << "    return " << func_decl.name.name << "(";
    
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) go_code << ", ";
        auto go_type = TypeMapper::map_type(func_decl.parameters[i].type.type_name.name, TargetLanguage::Go, binding.type_mappings);
        if (go_type && *go_type == "string") {
            go_code << "C.GoString(" << func_decl.parameters[i].name.name << ")";
        } else {
            go_code << *go_type << "(" << func_decl.parameters[i].name.name << ")";
        }
    }
    
    go_code << ")\n";
    go_code << "}\n";
    
    return go_code.str();
}

std::expected<std::string, std::string>
FFIBindingGenerator::generate_cpp_binding(const parser::ast::function_definition& func_decl,
                                         const FFIBinding& binding) {
    
    std::ostringstream cpp_code;
    
    // Generate includes
    cpp_code << generate_imports(binding) << "\n";
    
    // Add standard C++ headers for FFI
    cpp_code << "#include <memory>\n";
    cpp_code << "#include <string>\n";
    cpp_code << "#include <vector>\n";
    cpp_code << "#include <stdexcept>\n\n";
    
    // Generate namespace if specified
    if (!binding.package_name.empty()) {
        cpp_code << "namespace " << binding.package_name << " {\n\n";
    }
    
    // Generate extern "C" wrapper for C interop
    cpp_code << "extern \"C\" {\n";
    
    auto c_signature = TypeMapper::map_function_signature(func_decl, TargetLanguage::C, binding.type_mappings);
    if (!c_signature) return c_signature;
    
    cpp_code << "    " << *c_signature << ";\n";
    cpp_code << "}\n\n";
    
    // Generate C++ wrapper function
    auto cpp_signature = TypeMapper::map_function_signature(func_decl, TargetLanguage::Cpp, binding.type_mappings);
    if (!cpp_signature) return cpp_signature;
    
    cpp_code << "// C++ wrapper function\n";
    cpp_code << *cpp_signature << " {\n";
    
    // Generate parameter conversion and validation
    for (const auto& param : func_decl.parameters) {
        auto cpp_type = TypeMapper::map_type(param.type.type_name.name, TargetLanguage::Cpp, binding.type_mappings);
        if (cpp_type && *cpp_type == "std::string") {
            cpp_code << "    if (" << param.name.name << ".empty()) {\n";
            cpp_code << "        throw std::invalid_argument(\"Parameter " << param.name.name << " cannot be empty\");\n";
            cpp_code << "    }\n";
        }
    }
    
    // Generate function call with error handling
    cpp_code << "    try {\n";
    cpp_code << "        ";
    if (!func_decl.return_type.type_name.name.empty()) {
        cpp_code << "auto result = ";
    }
    
    cpp_code << func_decl.name.name << "(";
    
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) cpp_code << ", ";
        
        auto cpp_type = TypeMapper::map_type(func_decl.parameters[i].type.type_name.name, TargetLanguage::Cpp, binding.type_mappings);
        if (cpp_type && *cpp_type == "std::string") {
            cpp_code << func_decl.parameters[i].name.name << ".c_str()";
        } else {
            cpp_code << func_decl.parameters[i].name.name;
        }
    }
    
    cpp_code << ");\n";
    
    if (!func_decl.return_type.type_name.name.empty()) {
        cpp_code << "        return result;\n";
    }
    
    cpp_code << "    } catch (const std::exception& e) {\n";
    cpp_code << "        throw std::runtime_error(std::string(\"FFI call failed: \") + e.what());\n";
    cpp_code << "    }\n";
    cpp_code << "}\n\n";
    
    // Generate smart pointer wrapper for RAII if dealing with pointers
    if (!func_decl.return_type.type_name.name.empty()) {
        auto return_type = TypeMapper::map_type(func_decl.return_type.type_name.name, TargetLanguage::Cpp, binding.type_mappings);
        if (return_type && return_type->find("*") != std::string::npos) {
            cpp_code << "// RAII wrapper\n";
            cpp_code << "std::unique_ptr<" << *return_type << "> " << func_decl.name.name << "_unique(";
            
            for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
                if (i > 0) cpp_code << ", ";
                auto param_type = TypeMapper::map_type(func_decl.parameters[i].type.type_name.name, TargetLanguage::Cpp, binding.type_mappings);
                cpp_code << *param_type << " " << func_decl.parameters[i].name.name;
            }
            
            cpp_code << ") {\n";
            cpp_code << "    auto ptr = " << func_decl.name.name << "(";
            
            for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
                if (i > 0) cpp_code << ", ";
                cpp_code << func_decl.parameters[i].name.name;
            }
            
            cpp_code << ");\n";
            cpp_code << "    return std::unique_ptr<" << *return_type << ">(ptr);\n";
            cpp_code << "}\n\n";
        }
    }
    
    // Close namespace if specified
    if (!binding.package_name.empty()) {
        cpp_code << "} // namespace " << binding.package_name << "\n";
    }
    
    return cpp_code.str();
}

std::expected<std::string, std::string>
FFIBindingGenerator::generate_c_binding(const parser::ast::function_definition& func_decl,
                                       const FFIBinding& binding) {
    
    std::ostringstream c_code;
    
    // Generate includes
    c_code << generate_imports(binding) << "\n";
    
    // Add standard C headers
    c_code << "#include <stdio.h>\n";
    c_code << "#include <stdlib.h>\n";
    c_code << "#include <string.h>\n";
    c_code << "#include <stdint.h>\n";
    c_code << "#include <stdbool.h>\n\n";
    
    // Generate function declaration
    auto signature = TypeMapper::map_function_signature(func_decl, TargetLanguage::C, binding.type_mappings);
    if (!signature) return signature;
    
    c_code << "// Function declaration\n";
    c_code << *signature << ";\n\n";
    
    // Generate wrapper function with error checking
    c_code << "// Wrapper function with error checking\n";
    
    std::string wrapper_name = func_decl.name.name + "_safe";
    c_code << (func_decl.return_type.type_name.name.empty() ? "void" : 
              *TypeMapper::map_type(func_decl.return_type.type_name.name, TargetLanguage::C, binding.type_mappings));
    c_code << " " << wrapper_name << "(";
    
    // Parameters with error parameter
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) c_code << ", ";
        auto param_type = TypeMapper::map_type(func_decl.parameters[i].type.type_name.name, TargetLanguage::C, binding.type_mappings);
        c_code << *param_type << " " << func_decl.parameters[i].name.name;
    }
    
    if (!func_decl.parameters.empty()) c_code << ", ";
    c_code << "int* error_code) {\n";
    
    // Initialize error code
    c_code << "    if (error_code) *error_code = 0;\n\n";
    
    // Parameter validation
    for (const auto& param : func_decl.parameters) {
        auto param_type = TypeMapper::map_type(param.type.type_name.name, TargetLanguage::C, binding.type_mappings);
        if (param_type && *param_type == "char*") {
            c_code << "    if (!" << param.name.name << ") {\n";
            c_code << "        if (error_code) *error_code = -1;\n";
            c_code << "        fprintf(stderr, \"Error: Parameter " << param.name.name << " is NULL\\n\");\n";
            if (!func_decl.return_type.type_name.name.empty()) {
                auto return_type = TypeMapper::map_type(func_decl.return_type.type_name.name, TargetLanguage::C, binding.type_mappings);
                if (return_type && *return_type == "char*") {
                    c_code << "        return NULL;\n";
                } else {
                    c_code << "        return 0;\n";
                }
            } else {
                c_code << "        return;\n";
            }
            c_code << "    }\n";
        }
    }
    
    // Function call
    c_code << "\n    ";
    if (!func_decl.return_type.type_name.name.empty()) {
        c_code << "return ";
    }
    
    c_code << func_decl.name.name << "(";
    
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) c_code << ", ";
        c_code << func_decl.parameters[i].name.name;
    }
    
    c_code << ");\n";
    c_code << "}\n\n";
    
    // Generate convenience macros
    c_code << "// Convenience macros\n";
    c_code << "#define " << func_decl.name.name << "_call(";
    
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) c_code << ", ";
        c_code << func_decl.parameters[i].name.name;
    }
    
    c_code << ") \\\n";
    c_code << "    " << wrapper_name << "(";
    
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) c_code << ", ";
        c_code << func_decl.parameters[i].name.name;
    }
    
    if (!func_decl.parameters.empty()) c_code << ", ";
    c_code << "NULL)\n\n";
    
    // Generate error checking macro
    c_code << "#define " << func_decl.name.name << "_call_checked(";
    
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) c_code << ", ";
        c_code << func_decl.parameters[i].name.name;
    }
    
    if (!func_decl.parameters.empty()) c_code << ", ";
    c_code << "err) \\\n";
    c_code << "    " << wrapper_name << "(";
    
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) c_code << ", ";
        c_code << func_decl.parameters[i].name.name;
    }
    
    if (!func_decl.parameters.empty()) c_code << ", ";
    c_code << "&(err))\n\n";
    
    // Generate function pointer typedef
    c_code << "// Function pointer typedef\n";
    c_code << "typedef " << (func_decl.return_type.type_name.name.empty() ? "void" : 
                            *TypeMapper::map_type(func_decl.return_type.type_name.name, TargetLanguage::C, binding.type_mappings));
    c_code << " (*" << func_decl.name.name << "_fn_t)(";
    
    for (size_t i = 0; i < func_decl.parameters.size(); ++i) {
        if (i > 0) c_code << ", ";
        auto param_type = TypeMapper::map_type(func_decl.parameters[i].type.type_name.name, TargetLanguage::C, binding.type_mappings);
        c_code << *param_type;
    }
    
    if (func_decl.parameters.empty()) {
        c_code << "void";
    }
    
    c_code << ");\n";
    
    return c_code.str();
}

// ExternMacro implementation
std::expected<FFIBinding, std::string>
ExternMacro::parse_extern_annotation(const std::string& annotation_text) {
    
    FFIBinding binding;
    
    // Simple annotation parsing - in a full implementation this would be more robust
    // Expected format: @extern(lang: "java", class: "java.util.ArrayList")
    
    std::regex lang_regex(R"re(lang:\s*"([^"]+)")re");
    std::regex class_regex(R"re(class:\s*"([^"]+)")re");
    std::regex package_regex(R"re(package:\s*"([^"]+)")re");
    std::regex header_regex(R"re(header:\s*"([^"]+)")re");
    std::regex library_regex(R"re(library:\s*"([^"]+)")re");
    
    std::smatch match;
    
    // Parse lang parameter (required)
    if (std::regex_search(annotation_text, match, lang_regex)) {
        auto lang_result = parse_target_language(match[1].str());
        if (!lang_result) return std::unexpected(lang_result.error());
        binding.target_lang = *lang_result;
    } else {
        return std::unexpected("@extern annotation must specify 'lang' parameter");
    }
    
    // Parse optional parameters
    if (std::regex_search(annotation_text, match, class_regex)) {
        binding.class_name = match[1].str();
    }
    
    if (std::regex_search(annotation_text, match, package_regex)) {
        binding.package_name = match[1].str();
    }
    
    if (std::regex_search(annotation_text, match, header_regex)) {
        binding.header_name = match[1].str();
    }
    
    if (std::regex_search(annotation_text, match, library_regex)) {
        binding.library_name = match[1].str();
    }
    
    // Set default type mappings
    binding.type_mappings = TypeMapper::get_default_mappings(binding.target_lang);
    
    return binding;
}

std::expected<kernel::Value, std::string>
ExternMacro::apply_to_function(const parser::ast::function_definition& func_decl,
                              const FFIBinding& binding,
                              MacroExpander& expander) {
    
    // Generate native wrapper function
    auto wrapper_result = FFIBindingGenerator::generate_native_wrapper(func_decl, binding);
    if (!wrapper_result) return std::unexpected(wrapper_result.error());
    
    // For now, return a simple symbol representing the generated function
    // In a full implementation, this would return proper AST nodes
    auto function_symbol = std::make_shared<kernel::Symbol>(func_decl.name.name + "_extern");
    return kernel::Value(function_symbol);
}

std::expected<kernel::Value, std::string>
ExternMacro::apply_to_class(const parser::ast::class_definition& class_def,
                           const FFIBinding& binding,
                           MacroExpander& expander) {
    
    // Generate class binding
    auto binding_result = FFIBindingGenerator::generate_class_binding(class_def, binding);
    if (!binding_result) return std::unexpected(binding_result.error());
    
    // For now, return a simple symbol representing the generated class
    // In a full implementation, this would return proper AST nodes
    auto class_symbol = std::make_shared<kernel::Symbol>(class_def.name.name + "_extern");
    return kernel::Value(class_symbol);
}

kernel::Value ExternMacro::generate_native_call(
    const std::string& function_name,
    const std::vector<std::string>& param_names,
    const FFIBinding& binding) {
    
    // Generate: native_call("function_name", "signature", [args])
    
    // Create function name symbol
    auto func_name_symbol = std::make_shared<kernel::Symbol>("native_call");
    
    // Create arguments list
    std::vector<kernel::Value> args;
    
    // Function name
    args.push_back(kernel::Value(std::make_shared<kernel::Symbol>(function_name)));
    
    // Signature (simplified)
    std::string signature = target_language_to_string(binding.target_lang) + ":" + function_name;
    args.push_back(kernel::Value(std::make_shared<kernel::Symbol>(signature)));
    
    // Parameters array
    std::vector<kernel::Value> params;
    for (const auto& param : param_names) {
        params.push_back(kernel::Value(std::make_shared<kernel::Symbol>(param)));
    }
    args.push_back(kernel::list(params));
    
    // Create function call
    return kernel::list({
        kernel::Value(func_name_symbol),
        args[0], args[1], args[2]
    });
}

kernel::Value ExternMacro::generate_native_load(const FFIBinding& binding) {
    
    // Generate: native_load("library_path")
    
    auto func_symbol = std::make_shared<kernel::Symbol>("native_load");
    auto lib_symbol = std::make_shared<kernel::Symbol>(binding.library_name);
    
    return kernel::list({
        kernel::Value(func_symbol),
        kernel::Value(lib_symbol)
    });
}

// ExternDecorator implementation
ExternDecorator::ExternDecorator() 
    : Decorator("extern", [this](const parser::ast::class_definition& class_def, MacroExpander& expander) {
        return this->apply(class_def, expander);
    }) {
}

std::expected<kernel::Value, std::string> 
ExternDecorator::apply(const parser::ast::class_definition& class_def, MacroExpander& expander) const {
    
    // Extract @extern binding from class definition
    auto binding_result = extract_extern_binding(class_def);
    if (!binding_result) return std::unexpected(binding_result.error());
    
    // Apply @extern macro to class
    return ExternMacro::apply_to_class(class_def, *binding_result, expander);
}

std::expected<FFIBinding, std::string>
ExternDecorator::extract_extern_binding(const parser::ast::class_definition& class_def) const {
    
    // In a full implementation, this would parse actual @extern annotations from the AST
    // For now, we'll create a default binding
    
    FFIBinding binding;
    binding.target_lang = TargetLanguage::Java; // Default
    binding.class_name = "java.lang.Object"; // Default
    binding.type_mappings = TypeMapper::get_default_mappings(binding.target_lang);
    
    return binding;
}

// Registration functions
void register_extern_macro() {
    auto& registry = MacroRegistry::instance();
    
    // Register @extern as a macro
    auto extern_macro = make_macro(
        "extern",
        {"annotation", "definition"},
        [](const kernel::Value& ast_node, MacroExpander& expander) -> std::expected<kernel::Value, std::string> {
            
            // In a full implementation, this would:
            // 1. Parse the annotation parameters
            // 2. Extract the function/class definition
            // 3. Generate appropriate FFI bindings
            // 4. Return transformed AST
            
            // For now, return the original node
            return ast_node;
        }
    );
    
    registry.register_macro(extern_macro);
}

void register_extern_decorator() {
    auto& registry = DecoratorRegistry::instance();
    
    // Register @extern as a decorator
    auto extern_decorator = std::make_shared<ExternDecorator>();
    registry.register_decorator(extern_decorator);
}

// Annotation parser implementations
namespace extern_parser {

std::expected<TargetLanguage, std::string> parse_lang_param(const std::string& value) {
    return parse_target_language(value);
}

std::expected<std::string, std::string> parse_class_param(const std::string& value) {
    if (value.empty()) {
        return std::unexpected("Class parameter cannot be empty");
    }
    return value;
}

std::expected<std::string, std::string> parse_package_param(const std::string& value) {
    if (value.empty()) {
        return std::unexpected("Package parameter cannot be empty");
    }
    return value;
}

std::expected<std::string, std::string> parse_header_param(const std::string& value) {
    if (value.empty()) {
        return std::unexpected("Header parameter cannot be empty");
    }
    return value;
}

std::expected<std::string, std::string> parse_library_param(const std::string& value) {
    if (value.empty()) {
        return std::unexpected("Library parameter cannot be empty");
    }
    return value;
}

std::expected<std::map<std::string, std::string>, std::string> 
parse_type_mappings(const std::string& value) {
    
    std::map<std::string, std::string> mappings;
    
    // Simple JSON-like parsing for type mappings
    // Expected format: {"int": "int32", "string": "std::string"}
    
    std::regex mapping_regex(R"re("([^"]+)":\s*"([^"]+)")re");
    std::sregex_iterator iter(value.begin(), value.end(), mapping_regex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        const std::smatch& match = *iter;
        mappings[match[1].str()] = match[2].str();
    }
    
    return mappings;
}

} // namespace extern_parser

} // namespace meld::macro