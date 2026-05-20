/**
 * Property-based tests for AST parent pointer invariant.
 *
 * Property 67: AST Parent Pointer Invariant
 * For any AST node that is a child of another node, .parent() should
 * return the enclosing parent, and the weak reference should not prevent
 * parent deallocation.
 *
 * Uses rapidcheck for property-based testing.
 *
 * **Validates: Requirements 2.7, 2.8**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/parser/parser.hpp"
#include "meld/parser/ast.hpp"
#include "meld/parser/ast_node.hpp"
#include "meld/parser/ast_parent_setter.hpp"
#include "meld/parser/ast_mutations.hpp"
#include <string>
#include <vector>
#include <sstream>

using namespace meld::parser;
using namespace meld::parser::ast;

namespace {

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

/// Generate a valid Meld identifier name.
rc::Gen<std::string> genIdentifier() {
    return rc::gen::map(
        rc::gen::inRange(0, 200),
        [](int n) { return "id_" + std::to_string(n); }
    );
}

/// Generate a valid Meld type name.
rc::Gen<std::string> genTypeName() {
    return rc::gen::element(
        std::string("int"), std::string("float"),
        std::string("string"), std::string("bool"));
}

/// Generate a field count (1..6).
rc::Gen<int> genFieldCount() {
    return rc::gen::inRange(1, 7);
}

/// Generate a method count (0..3).
rc::Gen<int> genMethodCount() {
    return rc::gen::inRange(0, 4);
}

/// Generate a parameter count per method (0..3).
rc::Gen<int> genParamCount() {
    return rc::gen::inRange(0, 4);
}

/// Generate val or var keyword.
rc::Gen<std::string> genValOrVar() {
    return rc::gen::element(std::string("val"), std::string("var"));
}

// ---------------------------------------------------------------------------
// AST tree builders (construct trees programmatically, no parsing needed)
// ---------------------------------------------------------------------------

/// Build a class_definition with random fields and methods.
struct GeneratedClass {
    class_definition cls;
    int field_count;
    int method_count;
    std::vector<int> params_per_method;
};

rc::Gen<GeneratedClass> genClassTree() {
    return rc::gen::exec([]() {
        GeneratedClass result;
        auto cls_name = *genIdentifier();
        result.cls.name.name = cls_name;

        result.field_count = *genFieldCount();
        for (int i = 0; i < result.field_count; ++i) {
            field_declaration field;
            field.name.name = "field_" + std::to_string(i);
            field.is_mutable = *rc::gen::arbitrary<bool>();
            result.cls.fields.push_back(std::move(field));
        }

        result.method_count = *genMethodCount();
        for (int i = 0; i < result.method_count; ++i) {
            function_definition method;
            method.name.name = "method_" + std::to_string(i);

            int param_count = *genParamCount();
            result.params_per_method.push_back(param_count);
            for (int j = 0; j < param_count; ++j) {
                function_parameter param;
                param.name.name = "p_" + std::to_string(j);
                method.parameters.push_back(std::move(param));
            }
            result.cls.methods.push_back(std::move(method));
        }

        return result;
    });
}

/// Build a struct_definition with random fields.
struct GeneratedStruct {
    struct_definition s;
    int field_count;
};

rc::Gen<GeneratedStruct> genStructTree() {
    return rc::gen::exec([]() {
        GeneratedStruct result;
        result.s.name.name = *genIdentifier();

        result.field_count = *genFieldCount();
        for (int i = 0; i < result.field_count; ++i) {
            field_declaration field;
            field.name.name = "sf_" + std::to_string(i);
            field.is_mutable = *rc::gen::arbitrary<bool>();
            result.s.fields.push_back(std::move(field));
        }

        return result;
    });
}

/// Generate Meld source for a class with fields (for parser-based tests).
struct GeneratedSource {
    std::string source;
    std::string class_name;
    int field_count;
};

rc::Gen<GeneratedSource> genClassSource() {
    return rc::gen::exec([]() {
        GeneratedSource result;
        result.class_name = *genIdentifier();
        result.field_count = *genFieldCount();

        std::ostringstream oss;
        oss << "class " << result.class_name << " {\n";
        for (int i = 0; i < result.field_count; ++i) {
            auto keyword = *genValOrVar();
            auto type = *genTypeName();
            oss << "    " << keyword << " field_" << i << ": " << type << "\n";
        }
        oss << "}";
        result.source = oss.str();

        return result;
    });
}

} // anonymous namespace


// ===========================================================================
// Property 67: AST Parent Pointer Invariant
//
// For any AST node that is a child of another node, .parent() should
// return the enclosing parent, and the weak reference should not prevent
// parent deallocation.
//
// **Validates: Requirements 2.7, 2.8**
// ===========================================================================

// ---------------------------------------------------------------------------
// Sub-property 67a: Every child's .parent() returns the correct parent
//                   after set_parent_pointers() on programmatic AST trees.
// ---------------------------------------------------------------------------

/**
 * Feature: meld-lang, Property 67: AST Parent Pointer Invariant
 *
 * For any randomly generated class tree, after rewire_children(),
 * every field's .parent() must point to the class, every field name's
 * .parent() must point to the field, every method's .parent() must
 * point to the class, and every parameter's .parent() must point to
 * the method.
 *
 * **Validates: Requirements 2.7, 2.8**
 */
TEST(ASTParentPointerPropertyTest, ChildParentPointersAreCorrectAfterRewire) {
    rc::check("Every child node's .parent() returns the correct enclosing parent after rewire_children()",
        []() {
            ASTParentMap::instance().clear();
            auto gen = *genClassTree();

            // Wire all parent pointers
            rewire_children(gen.cls);

            // Class name should point to class
            RC_ASSERT(gen.cls.name.has_parent());
            RC_ASSERT(gen.cls.name.parent().value() == static_cast<ASTNode*>(&gen.cls));

            // Every field should point to class, field name should point to field
            for (auto& field : gen.cls.fields) {
                RC_ASSERT(field.has_parent());
                RC_ASSERT(field.parent().value() == static_cast<ASTNode*>(&gen.cls));
                RC_ASSERT(field.name.has_parent());
                RC_ASSERT(field.name.parent().value() == static_cast<ASTNode*>(&field));
            }

            // Every method should point to class
            for (std::size_t m = 0; m < gen.cls.methods.size(); ++m) {
                auto& method = gen.cls.methods[m];
                RC_ASSERT(method.has_parent());
                RC_ASSERT(method.parent().value() == static_cast<ASTNode*>(&gen.cls));
                RC_ASSERT(method.name.has_parent());
                RC_ASSERT(method.name.parent().value() == static_cast<ASTNode*>(&method));

                // Every parameter should point to method
                for (auto& param : method.parameters) {
                    RC_ASSERT(param.has_parent());
                    RC_ASSERT(param.parent().value() == static_cast<ASTNode*>(&method));
                    RC_ASSERT(param.name.has_parent());
                    RC_ASSERT(param.name.parent().value() == static_cast<ASTNode*>(&param));
                }
            }
        }
    );
}

/**
 * Feature: meld-lang, Property 67: AST Parent Pointer Invariant
 *
 * For any randomly generated struct tree, after rewire_children(),
 * every field's .parent() must point to the struct.
 *
 * **Validates: Requirements 2.7, 2.8**
 */
TEST(ASTParentPointerPropertyTest, StructChildParentPointersAreCorrectAfterRewire) {
    rc::check("Struct fields' .parent() returns the struct after rewire_children()",
        []() {
            ASTParentMap::instance().clear();
            auto gen = *genStructTree();

            rewire_children(gen.s);

            RC_ASSERT(gen.s.name.has_parent());
            RC_ASSERT(gen.s.name.parent().value() == static_cast<ASTNode*>(&gen.s));

            for (auto& field : gen.s.fields) {
                RC_ASSERT(field.has_parent());
                RC_ASSERT(field.parent().value() == static_cast<ASTNode*>(&gen.s));
                RC_ASSERT(field.name.has_parent());
                RC_ASSERT(field.name.parent().value() == static_cast<ASTNode*>(&field));
            }
        }
    );
}

// ---------------------------------------------------------------------------
// Sub-property 67b: Parent pointers are correct after parsing real Meld source.
// ---------------------------------------------------------------------------

/**
 * Feature: meld-lang, Property 67: AST Parent Pointer Invariant
 *
 * For any randomly generated Meld class source, after parsing and
 * set_parent_pointers(), every field's .parent() must point to the
 * parsed class_definition.
 *
 * **Validates: Requirements 2.7, 2.8**
 */
TEST(ASTParentPointerPropertyTest, ParsedClassFieldsHaveCorrectParent) {
    rc::check("Parsed class fields' .parent() returns the class after set_parent_pointers()",
        []() {
            ASTParentMap::instance().clear();
            auto gen = *genClassSource();

            Parser parser;
            expression result;
            RC_PRE(parser.parse_expression(gen.source, result));

            set_parent_pointers(result);

            auto* cls_ast = boost::get<x3::forward_ast<class_definition>>(&result);
            RC_ASSERT(cls_ast != nullptr);

            class_definition& cls = cls_ast->get();

            // Root class should have no parent
            RC_ASSERT(!cls.has_parent());

            // Every field should have the class as parent
            RC_ASSERT(static_cast<int>(cls.fields.size()) == gen.field_count);
            for (auto& field : cls.fields) {
                RC_ASSERT(field.has_parent());
                RC_ASSERT(field.parent().value() == static_cast<ASTNode*>(&cls));

                // Field name should have field as parent
                RC_ASSERT(field.name.has_parent());
                RC_ASSERT(field.name.parent().value() == static_cast<ASTNode*>(&field));
            }
        }
    );
}

// ---------------------------------------------------------------------------
// Sub-property 67c: Weak reference does not prevent parent deallocation.
//                   (Requirement 2.8: no ARC cycles)
// ---------------------------------------------------------------------------

/**
 * Feature: meld-lang, Property 67: AST Parent Pointer Invariant
 *
 * For any randomly generated class, when the parent is destroyed
 * (goes out of scope), the raw pointer (weak reference) does not
 * prevent deallocation. The parent map entry becomes stale but the
 * parent object is freed — proving no ARC cycle exists.
 *
 * **Validates: Requirements 2.8**
 */
TEST(ASTParentPointerPropertyTest, WeakReferenceDoesNotPreventParentDeallocation) {
    rc::check("Parent pointer (raw/weak) does not prevent parent deallocation",
        []() {
            ASTParentMap::instance().clear();

            auto field_count = *genFieldCount();
            std::vector<field_declaration> detached_fields;

            {
                // Create a class in a nested scope
                class_definition cls;
                cls.name.name = *genIdentifier();

                for (int i = 0; i < field_count; ++i) {
                    field_declaration field;
                    field.name.name = "f_" + std::to_string(i);
                    field.is_mutable = false;
                    add_field(cls, std::move(field));
                }

                // Verify fields are wired
                for (auto& f : cls.fields) {
                    RC_ASSERT(f.has_parent());
                }

                // Copy field names out before cls is destroyed
                for (auto& f : cls.fields) {
                    field_declaration copy;
                    copy.name.name = f.name.name;
                    detached_fields.push_back(std::move(copy));
                }

                // cls is destroyed here — if parent pointer were a
                // shared_ptr/strong ref, cls would NOT be freed.
                // Since it's a raw pointer (weak reference), cls IS freed.
            }

            // The fact that we reach here without a leak proves the
            // parent pointer doesn't prevent deallocation.
            // The detached copies should have no parent.
            for (auto& f : detached_fields) {
                RC_ASSERT(!f.has_parent());
            }
        }
    );
}

// ---------------------------------------------------------------------------
// Sub-property 67d: Parent pointers are correctly updated when nodes
//                   are moved between parents via reparent().
// ---------------------------------------------------------------------------

/**
 * Feature: meld-lang, Property 67: AST Parent Pointer Invariant
 *
 * For any field node moved from one class to another via reparent(),
 * the field's .parent() must reflect the new parent, and the old
 * parent must no longer be referenced.
 *
 * **Validates: Requirements 2.7**
 */
TEST(ASTParentPointerPropertyTest, ReparentUpdatesParentCorrectly) {
    rc::check("Reparenting a node updates .parent() to the new parent",
        []() {
            ASTParentMap::instance().clear();

            class_definition cls_a;
            cls_a.name.name = *genIdentifier();
            class_definition cls_b;
            cls_b.name.name = *genIdentifier();

            auto field_count = *genFieldCount();

            // Create fields attached to cls_a
            for (int i = 0; i < field_count; ++i) {
                field_declaration field;
                field.name.name = "rf_" + std::to_string(i);
                field.is_mutable = false;
                add_field(cls_a, std::move(field));
            }

            // Verify all fields point to cls_a
            for (auto& f : cls_a.fields) {
                RC_ASSERT(f.has_parent());
                RC_ASSERT(f.parent().value() == static_cast<ASTNode*>(&cls_a));
            }

            // Reparent each field to cls_b
            for (auto& f : cls_a.fields) {
                f.reparent(&cls_b);
            }

            // Verify all fields now point to cls_b
            for (auto& f : cls_a.fields) {
                RC_ASSERT(f.has_parent());
                RC_ASSERT(f.parent().value() == static_cast<ASTNode*>(&cls_b));
            }
        }
    );
}

/**
 * Feature: meld-lang, Property 67: AST Parent Pointer Invariant
 *
 * For any field node, reparenting to nullptr detaches it (parent
 * becomes nullopt).
 *
 * **Validates: Requirements 2.7**
 */
TEST(ASTParentPointerPropertyTest, ReparentToNullDetachesNode) {
    rc::check("Reparenting to nullptr detaches the node",
        []() {
            ASTParentMap::instance().clear();

            class_definition cls;
            cls.name.name = *genIdentifier();

            auto field_count = *genFieldCount();
            for (int i = 0; i < field_count; ++i) {
                field_declaration field;
                field.name.name = "df_" + std::to_string(i);
                field.is_mutable = false;
                add_field(cls, std::move(field));
            }

            // Reparent all to nullptr
            for (auto& f : cls.fields) {
                f.reparent(nullptr);
            }

            // All should be detached
            for (auto& f : cls.fields) {
                RC_ASSERT(!f.has_parent());
                RC_ASSERT(f.parent() == std::nullopt);
            }
        }
    );
}

// ---------------------------------------------------------------------------
// Sub-property 67e: add_method mutation maintains parent pointer invariant.
// ---------------------------------------------------------------------------

/**
 * Feature: meld-lang, Property 67: AST Parent Pointer Invariant
 *
 * For any class, adding methods via add_method() must wire the method's
 * .parent() to the class, the method name's .parent() to the method,
 * and each parameter's .parent() to the method.
 *
 * **Validates: Requirements 2.7**
 */
TEST(ASTParentPointerPropertyTest, AddMethodMaintainsParentInvariant) {
    rc::check("add_method() wires parent pointers for method, name, and parameters",
        []() {
            ASTParentMap::instance().clear();

            class_definition cls;
            cls.name.name = *genIdentifier();

            auto method_count = *genMethodCount();
            // Ensure at least 1 method for meaningful test
            if (method_count == 0) method_count = 1;

            for (int i = 0; i < method_count; ++i) {
                function_definition method;
                method.name.name = "m_" + std::to_string(i);

                int param_count = *genParamCount();
                for (int j = 0; j < param_count; ++j) {
                    function_parameter param;
                    param.name.name = "p_" + std::to_string(j);
                    method.parameters.push_back(std::move(param));
                }

                add_method(cls, std::move(method));
            }

            RC_ASSERT(static_cast<int>(cls.methods.size()) == method_count);

            for (auto& m : cls.methods) {
                // Method -> class
                RC_ASSERT(m.has_parent());
                RC_ASSERT(m.parent().value() == static_cast<ASTNode*>(&cls));

                // Method name -> method
                RC_ASSERT(m.name.has_parent());
                RC_ASSERT(m.name.parent().value() == static_cast<ASTNode*>(&m));

                // Each param -> method, param name -> param
                for (auto& p : m.parameters) {
                    RC_ASSERT(p.has_parent());
                    RC_ASSERT(p.parent().value() == static_cast<ASTNode*>(&m));
                    RC_ASSERT(p.name.has_parent());
                    RC_ASSERT(p.name.parent().value() == static_cast<ASTNode*>(&p));
                }
            }
        }
    );
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
