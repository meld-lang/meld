#include "meld/types/collection_literal_inference.hpp"
#include "meld/kernel/primitives.hpp"
#include <algorithm>
#include <sstream>

namespace meld::types {

using namespace parser::ast;
using namespace meta;

// Infer type for curly brace syntax
CollectionInferenceResult CollectionLiteralInference::inferCurlyBraces(
    const std::vector<anonymous_object_field>& fields) {
    
    // Empty collection requires annotation
    if (fields.empty()) {
        return CollectionInferenceResult(
            CollectionLiteralKind::EMPTY_COLLECTION,
            nullptr,
            true,
            "Empty collection requires type annotation"
        );
    }
    
    // Check if all field values have the same type
    if (areFieldTypesHomogeneous(fields)) {
        // Homogeneous → Map<String, T>
        auto valueType = getCommonFieldType(fields);
        auto mapType = std::make_shared<AnonymousMapType>(valueType);
        return CollectionInferenceResult(
            CollectionLiteralKind::ANONYMOUS_MAP,
            mapType
        );
    } else {
        // Heterogeneous → Anonymous Object
        std::vector<AnonymousObjectField> objFields;
        for (const auto& field : fields) {
            auto fieldType = inferExpressionType(field.value);
            objFields.emplace_back(field.field_name.name, fieldType);
        }
        auto objType = std::make_shared<AnonymousObjectType>(objFields);
        return CollectionInferenceResult(
            CollectionLiteralKind::ANONYMOUS_OBJECT,
            objType
        );
    }
}

// Infer type for square bracket syntax
CollectionInferenceResult CollectionLiteralInference::inferSquareBrackets(
    const std::vector<expression>& elements) {
    
    // Empty collection requires annotation
    if (elements.empty()) {
        return CollectionInferenceResult(
            CollectionLiteralKind::EMPTY_COLLECTION,
            nullptr,
            true,
            "Empty collection requires type annotation"
        );
    }
    
    // Check if all elements have the same type
    if (areTypesHomogeneous(elements)) {
        // Homogeneous → Array<T>
        auto elementType = getCommonType(elements);
        auto arrayType = std::make_shared<AnonymousArrayType>(elementType);
        return CollectionInferenceResult(
            CollectionLiteralKind::ANONYMOUS_ARRAY,
            arrayType
        );
    } else {
        // Heterogeneous → Tuple
        std::vector<AnonymousTupleElement> tuple_elements;
        for (size_t i = 0; i < elements.size(); ++i) {
            auto element_type = inferExpressionType(elements[i]);
            if (!element_type) {
                return CollectionInferenceResult(
                    CollectionLiteralKind::ANONYMOUS_TUPLE,
                    nullptr,
                    false,
                    "Could not infer type for tuple element " + std::to_string(i)
                );
            }
            tuple_elements.emplace_back(i, element_type);
        }
        auto tuple_type = std::make_shared<AnonymousTupleType>(tuple_elements);
        return CollectionInferenceResult(
            CollectionLiteralKind::ANONYMOUS_TUPLE,
            tuple_type,
            false,
            ""
        );
    }
}

// Infer type for square bracket syntax with named elements
CollectionInferenceResult CollectionLiteralInference::inferSquareBracketsNamed(
    const std::vector<tuple_element>& elements) {
    
    // Named elements always create a tuple
    std::vector<AnonymousTupleElement> tuple_elements;
    for (size_t i = 0; i < elements.size(); ++i) {
        auto element_type = inferExpressionType(elements[i].value);
        if (!element_type) {
            return CollectionInferenceResult(
                CollectionLiteralKind::ANONYMOUS_TUPLE,
                nullptr,
                false,
                "Could not infer type for named tuple element " + elements[i].name
            );
        }
        tuple_elements.emplace_back(i, element_type, elements[i].name);
    }
    auto tuple_type = std::make_shared<AnonymousTupleType>(tuple_elements);
    return CollectionInferenceResult(
        CollectionLiteralKind::ANONYMOUS_TUPLE,
        tuple_type,
        false,
        ""
    );
}

// Check if all expressions have the same type
bool CollectionLiteralInference::areTypesHomogeneous(const std::vector<expression>& exprs) {
    if (exprs.empty()) {
        return true;
    }
    
    auto firstType = inferExpressionType(exprs[0]);
    if (!firstType) {
        return false;
    }
    
    for (size_t i = 1; i < exprs.size(); ++i) {
        auto currentType = inferExpressionType(exprs[i]);
        if (!currentType || currentType->name() != firstType->name()) {
            return false;
        }
    }
    
    return true;
}

// Check if all fields have the same value type
bool CollectionLiteralInference::areFieldTypesHomogeneous(
    const std::vector<anonymous_object_field>& fields) {
    
    if (fields.empty()) {
        return true;
    }
    
    auto firstType = inferExpressionType(fields[0].value);
    if (!firstType) {
        return false;
    }
    
    for (size_t i = 1; i < fields.size(); ++i) {
        auto currentType = inferExpressionType(fields[i].value);
        if (!currentType || currentType->name() != firstType->name()) {
            return false;
        }
    }
    
    return true;
}

// Get the common type from a list of expressions
std::shared_ptr<MetaType> CollectionLiteralInference::getCommonType(
    const std::vector<expression>& exprs) {
    
    if (exprs.empty()) {
        return nullptr;
    }
    
    return inferExpressionType(exprs[0]);
}

// Get the common value type from fields
std::shared_ptr<MetaType> CollectionLiteralInference::getCommonFieldType(
    const std::vector<anonymous_object_field>& fields) {
    
    if (fields.empty()) {
        return nullptr;
    }
    
    return inferExpressionType(fields[0].value);
}

// Infer type from a single expression
std::shared_ptr<MetaType> CollectionLiteralInference::inferExpressionType(
    const expression& expr) {
    
    // Check for integer literal
    if (auto* int_lit = boost::get<integer_literal>(&expr)) {
        return std::make_shared<meta::PrimitiveMetaType>("Int", 8);
    }
    
    // Check for float literal
    if (auto* float_lit = boost::get<float_literal>(&expr)) {
        return std::make_shared<meta::PrimitiveMetaType>("Float", 8);
    }
    
    // Check for string literal
    if (auto* str_lit = boost::get<string_literal>(&expr)) {
        return std::make_shared<meta::PrimitiveMetaType>("String", 32);
    }
    
    // Check for boolean literal
    if (auto* bool_lit = boost::get<boolean_literal>(&expr)) {
        return std::make_shared<meta::PrimitiveMetaType>("Bool", 1);
    }
    
    // Check for identifier (would need symbol table lookup)
    if (auto* id = boost::get<identifier>(&expr)) {
        // TODO: Look up identifier type in symbol table
        return nullptr;
    }
    
    // For other expression types, return nullptr for now
    return nullptr;
}

// Helper to check if a type is a primitive type
bool CollectionLiteralInference::isPrimitiveType(const std::shared_ptr<MetaType>& type) {
    if (!type) {
        return false;
    }
    
    const std::string& name = type->name();
    return name == "Int" || name == "Float" || name == "Double" || 
           name == "String" || name == "Bool";
}

// Type annotation resolver implementation

std::shared_ptr<MetaType> TypeAnnotationResolver::resolveAnnotation(
    const std::string& type_annotation) {
    
    // Parse type annotation string
    // Examples: "Map<String, Int>", "Set<String>", "Array<Int>"
    
    if (type_annotation.find("Map<") == 0) {
        // Extract value type from Map<String, T>
        auto valueType = extractElementType(type_annotation);
        return std::make_shared<AnonymousMapType>(valueType);
    }
    
    if (type_annotation.find("Set<") == 0) {
        // Extract element type from Set<T>
        auto elementType = extractElementType(type_annotation);
        return std::make_shared<AnonymousSetType>(elementType);
    }
    
    if (type_annotation.find("Array<") == 0) {
        // Extract element type from Array<T>
        auto elementType = extractElementType(type_annotation);
        return std::make_shared<AnonymousArrayType>(elementType);
    }
    
    return nullptr;
}

bool TypeAnnotationResolver::isCollectionType(const std::string& type_annotation) {
    return type_annotation.find("Map<") == 0 ||
           type_annotation.find("Set<") == 0 ||
           type_annotation.find("Array<") == 0;
}

std::shared_ptr<MetaType> TypeAnnotationResolver::extractElementType(
    const std::string& type_annotation) {
    
    // Simple extraction for now
    // Map<String, Int> → Int
    // Set<String> → String
    // Array<Int> → Int
    
    size_t start = type_annotation.find('<');
    size_t end = type_annotation.rfind('>');
    
    if (start == std::string::npos || end == std::string::npos) {
        return nullptr;
    }
    
    std::string inner = type_annotation.substr(start + 1, end - start - 1);
    
    // For Map, extract the value type (after comma)
    if (type_annotation.find("Map<") == 0) {
        size_t comma = inner.find(',');
        if (comma != std::string::npos) {
            inner = inner.substr(comma + 1);
            // Trim whitespace
            inner.erase(0, inner.find_first_not_of(" \t"));
            inner.erase(inner.find_last_not_of(" \t") + 1);
        }
    }
    
    // Create primitive type based on name
    if (inner == "Int") {
        return std::make_shared<meta::PrimitiveMetaType>("Int", 8);
    } else if (inner == "String") {
        return std::make_shared<meta::PrimitiveMetaType>("String", 32);
    } else if (inner == "Bool") {
        return std::make_shared<meta::PrimitiveMetaType>("Bool", 1);
    } else if (inner == "Float") {
        return std::make_shared<meta::PrimitiveMetaType>("Float", 8);
    } else if (inner == "Double") {
        return std::make_shared<meta::PrimitiveMetaType>("Double", 8);
    }
    
    return nullptr;
}

} // namespace meld::types
