/**
 * Property-based tests for mandatory val/var annotation enforcement.
 *
 * Properties 1-6 from the design document.
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

rc::Gen<std::string> genFieldName() {
    return rc::gen::map(rc::gen::inRange(0, 100),
        [](int n) { return "f_" + std::to_string(n); });
}

rc::Gen<std::string> genTypeName() {
    return rc::gen::element(
        std::string("Int"), std::string("Float"),
        std::string("String"), std::string("Bool"));
}

rc::Gen<std::string> genStructName() {
    return rc::gen::map(rc::gen::inRange(0, 100),
        [](int n) { return "TestStruct" + std::to_string(n); });
}

rc::Gen<std::string> genClassName() {
    return rc::gen::map(rc::gen::inRange(0, 100),
        [](int n) { return "TestClass" + std::to_string(n); });
}

rc::Gen<std::string> genEnumName() {
    return rc::gen::map(rc::gen::inRange(0, 100),
        [](int n) { return "TestEnum" + std::to_string(n); });
}

struct ParseResult {
    bool structural_success;
    std::vector<expression> ast;
    std::vector<std::string> errors;
};

ParseResult parse_with_errors(const std::string& source) {
    ParseResult result;
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (!lexer.errors().empty()) {
        result.structural_success = false;
        return result;
    }
    TokenParser parser(tokens);
    result.structural_success = parser.parse_file(result.ast);
    result.errors = parser.errors();
    return result;
}

} // anonymous namespace

// ===========================================================================
// Feature: mandatory-val-var-annotations, Property 1:
// val/var on struct/class/enum members sets correct AST flags
//
// Validates: Requirements 1.1, 1.2, 2.1, 2.2, 3.1, 3.2, 5.1, 5.2
// ===========================================================================

TEST(MandatoryValVarPropertyTest, ValOnStructFieldsSetsCorrectFlags) {
    rc::check("val on struct fields: is_mutable=false, has_explicit_val=true",
        []() {
            auto name = *genStructName();
            auto count = *rc::gen::inRange(1, 5);
            std::ostringstream oss;
            std::vector<std::string> names;
            for (int i = 0; i < count; ++i) {
                auto fn = *genFieldName();
                auto tn = *genTypeName();
                names.push_back(fn);
                oss << "    val " << fn << ": " << tn << "\n";
            }
            std::string source = "struct " + name + " {\n" + oss.str() + "}";
            auto result = parse_with_errors(source);
            RC_ASSERT(result.structural_success);
            RC_ASSERT(result.errors.empty());
            auto* sd = boost::get<boost::spirit::x3::forward_ast<struct_definition>>(&result.ast[0]);
            RC_ASSERT(sd != nullptr);
            for (int i = 0; i < count; ++i) {
                RC_ASSERT(!sd->get().fields[i].is_mutable);
                RC_ASSERT(sd->get().fields[i].has_explicit_val);
            }
        });
}

TEST(MandatoryValVarPropertyTest, VarOnStructFieldsSetsCorrectFlags) {
    rc::check("var on struct fields: is_mutable=true, has_explicit_val=false",
        []() {
            auto name = *genStructName();
            auto count = *rc::gen::inRange(1, 5);
            std::ostringstream oss;
            for (int i = 0; i < count; ++i) {
                oss << "    var " << *genFieldName() << ": " << *genTypeName() << "\n";
            }
            std::string source = "struct " + name + " {\n" + oss.str() + "}";
            auto result = parse_with_errors(source);
            RC_ASSERT(result.structural_success);
            RC_ASSERT(result.errors.empty());
            auto* sd = boost::get<boost::spirit::x3::forward_ast<struct_definition>>(&result.ast[0]);
            RC_ASSERT(sd != nullptr);
            for (int i = 0; i < count; ++i) {
                RC_ASSERT(sd->get().fields[i].is_mutable);
                RC_ASSERT(!sd->get().fields[i].has_explicit_val);
            }
        });
}

TEST(MandatoryValVarPropertyTest, ValOnClassFieldsSetsCorrectFlags) {
    rc::check("val on class fields: is_mutable=false, has_explicit_val=true",
        []() {
            auto name = *genClassName();
            auto fn = *genFieldName();
            auto tn = *genTypeName();
            std::string source = "class " + name + " {\n    val " + fn + ": " + tn + "\n}";
            auto result = parse_with_errors(source);
            RC_ASSERT(result.structural_success);
            RC_ASSERT(result.errors.empty());
            auto* cd = boost::get<boost::spirit::x3::forward_ast<class_definition>>(&result.ast[0]);
            RC_ASSERT(cd != nullptr);
            RC_ASSERT(!cd->get().fields[0].is_mutable);
            RC_ASSERT(cd->get().fields[0].has_explicit_val);
        });
}

TEST(MandatoryValVarPropertyTest, ValOnEnumVariantFieldsSetsCorrectFlags) {
    rc::check("val on enum variant fields: is_mutable=false, has_explicit_val=true",
        []() {
            auto ename = *genEnumName();
            auto fn = *genFieldName();
            auto tn = *genTypeName();
            std::string source = "enum " + ename + " {\n    Variant(val " + fn + ": " + tn + ")\n}";
            auto result = parse_with_errors(source);
            RC_ASSERT(result.structural_success);
            RC_ASSERT(result.errors.empty());
            auto* ed = boost::get<boost::spirit::x3::forward_ast<enum_definition>>(&result.ast[0]);
            RC_ASSERT(ed != nullptr);
            RC_ASSERT(ed->get().variants[0].associated_fields[0].has_explicit_val);
            RC_ASSERT(!ed->get().variants[0].associated_fields[0].is_mutable);
        });
}

// ===========================================================================
// Feature: mandatory-val-var-annotations, Property 2:
// Bare struct/class/enum members produce error
//
// Validates: Requirements 1.3, 2.3, 3.3, 5.3
// ===========================================================================

TEST(MandatoryValVarPropertyTest, BareStructFieldsProduceError) {
    rc::check("Bare struct fields produce missing annotation error",
        []() {
            auto name = *genStructName();
            auto count = *rc::gen::inRange(1, 4);
            std::ostringstream oss;
            for (int i = 0; i < count; ++i) {
                oss << "    " << *genFieldName() << ": " << *genTypeName() << "\n";
            }
            std::string source = "struct " + name + " {\n" + oss.str() + "}";
            auto result = parse_with_errors(source);
            RC_ASSERT(result.structural_success);
            RC_ASSERT(static_cast<int>(result.errors.size()) == count);
            for (const auto& err : result.errors) {
                RC_ASSERT(err.find("Missing mutability annotation") != std::string::npos);
            }
        });
}

TEST(MandatoryValVarPropertyTest, BareClassFieldsProduceError) {
    rc::check("Bare class fields produce missing annotation error",
        []() {
            auto name = *genClassName();
            auto fn = *genFieldName();
            auto tn = *genTypeName();
            std::string source = "class " + name + " {\n    " + fn + ": " + tn + "\n}";
            auto result = parse_with_errors(source);
            RC_ASSERT(result.structural_success);
            RC_ASSERT(result.errors.size() == 1u);
            RC_ASSERT(result.errors[0].find("Missing mutability annotation") != std::string::npos);
        });
}

TEST(MandatoryValVarPropertyTest, BareEnumVariantFieldsProduceError) {
    rc::check("Bare enum variant fields produce missing annotation error",
        []() {
            auto ename = *genEnumName();
            auto fn = *genFieldName();
            auto tn = *genTypeName();
            std::string source = "enum " + ename + " {\n    Variant(" + fn + ": " + tn + ")\n}";
            auto result = parse_with_errors(source);
            RC_ASSERT(result.structural_success);
            RC_ASSERT(result.errors.size() == 1u);
            RC_ASSERT(result.errors[0].find("Missing mutability annotation") != std::string::npos);
        });
}

// ===========================================================================
// Feature: mandatory-val-var-annotations, Property 3:
// val/var on function parameters sets correct flags
//
// Validates: Requirements 4.1, 4.2
// ===========================================================================

TEST(MandatoryValVarPropertyTest, ValOnFunctionParametersSetsCorrectFlags) {
    rc::check("val on function parameters: has_explicit_val=true, is_mutable=false",
        []() {
            auto count = *rc::gen::inRange(1, 4);
            std::ostringstream oss;
            for (int i = 0; i < count; ++i) {
                if (i > 0) oss << ", ";
                oss << "val p_" << i << ": " << *genTypeName();
            }
            std::string source = "fn test_func(" + oss.str() + ") -> Int {\n    return p_0\n}";
            auto result = parse_with_errors(source);
            RC_ASSERT(result.structural_success);
            RC_ASSERT(result.errors.empty());
            auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&result.ast[0]);
            RC_ASSERT(fd != nullptr);
            for (int i = 0; i < count; ++i) {
                RC_ASSERT(fd->get().parameters[i].has_explicit_val);
                RC_ASSERT(!fd->get().parameters[i].is_mutable);
            }
        });
}

TEST(MandatoryValVarPropertyTest, VarOnFunctionParametersSetsCorrectFlags) {
    rc::check("var on function parameters: is_mutable=true",
        []() {
            auto pname = *genFieldName();
            auto tname = *genTypeName();
            std::string source = "fn test_func(" + pname + ": var " + tname + ") -> " + tname + " {\n    return " + pname + "\n}";
            auto result = parse_with_errors(source);
            RC_ASSERT(result.structural_success);
            RC_ASSERT(result.errors.empty());
            auto* fd = boost::get<boost::spirit::x3::forward_ast<function_definition>>(&result.ast[0]);
            RC_ASSERT(fd != nullptr);
            RC_ASSERT(fd->get().parameters[0].is_mutable);
        });
}

// ===========================================================================
// Feature: mandatory-val-var-annotations, Property 4:
// Bare function parameters produce error
//
// Validates: Requirements 4.3
// ===========================================================================

TEST(MandatoryValVarPropertyTest, BareFunctionParametersProduceError) {
    rc::check("Bare function parameters produce missing annotation error",
        []() {
            auto pname = *genFieldName();
            auto tname = *genTypeName();
            std::string source = "fn test_func(" + pname + ": " + tname + ") -> " + tname + " {\n    return " + pname + "\n}";
            auto result = parse_with_errors(source);
            RC_ASSERT(result.structural_success);
            RC_ASSERT(result.errors.size() == 1u);
            RC_ASSERT(result.errors[0].find("Missing mutability annotation") != std::string::npos);
        });
}

// ===========================================================================
// Feature: mandatory-val-var-annotations, Property 5:
// Mutability decorators accepted as annotation
//
// Validates: Requirements 4.4, 4.5
// ===========================================================================

TEST(MandatoryValVarPropertyTest, ConstDecoratorAcceptedAsAnnotation) {
    rc::check("@const decorator on parameter: no missing annotation error",
        []() {
            auto pname = *genFieldName();
            auto tname = *genTypeName();
            std::string source = "fn test_func(@const " + pname + ": " + tname + ") -> " + tname + " {\n    return " + pname + "\n}";
            auto result = parse_with_errors(source);
            RC_ASSERT(result.structural_success);
            RC_ASSERT(result.errors.empty());
        });
}

TEST(MandatoryValVarPropertyTest, MutDecoratorAcceptedAsAnnotation) {
    rc::check("@mut decorator on parameter: no missing annotation error",
        []() {
            auto pname = *genFieldName();
            auto tname = *genTypeName();
            std::string source = "fn test_func(@mut " + pname + ": " + tname + ") -> " + tname + " {\n    return " + pname + "\n}";
            auto result = parse_with_errors(source);
            RC_ASSERT(result.structural_success);
            RC_ASSERT(result.errors.empty());
        });
}

// ===========================================================================
// Feature: mandatory-val-var-annotations, Property 6:
// Error recovery reports all missing annotations
//
// Validates: Requirements 6.1, 6.2
// ===========================================================================

TEST(MandatoryValVarPropertyTest, ErrorRecoveryReportsAllMissingAnnotations) {
    rc::check("N bare fields produce exactly N errors in a single pass",
        []() {
            auto name = *genStructName();
            auto count = *rc::gen::inRange(1, 6);
            std::ostringstream oss;
            for (int i = 0; i < count; ++i) {
                oss << "    f_" << i << ": " << *genTypeName() << "\n";
            }
            std::string source = "struct " + name + " {\n" + oss.str() + "}";
            auto result = parse_with_errors(source);
            RC_ASSERT(result.structural_success);
            RC_ASSERT(static_cast<int>(result.errors.size()) == count);

            // All recovered as immutable
            auto* sd = boost::get<boost::spirit::x3::forward_ast<struct_definition>>(&result.ast[0]);
            RC_ASSERT(sd != nullptr);
            RC_ASSERT(static_cast<int>(sd->get().fields.size()) == count);
            for (int i = 0; i < count; ++i) {
                RC_ASSERT(!sd->get().fields[i].is_mutable);
            }
        });
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
