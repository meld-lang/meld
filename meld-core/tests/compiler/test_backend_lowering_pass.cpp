#include <gtest/gtest.h>
#include "meld/compiler/backend_lowering_pass.hpp"
#include "meld/std/mem.hpp"
#include "meld/types/memory.hpp"

using namespace meld::compiler;
using namespace meld::std_mem;

// ===========================================================================
// 10.1 — Hold[T] and View[T] lowering to MeldRef/WeakRef
// ===========================================================================

// ---------------------------------------------------------------------------
// Type detection helpers
// ---------------------------------------------------------------------------

TEST(BackendLoweringTypeDetection, IsOwnType) {
    EXPECT_TRUE(BackendLoweringPass::is_own_type("Hold[Node]"));
    EXPECT_TRUE(BackendLoweringPass::is_own_type("std.mem.Hold[Node]"));
    EXPECT_TRUE(BackendLoweringPass::is_own_type("Own<Node>"));
    EXPECT_FALSE(BackendLoweringPass::is_own_type("View[Node]"));
    EXPECT_FALSE(BackendLoweringPass::is_own_type("Node"));
    EXPECT_FALSE(BackendLoweringPass::is_own_type(""));
}

TEST(BackendLoweringTypeDetection, IsLinkType) {
    EXPECT_TRUE(BackendLoweringPass::is_link_type("View[Node]"));
    EXPECT_TRUE(BackendLoweringPass::is_link_type("std.mem.View[Node]"));
    EXPECT_TRUE(BackendLoweringPass::is_link_type("Link<Node>"));
    EXPECT_FALSE(BackendLoweringPass::is_link_type("Hold[Node]"));
    EXPECT_FALSE(BackendLoweringPass::is_link_type("Node"));
    EXPECT_FALSE(BackendLoweringPass::is_link_type(""));
}

TEST(BackendLoweringTypeDetection, ExtractInnerType) {
    EXPECT_EQ(BackendLoweringPass::extract_inner_type("Hold[Node]"), "Node");
    EXPECT_EQ(BackendLoweringPass::extract_inner_type("View[Widget]"), "Widget");
    EXPECT_EQ(BackendLoweringPass::extract_inner_type("Own<Node>"), "Node");
    EXPECT_EQ(BackendLoweringPass::extract_inner_type("Link<Widget>"), "Widget");
    EXPECT_EQ(BackendLoweringPass::extract_inner_type("Node"), "");
    EXPECT_EQ(BackendLoweringPass::extract_inner_type(""), "");
}

TEST(BackendLoweringTypeDetection, ToCppType) {
    EXPECT_EQ(BackendLoweringPass::to_cpp_type("Hold[Node]"),
              "meld::std_mem::Own<Node>");
    EXPECT_EQ(BackendLoweringPass::to_cpp_type("View[Widget]"),
              "meld::std_mem::Link<Widget>");
    EXPECT_EQ(BackendLoweringPass::to_cpp_type("int"), "int");
}

// ---------------------------------------------------------------------------
// Hold[T] declaration lowering
// ---------------------------------------------------------------------------

TEST(BackendLoweringOwn, DeclarationWithoutInitializer) {
    BackendLoweringPass pass;
    auto frag = pass.lower_own_declaration("node", "Node", "");

    EXPECT_EQ(frag.kind, LoweredFragment::Kind::OwnDeclaration);
    EXPECT_EQ(frag.cpp_code, "meld::std_mem::Own<Node> node;");
    EXPECT_EQ(frag.variable_name, "node");
    EXPECT_EQ(frag.inner_type, "Node");
}

TEST(BackendLoweringOwn, DeclarationWithInitializer) {
    BackendLoweringPass pass;
    auto frag = pass.lower_own_declaration(
        "root", "TreeNode",
        "std::make_shared<TreeNode>(\"root\")"
    );

    EXPECT_EQ(frag.kind, LoweredFragment::Kind::OwnDeclaration);
    EXPECT_EQ(frag.cpp_code,
        "meld::std_mem::Own<TreeNode> root = "
        "std::make_shared<TreeNode>(\"root\");");
    EXPECT_EQ(frag.inner_type, "TreeNode");
}

TEST(BackendLoweringOwn, ScopeExitComment) {
    BackendLoweringPass pass;
    auto frag = pass.lower_own_scope_exit("node", "Node");

    EXPECT_EQ(frag.kind, LoweredFragment::Kind::OwnScopeExit);
    EXPECT_NE(frag.cpp_code.find("ref_count_--"), std::string::npos);
    EXPECT_NE(frag.cpp_code.find("non-atomic"), std::string::npos);
}

// ---------------------------------------------------------------------------
// View[T] declaration lowering
// ---------------------------------------------------------------------------

TEST(BackendLoweringLink, DeclarationWithoutInitializer) {
    BackendLoweringPass pass;
    auto frag = pass.lower_link_declaration("observer", "Node", "");

    EXPECT_EQ(frag.kind, LoweredFragment::Kind::LinkDeclaration);
    EXPECT_EQ(frag.cpp_code, "meld::std_mem::Link<Node> observer;");
    EXPECT_EQ(frag.variable_name, "observer");
    EXPECT_EQ(frag.inner_type, "Node");
}

TEST(BackendLoweringLink, DeclarationWithInitializer) {
    BackendLoweringPass pass;
    auto frag = pass.lower_link_declaration(
        "parent_ref", "Parent",
        "meld::std_mem::link(owner)"
    );

    EXPECT_EQ(frag.kind, LoweredFragment::Kind::LinkDeclaration);
    EXPECT_EQ(frag.cpp_code,
        "meld::std_mem::Link<Parent> parent_ref = "
        "meld::std_mem::link(owner);");
}

TEST(BackendLoweringLink, ScopeExitComment) {
    BackendLoweringPass pass;
    auto frag = pass.lower_link_scope_exit("observer", "Node");

    EXPECT_EQ(frag.kind, LoweredFragment::Kind::LinkScopeExit);
    EXPECT_NE(frag.cpp_code.find("weak_count_--"), std::string::npos);
    EXPECT_NE(frag.cpp_code.find("non-atomic"), std::string::npos);
}

// ---------------------------------------------------------------------------
// View[T] upgrade lowering (if val x = link_ref)
// ---------------------------------------------------------------------------

TEST(BackendLoweringLink, UpgradeLowering) {
    BackendLoweringPass pass;
    auto frag = pass.lower_link_upgrade("node", "link_ref", "Node");

    EXPECT_EQ(frag.kind, LoweredFragment::Kind::LinkUpgrade);
    EXPECT_EQ(frag.cpp_code, "if (auto node = link_ref.upgrade())");
    EXPECT_EQ(frag.variable_name, "node");
    EXPECT_EQ(frag.meld_source, "if val node = link_ref");
}

TEST(BackendLoweringLink, UpgradeUsesUpgradeMethod) {
    // Verify the lowered code uses .upgrade() (not .lock())
    // per the design doc: Link<T>::upgrade() wraps weak_ptr::lock()
    BackendLoweringPass pass;
    auto frag = pass.lower_link_upgrade("x", "my_link", "Widget");

    EXPECT_NE(frag.cpp_code.find(".upgrade()"), std::string::npos);
    EXPECT_EQ(frag.cpp_code.find(".lock()"), std::string::npos);
}

// ===========================================================================
// 10.2 — std.mem.move() lowering to C++ move semantics
// ===========================================================================

TEST(BackendLoweringMove, MoveCallLowering) {
    BackendLoweringPass pass;
    auto frag = pass.lower_move_call("source", "Node");

    EXPECT_EQ(frag.kind, LoweredFragment::Kind::MoveCall);
    EXPECT_EQ(frag.cpp_code, "meld::std_mem::mem_move(source)");
    EXPECT_EQ(frag.meld_source, "std.mem.move(source)");
}

TEST(BackendLoweringMove, NoRetainReleaseInMoveOutput) {
    // Verify that the lowered move code does NOT contain retain/release
    BackendLoweringPass pass;
    auto frag = pass.lower_move_call("x", "TreeNode");

    EXPECT_EQ(frag.cpp_code.find("retain"), std::string::npos);
    EXPECT_EQ(frag.cpp_code.find("release"), std::string::npos);
    EXPECT_EQ(frag.cpp_code.find("ref_count"), std::string::npos);
}

TEST(BackendLoweringMove, MoveUsesMemMove) {
    // Verify the lowered code calls mem_move (which uses std::move internally)
    BackendLoweringPass pass;
    auto frag = pass.lower_move_call("owner", "Gadget");

    EXPECT_NE(frag.cpp_code.find("mem_move"), std::string::npos);
}

// ===========================================================================
// Verify actual C++ move semantics at runtime (integration)
// ===========================================================================

class TestWidget : public meld::types::ManagedObject {
public:
    explicit TestWidget(int id) : id_(id) {}
    int id() const { return id_; }
private:
    int id_;
};

TEST(BackendLoweringMoveIntegration, MovePreservesRefCount) {
    // Requirement 4.4: move lowers to MeldRef<T> move constructor
    // via std::move — no retain/release calls
    auto ptr = std::make_shared<TestWidget>(42);
    Own<TestWidget> source(ptr);

    long count_before = ptr.use_count();
    Own<TestWidget> dest = mem_move(source);
    long count_after = ptr.use_count();

    // Move must not change the reference count
    EXPECT_EQ(count_before, count_after);
    EXPECT_EQ(dest->id(), 42);
}

TEST(BackendLoweringMoveIntegration, MoveInvalidatesSource) {
    auto ptr = std::make_shared<TestWidget>(99);
    Own<TestWidget> source(ptr);

    Own<TestWidget> dest = mem_move(source);

    // Source should be invalidated (null) after move
    EXPECT_FALSE(static_cast<bool>(source));
    EXPECT_TRUE(static_cast<bool>(dest));
}

// ===========================================================================
// Full pass run (empty input — smoke test)
// ===========================================================================

TEST(BackendLoweringPass, EmptyInputProducesNoFragments) {
    BackendLoweringPass pass;
    IntrinsicResolutionRegistry registry;
    std::vector<meld::parser::ast::expression> empty;

    auto result = pass.run(empty, registry);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.fragments.empty());
    EXPECT_EQ(result.own_declarations_lowered, 0u);
    EXPECT_EQ(result.link_declarations_lowered, 0u);
    EXPECT_EQ(result.move_calls_lowered, 0u);
    EXPECT_EQ(result.link_upgrades_lowered, 0u);
}
