#include "meld/daemon/dependency_graph.hpp"

#include <gtest/gtest.h>

namespace meld::daemon {
namespace {

class DependencyGraphTest : public ::testing::Test {
protected:
    DependencyGraph graph;
};

TEST_F(DependencyGraphTest, EmptyGraph) {
    EXPECT_TRUE(graph.empty());
    EXPECT_EQ(graph.size(), 0u);
    EXPECT_TRUE(graph.all_names().empty());
}

TEST_F(DependencyGraphTest, UpsertAndGet) {
    graph.upsert({"meld-lang", "0.1.0", "registry", "", {}});
    EXPECT_EQ(graph.size(), 1u);

    auto node = graph.get("meld-lang");
    ASSERT_TRUE(node.has_value());
    EXPECT_EQ(node->version, "0.1.0");
}

TEST_F(DependencyGraphTest, Remove) {
    graph.upsert({"a", "1.0", "registry", "", {}});
    graph.upsert({"b", "2.0", "registry", "", {}});
    EXPECT_TRUE(graph.remove("a"));
    EXPECT_EQ(graph.size(), 1u);
    EXPECT_FALSE(graph.get("a").has_value());
}

TEST_F(DependencyGraphTest, RemoveNonexistent) {
    EXPECT_FALSE(graph.remove("nonexistent"));
}

TEST_F(DependencyGraphTest, DiffDetectsAdded) {
    graph.upsert({"a", "1.0", "registry", "", {}});
    auto d = graph.diff({
        {"a", "1.0", "registry", "", {}},
        {"b", "2.0", "registry", "", {}},
    });
    EXPECT_EQ(d.added.size(), 1u);
    EXPECT_EQ(d.added[0], "b");
    EXPECT_TRUE(d.removed.empty());
    EXPECT_TRUE(d.changed.empty());
}

TEST_F(DependencyGraphTest, DiffDetectsRemoved) {
    graph.upsert({"a", "1.0", "registry", "", {}});
    graph.upsert({"b", "2.0", "registry", "", {}});
    auto d = graph.diff({{"a", "1.0", "registry", "", {}}});
    EXPECT_EQ(d.removed.size(), 1u);
    EXPECT_EQ(d.removed[0], "b");
}

TEST_F(DependencyGraphTest, DiffDetectsChanged) {
    graph.upsert({"a", "1.0", "registry", "", {}});
    auto d = graph.diff({{"a", "2.0", "registry", "", {}}});
    EXPECT_EQ(d.changed.size(), 1u);
    EXPECT_EQ(d.changed[0], "a");
}

TEST_F(DependencyGraphTest, DiffEmptyWhenNoChanges) {
    graph.upsert({"a", "1.0", "registry", "", {}});
    auto d = graph.diff({{"a", "1.0", "registry", "", {}}});
    EXPECT_TRUE(d.empty());
}

TEST_F(DependencyGraphTest, TransitiveClosure) {
    graph.upsert({"a", "1.0", "registry", "", {"b", "c"}});
    graph.upsert({"b", "1.0", "registry", "", {"d"}});
    graph.upsert({"c", "1.0", "registry", "", {}});
    graph.upsert({"d", "1.0", "registry", "", {}});

    auto closure = graph.transitive_closure("a");
    EXPECT_EQ(closure.size(), 3u);
    EXPECT_TRUE(closure.count("b"));
    EXPECT_TRUE(closure.count("c"));
    EXPECT_TRUE(closure.count("d"));
}

TEST_F(DependencyGraphTest, ParseMeldToml) {
    std::string toml = R"(
[package]
name = "my-project"
version = "0.1.0"

[dependencies]
meld-lang = "0.1.0"
meld-async = "0.2.0"

[build]
target = "native"
)";
    auto deps = DependencyGraph::parse_meld_toml(toml);
    ASSERT_EQ(deps.size(), 2u);
    EXPECT_EQ(deps[0].name, "meld-lang");
    EXPECT_EQ(deps[0].version, "0.1.0");
    EXPECT_EQ(deps[1].name, "meld-async");
    EXPECT_EQ(deps[1].version, "0.2.0");
}

TEST_F(DependencyGraphTest, ReplaceAll) {
    graph.upsert({"old", "1.0", "registry", "", {}});
    graph.replace_all({
        {"new1", "1.0", "registry", "", {}},
        {"new2", "2.0", "registry", "", {}},
    });
    EXPECT_EQ(graph.size(), 2u);
    EXPECT_FALSE(graph.get("old").has_value());
    EXPECT_TRUE(graph.get("new1").has_value());
    EXPECT_TRUE(graph.get("new2").has_value());
}

}  // namespace
}  // namespace meld::daemon
