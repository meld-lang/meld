#include <gtest/gtest.h>
#include "meld/compiler/module_definition.hpp"
#include "meld/parser/parser.hpp"
#include "meld/macro/attribute.hpp"

using namespace meld::compiler;
using namespace meld::parser;
namespace x3 = boost::spirit::x3;

// ============================================================================
// ModuleDefinition::derive_module_name / derive_module_path
// ============================================================================

TEST(ModuleDefinitionTest, DeriveModuleNameFromSimplePath) {
    EXPECT_EQ(ModuleDefinition::derive_module_name("math.meld"), "math");
}

TEST(ModuleDefinitionTest, DeriveModuleNameFromNestedPath) {
    EXPECT_EQ(ModuleDefinition::derive_module_name("app/services/auth.meld"),
              "app.services.auth");
}

TEST(ModuleDefinitionTest, DeriveModuleNameFromWindowsPath) {
    EXPECT_EQ(ModuleDefinition::derive_module_name("app\\services\\auth.meld"),
              "app.services.auth");
}

TEST(ModuleDefinitionTest, DeriveModuleNameStripsLeadingDotSlash) {
    EXPECT_EQ(ModuleDefinition::derive_module_name("./app/services/auth.meld"),
              "app.services.auth");
}

TEST(ModuleDefinitionTest, DeriveModulePathSegments) {
    auto path = ModuleDefinition::derive_module_path("app/services/auth.meld");
    ASSERT_EQ(path.size(), 3);
    EXPECT_EQ(path[0], "app");
    EXPECT_EQ(path[1], "services");
    EXPECT_EQ(path[2], "auth");
}

TEST(ModuleDefinitionTest, DeriveModulePathSingleFile) {
    auto path = ModuleDefinition::derive_module_path("prelude.meld");
    ASSERT_EQ(path.size(), 1);
    EXPECT_EQ(path[0], "prelude");
}

// ============================================================================
// Constructor
// ============================================================================

TEST(ModuleDefinitionTest, ConstructorSetsModuleName) {
    ModuleDefinition mod("app/services/auth.meld");
    EXPECT_EQ(mod.module_name(), "app.services.auth");
    EXPECT_EQ(mod.file_path(), "app/services/auth.meld");
    ASSERT_EQ(mod.module_path().size(), 3);
    EXPECT_EQ(mod.module_path()[0], "app");
    EXPECT_EQ(mod.module_path()[1], "services");
    EXPECT_EQ(mod.module_path()[2], "auth");
}

// ============================================================================
// build_from_expressions — all top-level declarations exported by default
// ============================================================================

TEST(ModuleDefinitionTest, ExportsValDeclaration) {
    // Parse: val PI = 3.14159
    Parser parser;
    std::vector<ast::expression> exprs;
    ASSERT_TRUE(parser.parse_file("val PI = 3.14159", exprs));
    
    ModuleDefinition mod("math.meld");
    mod.build_from_expressions(exprs);
    
    ASSERT_EQ(mod.symbols().size(), 1);
    EXPECT_EQ(mod.symbols()[0].name, "PI");
    EXPECT_EQ(mod.symbols()[0].kind, ModuleSymbol::Kind::VAL);
    EXPECT_EQ(mod.symbols()[0].visibility, SymbolVisibility::PUBLIC);
    EXPECT_TRUE(mod.is_exported("PI"));
}

TEST(ModuleDefinitionTest, ExportsVarDeclaration) {
    Parser parser;
    std::vector<ast::expression> exprs;
    ASSERT_TRUE(parser.parse_file("var counter = 0", exprs));
    
    ModuleDefinition mod("state.meld");
    mod.build_from_expressions(exprs);
    
    ASSERT_EQ(mod.symbols().size(), 1);
    EXPECT_EQ(mod.symbols()[0].name, "counter");
    EXPECT_EQ(mod.symbols()[0].kind, ModuleSymbol::Kind::VAR);
    EXPECT_TRUE(mod.is_exported("counter"));
}

TEST(ModuleDefinitionTest, ExportsFunctionDefinition) {
    Parser parser;
    std::vector<ast::expression> exprs;
    ASSERT_TRUE(parser.parse_file("fnc add(a: int, b: int) -> int { a }", exprs));
    
    ModuleDefinition mod("math.meld");
    mod.build_from_expressions(exprs);
    
    ASSERT_EQ(mod.symbols().size(), 1);
    EXPECT_EQ(mod.symbols()[0].name, "add");
    EXPECT_EQ(mod.symbols()[0].kind, ModuleSymbol::Kind::FUNCTION);
    EXPECT_TRUE(mod.is_exported("add"));
}

TEST(ModuleDefinitionTest, ExportsMultipleDeclarations) {
    std::string code = R"(
        val PI = 3.14
        var count = 0
        fnc increment() -> int { count }
    )";
    
    Parser parser;
    std::vector<ast::expression> exprs;
    ASSERT_TRUE(parser.parse_file(code, exprs));
    
    ModuleDefinition mod("utils.meld");
    mod.build_from_expressions(exprs);
    
    EXPECT_EQ(mod.symbols().size(), 3);
    EXPECT_TRUE(mod.has_symbol("PI"));
    EXPECT_TRUE(mod.has_symbol("count"));
    EXPECT_TRUE(mod.has_symbol("increment"));
    EXPECT_TRUE(mod.is_exported("PI"));
    EXPECT_TRUE(mod.is_exported("count"));
    EXPECT_TRUE(mod.is_exported("increment"));
}

// ============================================================================
// @private annotation — restricts visibility
// ============================================================================

TEST(ModuleDefinitionTest, PrivateAnnotationHidesSymbol) {
    Parser parser;
    std::vector<ast::expression> exprs;
    ASSERT_TRUE(parser.parse_file("val SECRET = 42", exprs));
    
    // Simulate @private by passing the symbol name in the private set
    std::unordered_set<std::string> private_syms = {"SECRET"};
    
    ModuleDefinition mod("auth.meld");
    mod.build_from_expressions(exprs, private_syms);
    
    ASSERT_EQ(mod.symbols().size(), 1);
    EXPECT_EQ(mod.symbols()[0].name, "SECRET");
    EXPECT_EQ(mod.symbols()[0].visibility, SymbolVisibility::PRIVATE);
    EXPECT_FALSE(mod.is_exported("SECRET"));
    EXPECT_TRUE(mod.has_symbol("SECRET"));
}

TEST(ModuleDefinitionTest, MixedPublicAndPrivateSymbols) {
    std::string code = R"(
        val SECRET_KEY = 42
        fnc authenticate() -> int { SECRET_KEY }
    )";
    
    Parser parser;
    std::vector<ast::expression> exprs;
    ASSERT_TRUE(parser.parse_file(code, exprs));
    
    std::unordered_set<std::string> private_syms = {"SECRET_KEY"};
    
    ModuleDefinition mod("auth.meld");
    mod.build_from_expressions(exprs, private_syms);
    
    EXPECT_EQ(mod.symbols().size(), 2);
    
    // SECRET_KEY is private
    auto* secret = mod.find_symbol("SECRET_KEY");
    ASSERT_NE(secret, nullptr);
    EXPECT_EQ(secret->visibility, SymbolVisibility::PRIVATE);
    EXPECT_FALSE(mod.is_exported("SECRET_KEY"));
    
    // authenticate is public
    auto* auth = mod.find_symbol("authenticate");
    ASSERT_NE(auth, nullptr);
    EXPECT_EQ(auth->visibility, SymbolVisibility::PUBLIC);
    EXPECT_TRUE(mod.is_exported("authenticate"));
}

TEST(ModuleDefinitionTest, PublicSymbolsFilter) {
    std::string code = R"(
        val PUBLIC_VAL = 1
        val PRIVATE_VAL = 2
        fnc public_fn() -> int { 0 }
    )";
    
    Parser parser;
    std::vector<ast::expression> exprs;
    ASSERT_TRUE(parser.parse_file(code, exprs));
    
    std::unordered_set<std::string> private_syms = {"PRIVATE_VAL"};
    
    ModuleDefinition mod("mixed.meld");
    mod.build_from_expressions(exprs, private_syms);
    
    auto pub = mod.public_symbols();
    EXPECT_EQ(pub.size(), 2);
    
    auto priv = mod.private_symbols();
    EXPECT_EQ(priv.size(), 1);
    EXPECT_EQ(priv[0]->name, "PRIVATE_VAL");
}

// ============================================================================
// Import declarations are NOT exported as symbols
// ============================================================================

TEST(ModuleDefinitionTest, ImportsAreNotExportedSymbols) {
    std::string code = R"(
        imp std.math
        val x = 42
    )";
    
    Parser parser;
    std::vector<ast::expression> exprs;
    ASSERT_TRUE(parser.parse_file(code, exprs));
    
    ModuleDefinition mod("app.meld");
    mod.build_from_expressions(exprs);
    
    // Only val x should be a symbol, not the import
    EXPECT_EQ(mod.symbols().size(), 1);
    EXPECT_TRUE(mod.has_symbol("x"));
    EXPECT_FALSE(mod.has_symbol("std"));
    EXPECT_FALSE(mod.has_symbol("math"));
}

// ============================================================================
// find_symbol returns nullptr for unknown symbols
// ============================================================================

TEST(ModuleDefinitionTest, FindSymbolReturnsNullForUnknown) {
    ModuleDefinition mod("empty.meld");
    EXPECT_EQ(mod.find_symbol("nonexistent"), nullptr);
    EXPECT_FALSE(mod.has_symbol("nonexistent"));
    EXPECT_FALSE(mod.is_exported("nonexistent"));
}

// ============================================================================
// register_in_namespace integration
// ============================================================================

TEST(ModuleDefinitionTest, RegisterInNamespaceOnlyExportsPublic) {
    std::string code = R"(
        val PUBLIC_VAL = 1
        val PRIVATE_VAL = 2
    )";
    
    Parser parser;
    std::vector<ast::expression> exprs;
    ASSERT_TRUE(parser.parse_file(code, exprs));
    
    std::unordered_set<std::string> private_syms = {"PRIVATE_VAL"};
    
    ModuleDefinition mod("test.meld");
    mod.build_from_expressions(exprs, private_syms);
    
    // Create a namespace scope and register
    auto scope = std::make_shared<meld::kernel::NamespaceScope>(
        std::vector<std::string>{"test"});
    mod.register_in_namespace(scope);
    
    // PUBLIC_VAL should be registered
    EXPECT_NE(scope->lookup_local("PUBLIC_VAL"), nullptr);
    
    // PRIVATE_VAL should NOT be registered
    EXPECT_EQ(scope->lookup_local("PRIVATE_VAL"), nullptr);
}

// ============================================================================
// ModuleRegistry
// ============================================================================

TEST(ModuleRegistryTest, RegisterAndFindByName) {
    auto& registry = ModuleRegistry::instance();
    registry.clear();
    
    auto mod = std::make_shared<ModuleDefinition>("app/services/auth.meld");
    registry.register_module(mod);
    
    auto found = registry.find_module("app.services.auth");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->module_name(), "app.services.auth");
}

TEST(ModuleRegistryTest, RegisterAndFindByPath) {
    auto& registry = ModuleRegistry::instance();
    registry.clear();
    
    auto mod = std::make_shared<ModuleDefinition>("app/services/auth.meld");
    registry.register_module(mod);
    
    auto found = registry.find_module_by_path("app/services/auth.meld");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->module_name(), "app.services.auth");
}

TEST(ModuleRegistryTest, FindReturnsNullForUnknown) {
    auto& registry = ModuleRegistry::instance();
    registry.clear();
    
    EXPECT_EQ(registry.find_module("nonexistent"), nullptr);
    EXPECT_EQ(registry.find_module_by_path("nonexistent.meld"), nullptr);
}

TEST(ModuleRegistryTest, ClearRemovesAllModules) {
    auto& registry = ModuleRegistry::instance();
    registry.clear();
    
    registry.register_module(std::make_shared<ModuleDefinition>("a.meld"));
    registry.register_module(std::make_shared<ModuleDefinition>("b.meld"));
    EXPECT_EQ(registry.modules().size(), 2);
    
    registry.clear();
    EXPECT_EQ(registry.modules().size(), 0);
}

// ============================================================================
// @private attribute registration
// ============================================================================

TEST(PrivateAttributeTest, PrivateAttributeIsRegistered) {
    // Ensure standard attributes are registered
    meld::macro::register_standard_attributes();
    
    auto& attr_registry = meld::macro::AttributeMacroRegistry::instance();
    EXPECT_TRUE(attr_registry.has_attribute("private"));
    
    auto attr = attr_registry.get_attribute("private");
    ASSERT_TRUE(attr.has_value());
    
    // Verify it can be applied to functions, structs, enums, fields, and modules
    EXPECT_TRUE(attr.value()->can_apply_to(meld::macro::AttributeTarget::Function));
    EXPECT_TRUE(attr.value()->can_apply_to(meld::macro::AttributeTarget::Struct));
    EXPECT_TRUE(attr.value()->can_apply_to(meld::macro::AttributeTarget::Enum));
    EXPECT_TRUE(attr.value()->can_apply_to(meld::macro::AttributeTarget::Field));
    EXPECT_TRUE(attr.value()->can_apply_to(meld::macro::AttributeTarget::Module));
}
