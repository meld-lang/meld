#include "meld/daemon/semantic_model.hpp"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace meld::daemon {
namespace {

class SemanticModelTest : public ::testing::Test {
protected:
    SemanticModel model;

    FileSemantics make_file(const std::string& path, const std::string& name) {
        FileSemantics fs;
        fs.path = path;
        auto root = std::make_shared<ASTNode>();
        root->kind = "module";
        root->name = name;
        root->location = {path, 1, 1};
        fs.ast = root;
        return fs;
    }
};

TEST_F(SemanticModelTest, EmptyModelHasNoFiles) {
    EXPECT_EQ(model.file_count(), 0u);
    EXPECT_TRUE(model.get_indexed_files().empty());
}

TEST_F(SemanticModelTest, UpdateAndRetrieveFile) {
    auto sem = make_file("test.meld", "test");
    model.update_file("test.meld", std::move(sem));

    EXPECT_EQ(model.file_count(), 1u);
    EXPECT_TRUE(model.has_file("test.meld"));

    auto ast = model.get_ast("test.meld");
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->name, "test");
}

TEST_F(SemanticModelTest, RemoveFile) {
    model.update_file("a.meld", make_file("a.meld", "a"));
    model.update_file("b.meld", make_file("b.meld", "b"));
    EXPECT_EQ(model.file_count(), 2u);

    model.remove_file("a.meld");
    EXPECT_EQ(model.file_count(), 1u);
    EXPECT_FALSE(model.has_file("a.meld"));
    EXPECT_TRUE(model.has_file("b.meld"));
}

TEST_F(SemanticModelTest, ClearRemovesAll) {
    model.update_file("a.meld", make_file("a.meld", "a"));
    model.update_file("b.meld", make_file("b.meld", "b"));
    model.clear();
    EXPECT_EQ(model.file_count(), 0u);
}

TEST_F(SemanticModelTest, GetDiagnosticsForFile) {
    FileSemantics sem;
    sem.path = "err.meld";
    Diagnostic d;
    d.location = {"err.meld", 5, 3};
    d.severity = DiagnosticSeverity::Error;
    d.message = "type mismatch";
    d.rule_id = "E0042";
    sem.diagnostics.push_back(d);
    model.update_file("err.meld", std::move(sem));

    auto diags = model.get_diagnostics("err.meld");
    ASSERT_EQ(diags.size(), 1u);
    EXPECT_EQ(diags[0].message, "type mismatch");
    EXPECT_EQ(diags[0].rule_id, "E0042");
}

TEST_F(SemanticModelTest, GetAllDiagnostics) {
    FileSemantics s1;
    s1.path = "a.meld";
    s1.diagnostics.push_back({{"a.meld", 1, 1}, DiagnosticSeverity::Error, "err1", "E1", "", ""});
    model.update_file("a.meld", std::move(s1));

    FileSemantics s2;
    s2.path = "b.meld";
    s2.diagnostics.push_back({{"b.meld", 2, 1}, DiagnosticSeverity::Warning, "warn1", "W1", "", ""});
    model.update_file("b.meld", std::move(s2));

    auto all = model.get_all_diagnostics();
    EXPECT_EQ(all.size(), 2u);
}

TEST_F(SemanticModelTest, GetAstReturnsNullForUnknownFile) {
    EXPECT_EQ(model.get_ast("nonexistent.meld"), nullptr);
}

TEST_F(SemanticModelTest, GetDiagnosticsReturnsEmptyForUnknownFile) {
    EXPECT_TRUE(model.get_diagnostics("nonexistent.meld").empty());
}

TEST_F(SemanticModelTest, QueryTypeFindsSymbol) {
    FileSemantics sem;
    sem.path = "types.meld";
    auto root = std::make_shared<ASTNode>();
    root->kind = "module";
    root->name = "types";
    auto child = std::make_shared<ASTNode>();
    child->kind = "val_declaration";
    child->name = "x";
    child->type_info = "Int";
    root->children.push_back(child);
    sem.ast = root;
    model.update_file("types.meld", std::move(sem));

    auto info = model.query_type("types.meld", "x");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->qualified_type, "Int");
}

TEST_F(SemanticModelTest, ConcurrentReadsDoNotBlock) {
    model.update_file("test.meld", make_file("test.meld", "test"));

    std::vector<std::thread> readers;
    std::atomic<int> success_count{0};

    for (int i = 0; i < 10; ++i) {
        readers.emplace_back([&] {
            auto ast = model.get_ast("test.meld");
            if (ast && ast->name == "test") {
                ++success_count;
            }
        });
    }

    for (auto& t : readers) t.join();
    EXPECT_EQ(success_count.load(), 10);
}

}  // namespace
}  // namespace meld::daemon
