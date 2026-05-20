#include "meld/api/holographic_extensions.hpp"
#include "meld/api/holographic_view.hpp"
#include "meld/compiler/ast_printer.hpp"
#include <sstream>
#include <regex>

namespace meld::api::extensions {

// Helper function to count tokens (same as in holographic_view.cpp)
size_t count_tokens_internal(const std::string& text) {
    if (text.empty()) return 0;
    
    // Simple tokenization: split on whitespace and common delimiters
    std::regex token_regex(R"(\s+|[{}();,\[\]\.])");
    std::sregex_token_iterator iter(text.begin(), text.end(), token_regex, -1);
    std::sregex_token_iterator end;
    
    size_t count = 0;
    for (; iter != end; ++iter) {
        if (!iter->str().empty()) {
            count++;
        }
    }
    return count;
}

// ClassDefinitionExtensions implementation
ClassHologram ClassDefinitionExtensions::toHologram(const parser::ast::class_definition& class_def) {
    return HolographicView::fromClass(class_def);
}

std::unordered_map<std::string, std::string> ClassDefinitionExtensions::getCompressionStats(const parser::ast::class_definition& class_def) {
    ClassHologram hologram = toHologram(class_def);
    
    std::unordered_map<std::string, std::string> stats;
    stats["class_name"] = class_def.name.name;
    stats["original_tokens"] = std::to_string(hologram.getOriginalTokenCount());
    stats["hologram_tokens"] = std::to_string(hologram.getHologramTokenCount());
    stats["compression_ratio"] = std::to_string(hologram.getCompressionRatio());
    stats["compression_percentage"] = std::to_string(hologram.getCompressionRatio() * 100.0) + "%";
    stats["fields_count"] = std::to_string(hologram.getFields().size());
    stats["properties_count"] = std::to_string(hologram.getProperties().size());
    stats["methods_count"] = std::to_string(hologram.getMethods().size());
    
    return stats;
}

std::string ClassDefinitionExtensions::toToon(const parser::ast::class_definition& class_def) {
    ClassHologram hologram = toHologram(class_def);
    return hologram.toToon();
}

// StructDefinitionExtensions implementation
TypeDefinitionHologram StructDefinitionExtensions::toHologram(const parser::ast::struct_definition& struct_def) {
    TypeDefinitionHologram type_hologram;
    type_hologram.kind = TypeDefinitionHologram::Kind::STRUCT;
    type_hologram.name = struct_def.name.name;
    
    // Extract type parameters
    for (const auto& type_param : struct_def.type_parameters) {
        std::string param_str = type_param.name.name;
        if (!type_param.variance.empty()) {
            param_str = type_param.variance + " " + param_str;
        }
        type_hologram.type_parameters.push_back(param_str);
    }
    
    // Extract fields
    for (const auto& field : struct_def.fields) {
        std::string prefix;
        if (field.is_mutable) {
            prefix = "var ";
        } else if (field.has_explicit_val) {
            prefix = "val ";
        }
        std::string field_str = prefix +
                               field.name.name + ": " + 
                               field.type.type_name.name; // Simplified type extraction
        type_hologram.fields.push_back(field_str);
    }
    
    // Extract properties
    for (const auto& property : struct_def.properties) {
        std::string prefix;
        if (property.is_mutable) {
            prefix = "var ";
        } else if (property.has_explicit_val) {
            prefix = "val ";
        }
        std::string prop_str = prefix +
                              property.name.name + ": " + 
                              property.type.type_name.name; // Simplified type extraction
        
        if (property.has_getter || property.has_setter) {
            prop_str += " { ";
            if (property.has_getter) prop_str += "get ";
            if (property.has_setter) prop_str += "set ";
            prop_str += "}";
        }
        
        type_hologram.properties.push_back(prop_str);
    }
    
    return type_hologram;
}

// FunctionDefinitionExtensions implementation
FunctionSignatureHologram FunctionDefinitionExtensions::toHologram(const parser::ast::function_definition& func_def) {
    FunctionSignatureHologram sig;
    sig.name = func_def.name.name;
    
    // Extract parameters
    for (const auto& param : func_def.parameters) {
        std::string param_str = param.name.name + ": " + param.type.type_name.name;
        if (param.has_default) {
            param_str += " = <default>";
        }
        if (param.is_rest) {
            param_str = "..." + param_str;
        }
        sig.parameters.push_back(param_str);
    }
    
    // Extract return type
    if (func_def.has_return_type) {
        sig.return_type = func_def.return_type.type_name.name;
    }
    
    // Extract named returns
    if (func_def.has_named_returns) {
        for (const auto& named_ret : func_def.named_returns) {
            std::string ret_str = named_ret.name.name + ": " + named_ret.type.type_name.name;
            if (named_ret.has_default) {
                ret_str += " = <default>";
            }
            sig.named_returns.push_back(ret_str);
        }
    }
    
    // Extract effects
    if (func_def.has_effects) {
        for (const auto& effect : func_def.effects_clause) {
            sig.effects.push_back(effect.name);
        }
    }
    
    // Extract contracts (simplified - would need full AST analysis)
    auto contracts = HolographicUtils::extractContracts(func_def);
    sig.require_contract = contracts.first;
    sig.ensure_contract = contracts.second;
    
    return sig;
}

// IRModuleExtensions implementation
ModuleHologram IRModuleExtensions::toHologram(const compiler::ir::Module& ir_module) {
    return HolographicView::fromIR(ir_module);
}

std::unordered_map<std::string, std::string> IRModuleExtensions::getCompressionStats(const compiler::ir::Module& ir_module) {
    ModuleHologram hologram = toHologram(ir_module);
    return HolographicView::getCompressionStats(hologram);
}

std::string IRModuleExtensions::toToon(const compiler::ir::Module& ir_module) {
    ModuleHologram hologram = toHologram(ir_module);
    return hologram.toToon();
}

// ModuleASTExtensions implementation
ModuleHologram ModuleASTExtensions::toHologram(const std::vector<parser::ast::expression>& module_ast,
                                             const std::string& module_name) {
    return HolographicView::fromAST(module_ast, module_name);
}

std::unordered_map<std::string, std::string> ModuleASTExtensions::getCompressionStats(const std::vector<parser::ast::expression>& module_ast,
                                                                                     const std::string& module_name) {
    ModuleHologram hologram = toHologram(module_ast, module_name);
    return HolographicView::getCompressionStats(hologram);
}

std::string ModuleASTExtensions::toToon(const std::vector<parser::ast::expression>& module_ast,
                                      const std::string& module_name) {
    ModuleHologram hologram = toHologram(module_ast, module_name);
    return hologram.toToon();
}

// HolographicUtils implementation
bool HolographicUtils::validateCompression(const std::string& original_source,
                                         const std::string& hologram_source,
                                         double min_compression_ratio) {
    size_t original_tokens = countTokens(original_source);
    size_t hologram_tokens = countTokens(hologram_source);
    
    if (original_tokens == 0) return false;
    
    double compression_ratio = 1.0 - (static_cast<double>(hologram_tokens) / static_cast<double>(original_tokens));
    return compression_ratio >= min_compression_ratio;
}

std::unordered_map<std::string, std::string> HolographicUtils::calculateCompressionStats(const std::string& original_text,
                                                                                        const std::string& compressed_text) {
    size_t original_tokens = countTokens(original_text);
    size_t compressed_tokens = countTokens(compressed_text);
    
    double compression_ratio = 0.0;
    if (original_tokens > 0) {
        compression_ratio = 1.0 - (static_cast<double>(compressed_tokens) / static_cast<double>(original_tokens));
    }
    
    std::unordered_map<std::string, std::string> stats;
    stats["original_tokens"] = std::to_string(original_tokens);
    stats["compressed_tokens"] = std::to_string(compressed_tokens);
    stats["compression_ratio"] = std::to_string(compression_ratio);
    stats["compression_percentage"] = std::to_string(compression_ratio * 100.0) + "%";
    stats["original_size"] = std::to_string(original_text.size());
    stats["compressed_size"] = std::to_string(compressed_text.size());
    stats["size_reduction"] = std::to_string(original_text.size() - compressed_text.size());
    
    return stats;
}

size_t HolographicUtils::countTokens(const std::string& text) {
    return count_tokens_internal(text);
}

std::pair<std::string, std::string> HolographicUtils::extractContracts(const parser::ast::function_definition& func_def) {
    // This is a simplified implementation
    // In a full implementation, we would need to parse the function body
    // and extract require/ensure blocks
    
    std::string require_contract;
    std::string ensure_contract;
    
    // For now, return empty contracts
    // TODO: Implement full contract extraction from function body AST
    
    return std::make_pair(require_contract, ensure_contract);
}

std::vector<std::string> HolographicUtils::extractEffects(const parser::ast::function_definition& func_def) {
    std::vector<std::string> effects;
    
    if (func_def.has_effects) {
        for (const auto& effect : func_def.effects_clause) {
            effects.push_back(effect.name);
        }
    }
    
    return effects;
}

} // namespace meld::api::extensions