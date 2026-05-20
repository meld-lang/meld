#include <gtest/gtest.h>
#include "meld/compiler/intrinsic_resolution_pass.hpp"
#include "meld/std/mem.hpp"

using namespace meld::compiler;
using namespace meld::std_mem;

// ===========================================================================
// IntrinsicResolutionRegistry — unit tests
// ===========================================================================

class IntrinsicRegistryTest : public ::testing::Test {
protected:
    IntrinsicResolutionRegistry registry;
};

TEST_F(IntrinsicRegistryTest, EmptyRegistryHasNoEntries) {
    EXPECT_EQ(registry.size(), 0u);
    EXPECT_FALSE(registry.has("memory_strategy"));
    EXPECT_FALSE(registry.has("managed_container"));
    EXPECT_FALSE(registry.has("memory_move"));
}

TEST_F(IntrinsicRegistryTest, RegisterAndLookupSingleEntry) {
    registry.register_entry(IntrinsicEntry{
        .intrinsic_name = "memory_strategy",
        .entity_name = "Storable",
        .entity_kind = IntrinsicEntityKind::Trait,
        .source_file = "std.mem"
    });

    EXPECT_TRUE(registry.has("memory_strategy"));
    EXPECT_EQ(registry.size(), 1u);

    auto* primary = registry.lookup_primary("memory_strategy");
    ASSERT_NE(primary, nullptr);
    EXPECT_EQ(primary->entity_name, "Storable");
    EXPECT_EQ(primary->entity_kind, IntrinsicEntityKind::Trait);
}

TEST_F(IntrinsicRegistryTest, LookupReturnsNullForUnknown) {
    EXPECT_EQ(registry.lookup_primary("nonexistent"), nullptr);
    EXPECT_TRUE(registry.lookup("nonexistent").empty());
}

TEST_F(IntrinsicRegistryTest, MultipleEntriesForSameIntrinsic) {
    // managed_container maps to List, Map, Set, Queue
    registry.register_entry(IntrinsicEntry{
        .intrinsic_name = "managed_container",
        .entity_name = "List",
        .entity_kind = IntrinsicEntityKind::Container
    });
    registry.register_entry(IntrinsicEntry{
        .intrinsic_name = "managed_container",
        .entity_name = "Map",
        .entity_kind = IntrinsicEntityKind::Container
    });
    registry.register_entry(IntrinsicEntry{
        .intrinsic_name = "managed_container",
        .entity_name = "Set",
        .entity_kind = IntrinsicEntityKind::Container
    });

    EXPECT_TRUE(registry.has("managed_container"));
    EXPECT_EQ(registry.size(), 3u);

    auto entries = registry.lookup("managed_container");
    EXPECT_EQ(entries.size(), 3u);

    // Primary is the first registered
    auto* primary = registry.lookup_primary("managed_container");
    ASSERT_NE(primary, nullptr);
    EXPECT_EQ(primary->entity_name, "List");
}

TEST_F(IntrinsicRegistryTest, EntityForConvenience) {
    registry.register_entry(IntrinsicEntry{
        .intrinsic_name = "memory_move",
        .entity_name = "std.mem.move",
        .entity_kind = IntrinsicEntityKind::Function
    });

    auto entity = registry.entity_for("memory_move");
    ASSERT_TRUE(entity.has_value());
    EXPECT_EQ(entity.value(), "std.mem.move");

    auto missing = registry.entity_for("nonexistent");
    EXPECT_FALSE(missing.has_value());
}

TEST_F(IntrinsicRegistryTest, ClearRemovesAllEntries) {
    registry.register_entry(IntrinsicEntry{
        .intrinsic_name = "memory_strategy",
        .entity_name = "Storable",
        .entity_kind = IntrinsicEntityKind::Trait
    });
    EXPECT_EQ(registry.size(), 1u);

    registry.clear();
    EXPECT_EQ(registry.size(), 0u);
    EXPECT_FALSE(registry.has("memory_strategy"));
}

TEST_F(IntrinsicRegistryTest, ConvenienceCheckers) {
    IntrinsicResolutionPass pass;
    IntrinsicResolutionRegistry reg;
    pass.register_std_mem_intrinsics(reg);

    EXPECT_TRUE(reg.has_memory_strategy());
    EXPECT_TRUE(reg.has_managed_container());
    EXPECT_TRUE(reg.has_memory_move());
}

// ===========================================================================
// IntrinsicResolutionPass — unit tests
// ===========================================================================

class IntrinsicPassTest : public ::testing::Test {
protected:
    IntrinsicResolutionPass pass;
};

TEST_F(IntrinsicPassTest, StdMemIntrinsicsRegistered) {
    // Running with empty expressions should still seed std.mem intrinsics
    std::vector<meld::parser::ast::expression> empty_exprs;
    auto result = pass.run(empty_exprs, "test.meld");

    EXPECT_TRUE(result.success);

    // memory_strategy → Storable
    EXPECT_TRUE(result.registry.has_memory_strategy());
    auto* storable = result.registry.lookup_primary("memory_strategy");
    ASSERT_NE(storable, nullptr);
    EXPECT_EQ(storable->entity_name, "Storable");
    EXPECT_EQ(storable->entity_kind, IntrinsicEntityKind::Trait);

    // managed_container → List, Map, Set, Queue
    EXPECT_TRUE(result.registry.has_managed_container());
    auto containers = result.registry.lookup("managed_container");
    EXPECT_EQ(containers.size(), 4u);

    // memory_move → std.mem.move
    EXPECT_TRUE(result.registry.has_memory_move());
    auto* move_entry = result.registry.lookup_primary("memory_move");
    ASSERT_NE(move_entry, nullptr);
    EXPECT_EQ(move_entry->entity_name, "std.mem.move");
    EXPECT_EQ(move_entry->entity_kind, IntrinsicEntityKind::Function);
}

TEST_F(IntrinsicPassTest, RegistryMatchesDesignMapping) {
    // Verify the registry matches the design doc's IntrinsicRegistry table:
    //   memory_strategy  → Storable trait
    //   managed_container → vec, dict, set
    //   memory_move      → std.mem.move function
    std::vector<meld::parser::ast::expression> empty_exprs;
    auto result = pass.run(empty_exprs);

    // Verify memory_strategy maps to Storable
    auto entity = result.registry.entity_for(IntrinsicTag::memory_strategy);
    ASSERT_TRUE(entity.has_value());
    EXPECT_EQ(entity.value(), "Storable");

    // Verify managed_container has List, Map, Set, Queue
    auto containers = result.registry.lookup(IntrinsicTag::managed_container);
    ASSERT_EQ(containers.size(), 4u);
    std::set<std::string> container_names;
    for (const auto* e : containers) {
        container_names.insert(e->entity_name);
    }
    EXPECT_TRUE(container_names.count("List"));
    EXPECT_TRUE(container_names.count("Map"));
    EXPECT_TRUE(container_names.count("Set"));
    EXPECT_TRUE(container_names.count("Queue"));

    // Verify memory_move maps to std.mem.move
    entity = result.registry.entity_for(IntrinsicTag::memory_move);
    ASSERT_TRUE(entity.has_value());
    EXPECT_EQ(entity.value(), "std.mem.move");
}

TEST_F(IntrinsicPassTest, ScanClassDefinitionOwn) {
    // Simulate a class definition for "Own" in the AST
    meld::parser::ast::class_definition own_class;
    own_class.name.name = "Own";

    std::vector<meld::parser::ast::expression> exprs;
    exprs.push_back(boost::spirit::x3::forward_ast<
        meld::parser::ast::class_definition>(own_class));

    auto result = pass.run(exprs, "std/mem.meld");

    // Own should be registered as a memory_strategy implementor
    auto entries = result.registry.lookup(IntrinsicTag::memory_strategy);
    bool found_own = false;
    for (const auto* e : entries) {
        if (e->entity_name == "Own" &&
            e->entity_kind == IntrinsicEntityKind::Class) {
            found_own = true;
            break;
        }
    }
    EXPECT_TRUE(found_own) << "Own class should be registered under memory_strategy";
}

TEST_F(IntrinsicPassTest, ScanClassDefinitionLink) {
    meld::parser::ast::class_definition link_class;
    link_class.name.name = "Link";

    std::vector<meld::parser::ast::expression> exprs;
    exprs.push_back(boost::spirit::x3::forward_ast<
        meld::parser::ast::class_definition>(link_class));

    auto result = pass.run(exprs, "std/mem.meld");

    auto entries = result.registry.lookup(IntrinsicTag::memory_strategy);
    bool found_link = false;
    for (const auto* e : entries) {
        if (e->entity_name == "Link" &&
            e->entity_kind == IntrinsicEntityKind::Class) {
            found_link = true;
            break;
        }
    }
    EXPECT_TRUE(found_link) << "Link class should be registered under memory_strategy";
}

TEST_F(IntrinsicPassTest, ScanStructDefinitionStorable) {
    // Simulate a struct definition for "Storable" (traits are structs in AST)
    meld::parser::ast::struct_definition storable_struct;
    storable_struct.name.name = "Storable";

    std::vector<meld::parser::ast::expression> exprs;
    exprs.push_back(boost::spirit::x3::forward_ast<
        meld::parser::ast::struct_definition>(storable_struct));

    auto result = pass.run(exprs, "std/mem.meld");

    // Storable should be registered as memory_strategy trait
    EXPECT_TRUE(result.registry.has_memory_strategy());

    // Should have info diagnostic about resolving the trait
    bool found_info = false;
    for (const auto& diag : result.diagnostics) {
        if (diag.level == IntrinsicDiagnostic::Level::Info &&
            diag.message.find("Storable") != std::string::npos) {
            found_info = true;
            break;
        }
    }
    EXPECT_TRUE(found_info) << "Should emit info diagnostic for Storable resolution";
}

TEST_F(IntrinsicPassTest, ScanFunctionDefinitionMove) {
    // Simulate a function definition for "move"
    meld::parser::ast::function_definition move_func;
    move_func.name.name = "move";

    std::vector<meld::parser::ast::expression> exprs;
    exprs.push_back(boost::spirit::x3::forward_ast<
        meld::parser::ast::function_definition>(move_func));

    auto result = pass.run(exprs, "std/mem.meld");

    // move should be registered under memory_move
    EXPECT_TRUE(result.registry.has_memory_move());
    auto entries = result.registry.lookup(IntrinsicTag::memory_move);
    bool found_move = false;
    for (const auto* e : entries) {
        if (e->entity_name == "move") {
            found_move = true;
            break;
        }
    }
    EXPECT_TRUE(found_move) << "move function should be registered under memory_move";
}

TEST_F(IntrinsicPassTest, UnrelatedDeclarationsIgnored) {
    // A class that is NOT an intrinsic type should not be registered
    meld::parser::ast::class_definition user_class;
    user_class.name.name = "UserProfile";

    std::vector<meld::parser::ast::expression> exprs;
    exprs.push_back(boost::spirit::x3::forward_ast<
        meld::parser::ast::class_definition>(user_class));

    auto result = pass.run(exprs, "app.meld");

    // Only std.mem seeded entries should exist — no UserProfile
    auto all = result.registry.all_entries();
    for (const auto& entry : all) {
        EXPECT_NE(entry.entity_name, "UserProfile")
            << "UserProfile should not be in the intrinsic registry";
    }
}

TEST_F(IntrinsicPassTest, LastRegistryAccessor) {
    std::vector<meld::parser::ast::expression> empty_exprs;
    pass.run(empty_exprs);

    // The pass should store the registry for later access
    const auto& reg = pass.registry();
    EXPECT_TRUE(reg.has_memory_strategy());
    EXPECT_TRUE(reg.has_managed_container());
    EXPECT_TRUE(reg.has_memory_move());
}

// ===========================================================================
// Extract intrinsic annotation — static helper tests
// ===========================================================================

TEST(ExtractIntrinsicTest, KnownTypeNames) {
    auto tag = IntrinsicResolutionPass::extract_intrinsic_annotation("Storable");
    ASSERT_TRUE(tag.has_value());
    EXPECT_EQ(tag.value(), "memory_strategy");

    tag = IntrinsicResolutionPass::extract_intrinsic_annotation("Own");
    ASSERT_TRUE(tag.has_value());
    EXPECT_EQ(tag.value(), "memory_strategy");

    tag = IntrinsicResolutionPass::extract_intrinsic_annotation("Link");
    ASSERT_TRUE(tag.has_value());
    EXPECT_EQ(tag.value(), "memory_strategy");

    // Old names still resolve for backward compatibility
    tag = IntrinsicResolutionPass::extract_intrinsic_annotation("vec");
    ASSERT_TRUE(tag.has_value());
    EXPECT_EQ(tag.value(), "managed_container");

    // New standard collection names
    tag = IntrinsicResolutionPass::extract_intrinsic_annotation("List");
    ASSERT_TRUE(tag.has_value());
    EXPECT_EQ(tag.value(), "managed_container");

    tag = IntrinsicResolutionPass::extract_intrinsic_annotation("Map");
    ASSERT_TRUE(tag.has_value());
    EXPECT_EQ(tag.value(), "managed_container");

    tag = IntrinsicResolutionPass::extract_intrinsic_annotation("Set");
    ASSERT_TRUE(tag.has_value());
    EXPECT_EQ(tag.value(), "managed_container");

    tag = IntrinsicResolutionPass::extract_intrinsic_annotation("Queue");
    ASSERT_TRUE(tag.has_value());
    EXPECT_EQ(tag.value(), "managed_container");

    tag = IntrinsicResolutionPass::extract_intrinsic_annotation("move");
    ASSERT_TRUE(tag.has_value());
    EXPECT_EQ(tag.value(), "memory_move");
}

TEST(ExtractIntrinsicTest, UnknownNameReturnsNullopt) {
    auto tag = IntrinsicResolutionPass::extract_intrinsic_annotation("MyClass");
    EXPECT_FALSE(tag.has_value());
}

TEST(ExtractIntrinsicTest, ExplicitAnnotationList) {
    // When the parser supports @intrinsic annotations as explicit metadata
    std::vector<std::string> annotations = {"intrinsic(memory_strategy)"};
    auto tag = IntrinsicResolutionPass::extract_intrinsic_annotation(
        "CustomTrait", annotations);
    ASSERT_TRUE(tag.has_value());
    EXPECT_EQ(tag.value(), "memory_strategy");
}

TEST(ExtractIntrinsicTest, BareTagInAnnotationList) {
    std::vector<std::string> annotations = {"memory_move"};
    auto tag = IntrinsicResolutionPass::extract_intrinsic_annotation(
        "custom_move", annotations);
    ASSERT_TRUE(tag.has_value());
    EXPECT_EQ(tag.value(), "memory_move");
}

TEST(IsKnownIntrinsicTagTest, ValidTags) {
    EXPECT_TRUE(IntrinsicResolutionPass::is_known_intrinsic_tag("memory_strategy"));
    EXPECT_TRUE(IntrinsicResolutionPass::is_known_intrinsic_tag("managed_container"));
    EXPECT_TRUE(IntrinsicResolutionPass::is_known_intrinsic_tag("memory_move"));
}

TEST(IsKnownIntrinsicTagTest, InvalidTags) {
    EXPECT_FALSE(IntrinsicResolutionPass::is_known_intrinsic_tag("unknown"));
    EXPECT_FALSE(IntrinsicResolutionPass::is_known_intrinsic_tag(""));
    EXPECT_FALSE(IntrinsicResolutionPass::is_known_intrinsic_tag("memory"));
}
