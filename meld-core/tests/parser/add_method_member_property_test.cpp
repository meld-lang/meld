/**
 * Property-based tests for class_definition::add_method() member API.
 *
 * Feature: meld-lang, Property 67: AST Parent Pointer Invariant
 *
 * For any class with methods added via the member add_method() API,
 * the parent pointer invariant must hold: every added method's .parent()
 * returns the class, and find_method()/has_method() correctly reflect
 * the method table state.
 *
 * Uses rapidcheck for property-based testing.
 *
 * **Validates: Requirements 25B.13**
 */

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include "meld/parser/ast.hpp"
#include "meld/parser/ast_node.hpp"
#include "meld/parser/ast_parent_setter.hpp"
#include <string>
#include <set>

using namespace meld::parser;
using namespace meld::parser::ast;

namespace {

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------

rc::Gen<std::string> genMethodName() {
    return rc::gen::map(
        rc::gen::inRange(0, 200),
        [](int n) { return "method_" + std::to_string(n); }
    );
}

rc::Gen<int> genMethodCount() {
    return rc::gen::inRange(1, 8);
}

rc::Gen<int> genParamCount() {
    return rc::gen::inRange(0, 5);
}

} // anonymous namespace

// ===========================================================================
// Property: Member add_method() maintains parent pointer invariant
//
// For any class and any number of methods added via cls.add_method(),
// every method's .parent() must point to the class, every method name's
// .parent() must point to the method, and every parameter's .parent()
// must point to the method.
//
// **Validates: Requirements 25B.13**
// ===========================================================================

TEST(AddMethodMemberPropertyTest, MemberAddMethodMaintainsParentInvariant) {
    rc::check("cls.add_method() wires parent pointers for method, name, and parameters",
        []() {
            ASTParentMap::instance().clear();

            class_definition cls;
            cls.name.name = "TestClass";

            auto method_count = *genMethodCount();

            for (int i = 0; i < method_count; ++i) {
                function_definition method;
                method.name.name = "m_" + std::to_string(i);

                int param_count = *genParamCount();
                for (int j = 0; j < param_count; ++j) {
                    function_parameter param;
                    param.name.name = "p_" + std::to_string(j);
                    method.parameters.push_back(std::move(param));
                }

                cls.add_method(std::move(method));
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

// ===========================================================================
// Property: find_method() and has_method() are consistent with add_method()
//
// For any set of uniquely-named methods added via cls.add_method(),
// has_method(name) returns true for every added name and false for
// names not added. find_method(name) returns a non-null pointer for
// every added name with the correct name field.
//
// **Validates: Requirements 25B.13**
// ===========================================================================

TEST(AddMethodMemberPropertyTest, MethodTableLookupIsConsistentWithAddMethod) {
    rc::check("has_method() and find_method() reflect all methods added via cls.add_method()",
        []() {
            ASTParentMap::instance().clear();

            class_definition cls;
            cls.name.name = "LookupClass";

            auto method_count = *genMethodCount();
            std::set<std::string> added_names;

            for (int i = 0; i < method_count; ++i) {
                std::string name = "fn_" + std::to_string(i);
                added_names.insert(name);

                function_definition method;
                method.name.name = name;
                cls.add_method(std::move(method));
            }

            // Every added name should be findable
            for (const auto& name : added_names) {
                RC_ASSERT(cls.has_method(name));
                auto* found = cls.find_method(name);
                RC_ASSERT(found != nullptr);
                RC_ASSERT(found->name.name == name);
            }

            // A name that was never added should not be findable
            RC_ASSERT(!cls.has_method("__never_added__"));
            RC_ASSERT(cls.find_method("__never_added__") == nullptr);
        }
    );
}

// ===========================================================================
// Property: struct_definition::add_method() maintains parent invariant
//
// Same invariant as class, but for struct_definition.
//
// **Validates: Requirements 25B.13**
// ===========================================================================

TEST(AddMethodMemberPropertyTest, StructMemberAddMethodMaintainsParentInvariant) {
    rc::check("struct.add_method() wires parent pointers correctly",
        []() {
            ASTParentMap::instance().clear();

            struct_definition s;
            s.name.name = "TestStruct";

            auto method_count = *genMethodCount();

            for (int i = 0; i < method_count; ++i) {
                function_definition method;
                method.name.name = "sm_" + std::to_string(i);

                int param_count = *genParamCount();
                for (int j = 0; j < param_count; ++j) {
                    function_parameter param;
                    param.name.name = "sp_" + std::to_string(j);
                    method.parameters.push_back(std::move(param));
                }

                s.add_method(std::move(method));
            }

            RC_ASSERT(static_cast<int>(s.methods.size()) == method_count);

            for (auto& m : s.methods) {
                RC_ASSERT(m.has_parent());
                RC_ASSERT(m.parent().value() == static_cast<ASTNode*>(&s));
                RC_ASSERT(m.name.has_parent());
                RC_ASSERT(m.name.parent().value() == static_cast<ASTNode*>(&m));

                for (auto& p : m.parameters) {
                    RC_ASSERT(p.has_parent());
                    RC_ASSERT(p.parent().value() == static_cast<ASTNode*>(&m));
                }
            }

            // Verify lookup works on struct too
            for (auto& m : s.methods) {
                RC_ASSERT(s.has_method(m.name.name));
                RC_ASSERT(s.find_method(m.name.name) != nullptr);
            }
        }
    );
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
