#pragma once

#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include "ast_node.hpp"
#include <string>
#include <vector>

namespace meld::parser::ast {

namespace x3 = boost::spirit::x3;

// Forward declarations
struct expression;
struct identifier;
struct integer_literal;
struct float_literal;
struct string_literal;
struct regex_literal;
struct boolean_literal;
struct list_expression;
struct spread_expression;
struct function_call;
struct val_declaration;
struct var_declaration;
struct binary_operation;
struct unary_operation;
struct generic_type_parameter;
struct type_annotation;
struct field_declaration;
struct struct_definition;
struct class_definition;
struct enum_variant;
struct enum_definition;
struct type_definition;
struct named_parameter;
struct initialization_block;
struct tuple_element;
struct tuple_literal;
struct tuple_indexing;
struct array_indexing;
struct safe_navigation_expression;
struct elvis_expression;
struct tuple_destructuring;
struct destructuring_binding;
struct function_parameter;
struct function_definition;
struct named_argument;
struct block_expression;
struct named_return_value;
struct pipeline_expression;
struct custom_operator_definition;
struct namespace_declaration;
struct import_declaration;
struct anonymous_object_literal;
struct anonymous_map_literal;
struct anonymous_set_literal;
struct anonymous_array_literal;
struct anonymous_tuple_literal;
struct effect_definition;
struct effect_operation;
struct perform_expression;
struct implicit_effect_call;
struct handler_function;
struct handle_expression;
struct inline_trait_impl;
struct resume_expression;
struct refinement_type_definition;
struct contract_block;
struct require_clause;
struct ensure_clause;
struct old_expression;
struct test_block;
struct assertion_expression;
struct flow_definition;
struct flow_state;
struct flow_transition;
struct flow_guard_condition;
struct pattern_field;
struct pattern;

// Identifier
// Annotation: @name or @name(key: value, ...)
struct annotation : ASTNode {
    std::string name;                                    // e.g., "constructor", "infix"
    std::vector<std::pair<std::string, std::string>> args;  // key-value pairs
};

struct identifier : ASTNode {
    std::string name;
    
    identifier() = default;
    explicit identifier(std::string n) : name(std::move(n)) {}
    identifier& operator=(std::string n) { name = std::move(n); return *this; }
};

// Literals
struct integer_literal : ASTNode {
    int64_t value;
    std::string suffix; // L for long
};

struct float_literal : ASTNode {
    double value;
    std::string suffix; // F for float, D for double
};

struct string_literal : ASTNode {
    std::string value;
    bool has_interpolation;
    bool has_modifier_interpolation = false;  // true when ${:modifier expr} is present
    bool is_multiline = false;  // true for triple-quoted/triple-backtick strings
    bool is_template = false;   // true for backtick strings (evaluated), false for double-quoted (static)
};

// Backward compatibility alias - multiline_string_literal merged into string_literal
using multiline_string_literal = string_literal;

struct regex_literal : ASTNode {
    std::string pattern;
    std::string flags;
};

struct boolean_literal : ASTNode {
    bool value;
};

// List expression
struct list_expression : ASTNode {
    std::vector<x3::forward_ast<expression>> elements;
};

// Spread expression (...collection)
struct spread_expression : ASTNode {
    x3::forward_ast<expression> collection;
};

// Function call (can have positional or named arguments)
struct function_call : ASTNode {
    identifier function_name;
    std::vector<x3::forward_ast<expression>> arguments;  // Positional arguments
    std::vector<named_argument> named_arguments;  // Named arguments
};

// Declarations
struct val_declaration : ASTNode {
    identifier name;
    bool has_type_annotation = false;
    x3::forward_ast<type_annotation> type_ann;
    x3::forward_ast<expression> value;
};

struct var_declaration : ASTNode {
    identifier name;
    bool has_type_annotation = false;
    x3::forward_ast<type_annotation> type_ann;
    x3::forward_ast<expression> value;
};

// Operations
struct binary_operation : ASTNode {
    std::string op;
    x3::forward_ast<expression> left;
    x3::forward_ast<expression> right;
};

struct unary_operation : ASTNode {
    std::string op;
    x3::forward_ast<expression> operand;
};

// Generic type parameter
struct generic_type_parameter : ASTNode {
    identifier name;
    std::string variance;  // "in", "out", or "" for invariant
    bool has_bound = false;
    x3::forward_ast<type_annotation> bound;  // Upper bound constraint
};

// Type annotation
struct type_annotation : ASTNode {
    identifier type_name;
    bool is_nullable = false;
    
    // Mutability qualifier for generic type parameters (Req 165)
    // When this type_annotation appears as a generic type argument,
    // it may carry a val/var qualifier: e.g., Hold[val T], Map[val K, var V]
    enum class MutabilityQualifier { NONE, VAL, VAR };
    MutabilityQualifier mutability_qualifier = MutabilityQualifier::NONE;
    
    // Generic type arguments (e.g., List[Int], Map[String, Int])
    bool has_type_arguments = false;
    std::vector<x3::forward_ast<type_annotation>> type_arguments;
    
    // Union and intersection types
    bool is_union = false;  // true for Type | Type
    bool is_intersection = false;  // true for Type & Type
    std::vector<x3::forward_ast<type_annotation>> union_types;  // For union types
    std::vector<x3::forward_ast<type_annotation>> intersection_types;  // For intersection types
};

// Field visibility levels (for macro-generated visibility changes)
enum class FieldVisibility {
    DEFAULT,        // No explicit visibility — follows class default
    PUBLIC,         // pub
    PACKAGE_PRIVATE // @visibility(pkg) — visible within the same package
};

// Field declaration
struct field_declaration : ASTNode {
    bool is_mutable; // true for var, false for val
    bool has_explicit_val = false; // true when 'val' was explicitly written
    identifier name;
    type_annotation type;
    FieldVisibility visibility = FieldVisibility::DEFAULT; // Requirements: 17.3
};

// Block expression (for property getters/setters and other blocks)
struct block_expression : ASTNode {
    std::vector<x3::forward_ast<expression>> statements;
};

// Property getter block
struct property_getter : ASTNode {
    block_expression body;
};

// Property setter block  
struct property_setter : ASTNode {
    identifier value_param;  // Parameter name (typically "value")
    block_expression body;
};

// Property declaration
struct property_declaration : ASTNode {
    bool is_mutable; // true for var, false for val
    bool has_explicit_val = false; // true when 'val' was explicitly written
    identifier name;
    type_annotation type;
    bool has_getter = false;
    bool has_setter = false;
    property_getter getter;
    property_setter setter;
    bool is_computed = false; // true if no backing field
};

// Struct definition
struct struct_definition : ASTNode {
    identifier name;
    std::vector<generic_type_parameter> type_parameters;  // Generic type parameters
    std::vector<field_declaration> fields;
    std::vector<property_declaration> properties;

    // Methods injected by macros (not populated by parser, not in Fusion adaptation).
    // Requirements: 25B.13
    std::vector<function_definition> methods;

    /**
     * Add a generated method to this struct, wiring parent pointers.
     * This is the member-function counterpart of the free function
     * add_method(struct_definition&, function_definition) in ast_mutations.hpp.
     * Requirements: 25B.13
     */
    void add_method(function_definition method);

    /**
     * Look up a method by name in this struct's method table.
     * Returns a pointer to the method if found, nullptr otherwise.
     * Requirements: 25B.13
     */
    function_definition* find_method(const std::string& name);
    const function_definition* find_method(const std::string& name) const;

    /**
     * Check whether a method with the given name exists in this struct.
     * Requirements: 25B.13
     */
    bool has_method(const std::string& name) const;
};

// Class definition
struct class_definition : ASTNode {
    identifier name;
    std::vector<generic_type_parameter> type_parameters;  // Generic type parameters
    std::vector<field_declaration> fields;
    std::vector<property_declaration> properties;

    // Methods injected by macros (not populated by parser, not in Fusion adaptation).
    // Field-level macros like @Getter use parent_class.add_method() to inject here.
    // Requirements: 25B.13
    std::vector<function_definition> methods;

    /**
     * Add a generated method to this class, wiring parent pointers.
     * This is the primary API for field-level macros:
     *   parent_class.add_method(getter)
     *
     * The method is appended to the methods vector, its parent pointer
     * is set to this class, and all child nodes (name, parameters) are
     * wired to the method.
     * Requirements: 25B.13
     */
    void add_method(function_definition method);

    /**
     * Look up a method by name in this class's method table.
     * Returns a pointer to the method if found, nullptr otherwise.
     * Requirements: 25B.13
     */
    function_definition* find_method(const std::string& name);
    const function_definition* find_method(const std::string& name) const;

    /**
     * Check whether a method with the given name exists in this class.
     * Requirements: 25B.13
     */
    bool has_method(const std::string& name) const;
};

// Enum variant (for algebraic data types)
struct enum_variant : ASTNode {
    identifier name;
    bool has_associated_data = false;
    std::vector<field_declaration> associated_fields;  // For variants with data: Success(message: string)
    std::vector<type_annotation> associated_types;     // For simple associated types: Some(T)
};

// Enum definition (algebraic data type)
struct enum_definition : ASTNode {
    identifier name;
    std::vector<generic_type_parameter> type_parameters;  // Generic type parameters
    std::vector<enum_variant> variants;
};

// Unified type definition: <kind> <name> [generics] { members }
// Examples: class Person { }, struct Point { }, enum Color { }, type data { }
// The kind is an identifier (class, struct, enum, trait, or user-defined).
// Members can be fields (val/var name: Type), variants (Name, Name(fields)),
// or any expressions (for meta-definitions via meta_set).
struct type_definition : ASTNode {
    identifier kind;  // "class", "struct", "enum", "type", or user-defined
    identifier name;
    std::vector<generic_type_parameter> type_parameters;
    std::vector<field_declaration> fields;
    std::vector<property_declaration> properties;
    std::vector<enum_variant> variants;  // For enum-like type kinds
    std::vector<x3::forward_ast<expression>> body;  // For meta-definitions and general members

    // Methods injected by macros (not populated by parser)
    std::vector<function_definition> methods;

    void add_method(function_definition method);
    function_definition* find_method(const std::string& name);
    const function_definition* find_method(const std::string& name) const;
    bool has_method(const std::string& name) const;
};

// Type alias declaration
struct typealias_declaration : ASTNode {
    identifier alias_name;
    type_annotation target_type;
};

// Newtype wrapper declaration (zero-cost type safety abstraction)
struct newtype_declaration : ASTNode {
    identifier wrapper_name;
    type_annotation wrapped_type;
    bool is_transparent = true;  // For zero-cost optimization
};

// Named parameter assignment (for initialization blocks)
struct named_parameter : ASTNode {
    identifier name;
    x3::forward_ast<expression> value;
};

// Initialization block (Ceylon-style)
struct initialization_block : ASTNode {
    identifier type_name;
    std::vector<named_parameter> parameters;
};

// Tuple element (can be named or unnamed)
struct tuple_element : ASTNode {
    std::string name;  // Empty string for unnamed elements
    x3::forward_ast<expression> value;
    bool is_named = false;
};

// Tuple literal
struct tuple_literal : ASTNode {
    std::vector<tuple_element> elements;
};

// Tuple indexing (tuple.0, tuple.1, or tuple.name)
struct tuple_indexing : ASTNode {
    x3::forward_ast<expression> tuple;
    std::string index;  // Either numeric index or field name
    bool is_numeric = true;
};

// Array indexing (array[index])
struct array_indexing : ASTNode {
    x3::forward_ast<expression> array;
    x3::forward_ast<expression> index;  // Expression that evaluates to an index
};

// Safe navigation expression (nullable?.field)
struct safe_navigation_expression : ASTNode {
    x3::forward_ast<expression> nullable_expr;  // The nullable expression
    std::string field_name;  // The field to access safely
};

// Elvis expression (nullable ?? default)
struct elvis_expression : ASTNode {
    x3::forward_ast<expression> nullable_expr;  // The nullable expression
    x3::forward_ast<expression> default_value;  // The default value if null
};

// Tuple destructuring binding — a single binding with its own val/var qualifier (Req 169)
struct destructuring_binding : ASTNode {
    identifier name;           // The variable name (or "_" for placeholder)
    bool is_val = true;        // true for val, false for var
    bool is_placeholder = false; // true for "_" (no qualifier needed)
};

// Tuple destructuring: (val a, var b, val c) = tuple  (Req 169.7)
// Each binding carries its own val/var qualifier.
// The old shorthand `val (a, b) = tuple` is NOT supported.
struct tuple_destructuring : ASTNode {
    std::vector<destructuring_binding> bindings;
    x3::forward_ast<expression> tuple_expr;
};

// Parameter decorator
// Note: #undef CONST needed because Windows <windef.h> defines CONST as a macro
#ifdef CONST
#undef CONST
#endif
enum class ParameterDecorator {
    NONE,
    CONST,  // @const - immutable parameter
    MUT,    // @mut - explicitly mutable parameter
    REF,    // @ref - pass by reference
    MOVE    // @move - ownership transfer
};

// Function parameter with optional default value and decorator
struct function_parameter : ASTNode {
    ParameterDecorator decorator = ParameterDecorator::NONE;
    bool has_explicit_val = false; // true when 'val' was explicitly written
    bool is_mutable = false; // true when 'var' modifier is used on parameter (Req 57.4)
    identifier name;
    type_annotation type;
    bool has_default = false;
    bool is_rest = false;  // true for rest parameters (...args)
    bool is_destructured = false;  // true for tuple destructuring parameters
    std::vector<identifier> destructured_names;  // For tuple destructuring: (a, b, c)
    x3::forward_ast<expression> default_value;
};

// Named return value
struct named_return_value : ASTNode {
    identifier name;
    type_annotation type;
    bool has_default = false;
    x3::forward_ast<expression> default_value;
};

// Design by Contract structures

// old() expression for referencing pre-state values in postconditions
struct old_expression : ASTNode {
    x3::forward_ast<expression> expression;  // The expression to capture pre-state
};

// Require clause (precondition)
struct require_clause : ASTNode {
    x3::forward_ast<expression> condition;  // Boolean condition that must be true
    std::string message;  // Optional error message
    bool has_message = false;
};

// Ensure clause (postcondition)
struct ensure_clause : ASTNode {
    x3::forward_ast<expression> condition;  // Boolean condition that must be true after execution
    std::string message;  // Optional error message
    bool has_message = false;
};

// Contract block containing require and ensure clauses
struct contract_block : ASTNode {
    std::vector<require_clause> preconditions;  // Renamed from 'requires' to avoid C++20 keyword conflict
    std::vector<ensure_clause> postconditions;  // Renamed from 'ensures' to avoid C++20 keyword conflict
    bool has_requires = false;
    bool has_ensures = false;
};

// Assertion expression for test blocks
struct assertion_expression : ASTNode {
    x3::forward_ast<expression> condition;
    std::string message;  // Optional failure message
    bool has_message = false;
};

// Test block for inline micro-tests
struct test_block : ASTNode {
    std::string description;  // Optional test description
    std::vector<assertion_expression> assertions;
    x3::forward_ast<block_expression> body;  // Test code
    bool has_description = false;
    bool is_property_test = false;  // true if using forall
    std::vector<identifier> forall_variables;  // Variables for property-based testing
};

// Function definition with fn keyword
struct function_definition : ASTNode {
    identifier name;
    std::vector<function_parameter> parameters;
    std::vector<std::vector<function_parameter>> curried_parameter_groups;  // For curried syntax: fn foo(a)(b)(c)
    type_annotation return_type;  // For simple return types
    std::vector<named_return_value> named_returns;  // For named return values
    bool has_return_type = false;
    bool has_named_returns = false;
    bool is_curried = false;  // true if using curried syntax
    bool is_mutating = false;  // true when declared with 'var fnc' (Req 57.1)
    std::vector<identifier> effects_clause;  // Effects declared in effects clause
    bool has_effects = false;  // true if effects clause present
    contract_block contracts;  // Design by Contract clauses
    bool has_contracts = false;  // true if contract block present
    std::vector<test_block> tests;  // Inline micro-tests
    bool has_tests = false;  // true if test blocks present
    x3::forward_ast<block_expression> body;
};

// Named argument (for function calls with named parameters)
struct named_argument : ASTNode {
    identifier name;
    x3::forward_ast<expression> value;
};

// Lambda parameter (simplified - just identifier for now)
struct lambda_parameter : ASTNode {
    identifier name;
    bool has_type = false;
    type_annotation type;
};

// Lambda expression with => syntax: { params => body }
struct lambda_expression : ASTNode {
    std::vector<lambda_parameter> parameters;
    x3::forward_ast<expression> body;
    bool is_block = false;  // true if body is a block expression
};

// Function type annotation (for function types)
struct function_type : ASTNode {
    std::vector<type_annotation> parameter_types;
    type_annotation return_type;
    std::vector<named_return_value> named_returns;  // For named return types
    bool has_named_returns = false;
};

// Extension method definition (extend TypeName { func methodName(...) { ... } })
struct extension_method : ASTNode {
    identifier name;
    std::vector<function_parameter> parameters;
    type_annotation return_type;
    std::vector<named_return_value> named_returns;
    bool has_return_type = false;
    bool has_named_returns = false;
    x3::forward_ast<block_expression> body;
};

// Extension block (extend TypeName { ... })
struct extension_block : ASTNode {
    type_annotation target_type;
    std::vector<extension_method> methods;
};

// Custom operator definition
enum class CustomOperatorType {
    PREFIX,
    INFIX,
    POSTFIX
};

enum class CustomOperatorAssociativity {
    LEFT,
    RIGHT,
    NONE
};

// Operator function definition (operator func +(...) { ... })
// This is for operator overloading on types
struct operator_function : ASTNode {
    std::string symbol;  // The operator symbol (+, -, *, etc.)
    std::vector<function_parameter> parameters;
    type_annotation return_type;
    bool has_return_type = false;
    x3::forward_ast<block_expression> body;
};

// Custom operator definition (infix func <|>(...) precedence 100 associativity left { ... })
struct custom_operator_definition : ASTNode {
    CustomOperatorType op_type;
    std::string symbol;
    int precedence = 50;  // Default precedence
    CustomOperatorAssociativity associativity = CustomOperatorAssociativity::LEFT;
    std::vector<function_parameter> parameters;
    type_annotation return_type;
    bool has_return_type = false;
    x3::forward_ast<block_expression> body;
};

// Pipeline expression (value |> function)
struct pipeline_expression : ASTNode {
    x3::forward_ast<expression> value;
    x3::forward_ast<expression> function;
};

// Pattern matching structures

// Pattern types
enum class PatternType {
    LITERAL,        // Literal pattern: 0, "hello", true
    TYPE,           // Type pattern with binding: x: Int
    WILDCARD,       // Wildcard pattern: _
    DESTRUCTURING,  // Destructuring pattern: Point { x, y }
    ENUM_VARIANT,   // Enum variant pattern: Color.Red or Result.Ok(value)
    NESTED          // Nested destructuring: Point { x: Point { x: inner_x, y: inner_y }, y }
};

// Nested pattern field for deep destructuring
struct pattern_field : ASTNode {
    identifier field_name;
    x3::forward_ast<pattern> field_pattern;
};

// Pattern structure
struct pattern : ASTNode {
    PatternType type;
    
    // For literal patterns
    x3::forward_ast<expression> literal_value;
    
    // For type patterns
    identifier binding_name;           // Variable name to bind
    type_annotation binding_type;      // Type annotation
    bool has_type = false;             // Whether type annotation is present
    
    // For destructuring patterns
    identifier struct_name;            // Name of struct/class being destructured
    std::vector<identifier> field_bindings;  // Field names to bind
    
    // For enum variant patterns
    identifier enum_name;              // Name of enum
    identifier variant_name;           // Name of variant
    std::vector<x3::forward_ast<pattern>> nested_patterns;  // Nested patterns for associated data
    
    // For nested destructuring
    std::vector<pattern_field> field_patterns;  // Field patterns for deep destructuring
    
    // Pattern guard (optional if condition)
    bool has_guard = false;
    x3::forward_ast<expression> guard_condition;
};

// Match case (case pattern => expression)
struct match_case : ASTNode {
    pattern case_pattern;
    x3::forward_ast<expression> result_expression;
};

// Match expression (match value { case ... })
struct match_expression : ASTNode {
    x3::forward_ast<expression> matched_value;
    std::vector<match_case> cases;
};

// Query expression structures (LINQ-style)

// From clause (from x in collection)
struct query_from_clause : ASTNode {
    identifier binding;
    x3::forward_ast<expression> source;
};

// Where clause (where condition)
struct query_where_clause : ASTNode {
    x3::forward_ast<expression> condition;
};

// Join clause (join y in collection on left_key == right_key)
struct query_join_clause : ASTNode {
    identifier binding;
    x3::forward_ast<expression> source;
    x3::forward_ast<expression> left_key;
    x3::forward_ast<expression> right_key;
};

// Group clause (group expr by key_expr into binding)
struct query_group_clause : ASTNode {
    x3::forward_ast<expression> value_expr;
    x3::forward_ast<expression> key_expr;
    identifier into_binding;
    bool has_into = false;
};

// Select clause (select expression)
struct query_select_clause : ASTNode {
    x3::forward_ast<expression> projection;
};

// Query expression (query { from ... where ... select ... })
struct query_expression : ASTNode {
    query_from_clause from_clause;
    std::vector<query_where_clause> where_clauses;
    std::vector<query_join_clause> join_clauses;
    bool has_group = false;
    query_group_clause group_clause;
    query_select_clause select_clause;
};

// Namespace declaration (namespace com.example.myapp { ... })
struct namespace_declaration : ASTNode {
    std::vector<std::string> name_parts;  // ["com", "example", "myapp"]
    std::vector<x3::forward_ast<expression>> body;  // Declarations within the namespace
};

// Import declaration types
enum class ImportType {
    SPECIFIC,   // import com.example.User (legacy)
    WILDCARD,   // import com.example.* (legacy)
    ALIASED,    // import com.example.models as Models (legacy)
    IMP_BASIC,        // imp std.math
    IMP_ALIASED,      // imp m = std.math
    IMP_DESTRUCTURED  // imp { sin, cos } = std.math  or  imp { sin -> s } = std.math
};

// Symbol mapping for imp destructured imports
struct imp_symbol_mapping : ASTNode {
    std::string original_name;   // Original symbol name from the module
    std::string local_name;      // Local binding name (same as original if no alias)
    bool has_alias = false;      // true if -> alias was used
};

// Import declaration (supports both legacy 'import' and new 'imp' syntax)
struct import_declaration : ASTNode {
    ImportType import_type;
    std::vector<std::string> namespace_path;  // Module path segments
    std::string alias;  // For aliased imports (both legacy and imp)
    bool has_alias = false;
    std::vector<imp_symbol_mapping> symbols;  // For IMP_DESTRUCTURED form
    bool is_imp = false;  // true if parsed from 'imp' keyword (new syntax)
};

// Anonymous object field (field: value)
struct anonymous_object_field : ASTNode {
    identifier field_name;
    x3::forward_ast<expression> value;
};

// Anonymous object literal {field1: value1, field2: value2}
// Used for structural types with heterogeneous field types
// Also used for map literals when is_map is true
struct anonymous_object_literal : ASTNode {
    std::vector<anonymous_object_field> fields;
    bool is_map = false;  // true when representing Map<String, T> with homogeneous value types
};

// Anonymous map literal {key1: value1, key2: value2}
// Used for Map<String, T> with homogeneous value types
struct anonymous_map_literal : ASTNode {
    std::vector<anonymous_object_field> entries;  // Reuse field structure for key-value pairs
};

// Anonymous set literal {"elem1", "elem2", "elem3"}
// Used for Set<T> with homogeneous element types
struct anonymous_set_literal : ASTNode {
    std::vector<x3::forward_ast<expression>> elements;
};

// Anonymous array literal [elem1, elem2, elem3]
// Used for Array<T> with homogeneous element types
struct anonymous_array_literal : ASTNode {
    std::vector<x3::forward_ast<expression>> elements;
};

// Anonymous tuple literal [value1, value2, value3] or [x: 10, y: 20]
// Used for tuples with heterogeneous element types
struct anonymous_tuple_literal : ASTNode {
    std::vector<tuple_element> elements;
};

// Effect-related structures

// Effect operation signature (abstract operation in effect definition)
struct effect_operation : ASTNode {
    identifier name;
    std::vector<function_parameter> parameters;
    type_annotation return_type;
    bool has_return_type = false;
};

// Effect definition (effect FileSystem { fn read(...) -> string })
struct effect_definition : ASTNode {
    identifier name;
    std::vector<effect_operation> operations;
};

// Perform expression (perform EffectIO.read(path))
struct perform_expression : ASTNode {
    identifier effect_name;
    identifier operation_name;
    std::vector<x3::forward_ast<expression>> arguments;
    bool is_deprecated = true;
};

// Implicit effect call: Effect.operation(args) without perform wrapper
struct implicit_effect_call : ASTNode {
    identifier effect_name;       // e.g., "FileSystem"
    identifier operation_name;    // e.g., "read"
    std::vector<x3::forward_ast<expression>> arguments;
};

// Handler function (implementation of an effect operation in a handler)
struct handler_function : ASTNode {
    identifier name;
    std::vector<function_parameter> parameters;
    x3::forward_ast<block_expression> body;
};

// Inline trait implementation: TypeName { fnc op1() { ... } fnc op2() { ... } }
// General-purpose anonymous implementation of a trait/interface.
// Used by handle() for effect handlers, but also available as a standalone expression.
struct inline_trait_impl : ASTNode {
    identifier trait_name;
    std::vector<handler_function> methods;  // fnc definitions inside the block
};

// Handle expression: handle({ body }, Effect1 { fnc ... }, Effect2 { fnc ... })
// First arg is computation closure, remaining args are inline trait implementations.
struct handle_expression : ASTNode {
    x3::forward_ast<block_expression> body;
    std::vector<inline_trait_impl> handlers;  // one per effect
};

// Resume expression (resume(value))
struct resume_expression : ASTNode {
    x3::forward_ast<expression> value;
    bool has_value = false;  // false for resume() without value
};

// Refinement type definition (type Name -> BaseType where { predicate })
struct refinement_type_definition : ASTNode {
    identifier name;                    // The refinement type name
    type_annotation base_type;          // The base type being refined
    x3::forward_ast<expression> predicate;  // The predicate expression (it >= 0)
};

// Flow construct structures for state machines

// Flow guard condition (when clause in transitions)
struct flow_guard_condition : ASTNode {
    x3::forward_ast<expression> condition;  // Boolean condition expression
};

// Flow transition (on(Event) => goto TargetState)
struct flow_transition : ASTNode {
    identifier event_name;              // Event that triggers the transition
    identifier target_state;            // Target state to transition to
    bool has_guard = false;             // Whether guard condition is present
    flow_guard_condition guard;         // Optional guard condition
};

// Flow state definition
struct flow_state : ASTNode {
    identifier name;                    // State name
    std::vector<flow_transition> transitions;  // Transitions from this state
    bool has_entry_action = false;      // Whether entry action is present
    bool has_exit_action = false;       // Whether exit action is present
    x3::forward_ast<block_expression> entry_action;  // Entry action block
    x3::forward_ast<block_expression> exit_action;   // Exit action block
    bool is_terminal = false;           // Whether this is a terminal state (no outgoing transitions)
};

// Flow definition (flow StateMachineName(initial: InitialState) { ... })
struct flow_definition : ASTNode {
    identifier name;                    // Flow name
    identifier initial_state;           // Initial state name
    std::vector<flow_state> states;     // All states in the flow
};

// Return statement (rtn expression or rtn (name1 = value1, name2 = value2))
struct return_statement : ASTNode {
    bool has_expression = false;
    x3::forward_ast<expression> expr;  // For simple return: rtn value (renamed from 'expression' to avoid shadowing type name)
    
    bool has_named_returns = false;
    std::vector<named_argument> named_returns;  // For named returns: rtn (out1 = value1, out2 = value2)
    
    bool is_tuple_return = false;
    std::vector<x3::forward_ast<expression>> tuple_values;  // For tuple return: rtn (value1, value2, value3)
};

// Named return value assignment (quotient = a / b)
struct named_return_assignment : ASTNode {
    identifier return_name;
    x3::forward_ast<expression> value;
};

// Expression variant
struct expression : x3::variant<
    identifier,
    integer_literal,
    float_literal,
    string_literal,
    regex_literal,
    boolean_literal,
    x3::forward_ast<list_expression>,
    x3::forward_ast<spread_expression>,
    x3::forward_ast<function_call>,
    x3::forward_ast<val_declaration>,
    x3::forward_ast<var_declaration>,
    x3::forward_ast<binary_operation>,
    x3::forward_ast<unary_operation>,
    x3::forward_ast<struct_definition>,
    x3::forward_ast<class_definition>,
    x3::forward_ast<enum_definition>,
    x3::forward_ast<typealias_declaration>,
    x3::forward_ast<newtype_declaration>,
    x3::forward_ast<initialization_block>,
    x3::forward_ast<tuple_literal>,
    x3::forward_ast<tuple_indexing>,
    x3::forward_ast<array_indexing>,
    x3::forward_ast<safe_navigation_expression>,
    x3::forward_ast<elvis_expression>,
    x3::forward_ast<tuple_destructuring>,
    x3::forward_ast<function_definition>,
    x3::forward_ast<lambda_expression>,
    x3::forward_ast<extension_block>,
    x3::forward_ast<operator_function>,
    x3::forward_ast<custom_operator_definition>,
    x3::forward_ast<pipeline_expression>,
    x3::forward_ast<match_expression>,
    x3::forward_ast<query_expression>,
    x3::forward_ast<namespace_declaration>,
    x3::forward_ast<import_declaration>,
    x3::forward_ast<anonymous_object_literal>,
    x3::forward_ast<anonymous_array_literal>,
    x3::forward_ast<anonymous_tuple_literal>,
    x3::forward_ast<effect_definition>,
    x3::forward_ast<perform_expression>,
    x3::forward_ast<implicit_effect_call>,
    x3::forward_ast<handle_expression>,
    x3::forward_ast<resume_expression>,
    x3::forward_ast<return_statement>,
    x3::forward_ast<named_return_assignment>,
    x3::forward_ast<refinement_type_definition>,
    x3::forward_ast<flow_definition>,
    x3::forward_ast<old_expression>,
    x3::forward_ast<test_block>,
    x3::forward_ast<assertion_expression>
> {
    using base_type::base_type;
    using base_type::operator=;
};

} // namespace meld::parser::ast

// Tell X3 how to move a std::string into an identifier
namespace boost::spirit::x3::traits {
    inline void move_to(std::string& src, meld::parser::ast::identifier& dest) {
        dest.name = std::move(src);
    }
    inline void move_to(std::string&& src, meld::parser::ast::identifier& dest) {
        dest.name = std::move(src);
    }
}

// Boost Fusion adaptors
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::annotation, name, args)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::identifier, name)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::integer_literal, value, suffix)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::float_literal, value, suffix)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::string_literal, value, has_interpolation, is_multiline)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::regex_literal, pattern, flags)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::boolean_literal, value)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::list_expression, elements)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::spread_expression, collection)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::function_call, function_name, arguments, named_arguments)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::val_declaration, name, has_type_annotation, type_ann, value)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::var_declaration, name, has_type_annotation, type_ann, value)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::binary_operation, op, left, right)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::unary_operation, op, operand)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::generic_type_parameter, name, variance, has_bound, bound)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::type_annotation, type_name, is_nullable, has_type_arguments, type_arguments, is_union, is_intersection, union_types, intersection_types)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::field_declaration, is_mutable, has_explicit_val, name, type)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::block_expression, statements)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::property_getter, body)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::property_setter, value_param, body)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::property_declaration, is_mutable, has_explicit_val, name, type, has_getter, has_setter, getter, setter, is_computed)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::struct_definition, name, type_parameters, fields, properties)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::class_definition, name, type_parameters, fields, properties)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::enum_variant, name, has_associated_data, associated_fields, associated_types)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::enum_definition, name, type_parameters, variants)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::type_definition, kind, name, type_parameters, fields, properties, variants, body)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::typealias_declaration, alias_name, target_type)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::newtype_declaration, wrapper_name, wrapped_type, is_transparent)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::named_parameter, name, value)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::initialization_block, type_name, parameters)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::tuple_element, name, value, is_named)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::tuple_literal, elements)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::tuple_indexing, tuple, index, is_numeric)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::array_indexing, array, index)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::safe_navigation_expression, nullable_expr, field_name)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::elvis_expression, nullable_expr, default_value)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::destructuring_binding, name, is_val, is_placeholder)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::tuple_destructuring, bindings, tuple_expr)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::function_parameter, decorator, has_explicit_val, name, type, has_default, is_rest, is_destructured, destructured_names, default_value)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::named_return_value, name, type, has_default, default_value)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::function_definition, name, parameters, return_type, named_returns, has_return_type, has_named_returns, effects_clause, has_effects, contracts, has_contracts, tests, has_tests, body)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::named_argument, name, value)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::lambda_parameter, name, has_type, type)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::lambda_expression, parameters, body, is_block)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::function_type, parameter_types, return_type, named_returns, has_named_returns)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::extension_method, name, parameters, return_type, named_returns, has_return_type, has_named_returns, body)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::extension_block, target_type, methods)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::operator_function, symbol, parameters, return_type, has_return_type, body)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::custom_operator_definition, op_type, symbol, precedence, associativity, parameters, return_type, has_return_type, body)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::pipeline_expression, value, function)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::pattern_field, field_name, field_pattern)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::pattern, type, literal_value, binding_name, binding_type, has_type, struct_name, field_bindings, enum_name, variant_name, nested_patterns, field_patterns, has_guard, guard_condition)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::match_case, case_pattern, result_expression)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::match_expression, matched_value, cases)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::query_from_clause, binding, source)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::query_where_clause, condition)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::query_join_clause, binding, source, left_key, right_key)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::query_group_clause, value_expr, key_expr, into_binding, has_into)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::query_select_clause, projection)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::query_expression, from_clause, where_clauses, join_clauses, has_group, group_clause, select_clause)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::namespace_declaration, name_parts, body)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::imp_symbol_mapping, original_name, local_name, has_alias)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::import_declaration, import_type, namespace_path, alias, has_alias, symbols, is_imp)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::anonymous_object_field, field_name, value)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::anonymous_object_literal, fields, is_map)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::anonymous_map_literal, entries)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::anonymous_set_literal, elements)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::anonymous_array_literal, elements)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::anonymous_tuple_literal, elements)

// Effect-related structures
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::effect_operation, name, parameters, return_type, has_return_type)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::effect_definition, name, operations)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::perform_expression, effect_name, operation_name, arguments, is_deprecated)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::implicit_effect_call, effect_name, operation_name, arguments)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::handler_function, name, parameters, body)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::inline_trait_impl, trait_name, methods)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::handle_expression, body, handlers)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::resume_expression, value, has_value)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::return_statement, has_expression, expr, has_named_returns, named_returns, is_tuple_return, tuple_values)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::named_return_assignment, return_name, value)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::refinement_type_definition, name, base_type, predicate)

// Design by Contract structures
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::old_expression, expression)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::require_clause, condition, message, has_message)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::ensure_clause, condition, message, has_message)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::contract_block, preconditions, postconditions, has_requires, has_ensures)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::assertion_expression, condition, message, has_message)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::test_block, description, assertions, body, has_description, is_property_test, forall_variables)

// Flow construct structures
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::flow_guard_condition, condition)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::flow_transition, event_name, target_state, has_guard, guard)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::flow_state, name, transitions, has_entry_action, has_exit_action, entry_action, exit_action, is_terminal)
BOOST_FUSION_ADAPT_STRUCT(meld::parser::ast::flow_definition, name, initial_state, states)
