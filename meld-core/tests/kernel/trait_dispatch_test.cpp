#include <gtest/gtest.h>
#include "../../include/meld/kernel/trait_dispatch.hpp"
#include "../../include/meld/meta/advanced_traits.hpp"
#include "../../include/meld/meta/metatype.hpp"

using namespace meld::kernel;
using namespace meld::meta;

class TraitDispatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use a fresh registry per test to avoid cross-contamination
        registry_.clear();
        integration_ = std::make_unique<TraitDispatchIntegration>(registry_);

        // Standard types
        int_type = TypeRegistry::instance().get_int_type();
        string_type = TypeRegistry::instance().get_string_type();
        bool_type = TypeRegistry::instance().get_bool_type();

        // Class hierarchy: Shape -> Circle, Rectangle
        shape_type = MetaType::create_class("Shape", {}, {});
        TypeRegistry::instance().register_type("Shape", shape_type);

        circle_type = std::make_shared<ClassMetaType>(
            "Circle",
            std::vector<Field>{},
            std::vector<Method>{},
            std::vector<Property>{},
            std::dynamic_pointer_cast<ClassMetaType>(shape_type)
        );
        TypeRegistry::instance().register_type("Circle", circle_type);

        rect_type = std::make_shared<ClassMetaType>(
            "Rectangle",
            std::vector<Field>{},
            std::vector<Method>{},
            std::vector<Property>{},
            std::dynamic_pointer_cast<ClassMetaType>(shape_type)
        );
        TypeRegistry::instance().register_type("Rectangle", rect_type);

        dummy_impl = Value(std::make_shared<Integer>(0));
    }

    void TearDown() override {
        registry_.clear();
    }

    DispatchRegistry registry_;
    std::unique_ptr<TraitDispatchIntegration> integration_;

    std::shared_ptr<MetaType> int_type;
    std::shared_ptr<MetaType> string_type;
    std::shared_ptr<MetaType> bool_type;
    std::shared_ptr<MetaType> shape_type;
    std::shared_ptr<MetaType> circle_type;
    std::shared_ptr<MetaType> rect_type;
    Value dummy_impl;

    // Helper: create a simple trait with one method
    std::shared_ptr<AdvancedTraitMetaType> make_trait(
        const std::string& trait_name,
        const std::string& method_name,
        std::vector<std::shared_ptr<MetaType>> param_types,
        std::shared_ptr<MetaType> return_type) {

        Method m(method_name, std::move(param_types), std::move(return_type), dummy_impl);
        return std::make_shared<AdvancedTraitMetaType>(
            trait_name, std::vector<Method>{m});
    }

    // Helper: create a trait implementation
    std::shared_ptr<TraitImplementation> make_impl(
        std::shared_ptr<MetaType> type,
        std::shared_ptr<AdvancedTraitMetaType> trait,
        std::map<std::string, Value> method_impls = {}) {

        return std::make_shared<TraitImplementation>(
            std::move(type), std::move(trait),
            std::map<std::string, std::shared_ptr<MetaType>>{},
            std::move(method_impls));
    }
};

// ===== Trait-based dispatch resolution =====

TEST_F(TraitDispatchTest, RegisterAndResolveTraitMethod) {
    // Create trait Drawable with method "draw"
    auto drawable = make_trait("Drawable", "draw", {}, string_type);

    // Implement Drawable for Circle
    Value circle_draw(std::make_shared<Integer>(1));
    auto impl = make_impl(circle_type, drawable, {{"draw", circle_draw}});

    auto result = integration_->register_trait_implementation(impl);
    ASSERT_TRUE(result.has_value()) << result.error();

    // Resolve draw for Circle receiver
    auto resolved = integration_->resolve_trait_dispatch("draw", circle_type, {});
    ASSERT_TRUE(resolved.has_value()) << resolved.error();
    EXPECT_EQ(resolved.value()->param_types[0]->name(), "Circle");
}

TEST_F(TraitDispatchTest, TraitDispatchSelectsMostSpecific) {
    // Trait with method "area"
    auto measurable = make_trait("Measurable", "area", {}, int_type);

    // Implement for Shape (general) and Circle (specific)
    Value shape_area(std::make_shared<Integer>(10));
    Value circle_area(std::make_shared<Integer>(20));

    auto shape_impl = make_impl(shape_type, measurable, {{"area", shape_area}});
    auto circle_impl = make_impl(circle_type, measurable, {{"area", circle_area}});

    ASSERT_TRUE(integration_->register_trait_implementation(shape_impl).has_value());
    ASSERT_TRUE(integration_->register_trait_implementation(circle_impl).has_value());

    // Resolving for Circle should pick the Circle-specific implementation
    auto resolved = integration_->resolve_trait_dispatch("area", circle_type, {});
    ASSERT_TRUE(resolved.has_value()) << resolved.error();
    EXPECT_EQ(resolved.value()->param_types[0]->name(), "Circle");
}

TEST_F(TraitDispatchTest, TraitDispatchFallsBackToRegistry) {
    // Register a plain function in the registry (not via trait)
    auto sig = std::make_shared<FunctionSignature>(
        "compute",
        std::vector<std::shared_ptr<MetaType>>{shape_type, int_type},
        int_type,
        dummy_impl
    );
    registry_.register_function(sig);

    // resolve_trait_dispatch should fall back to the registry
    auto resolved = integration_->resolve_trait_dispatch(
        "compute", shape_type, {int_type});
    ASSERT_TRUE(resolved.has_value()) << resolved.error();
    EXPECT_EQ(resolved.value()->name, "compute");
}

TEST_F(TraitDispatchTest, RegisterNullImplFails) {
    auto result = integration_->register_trait_implementation(nullptr);
    ASSERT_FALSE(result.has_value());
}

// ===== Ownership-aware method resolution =====

TEST_F(TraitDispatchTest, OwnershipQualifiedDispatch) {
    // Register two versions of "process": one for Owned, one for Borrowed
    Value owned_impl(std::make_shared<Integer>(100));
    Value borrowed_impl(std::make_shared<Integer>(200));

    auto owned_sig = std::make_shared<FunctionSignature>(
        "process",
        std::vector<std::shared_ptr<MetaType>>{shape_type},
        int_type,
        owned_impl
    );
    auto borrowed_sig = std::make_shared<FunctionSignature>(
        "process",
        std::vector<std::shared_ptr<MetaType>>{shape_type},
        int_type,
        borrowed_impl
    );

    integration_->register_ownership_qualified(
        owned_sig, {OwnershipQualifier::Owned});
    integration_->register_ownership_qualified(
        borrowed_sig, {OwnershipQualifier::Borrowed});

    // Resolve with Owned qualifier
    auto result_owned = integration_->resolve_with_ownership(
        "process", {shape_type}, {OwnershipQualifier::Owned});
    ASSERT_TRUE(result_owned.has_value()) << result_owned.error();
    // The owned version should be selected (exact match scores higher)
    auto resolved_impl_owned = result_owned.value()->implementation;
    EXPECT_EQ(resolved_impl_owned.as<Integer>()->value(), 100);

    // Resolve with Borrowed qualifier
    auto result_borrowed = integration_->resolve_with_ownership(
        "process", {shape_type}, {OwnershipQualifier::Borrowed});
    ASSERT_TRUE(result_borrowed.has_value()) << result_borrowed.error();
    auto resolved_impl_borrowed = result_borrowed.value()->implementation;
    EXPECT_EQ(resolved_impl_borrowed.as<Integer>()->value(), 200);
}

TEST_F(TraitDispatchTest, OwnedImplicitlyBorrowable) {
    // Register only a Borrowed version
    Value borrowed_impl(std::make_shared<Integer>(42));
    auto sig = std::make_shared<FunctionSignature>(
        "read_data",
        std::vector<std::shared_ptr<MetaType>>{int_type},
        string_type,
        borrowed_impl
    );
    integration_->register_ownership_qualified(
        sig, {OwnershipQualifier::Borrowed});

    // Resolve with Owned qualifier — should still match because
    // Owned can be implicitly borrowed
    auto result = integration_->resolve_with_ownership(
        "read_data", {int_type}, {OwnershipQualifier::Owned});
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result.value()->implementation.as<Integer>()->value(), 42);
}

TEST_F(TraitDispatchTest, BorrowedCannotSatisfyMutableBorrow) {
    // Register only a BorrowedMut version
    Value mut_impl(std::make_shared<Integer>(99));
    auto sig = std::make_shared<FunctionSignature>(
        "mutate",
        std::vector<std::shared_ptr<MetaType>>{int_type},
        int_type,
        mut_impl
    );
    integration_->register_ownership_qualified(
        sig, {OwnershipQualifier::BorrowedMut});

    // Resolve with Borrowed (immutable) qualifier — should fail
    // because immutable borrow cannot satisfy mutable requirement
    auto result = integration_->resolve_with_ownership(
        "mutate", {int_type}, {OwnershipQualifier::Borrowed});
    // Should fall back to registry, which also won't have it
    // The qualified match should fail, but registry fallback may or may not find it
    // Since we didn't register in the plain registry, this should fail
    EXPECT_FALSE(result.has_value());
}

TEST_F(TraitDispatchTest, OwnershipFallsBackToPlainRegistry) {
    // Register a plain function (no ownership qualifier)
    auto sig = std::make_shared<FunctionSignature>(
        "plain_fn",
        std::vector<std::shared_ptr<MetaType>>{int_type},
        int_type,
        dummy_impl
    );
    registry_.register_function(sig);

    // Resolve with ownership qualifiers — should fall back to plain registry
    auto result = integration_->resolve_with_ownership(
        "plain_fn", {int_type}, {OwnershipQualifier::Owned});
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result.value()->name, "plain_fn");
}


// ===== Dispatch cache optimization =====

TEST_F(TraitDispatchTest, CachePopulatedOnResolve) {
    // Register an ownership-qualified signature
    auto sig = std::make_shared<FunctionSignature>(
        "cached_fn",
        std::vector<std::shared_ptr<MetaType>>{int_type},
        int_type,
        dummy_impl
    );
    integration_->register_ownership_qualified(
        sig, {OwnershipQualifier::Owned});

    EXPECT_EQ(integration_->cache_size(), 0u);

    // First resolve — cache miss, populates cache
    auto result1 = integration_->resolve_with_ownership(
        "cached_fn", {int_type}, {OwnershipQualifier::Owned});
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(integration_->cache_size(), 1u);
    EXPECT_EQ(integration_->cache_misses(), 1u);

    // Second resolve — cache hit
    auto result2 = integration_->resolve_with_ownership(
        "cached_fn", {int_type}, {OwnershipQualifier::Owned});
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(integration_->cache_hits(), 1u);
}

TEST_F(TraitDispatchTest, CacheClearedOnNewRegistration) {
    auto sig = std::make_shared<FunctionSignature>(
        "evolving_fn",
        std::vector<std::shared_ptr<MetaType>>{int_type},
        int_type,
        dummy_impl
    );
    integration_->register_ownership_qualified(
        sig, {OwnershipQualifier::Owned});

    // Populate cache
    integration_->resolve_with_ownership(
        "evolving_fn", {int_type}, {OwnershipQualifier::Owned});
    EXPECT_EQ(integration_->cache_size(), 1u);

    // Register a new qualified signature — cache should be cleared
    auto sig2 = std::make_shared<FunctionSignature>(
        "evolving_fn",
        std::vector<std::shared_ptr<MetaType>>{string_type},
        string_type,
        dummy_impl
    );
    integration_->register_ownership_qualified(
        sig2, {OwnershipQualifier::Borrowed});
    EXPECT_EQ(integration_->cache_size(), 0u);
}

TEST_F(TraitDispatchTest, ClearCacheResetsStats) {
    auto sig = std::make_shared<FunctionSignature>(
        "stats_fn",
        std::vector<std::shared_ptr<MetaType>>{int_type},
        int_type,
        dummy_impl
    );
    integration_->register_ownership_qualified(
        sig, {OwnershipQualifier::Owned});

    // Generate some hits and misses
    integration_->resolve_with_ownership(
        "stats_fn", {int_type}, {OwnershipQualifier::Owned});
    integration_->resolve_with_ownership(
        "stats_fn", {int_type}, {OwnershipQualifier::Owned});

    EXPECT_GT(integration_->cache_hits() + integration_->cache_misses(), 0u);

    integration_->clear_cache();
    EXPECT_EQ(integration_->cache_size(), 0u);
    EXPECT_EQ(integration_->cache_hits(), 0u);
    EXPECT_EQ(integration_->cache_misses(), 0u);
}

TEST_F(TraitDispatchTest, LookupCacheDirectly) {
    // Empty cache — should return nullopt
    auto miss = integration_->lookup_cache(
        "no_fn", {int_type}, {OwnershipQualifier::Owned});
    EXPECT_FALSE(miss.has_value());

    // Populate via resolve
    auto sig = std::make_shared<FunctionSignature>(
        "lookup_fn",
        std::vector<std::shared_ptr<MetaType>>{int_type},
        int_type,
        dummy_impl
    );
    integration_->register_ownership_qualified(
        sig, {OwnershipQualifier::Owned});
    integration_->resolve_with_ownership(
        "lookup_fn", {int_type}, {OwnershipQualifier::Owned});

    // Now lookup should hit
    auto hit = integration_->lookup_cache(
        "lookup_fn", {int_type}, {OwnershipQualifier::Owned});
    EXPECT_TRUE(hit.has_value());
}

// ===== Monomorphization hints =====

TEST_F(TraitDispatchTest, RecordAndRetrieveMonoHints) {
    integration_->record_monomorphization_hint("generic_fn", {int_type, string_type});
    integration_->record_monomorphization_hint("generic_fn", {bool_type});

    auto hints = integration_->get_monomorphization_hints("generic_fn");
    EXPECT_EQ(hints.size(), 2u);
    EXPECT_EQ(hints[0].size(), 2u);
    EXPECT_EQ(hints[0][0]->name(), "Int");
    EXPECT_EQ(hints[1].size(), 1u);
    EXPECT_EQ(hints[1][0]->name(), "Bool");
}

TEST_F(TraitDispatchTest, DuplicateMonoHintsIgnored) {
    integration_->record_monomorphization_hint("dup_fn", {int_type});
    integration_->record_monomorphization_hint("dup_fn", {int_type});

    auto hints = integration_->get_monomorphization_hints("dup_fn");
    EXPECT_EQ(hints.size(), 1u);
}

TEST_F(TraitDispatchTest, NoHintsReturnsEmpty) {
    auto hints = integration_->get_monomorphization_hints("unknown_fn");
    EXPECT_TRUE(hints.empty());
}

// ===== Integration with existing DispatchRegistry =====

TEST_F(TraitDispatchTest, RegistryAccessor) {
    // Verify the integration exposes the same registry
    auto sig = std::make_shared<FunctionSignature>(
        "reg_test",
        std::vector<std::shared_ptr<MetaType>>{int_type},
        int_type,
        dummy_impl
    );
    integration_->registry().register_function(sig);

    auto sigs = registry_.get_signatures("reg_test");
    EXPECT_EQ(sigs.size(), 1u);
}

TEST_F(TraitDispatchTest, TraitRegistrationPopulatesRegistry) {
    auto drawable = make_trait("Drawable", "draw", {}, string_type);
    Value draw_impl(std::make_shared<Integer>(7));
    auto impl = make_impl(circle_type, drawable, {{"draw", draw_impl}});

    ASSERT_TRUE(integration_->register_trait_implementation(impl).has_value());

    // The registry should now have signatures for both qualified and bare names
    auto qualified_sigs = registry_.get_signatures("Drawable::draw");
    EXPECT_FALSE(qualified_sigs.empty());

    auto bare_sigs = registry_.get_signatures("draw");
    EXPECT_FALSE(bare_sigs.empty());
}

TEST_F(TraitDispatchTest, MultipleTraitImplsCoexist) {
    auto serializable = make_trait("Serializable", "serialize", {}, string_type);

    Value circle_ser(std::make_shared<Integer>(1));
    Value rect_ser(std::make_shared<Integer>(2));

    auto circle_impl = make_impl(circle_type, serializable, {{"serialize", circle_ser}});
    auto rect_impl = make_impl(rect_type, serializable, {{"serialize", rect_ser}});

    ASSERT_TRUE(integration_->register_trait_implementation(circle_impl).has_value());
    ASSERT_TRUE(integration_->register_trait_implementation(rect_impl).has_value());

    // Resolve for Circle
    auto res_circle = integration_->resolve_trait_dispatch("serialize", circle_type, {});
    ASSERT_TRUE(res_circle.has_value()) << res_circle.error();
    EXPECT_EQ(res_circle.value()->param_types[0]->name(), "Circle");

    // Resolve for Rectangle
    auto res_rect = integration_->resolve_trait_dispatch("serialize", rect_type, {});
    ASSERT_TRUE(res_rect.has_value()) << res_rect.error();
    EXPECT_EQ(res_rect.value()->param_types[0]->name(), "Rectangle");
}
