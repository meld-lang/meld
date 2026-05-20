#include <gtest/gtest.h>
#include "meld/parser/parser.hpp"
#include "meld/kernel/namespace_registry.hpp"
#include "meld/kernel/symbol_table.hpp"

using namespace meld::parser;
using namespace meld::kernel;
namespace x3 = boost::spirit::x3;

TEST(NamespaceTest, ParseSimpleNamespace) {
    std::string code = R"(
        namespace com.example.myapp {
            val x = 42
        }
    )";
    
    Parser parser;
    std::vector<ast::expression> result;
    
    ASSERT_TRUE(parser.parse_file(code, result));
    ASSERT_EQ(result.size(), 1);
    
    auto* ns = boost::get<x3::forward_ast<ast::namespace_declaration>>(&result[0]);
    ASSERT_NE(ns, nullptr);
    
    const auto& ns_decl = ns->get();
    ASSERT_EQ(ns_decl.name_parts.size(), 3);
    EXPECT_EQ(ns_decl.name_parts[0], "com");
    EXPECT_EQ(ns_decl.name_parts[1], "example");
    EXPECT_EQ(ns_decl.name_parts[2], "myapp");
    EXPECT_EQ(ns_decl.body.size(), 1);
}

TEST(NamespaceTest, ParseNestedNamespace) {
    std::string code = R"(
        namespace com.example {
            namespace models {
                val User = 1
            }
        }
    )";
    
    Parser parser;
    std::vector<ast::expression> result;
    
    ASSERT_TRUE(parser.parse_file(code, result));
    ASSERT_EQ(result.size(), 1);
    
    auto* ns = boost::get<x3::forward_ast<ast::namespace_declaration>>(&result[0]);
    ASSERT_NE(ns, nullptr);
    
    const auto& ns_decl = ns->get();
    ASSERT_EQ(ns_decl.name_parts.size(), 2);
    EXPECT_EQ(ns_decl.name_parts[0], "com");
    EXPECT_EQ(ns_decl.name_parts[1], "example");
    ASSERT_EQ(ns_decl.body.size(), 1);
    
    // Check nested namespace
    auto* nested_ns = boost::get<x3::forward_ast<ast::namespace_declaration>>(&ns_decl.body[0].get());
    ASSERT_NE(nested_ns, nullptr);
    
    const auto& nested_decl = nested_ns->get();
    ASSERT_EQ(nested_decl.name_parts.size(), 1);
    EXPECT_EQ(nested_decl.name_parts[0], "models");
}

TEST(NamespaceTest, ParseSpecificImport) {
    std::string code = "imp com.example.User";
    
    Parser parser;
    std::vector<ast::expression> result;
    
    ASSERT_TRUE(parser.parse_file(code, result));
    ASSERT_EQ(result.size(), 1);
    
    auto* imp = boost::get<x3::forward_ast<ast::import_declaration>>(&result[0]);
    ASSERT_NE(imp, nullptr);
    
    const auto& imp_decl = imp->get();
    EXPECT_EQ(imp_decl.import_type, ast::ImportType::IMP_BASIC);
    EXPECT_TRUE(imp_decl.is_imp);
    ASSERT_EQ(imp_decl.namespace_path.size(), 3);
    EXPECT_EQ(imp_decl.namespace_path[0], "com");
    EXPECT_EQ(imp_decl.namespace_path[1], "example");
    EXPECT_EQ(imp_decl.namespace_path[2], "User");
}

TEST(NamespaceTest, ParseWildcardImport) {
    std::string code = "imp com.example.models";
    
    Parser parser;
    std::vector<ast::expression> result;
    
    ASSERT_TRUE(parser.parse_file(code, result));
    ASSERT_EQ(result.size(), 1);
    
    auto* imp = boost::get<x3::forward_ast<ast::import_declaration>>(&result[0]);
    ASSERT_NE(imp, nullptr);
    
    const auto& imp_decl = imp->get();
    EXPECT_EQ(imp_decl.import_type, ast::ImportType::IMP_BASIC);
    EXPECT_TRUE(imp_decl.is_imp);
    ASSERT_EQ(imp_decl.namespace_path.size(), 3);
    EXPECT_EQ(imp_decl.namespace_path[0], "com");
    EXPECT_EQ(imp_decl.namespace_path[1], "example");
    EXPECT_EQ(imp_decl.namespace_path[2], "models");
}

TEST(NamespaceTest, ParseAliasedImport) {
    std::string code = "imp Models = com.example.models";
    
    Parser parser;
    std::vector<ast::expression> result;
    
    ASSERT_TRUE(parser.parse_file(code, result));
    ASSERT_EQ(result.size(), 1);
    
    auto* imp = boost::get<x3::forward_ast<ast::import_declaration>>(&result[0]);
    ASSERT_NE(imp, nullptr);
    
    const auto& imp_decl = imp->get();
    EXPECT_EQ(imp_decl.import_type, ast::ImportType::IMP_ALIASED);
    EXPECT_TRUE(imp_decl.is_imp);
    ASSERT_EQ(imp_decl.namespace_path.size(), 3);
    EXPECT_EQ(imp_decl.namespace_path[0], "com");
    EXPECT_EQ(imp_decl.namespace_path[1], "example");
    EXPECT_EQ(imp_decl.namespace_path[2], "models");
    EXPECT_TRUE(imp_decl.has_alias);
    EXPECT_EQ(imp_decl.alias, "Models");
}

TEST(NamespaceRegistryTest, CreateNamespace) {
    auto& registry = NamespaceRegistry::instance();
    
    auto ns = registry.get_or_create_namespace({"com", "example", "myapp"});
    ASSERT_NE(ns, nullptr);
    EXPECT_EQ(ns->qualified_name(), "com.example.myapp");
}

TEST(NamespaceRegistryTest, RegisterAndLookupSymbol) {
    auto& registry = NamespaceRegistry::instance();
    auto& symbol_table = SymbolTable::instance();
    
    auto ns = registry.get_or_create_namespace({"test", "namespace"});
    auto user_symbol = symbol_table.intern("TestUser");
    ns->register_symbol("TestUser", user_symbol);
    
    auto found = ns->lookup_local("TestUser");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name(), "TestUser");
}

TEST(NamespaceRegistryTest, NestedNamespaces) {
    auto& registry = NamespaceRegistry::instance();
    
    auto parent_ns = registry.get_or_create_namespace({"parent"});
    auto child_ns = registry.get_or_create_namespace({"parent", "child"});
    
    EXPECT_EQ(parent_ns->qualified_name(), "parent");
    EXPECT_EQ(child_ns->qualified_name(), "parent.child");
    EXPECT_EQ(child_ns->parent(), parent_ns.get());
}

TEST(NamespaceRegistryTest, ResolveQualifiedName) {
    auto& registry = NamespaceRegistry::instance();
    auto& symbol_table = SymbolTable::instance();
    
    auto ns = registry.get_or_create_namespace({"resolve", "test"});
    auto product_symbol = symbol_table.intern("Product");
    ns->register_symbol("Product", product_symbol);
    
    auto resolved = registry.resolve_qualified_name("resolve.test.Product");
    ASSERT_NE(resolved, nullptr);
    EXPECT_EQ(resolved->name(), "Product");
}
