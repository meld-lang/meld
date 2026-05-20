#include "meld/api/holographic_view.hpp"
#include "meld/api/meld_binary.hpp"
#include "meld/parser/parser.hpp"
#include "meld/compiler/ast_printer.hpp"
#include <sstream>
#include <algorithm>
#include <regex>
#include <iomanip>
#include <fstream>
#include <cstdio>
#include "meld/compat/visit.hpp"

namespace meld::api {

// Helper function to count tokens in text
size_t count_tokens(const std::string& text) {
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

// Helper function to extract type annotation as string
std::string type_annotation_to_string(const parser::ast::type_annotation& type_ann) {
    std::string result = type_ann.type_name.name;
    
    if (type_ann.has_type_arguments && !type_ann.type_arguments.empty()) {
        result += "<";
        for (size_t i = 0; i < type_ann.type_arguments.size(); ++i) {
            if (i > 0) result += ", ";
            result += type_annotation_to_string(type_ann.type_arguments[i].get());
        }
        result += ">";
    }
    
    if (type_ann.is_nullable) {
        result += "?";
    }
    
    if (type_ann.is_union && !type_ann.union_types.empty()) {
        for (const auto& union_type : type_ann.union_types) {
            result += " | " + type_annotation_to_string(union_type.get());
        }
    }
    
    if (type_ann.is_intersection && !type_ann.intersection_types.empty()) {
        for (const auto& intersection_type : type_ann.intersection_types) {
            result += " & " + type_annotation_to_string(intersection_type.get());
        }
    }
    
    return result;
}

// Helper function to extract parameter signature
std::string parameter_to_string(const parser::ast::function_parameter& param) {
    std::string result = param.name.name + ": " + type_annotation_to_string(param.type);
    
    if (param.has_default) {
        result += " = <default>";
    }
    
    if (param.is_rest) {
        result = "..." + result;
    }
    
    return result;
}

// FunctionSignatureHologram implementation
std::string FunctionSignatureHologram::to_string() const {
    std::ostringstream oss;
    
    if (is_extension) {
        oss << "extend " << extension_target << " {\n    ";
    }
    
    oss << "fnc " << name << "(";
    
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << parameters[i];
    }
    
    oss << ")";
    
    if (!return_type.empty()) {
        oss << " -> " << return_type;
    }
    
    if (!named_returns.empty()) {
        oss << " -> (";
        for (size_t i = 0; i < named_returns.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << named_returns[i];
        }
        oss << ")";
    }
    
    if (!effects.empty()) {
        oss << "\n        effects { ";
        for (size_t i = 0; i < effects.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << effects[i];
        }
        oss << " }";
    }
    
    if (!require_contract.empty()) {
        oss << "\n        require { " << require_contract << " }";
    }
    
    if (!ensure_contract.empty()) {
        oss << "\n        ensure { " << ensure_contract << " }";
    }
    
    if (is_extension) {
        oss << "\n}";
    }
    
    return oss.str();
}

// TypeDefinitionHologram implementation
std::string TypeDefinitionHologram::to_string() const {
    std::ostringstream oss;
    
    switch (kind) {
        case Kind::STRUCT:
            oss << "struct " << name;
            break;
        case Kind::CLASS:
            oss << "class " << name;
            break;
        case Kind::TRAIT:
            oss << "trait " << name;
            break;
        case Kind::ENUM:
            oss << "enum " << name;
            break;
        case Kind::TYPE_ALIAS:
            oss << "type " << name << " -> " << base_type;
            return oss.str();
        case Kind::REFINEMENT_TYPE:
            oss << "type " << name << " -> " << base_type << " where { " << refinement_predicate << " }";
            return oss.str();
    }
    
    if (!type_parameters.empty()) {
        oss << "<";
        for (size_t i = 0; i < type_parameters.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << type_parameters[i];
        }
        oss << ">";
    }
    
    if (!traits.empty()) {
        oss << " : ";
        for (size_t i = 0; i < traits.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << traits[i];
        }
    }
    
    oss << " {\n";
    
    // Add fields
    for (const auto& field : fields) {
        oss << "    " << field << "\n";
    }
    
    // Add properties
    for (const auto& property : properties) {
        oss << "    " << property << "\n";
    }
    
    // Add method signatures
    for (const auto& method : methods) {
        oss << "    " << method.to_string() << "\n";
    }
    
    oss << "}";
    
    return oss.str();
}

// ImportExportHologram implementation
std::string ImportExportHologram::to_string() const {
    std::ostringstream oss;
    
    switch (kind) {
        case Kind::IMPORT_SPECIFIC:
            oss << "import ";
            for (size_t i = 0; i < namespace_path.size(); ++i) {
                if (i > 0) oss << ".";
                oss << namespace_path[i];
            }
            break;
        case Kind::IMPORT_WILDCARD:
            oss << "import ";
            for (size_t i = 0; i < namespace_path.size(); ++i) {
                if (i > 0) oss << ".";
                oss << namespace_path[i];
            }
            oss << ".*";
            break;
        case Kind::IMPORT_ALIASED:
            oss << "import ";
            for (size_t i = 0; i < namespace_path.size(); ++i) {
                if (i > 0) oss << ".";
                oss << namespace_path[i];
            }
            oss << " as " << alias;
            break;
        case Kind::EXPORT_SPECIFIC:
            oss << "export ";
            for (size_t i = 0; i < exported_symbols.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << exported_symbols[i];
            }
            break;
        case Kind::EXPORT_WILDCARD:
            oss << "export *";
            break;
    }
    
    return oss.str();
}

// ClassHologram implementation
ClassHologram::ClassHologram(const parser::ast::class_definition& class_def) 
    : name_(class_def.name.name), original_token_count_(0) {
    
    // Extract type parameters
    for (const auto& type_param : class_def.type_parameters) {
        std::string param_str = type_param.name.name;
        if (!type_param.variance.empty()) {
            param_str = type_param.variance + " " + param_str;
        }
        type_parameters_.push_back(param_str);
    }
    
    extractFields(class_def);
    extractProperties(class_def);
    extractMethods(class_def);
    
    original_token_count_ = calculateOriginalTokens(class_def);
}

void ClassHologram::extractFields(const parser::ast::class_definition& class_def) {
    for (const auto& field : class_def.fields) {
        std::string prefix;
        if (field.is_mutable) {
            prefix = "var ";
        } else if (field.has_explicit_val) {
            prefix = "val ";
        }
        std::string field_str = prefix +
                               field.name.name + ": " + 
                               type_annotation_to_string(field.type);
        fields_.push_back(field_str);
    }
}

void ClassHologram::extractProperties(const parser::ast::class_definition& class_def) {
    for (const auto& property : class_def.properties) {
        std::string prop_str = (property.is_mutable ? "var " : "val ") + 
                              property.name.name + ": " + 
                              type_annotation_to_string(property.type);
        
        if (property.has_getter || property.has_setter) {
            prop_str += " { ";
            if (property.has_getter) prop_str += "get ";
            if (property.has_setter) prop_str += "set ";
            prop_str += "}";
        }
        
        properties_.push_back(prop_str);
    }
}

void ClassHologram::extractMethods(const parser::ast::class_definition& class_def) {
    // Note: In the current AST structure, methods are not directly part of class_definition
    // This would need to be extended when method definitions are added to classes
    // For now, we'll leave this empty and methods would be extracted separately
}

FunctionSignatureHologram ClassHologram::extractFunctionSignature(const parser::ast::function_definition& func_def) {
    FunctionSignatureHologram sig;
    sig.name = func_def.name.name;
    
    // Extract parameters
    for (const auto& param : func_def.parameters) {
        sig.parameters.push_back(parameter_to_string(param));
    }
    
    // Extract return type
    if (func_def.has_return_type) {
        sig.return_type = type_annotation_to_string(func_def.return_type);
    }
    
    // Extract named returns
    if (func_def.has_named_returns) {
        for (const auto& named_ret : func_def.named_returns) {
            std::string ret_str = named_ret.name.name + ": " + type_annotation_to_string(named_ret.type);
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
    
    return sig;
}

size_t ClassHologram::calculateOriginalTokens(const parser::ast::class_definition& class_def) {
    // This is a simplified token count - in a real implementation,
    // we would use the AST printer to generate the full source and count tokens
    size_t count = 0;
    
    // Class declaration
    count += 2; // "class" + name
    count += class_def.type_parameters.size() * 2; // type parameters
    count += 2; // "{" and "}"
    
    // Fields
    count += class_def.fields.size() * 4; // var/val + name + ":" + type
    
    // Properties  
    count += class_def.properties.size() * 4; // var/val + name + ":" + type
    
    // Add estimated tokens for property getters/setters
    for (const auto& prop : class_def.properties) {
        if (prop.has_getter) count += 10; // Estimated getter body tokens
        if (prop.has_setter) count += 10; // Estimated setter body tokens
    }
    
    return count;
}

std::string ClassHologram::toHologram() const {
    std::ostringstream oss;
    
    oss << "class " << name_;
    
    if (!type_parameters_.empty()) {
        oss << "<";
        for (size_t i = 0; i < type_parameters_.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << type_parameters_[i];
        }
        oss << ">";
    }
    
    oss << " {\n";
    
    // Add fields
    for (const auto& field : fields_) {
        oss << "    " << field << "\n";
    }
    
    // Add properties
    for (const auto& property : properties_) {
        oss << "    " << property << "\n";
    }
    
    // Add method signatures
    for (const auto& method : methods_) {
        oss << "    " << method.to_string() << "\n";
    }
    
    oss << "}";
    
    return oss.str();
}

size_t ClassHologram::getHologramTokenCount() const {
    return count_tokens(toHologram());
}

double ClassHologram::getCompressionRatio() const {
    if (original_token_count_ == 0) return 0.0;
    return 1.0 - (static_cast<double>(getHologramTokenCount()) / static_cast<double>(original_token_count_));
}

std::string ClassHologram::toToon() const {
    std::ostringstream oss;
    
    // Class metadata
    oss << "name: " << name_ << "\n";
    oss << "type_parameters_count: " << type_parameters_.size() << "\n";
    oss << "fields_count: " << fields_.size() << "\n";
    oss << "properties_count: " << properties_.size() << "\n";
    oss << "methods_count: " << methods_.size() << "\n";
    
    // Type parameters as inline array
    if (!type_parameters_.empty()) {
        oss << "type_parameters[" << type_parameters_.size() << "]: ";
        for (size_t i = 0; i < type_parameters_.size(); ++i) {
            if (i > 0) oss << ",";
            // Quote if contains spaces or special characters
            if (type_parameters_[i].find(' ') != std::string::npos || 
                type_parameters_[i].find(',') != std::string::npos) {
                oss << "\"" << type_parameters_[i] << "\"";
            } else {
                oss << type_parameters_[i];
            }
        }
        oss << "\n";
    }
    
    // Fields as tabular array
    if (!fields_.empty()) {
        oss << "fields[" << fields_.size() << "]{declaration}:\n";
        for (const auto& field : fields_) {
            // Quote field declarations since they contain colons and spaces
            oss << "  \"" << field << "\"\n";
        }
    }
    
    // Properties as tabular array
    if (!properties_.empty()) {
        oss << "properties[" << properties_.size() << "]{declaration}:\n";
        for (const auto& property : properties_) {
            // Quote property declarations since they contain colons and spaces
            oss << "  \"" << property << "\"\n";
        }
    }
    
    // Methods as tabular array
    if (!methods_.empty()) {
        oss << "methods[" << methods_.size() << "]{name,param_count,return_type,effects_count,has_contracts}:\n";
        for (const auto& method : methods_) {
            std::string return_type = method.return_type.empty() ? "void" : method.return_type;
            bool has_contracts = !method.require_contract.empty() || !method.ensure_contract.empty();
            
            oss << "  " << method.name << "," << method.parameters.size() << "," 
                << return_type << "," << method.effects.size() << "," 
                << (has_contracts ? "true" : "false") << "\n";
        }
    }
    
    return oss.str();
}

// ModuleHologram implementation
ModuleHologram::ModuleHologram(const std::string& module_name) 
    : name_(module_name), original_token_count_(0) {
}

ModuleHologram::ModuleHologram(const std::vector<parser::ast::expression>& module_ast) 
    : name_(""), original_token_count_(0) {
    processModuleAST(module_ast);
}

void ModuleHologram::addTypeDefinition(const TypeDefinitionHologram& type_def) {
    type_definitions_.push_back(type_def);
}

void ModuleHologram::addFunctionSignature(const FunctionSignatureHologram& func_sig) {
    function_signatures_.push_back(func_sig);
}

void ModuleHologram::addImportExport(const ImportExportHologram& import_export) {
    import_exports_.push_back(import_export);
}

void ModuleHologram::addClass(const ClassHologram& class_hologram) {
    classes_.push_back(class_hologram);
}

void ModuleHologram::processModuleAST(const std::vector<parser::ast::expression>& module_ast) {
    for (const auto& expr : module_ast) {
        processExpression(expr);
    }
    
    // Calculate original token count (simplified)
    original_token_count_ = module_ast.size() * 50; // Rough estimate
}

void ModuleHologram::processExpression(const parser::ast::expression& expr) {
    meld::compat::visit([this](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        
        if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::namespace_declaration>>) {
            processNamespaceDeclaration(node.get());
        } else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::import_declaration>>) {
            processImportDeclaration(node.get());
        } else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::struct_definition>>) {
            processStructDefinition(node.get());
        } else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::class_definition>>) {
            processClassDefinition(node.get());
        } else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::function_definition>>) {
            processFunctionDefinition(node.get());
        } else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::typealias_declaration>>) {
            processTypeAliasDeclaration(node.get());
        } else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::refinement_type_definition>>) {
            processRefinementTypeDefinition(node.get());
        }
        // Add other expression types as needed
    }, expr);
}

void ModuleHologram::processNamespaceDeclaration(const parser::ast::namespace_declaration& ns_decl) {
    // Process nested expressions within the namespace
    for (const auto& expr : ns_decl.body) {
        processExpression(expr);
    }
}

void ModuleHologram::processImportDeclaration(const parser::ast::import_declaration& import_decl) {
    ImportExportHologram import_hologram = extractImportExport(import_decl);
    addImportExport(import_hologram);
}

void ModuleHologram::processStructDefinition(const parser::ast::struct_definition& struct_def) {
    TypeDefinitionHologram type_hologram = extractTypeDefinition(struct_def);
    addTypeDefinition(type_hologram);
}

void ModuleHologram::processClassDefinition(const parser::ast::class_definition& class_def) {
    ClassHologram class_hologram(class_def);
    addClass(class_hologram);
    
    // Also add as type definition
    TypeDefinitionHologram type_hologram = extractTypeDefinition(class_def);
    addTypeDefinition(type_hologram);
}

void ModuleHologram::processFunctionDefinition(const parser::ast::function_definition& func_def) {
    FunctionSignatureHologram func_hologram = extractFunctionSignature(func_def);
    addFunctionSignature(func_hologram);
}

void ModuleHologram::processTypeAliasDeclaration(const parser::ast::typealias_declaration& alias_decl) {
    TypeDefinitionHologram type_hologram;
    type_hologram.kind = TypeDefinitionHologram::Kind::TYPE_ALIAS;
    type_hologram.name = alias_decl.alias_name.name;
    type_hologram.base_type = type_annotation_to_string(alias_decl.target_type);
    addTypeDefinition(type_hologram);
}

void ModuleHologram::processRefinementTypeDefinition(const parser::ast::refinement_type_definition& refinement_def) {
    TypeDefinitionHologram type_hologram;
    type_hologram.kind = TypeDefinitionHologram::Kind::REFINEMENT_TYPE;
    type_hologram.name = refinement_def.name.name;
    type_hologram.base_type = type_annotation_to_string(refinement_def.base_type);
    // Note: refinement_predicate extraction would need AST printer
    type_hologram.refinement_predicate = "<predicate>";
    addTypeDefinition(type_hologram);
}

FunctionSignatureHologram ModuleHologram::extractFunctionSignature(const parser::ast::function_definition& func_def) {
    FunctionSignatureHologram sig;
    sig.name = func_def.name.name;
    
    // Extract parameters
    for (const auto& param : func_def.parameters) {
        sig.parameters.push_back(parameter_to_string(param));
    }
    
    // Extract return type
    if (func_def.has_return_type) {
        sig.return_type = type_annotation_to_string(func_def.return_type);
    }
    
    // Extract named returns
    if (func_def.has_named_returns) {
        for (const auto& named_ret : func_def.named_returns) {
            std::string ret_str = named_ret.name.name + ": " + type_annotation_to_string(named_ret.type);
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
    
    return sig;
}

TypeDefinitionHologram ModuleHologram::extractTypeDefinition(const parser::ast::struct_definition& struct_def) {
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
                               type_annotation_to_string(field.type);
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
                              type_annotation_to_string(property.type);
        
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

TypeDefinitionHologram ModuleHologram::extractTypeDefinition(const parser::ast::class_definition& class_def) {
    TypeDefinitionHologram type_hologram;
    type_hologram.kind = TypeDefinitionHologram::Kind::CLASS;
    type_hologram.name = class_def.name.name;
    
    // Extract type parameters
    for (const auto& type_param : class_def.type_parameters) {
        std::string param_str = type_param.name.name;
        if (!type_param.variance.empty()) {
            param_str = type_param.variance + " " + param_str;
        }
        type_hologram.type_parameters.push_back(param_str);
    }
    
    // Extract fields
    for (const auto& field : class_def.fields) {
        std::string prefix;
        if (field.is_mutable) {
            prefix = "var ";
        } else if (field.has_explicit_val) {
            prefix = "val ";
        }
        std::string field_str = prefix +
                               field.name.name + ": " + 
                               type_annotation_to_string(field.type);
        type_hologram.fields.push_back(field_str);
    }
    
    // Extract properties
    for (const auto& property : class_def.properties) {
        std::string prefix;
        if (property.is_mutable) {
            prefix = "var ";
        } else if (property.has_explicit_val) {
            prefix = "val ";
        }
        std::string prop_str = prefix +
                              property.name.name + ": " + 
                              type_annotation_to_string(property.type);
        
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

ImportExportHologram ModuleHologram::extractImportExport(const parser::ast::import_declaration& import_decl) {
    ImportExportHologram import_hologram;
    
    switch (import_decl.import_type) {
        case parser::ast::ImportType::SPECIFIC:
            import_hologram.kind = ImportExportHologram::Kind::IMPORT_SPECIFIC;
            break;
        case parser::ast::ImportType::WILDCARD:
            import_hologram.kind = ImportExportHologram::Kind::IMPORT_WILDCARD;
            break;
        case parser::ast::ImportType::ALIASED:
            import_hologram.kind = ImportExportHologram::Kind::IMPORT_ALIASED;
            import_hologram.alias = import_decl.alias;
            break;
        default:
            // IMP_BASIC, IMP_ALIASED, IMP_DESTRUCTURED use the new imp syntax
            import_hologram.kind = ImportExportHologram::Kind::IMPORT_SPECIFIC;
            break;
    }
    
    import_hologram.namespace_path = import_decl.namespace_path;
    
    return import_hologram;
}

std::string ModuleHologram::toHologram() const {
    std::ostringstream oss;
    
    // Add module name if present
    if (!name_.empty()) {
        oss << "// Module: " << name_ << "\n\n";
    }
    
    // Add imports/exports
    for (const auto& import_export : import_exports_) {
        oss << import_export.to_string() << "\n";
    }
    
    if (!import_exports_.empty()) {
        oss << "\n";
    }
    
    // Add type definitions
    for (const auto& type_def : type_definitions_) {
        oss << type_def.to_string() << "\n\n";
    }
    
    // Add function signatures
    for (const auto& func_sig : function_signatures_) {
        oss << func_sig.to_string() << "\n\n";
    }
    
    // Add class holograms
    for (const auto& class_hologram : classes_) {
        oss << class_hologram.toHologram() << "\n\n";
    }
    
    return oss.str();
}

size_t ModuleHologram::getHologramTokenCount() const {
    return count_tokens(toHologram());
}

double ModuleHologram::getCompressionRatio() const {
    if (original_token_count_ == 0) return 0.0;
    return 1.0 - (static_cast<double>(getHologramTokenCount()) / static_cast<double>(original_token_count_));
}

std::string ModuleHologram::toMeldSource() const {
    return toHologram(); // Same as hologram for now
}

std::string ModuleHologram::toJson() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"name\": \"" << name_ << "\",\n";
    oss << "  \"original_token_count\": " << original_token_count_ << ",\n";
    oss << "  \"hologram_token_count\": " << getHologramTokenCount() << ",\n";
    oss << "  \"compression_ratio\": " << std::fixed << std::setprecision(3) << getCompressionRatio() << ",\n";
    oss << "  \"type_definitions\": [\n";
    
    for (size_t i = 0; i < type_definitions_.size(); ++i) {
        if (i > 0) oss << ",\n";
        oss << "    \"" << type_definitions_[i].name << "\"";
    }
    
    oss << "\n  ],\n";
    oss << "  \"function_signatures\": [\n";
    
    for (size_t i = 0; i < function_signatures_.size(); ++i) {
        if (i > 0) oss << ",\n";
        oss << "    \"" << function_signatures_[i].name << "\"";
    }
    
    oss << "\n  ]\n";
    oss << "}";
    
    return oss.str();
}

std::string ModuleHologram::toToon() const {
    std::ostringstream oss;
    
    // Module metadata
    oss << "name: " << (name_.empty() ? "unnamed" : name_) << "\n";
    oss << "original_token_count: " << original_token_count_ << "\n";
    oss << "hologram_token_count: " << getHologramTokenCount() << "\n";
    oss << "compression_ratio: " << std::fixed << std::setprecision(3) << getCompressionRatio() << "\n";
    
    // Import/Export relationships as tabular array
    if (!import_exports_.empty()) {
        oss << "imports[" << import_exports_.size() << "]{type,path,alias}:\n";
        for (const auto& import_export : import_exports_) {
            std::string type_str;
            switch (import_export.kind) {
                case ImportExportHologram::Kind::IMPORT_SPECIFIC:
                    type_str = "specific";
                    break;
                case ImportExportHologram::Kind::IMPORT_WILDCARD:
                    type_str = "wildcard";
                    break;
                case ImportExportHologram::Kind::IMPORT_ALIASED:
                    type_str = "aliased";
                    break;
                case ImportExportHologram::Kind::EXPORT_SPECIFIC:
                    type_str = "export_specific";
                    break;
                case ImportExportHologram::Kind::EXPORT_WILDCARD:
                    type_str = "export_wildcard";
                    break;
            }
            
            std::string path_str;
            for (size_t i = 0; i < import_export.namespace_path.size(); ++i) {
                if (i > 0) path_str += ".";
                path_str += import_export.namespace_path[i];
            }
            
            std::string alias_str = import_export.alias.empty() ? "null" : import_export.alias;
            
            oss << "  " << type_str << "," << path_str << "," << alias_str << "\n";
        }
    }
    
    // Type definitions as tabular array
    if (!type_definitions_.empty()) {
        oss << "types[" << type_definitions_.size() << "]{name,kind,fields_count,properties_count}:\n";
        for (const auto& type_def : type_definitions_) {
            std::string kind_str;
            switch (type_def.kind) {
                case TypeDefinitionHologram::Kind::STRUCT:
                    kind_str = "struct";
                    break;
                case TypeDefinitionHologram::Kind::CLASS:
                    kind_str = "class";
                    break;
                case TypeDefinitionHologram::Kind::TRAIT:
                    kind_str = "trait";
                    break;
                case TypeDefinitionHologram::Kind::ENUM:
                    kind_str = "enum";
                    break;
                case TypeDefinitionHologram::Kind::TYPE_ALIAS:
                    kind_str = "type_alias";
                    break;
                case TypeDefinitionHologram::Kind::REFINEMENT_TYPE:
                    kind_str = "refinement";
                    break;
            }
            
            oss << "  " << type_def.name << "," << kind_str << "," 
                << type_def.fields.size() << "," << type_def.properties.size() << "\n";
        }
    }
    
    // Function signatures as tabular array
    if (!function_signatures_.empty()) {
        oss << "functions[" << function_signatures_.size() << "]{name,param_count,return_type,effects_count}:\n";
        for (const auto& func_sig : function_signatures_) {
            std::string return_type = func_sig.return_type.empty() ? "void" : func_sig.return_type;
            
            oss << "  " << func_sig.name << "," << func_sig.parameters.size() << "," 
                << return_type << "," << func_sig.effects.size() << "\n";
        }
    }
    
    // Classes as nested objects with tabular method arrays
    if (!classes_.empty()) {
        oss << "classes[" << classes_.size() << "]:\n";
        for (const auto& class_hologram : classes_) {
            oss << "  - name: " << class_hologram.getName() << "\n";
            oss << "    fields_count: " << class_hologram.getFields().size() << "\n";
            oss << "    properties_count: " << class_hologram.getProperties().size() << "\n";
            oss << "    methods_count: " << class_hologram.getMethods().size() << "\n";
            
            // Methods as tabular array within the class
            if (!class_hologram.getMethods().empty()) {
                oss << "    methods[" << class_hologram.getMethods().size() << "]{name,param_count,return_type}:\n";
                for (const auto& method : class_hologram.getMethods()) {
                    std::string return_type = method.return_type.empty() ? "void" : method.return_type;
                    oss << "      " << method.name << "," << method.parameters.size() << "," << return_type << "\n";
                }
            }
        }
    }
    
    return oss.str();
}

size_t ModuleHologram::calculateTokenCount(const std::string& text) const {
    return count_tokens(text);
}

// HolographicView implementation
ModuleHologram HolographicView::fromSource(const std::string& source_code, const std::string& module_name) {
    // Parse the source code
    parser::Parser parser;
    std::vector<parser::ast::expression> expressions;
    
    bool parse_success = parser.parse_file(source_code, expressions);
    
    if (!parse_success) {
        // Return empty hologram on parse error
        ModuleHologram hologram(module_name);
        hologram.original_token_count_ = count_tokens(source_code);
        return hologram;
    }
    
    ModuleHologram hologram(expressions);
    if (!module_name.empty()) {
        hologram.name_ = module_name;
    }
    
    // Set original token count based on source
    hologram.original_token_count_ = count_tokens(source_code);
    
    return hologram;
}

ModuleHologram HolographicView::fromAST(const std::vector<parser::ast::expression>& module_ast, const std::string& module_name) {
    ModuleHologram hologram(module_ast);
    if (!module_name.empty()) {
        hologram.name_ = module_name;
    }
    return hologram;
}

ModuleHologram HolographicView::fromIR(const compiler::ir::Module& ir_module) {
    ModuleHologram hologram(ir_module.name);
    
    // Extract function signatures from IR
    for (const auto& func : ir_module.functions) {
        FunctionSignatureHologram sig;
        sig.name = func->name;
        
        // Extract parameters from IR function
        for (const auto& param : func->parameters) {
            sig.parameters.push_back(param->name + ": " + "unknown"); // IR doesn't preserve full type info
        }
        
        // Extract return type
        if (func->return_value) {
            sig.return_type = "unknown"; // IR doesn't preserve full type info
        }
        
        hologram.addFunctionSignature(sig);
    }
    
    // Extract type definitions from IR
    for (const auto& [type_name, meta_type] : ir_module.types) {
        TypeDefinitionHologram type_def;
        type_def.name = type_name;
        type_def.kind = TypeDefinitionHologram::Kind::STRUCT; // Default assumption
        hologram.addTypeDefinition(type_def);
    }
    
    return hologram;
}

ClassHologram HolographicView::fromClass(const parser::ast::class_definition& class_def) {
    return ClassHologram(class_def);
}

bool HolographicView::validateCompressionRatio(const ModuleHologram& hologram, double min_compression_ratio) {
    return hologram.getCompressionRatio() >= min_compression_ratio;
}

std::unordered_map<std::string, std::string> HolographicView::getCompressionStats(const ModuleHologram& hologram) {
    std::unordered_map<std::string, std::string> stats;
    
    stats["module_name"] = hologram.getName();
    stats["original_tokens"] = std::to_string(hologram.getOriginalTokenCount());
    stats["hologram_tokens"] = std::to_string(hologram.getHologramTokenCount());
    stats["compression_ratio"] = std::to_string(hologram.getCompressionRatio());
    stats["compression_percentage"] = std::to_string(hologram.getCompressionRatio() * 100.0) + "%";
    stats["type_definitions_count"] = std::to_string(hologram.getTypeDefinitions().size());
    stats["function_signatures_count"] = std::to_string(hologram.getFunctionSignatures().size());
    stats["classes_count"] = std::to_string(hologram.getClasses().size());
    stats["import_exports_count"] = std::to_string(hologram.getImportExports().size());
    
    return stats;
}

// ModuleHologram toMeldB implementation
std::vector<uint8_t> ModuleHologram::toMeldB() const {
    // Create a MeldBinary from this hologram
    auto binary = MeldBinary::fromHologram(*this);
    
    // Serialize to binary format
    std::vector<uint8_t> buffer;
    
    // Create a temporary file path for serialization
    std::string temp_path = "/tmp/hologram_" + std::to_string(reinterpret_cast<uintptr_t>(this)) + ".mldb";
    binary->save(temp_path);
    
    // Read the serialized data back
    std::ifstream file(temp_path, std::ios::binary);
    if (file.is_open()) {
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        buffer.resize(file_size);
        file.read(reinterpret_cast<char*>(buffer.data()), file_size);
        
        // Clean up temporary file
        std::remove(temp_path.c_str());
    }
    
    return buffer;
}

// ClassHologram toMeldB implementation
std::vector<uint8_t> ClassHologram::toMeldB() const {
    // Convert class hologram to module hologram for binary serialization
    ModuleHologram module_hologram(name_);
    
    // Add class as a type definition
    TypeDefinitionHologram type_def;
    type_def.name = name_;
    type_def.kind = TypeDefinitionHologram::Kind::CLASS;
    type_def.fields = fields_;
    type_def.properties = properties_;
    
    // Convert method signatures
    for (const auto& method : methods_) {
        FunctionSignatureHologram func_sig;
        func_sig.name = method.name;
        func_sig.parameters = method.parameters;
        func_sig.return_type = method.return_type;
        func_sig.effects = method.effects;
        module_hologram.addFunctionSignature(func_sig);
    }
    
    module_hologram.addTypeDefinition(type_def);
    
    // Set token counts for compression calculation
    module_hologram.original_token_count_ = original_token_count_;
    
    return module_hologram.toMeldB();
}

} // namespace meld::api