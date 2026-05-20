#pragma once

#include <vector>
#include <memory>
#include <variant>
#include "meld/parser/ast.hpp"
#include "meld/meta/metatype.hpp"
#include "anonymous_types.hpp"

namespace meld::types {

// Type inference result for collection literals
enum class CollectionLiteralKind {
    ANONYMOUS_OBJECT,   // {field: value, ...} with heterogeneous types
    ANONYMOUS_MAP,      // {key: value, ...} with homogeneous value types
    ANONYMOUS_SET,      // {"elem1", "elem2", ...} with homogeneous element types
    ANONYMOUS_ARRAY,    // [elem1, elem2, ...] with homogeneous element types
    ANONYMOUS_TUPLE,    // [value1, value2, ...] with heterogeneous element types
    EMPTY_COLLECTION    // {} or [] - requires type annotation
};

struct CollectionInferenceResult {
    CollectionLiteralKind kind;
    std::shared_ptr<meta::MetaType> inferred_type;
    bool requires_annotation;  // True for empty collections
    std::string error_message;
    
    CollectionInferenceResult(CollectionLiteralKind k, 
                             std::shared_ptr<meta::MetaType> t = nullptr,
                             bool req_ann = false,
                             const std::string& err = "")
        : kind(k), inferred_type(t), requires_annotation(req_ann), error_message(err) {}
};

// Type inference engine for collection literals
class CollectionLiteralInference {
public:
    // Infer type for curly brace syntax: {field: value, ...}
    // Rules:
    // - All values same type → Map<String, T>
    // - Different value types → Anonymous Object
    // - Empty → Requires annotation
    static CollectionInferenceResult inferCurlyBraces(
        const std::vector<parser::ast::anonymous_object_field>& fields);
    
    // Infer type for square bracket syntax: [elem1, elem2, ...]
    // Rules:
    // - All elements same type → Array<T>
    // - Different element types → Tuple
    // - Empty → Requires annotation
    static CollectionInferenceResult inferSquareBrackets(
        const std::vector<parser::ast::expression>& elements);
    
    // Infer type for square bracket syntax with named elements: [x: 10, y: 20]
    // Always infers to named tuple
    static CollectionInferenceResult inferSquareBracketsNamed(
        const std::vector<parser::ast::tuple_element>& elements);
    
    // Check if all expressions have the same type
    static bool areTypesHomogeneous(const std::vector<parser::ast::expression>& exprs);
    
    // Check if all fields have the same value type
    static bool areFieldTypesHomogeneous(
        const std::vector<parser::ast::anonymous_object_field>& fields);
    
    // Get the common type from a list of expressions (assumes homogeneous)
    static std::shared_ptr<meta::MetaType> getCommonType(
        const std::vector<parser::ast::expression>& exprs);
    
    // Get the common value type from fields (assumes homogeneous)
    static std::shared_ptr<meta::MetaType> getCommonFieldType(
        const std::vector<parser::ast::anonymous_object_field>& fields);
    
    // Infer type from a single expression
    static std::shared_ptr<meta::MetaType> inferExpressionType(
        const parser::ast::expression& expr);
    
private:
    // Helper to check if a type is a primitive type
    static bool isPrimitiveType(const std::shared_ptr<meta::MetaType>& type);
};

// Type annotation resolver for explicit type specifications
class TypeAnnotationResolver {
public:
    // Resolve explicit type annotation for empty collections
    // Examples:
    //   val emptyMap: Map<String, Int> = {}
    //   val emptySet: Set<String> = {}
    //   val emptyArray: Array<Int> = []
    static std::shared_ptr<meta::MetaType> resolveAnnotation(
        const std::string& type_annotation);
    
    // Check if a type annotation is for a collection type
    static bool isCollectionType(const std::string& type_annotation);
    
    // Extract element/value type from collection type annotation
    // Map<String, Int> → Int
    // Set<String> → String
    // Array<Int> → Int
    static std::shared_ptr<meta::MetaType> extractElementType(
        const std::string& type_annotation);
};

} // namespace meld::types
