/**
 * Property-based tests for explicit val annotation feature.
 *
 * This file contains property tests for the explicit val annotation on
 * struct/class fields, properties, and function parameters.
 *
 * Uses rapidcheck for property-based testing.
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"
#include <boost/variant.hpp>
#include <string>
#include <vector>
#include <sstream>

using namespace meld::parser;
using namespace meld::parser::ast;

namespace {

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

/// Generate a valid Meld field name with prefix to avoid keyword collisions.
rc::Gen<std::string> genFieldName() {
    return rc::gen::map(
        rc::gen::inRange(0, 100),
        [](int n) { return "f_" + std::to_string(n); }
    );
}

/// Generate a valid Meld type name (capitalized identifier).
rc::Gen<std::string> genTypeName() {
    return rc::gen::element(
        std::string("Int"), std::string("Float"),
        std::string("String"), std::string("Bool"),
        std::string("Double"));
}

/// Generate a struct name.
rc::Gen<std::string> genStructName() {
    return rc::gen::map(
        rc::gen::inRange(0, 100),
        [](int n) { return "TestStruct" + std::to_string(n); }
    );
}

/// Generate a class name.
rc::Gen<std::string> genClassName() {
    return rc::gen::map(
        rc::gen::inRange(0, 100),
        [](int n) { return "TestClass" + std::to_string(n); }
    );
}


// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Parse source into expressions, return success.
bool parse_source(const std::string& source, std::vector<expression>& results) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (!lexer.errors().empty()) return false;
    TokenParser parser(tokens);
    return parser.parse_file(results);
}

/// Parse source and capture errors (non-fatal annotation errors) and AST.
/// Returns true if structural parsing succeeded (AST was built).
bool parse_source_with_errors(const std::string& source,
                              std::vector<expression>& results,
                              std::vector<std::string>& errors) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (!lexer.errors().empty()) return false;
    TokenParser parser(tokens);
    bool ok = parser.parse_file(results);
    errors = parser.errors();
    return ok;
}

/// Parse source and capture warnings.
bool parse_source_with_warnings(const std::string& source,
                                std::vector<expression>& results,
                                std::vector<std::string>& warnings) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (!lexer.errors().empty()) return false;
    TokenParser parser(tokens);
    bool ok = parser.parse_file(results);
    warnings = parser.warnings();
    return ok;
}

/// Try to parse source; if it fails, capture the error message.
bool parse_fails_with_error(const std::string& source, std::string& error_msg) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (!lexer.errors().empty()) {
        error_msg = lexer.errors().front();
        return true;
    }
    TokenParser parser(tokens);
    std::vector<expression> results;
    if (!parser.parse_file(results)) {
        error_msg = parser.error_message();
        return true;
    }
    return false;
}

} // anonymous namespace

// ===========================================================================
// Property 1: Explicit val sets immutable with tracking flag
//
// For any struct/class field or property declaration prefixed with the `val`
// keyword, the parser SHALL produce an AST node where `is_mutable == false`
// AND `has_explicit_val == true`.
//
// **Validates: Requirements 1.1, 1.4, 2.1, 2.4, 4.1**
// ===========================================================================

/**
 * Feature: explicit-val-annotation, Property 1: Explicit val sets immutable with tracking flag
 *
 * For any randomly generated struct with val-prefixed fields,
 * every field SHALL have is_mutable == false and has_explicit_val == true.
 *
 * **Validates: Requirements 1.1, 1.4**
 */
TEST(ExplicitValPropertyTest, StructFieldsWithValAreImmutableAndTracked) {
    rc::check("Struct fields with explicit val must have is_mutable=false and has_explicit_val=true",
        []() {
            auto struct_name = *genStructName();
            auto field_count = *rc::gen::inRange(1, 5);

            std::ostringstream fields_oss;
            std::vector<std::string> field_names;
            for (int i = 0; i < field_count; ++i) {
                auto fname = *genFieldName();
                auto tname = *genTypeName();
                field_names.push_back(fname);
                fields_oss << "    val " << fname << ": " << tname << "\n";
            }

            std::string source =
                "struct " + struct_name + " {\n" +
                fields_oss.str() +
                "}";

            std::vector<expression> results;
            RC_ASSERT(parse_source(source, results));
            RC_ASSERT(!results.empty());

            auto* sd = boost::get<boost::spirit::x3::forward_ast<struct_definition>>(&results[0]);
            RC_ASSERT(sd != nullptr);
            const auto& def = sd->get();

            RC_ASSERT(static_cast<int>(def.fields.size()) == field_count);
            for (int i = 0; i < field_count; ++i) {
                RC_ASSERT(def.fields[i].name.name == field_names[i]);
                RC_ASSERT(!def.fields[i].is_mutable);
                RC_ASSERT(def.fields[i].has_explicit_val);
            }
        }
    );
}

/**
 * Feature: explicit-val-annotation, Property 1: Explicit val sets immutable with tracking flag
 *
 * For any randomly generated class with val-prefixed fields,
 * every field SHALL have is_mutable == false and has_explicit_val == true.
 *
 * **Validates: Requirements 2.1, 2.4**
 */
TEST(ExplicitValPropertyTest, ClassFieldsWithValAreImmutableAndTracked) {
    rc::check("Class fields with explicit val must have is_mutable=false and has_explicit_val=true",
        []() {
            auto class_name = *genClassName();
            auto field_count = *rc::gen::inRange(1, 5);

            std::ostringstream fields_oss;
            std::vector<std::string> field_names;
            for (int i = 0; i < field_count; ++i) {
                auto fname = *genFieldName();
                auto tname = *genTypeName();
                field_names.push_back(fname);
                fields_oss << "    val " << fname << ": " << tname << "\n";
            }

            std::string source =
                "class " + class_name + " {\n" +
                fields_oss.str() +
                "}";

            std::vector<expression> results;
            RC_ASSERT(parse_source(source, results));
            RC_ASSERT(!results.empty());

            auto* cd = boost::get<boost::spirit::x3::forward_ast<class_definition>>(&results[0]);
            RC_ASSERT(cd != nullptr);
            const auto& def = cd->get();

            RC_ASSERT(static_cast<int>(def.fields.size()) == field_count);
            for (int i = 0; i < field_count; ++i) {
                RC_ASSERT(def.fields[i].name.name == field_names[i]);
                RC_ASSERT(!def.fields[i].is_mutable);
                RC_ASSERT(def.fields[i].has_explicit_val);
            }
        }
    );
}

/**
 * Feature: explicit-val-annotation, Property 1: C#-style property syntax rejected
 *
 * For any randomly generated class with val-prefixed computed properties using
 * C#-style get/set blocks, the parser SHALL reject with a clear error message
 * suggesting @Property decorator instead.
 *
 * **Validates: Requirements 17.6, 17.7**
 */
TEST(ExplicitValPropertyTest, ClassPropertiesWithValGetSetRejected) {
    rc::check("Class properties with C#-style get/set blocks must be rejected with @Property suggestion",
        []() {
            auto class_name = *genClassName();
            auto prop_name = *genFieldName();
            auto type_name = *genTypeName();

            std::string source =
                "class " + class_name + " {\n"
                "    val " + prop_name + ": " + type_name + " {\n"
                "        get { 42 }\n"
                "    }\n"
                "}";

            std::string error_msg;
            RC_ASSERT(parse_fails_with_error(source, error_msg));
            RC_ASSERT(error_msg.find("C#-style property syntax is not supported in Meld") != std::string::npos);
            RC_ASSERT(error_msg.find("@Property") != std::string::npos);
        }
    );
}

// ===========================================================================
// Property 2: Existing mutability defaults preserved
//
// For any struct/class field declared with `var`, the parser SHALL produce
// `is_mutable == true` and `has_explicit_val == false`.
// For any field declared with no keyword, the parser SHALL produce
// `is_mutable == false` and `has_explicit_val == false`.
//
// **Validates: Requirements 1.2, 1.3, 2.2, 2.3, 3.2, 3.3**
// ===========================================================================

/**
 * Feature: explicit-val-annotation, Property 2
 *
 * Struct fields with `var` keyword must have is_mutable=true, has_explicit_val=false.
 *
 * **Validates: Requirements 1.2, 2.2**
 */
TEST(ExplicitValPropertyTest, VarFieldsAreMutableAndNotTracked) {
    rc::check("Struct fields with var must have is_mutable=true and has_explicit_val=false",
        []() {
            auto struct_name = *genStructName();
            auto field_count = *rc::gen::inRange(1, 5);

            std::ostringstream fields_oss;
            std::vector<std::string> field_names;
            for (int i = 0; i < field_count; ++i) {
                auto fname = *genFieldName();
                auto tname = *genTypeName();
                field_names.push_back(fname);
                fields_oss << "    var " << fname << ": " << tname << "\n";
            }

            std::string source =
                "struct " + struct_name + " {\n" +
                fields_oss.str() +
                "}";

            std::vector<expression> results;
            RC_ASSERT(parse_source(source, results));
            RC_ASSERT(!results.empty());

            auto* sd = boost::get<boost::spirit::x3::forward_ast<struct_definition>>(&results[0]);
            RC_ASSERT(sd != nullptr);
            const auto& def = sd->get();

            RC_ASSERT(static_cast<int>(def.fields.size()) == field_count);
            for (int i = 0; i < field_count; ++i) {
                RC_ASSERT(def.fields[i].is_mutable);
                RC_ASSERT(!def.fields[i].has_explicit_val);
            }
        }
    );
}

/**
 * Feature: explicit-val-annotation, Property 2
 *
 * Struct fields with no keyword now produce mandatory annotation errors.
 * Recovery treats them as immutable with has_explicit_val=false.
 *
 * **Validates: Requirements 1.3, 2.3 (updated for mandatory-val-var-annotations)**
 */
TEST(ExplicitValPropertyTest, NoKeywordFieldsProduceErrors) {
    rc::check("Struct fields with no keyword must produce annotation errors",
        []() {
            auto struct_name = *genStructName();
            auto field_count = *rc::gen::inRange(1, 5);

            std::ostringstream fields_oss;
            std::vector<std::string> field_names;
            for (int i = 0; i < field_count; ++i) {
                auto fname = *genFieldName();
                auto tname = *genTypeName();
                field_names.push_back(fname);
                fields_oss << "    " << fname << ": " << tname << "\n";
            }

            std::string source =
                "struct " + struct_name + " {\n" +
                fields_oss.str() +
                "}";

            std::vector<expression> results;
            std::vector<std::string> errors;
            RC_ASSERT(parse_source_with_errors(source, results, errors));
            RC_ASSERT(!results.empty());

            // Each bare field should produce an error
            RC_ASSERT(static_cast<int>(errors.size()) == field_count);
            for (const auto& err : errors) {
                RC_ASSERT(err.find("Missing mutability annotation") != std::string::npos);
            }

            // Recovery: fields are still in the AST as immutable
            auto* sd = boost::get<boost::spirit::x3::forward_ast<struct_definition>>(&results[0]);
            RC_ASSERT(sd != nullptr);
            const auto& def = sd->get();

            RC_ASSERT(static_cast<int>(def.fields.size()) == field_count);
            for (int i = 0; i < field_count; ++i) {
                RC_ASSERT(!def.fields[i].is_mutable);
                RC_ASSERT(!def.fields[i].has_explicit_val);
            }
        }
    );
}

/**
 * Feature: explicit-val-annotation, Property 2
 *
 * Mixed val/var/bare fields: val and var are accepted, bare produces error.
 * Recovery preserves correct flags on all fields.
 *
 * **Validates: Requirements 1.2, 1.3, 3.2, 3.3 (updated for mandatory-val-var-annotations)**
 */
TEST(ExplicitValPropertyTest, MixedFieldsPreserveCorrectFlags) {
    rc::check("Mixed val/var/bare fields each have correct mutability flags",
        []() {
            auto struct_name = *genStructName();

            auto val_name = *genFieldName();
            auto var_name = *genFieldName();
            auto def_name = *genFieldName();
            auto type1 = *genTypeName();
            auto type2 = *genTypeName();
            auto type3 = *genTypeName();

            RC_PRE(val_name != var_name && var_name != def_name && val_name != def_name);

            std::string source =
                "struct " + struct_name + " {\n"
                "    val " + val_name + ": " + type1 + "\n"
                "    var " + var_name + ": " + type2 + "\n"
                "    " + def_name + ": " + type3 + "\n"
                "}";

            std::vector<expression> results;
            std::vector<std::string> errors;
            RC_ASSERT(parse_source_with_errors(source, results, errors));
            RC_ASSERT(!results.empty());

            // 1 bare field → 1 error
            RC_ASSERT(errors.size() == 1u);
            RC_ASSERT(errors[0].find("Missing mutability annotation") != std::string::npos);

            auto* sd = boost::get<boost::spirit::x3::forward_ast<struct_definition>>(&results[0]);
            RC_ASSERT(sd != nullptr);
            const auto& def = sd->get();

            RC_ASSERT(def.fields.size() == 3u);

            // val field: immutable, tracked
            RC_ASSERT(!def.fields[0].is_mutable);
            RC_ASSERT(def.fields[0].has_explicit_val);

            // var field: mutable, not tracked
            RC_ASSERT(def.fields[1].is_mutable);
            RC_ASSERT(!def.fields[1].has_explicit_val);

            // bare field: recovered as immutable, not tracked
            RC_ASSERT(!def.fields[2].is_mutable);
            RC_ASSERT(!def.fields[2].has_explicit_val);
        }
    );
}

/**
 * Feature: explicit-val-annotation, Property 2
 *
 * Function parameters with no keyword now produce mandatory annotation errors.
 *
 * **Validates: Requirements 3.2, 3.3 (updated for mandatory-val-var-annotations)**
 */
TEST(ExplicitValPropertyTest, NoKeywordParametersProduceErrors) {
    rc::check("Function parameters with no keyword must produce annotation errors",
        []() {
            auto param_name = *genFieldName();
            auto type_name = *genTypeName();

            std::string source =
                "fn test_func(" + param_name + ": " + type_name + ") -> " + type_name + " {\n"
                "    return " + param_name + "\n"
                "}";

            std::vector<expression> results;
            std::vector<std::string> errors;
            RC_ASSERT(parse_source_with_errors(source, results, errors));
            RC_ASSERT(!results.empty());

            // 1 bare parameter → 1 error
            RC_ASSERT(errors.size() == 1u);
            RC_ASSERT(errors[0].find("Missing mutability annotation") != std::string::npos);

            auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&results[0]);
            RC_ASSERT(fd != nullptr);
            const auto& func = fd->get();

            RC_ASSERT(func.parameters.size() == 1u);
            RC_ASSERT(!func.parameters[0].has_explicit_val);
        }
    );
}

// ===========================================================================
// Property 3: Explicit val on function parameters
//
// For any function parameter prefixed with `val`, the parser SHALL produce
// `has_explicit_val == true` and treat the parameter as immutable.
//
// **Validates: Requirements 3.1, 3.4**
// ===========================================================================

/**
 * Feature: explicit-val-annotation, Property 3
 *
 * Function parameters with `val` must have has_explicit_val=true.
 *
 * **Validates: Requirements 3.1, 3.4**
 */
TEST(ExplicitValPropertyTest, ValParametersAreTracked) {
    rc::check("Function parameters with val must have has_explicit_val=true",
        []() {
            auto param_count = *rc::gen::inRange(1, 4);

            std::ostringstream params_oss;
            std::vector<std::string> param_names;
            for (int i = 0; i < param_count; ++i) {
                auto pname = "p_" + std::to_string(*rc::gen::inRange(0, 100));
                auto tname = *genTypeName();
                param_names.push_back(pname);
                if (i > 0) params_oss << ", ";
                params_oss << "val " << pname << ": " << tname;
            }

            std::string ret_type = *genTypeName();
            std::string source =
                "fn test_func(" + params_oss.str() + ") -> " + ret_type + " {\n"
                "    return " + param_names[0] + "\n"
                "}";

            std::vector<expression> results;
            RC_ASSERT(parse_source(source, results));
            RC_ASSERT(!results.empty());

            auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&results[0]);
            RC_ASSERT(fd != nullptr);
            const auto& func = fd->get();

            RC_ASSERT(static_cast<int>(func.parameters.size()) == param_count);
            for (int i = 0; i < param_count; ++i) {
                RC_ASSERT(func.parameters[i].has_explicit_val);
            }
        }
    );
}

// ===========================================================================
// Property 6: Redundant val + @const produces warning
//
// For any function parameter with both `val` and `@const`, the parser SHALL
// emit a warning containing "Redundant annotation: 'val' and '@const' both
// indicate immutability".
//
// **Validates: Requirements 3.5**
// ===========================================================================

/**
 * Feature: explicit-val-annotation, Property 6
 *
 * Parameters with val + @const must produce a redundancy warning.
 *
 * **Validates: Requirements 3.5**
 */
TEST(ExplicitValPropertyTest, ValConstProducesRedundancyWarning) {
    rc::check("val @const on parameter must produce redundancy warning",
        []() {
            auto param_name = *genFieldName();
            auto type_name = *genTypeName();

            std::string source =
                "fn test_func(val @const " + param_name + ": " + type_name + ") -> " + type_name + " {\n"
                "    return " + param_name + "\n"
                "}";

            std::vector<expression> results;
            std::vector<std::string> warnings;
            RC_ASSERT(parse_source_with_warnings(source, results, warnings));
            RC_ASSERT(!warnings.empty());
            RC_ASSERT(warnings[0].find("Redundant annotation: 'val' and '@const' both indicate immutability") != std::string::npos);
        }
    );
}

// ===========================================================================
// Property 7: Conflicting val + @mut produces error
//
// For any function parameter with both `val` and `@mut`, the parser SHALL
// reject the input and produce an error containing "Conflicting mutability
// annotations: 'val' and '@mut' cannot be combined".
//
// **Validates: Requirements 3.6**
// ===========================================================================

/**
 * Feature: explicit-val-annotation, Property 7
 *
 * Parameters with val + @mut must produce a conflicting error.
 *
 * **Validates: Requirements 3.6**
 */
TEST(ExplicitValPropertyTest, ValMutProducesConflictingError) {
    rc::check("val @mut on parameter must produce conflicting error",
        []() {
            auto param_name = *genFieldName();
            auto type_name = *genTypeName();

            std::string source =
                "fn test_func(val @mut " + param_name + ": " + type_name + ") -> " + type_name + " {\n"
                "    return " + param_name + "\n"
                "}";

            std::string error_msg;
            RC_ASSERT(parse_fails_with_error(source, error_msg));
            RC_ASSERT(error_msg.find("Conflicting mutability annotations: 'val' and '@mut' cannot be combined") != std::string::npos);
        }
    );
}

// ===========================================================================
// Property 8: Conflicting val + var produces error with location
//
// For any declaration that uses both `val` and `var` keywords, the parser
// SHALL reject the input and produce an error containing
// "Cannot use both 'val' and 'var' on the same declaration".
//
// **Validates: Requirements 6.1, 6.3**
// ===========================================================================

/**
 * Feature: explicit-val-annotation, Property 8
 *
 * Struct fields with `val var` must produce a parse error.
 *
 * **Validates: Requirements 6.1, 6.3**
 */
TEST(ExplicitValPropertyTest, ValVarOnStructFieldProducesError) {
    rc::check("val var on struct field must produce conflicting error",
        []() {
            auto struct_name = *genStructName();
            auto fname = *genFieldName();
            auto tname = *genTypeName();

            std::string source =
                "struct " + struct_name + " {\n"
                "    val var " + fname + ": " + tname + "\n"
                "}";

            std::string error_msg;
            RC_ASSERT(parse_fails_with_error(source, error_msg));
            RC_ASSERT(error_msg.find("Cannot use both 'val' and 'var' on the same declaration") != std::string::npos);
        }
    );
}

/**
 * Feature: explicit-val-annotation, Property 8
 *
 * Struct fields with `var val` must produce a parse error.
 *
 * **Validates: Requirements 6.1, 6.3**
 */
TEST(ExplicitValPropertyTest, VarValOnStructFieldProducesError) {
    rc::check("var val on struct field must produce conflicting error",
        []() {
            auto struct_name = *genStructName();
            auto fname = *genFieldName();
            auto tname = *genTypeName();

            std::string source =
                "struct " + struct_name + " {\n"
                "    var val " + fname + ": " + tname + "\n"
                "}";

            std::string error_msg;
            RC_ASSERT(parse_fails_with_error(source, error_msg));
            RC_ASSERT(error_msg.find("Cannot use both 'val' and 'var' on the same declaration") != std::string::npos);
        }
    );
}

/**
 * Feature: explicit-val-annotation, Property 8
 *
 * Function parameters with `val var` must produce a parse error.
 *
 * **Validates: Requirements 6.1, 6.3**
 */
TEST(ExplicitValPropertyTest, ValVarOnParameterProducesError) {
    rc::check("val var on function parameter must produce conflicting error",
        []() {
            auto param_name = *genFieldName();
            auto type_name = *genTypeName();

            std::string source =
                "fn test_func(val var " + param_name + ": " + type_name + ") -> " + type_name + " {\n"
                "    return " + param_name + "\n"
                "}";

            std::string error_msg;
            RC_ASSERT(parse_fails_with_error(source, error_msg));
            RC_ASSERT(error_msg.find("Cannot use both 'val' and 'var' on the same declaration") != std::string::npos);
        }
    );
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}