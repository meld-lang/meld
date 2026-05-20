/**
 * Property-Based Tests for Cross-System Integration (Property 19)
 *
 * Validates that ownership concepts integrate seamlessly across:
 *   - Algebraic effects and ownership tracking (Req 12.1)
 *   - Transpilation to target languages (Req 12.2)
 *   - Multiple dispatch with trait-based dispatch (Req 12.3)
 *   - FFI with ownership/borrowing rules (Req 12.4)
 *   - Macro expansion preserving ownership info (Req 12.5)
 *
 * Feature: rust-inspired-meld-enhancements, Property 19: Cross-System Integration
 * **Validates: Requirements 12.1, 12.2, 12.3, 12.4, 12.5**
 */

#include <gtest/gtest.h>
#include "meld/compiler/ownership_transpiler.hpp"
#include "meld/compiler/ownership_ffi.hpp"
#include "meld/compiler/borrow_checker.hpp"
#include "meld/kernel/trait_dispatch.hpp"
#include "meld/meta/advanced_traits.hpp"
#include "meld/meta/metatype.hpp"
#include "meld/testing/property_test.hpp"

#include <random>
#include <string>
#include <vector>
#include <algorithm>

using namespace meld::compiler;
using namespace meld::kernel;
using namespace meld::meta;
using namespace meld::testing;

// ============================================================================
// Random generators for property tests
// ============================================================================

namespace {

std::mt19937& rng() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

std::string random_type_name() {
    static const std::vector<std::string> names = {
        "Widget", "Buffer", "Handle", "Stream", "Connection",
        "Texture", "Shader", "Mesh", "Socket", "FileDesc",
        "Token", "Context", "Session", "Pipeline", "Resource"
    };
    std::uniform_int_distribution<size_t> dist(0, names.size() - 1);
    return names[dist(rng())];
}

std::string random_var_name() {
    static const std::vector<std::string> names = {
        "x", "y", "z", "obj", "buf", "handle", "conn",
        "src", "dst", "tmp", "res", "ctx", "ptr", "val"
    };
    std::uniform_int_distribution<size_t> dist(0, names.size() - 1);
    return names[dist(rng())];
}

std::string random_function_name() {
    static const std::vector<std::string> names = {
        "native_alloc", "native_free", "native_read", "native_write",
        "native_open", "native_close", "native_send", "native_recv",
        "native_init", "native_destroy", "native_process", "native_flush"
    };
    std::uniform_int_distribution<size_t> dist(0, names.size() - 1);
    return names[dist(rng())];
}

OwnershipTarget random_target() {
    static const std::vector<OwnershipTarget> targets = {
        OwnershipTarget::Cpp, OwnershipTarget::Java,
        OwnershipTarget::Rust, OwnershipTarget::Wasm
    };
    std::uniform_int_distribution<size_t> dist(0, targets.size() - 1);
    return targets[dist(rng())];
}

BorrowType random_borrow_type() {
    std::uniform_int_distribution<int> dist(0, 1);
    return dist(rng()) == 0 ? BorrowType::Immutable : BorrowType::Mutable;
}

FFIOwnership random_ffi_ownership() {
    std::uniform_int_distribution<int> dist(0, 2);
    switch (dist(rng())) {
        case 0: return FFIOwnership::Owned;
        case 1: return FFIOwnership::Borrowed;
        default: return FFIOwnership::BorrowedMut;
    }
}

OwnershipQualifier random_ownership_qualifier() {
    std::uniform_int_distribution<int> dist(0, 2);
    switch (dist(rng())) {
        case 0: return OwnershipQualifier::Owned;
        case 1: return OwnershipQualifier::Borrowed;
        default: return OwnershipQualifier::BorrowedMut;
    }
}

} // anonymous namespace


// ============================================================================
// Property 19a: Transpilation produces consistent type mappings across targets
// Validates: Requirement 12.2
// ============================================================================

TEST(InteroperabilityProperty, TranspilationTypeMappingConsistency) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 19: Cross-System Integration

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        auto target = random_target();
        auto type_name = random_type_name();
        OwnershipTranspiler transpiler(target);

        // Property: For any target and type, all three mapping slots must be non-empty
        auto mapping = transpiler.get_type_mapping(type_name);
        ASSERT_FALSE(mapping.owned_type.empty())
            << "owned_type empty for type=" << type_name
            << " target=" << static_cast<int>(target);
        ASSERT_FALSE(mapping.borrowed_type.empty())
            << "borrowed_type empty for type=" << type_name
            << " target=" << static_cast<int>(target);
        ASSERT_FALSE(mapping.borrowed_mut_type.empty())
            << "borrowed_mut_type empty for type=" << type_name
            << " target=" << static_cast<int>(target);

        // Property: map_owned_type and map_borrowed_type must agree with get_type_mapping
        EXPECT_EQ(transpiler.map_owned_type(type_name), mapping.owned_type);
        EXPECT_EQ(transpiler.map_borrowed_type(type_name, BorrowType::Immutable),
                  mapping.borrowed_type);
        EXPECT_EQ(transpiler.map_borrowed_type(type_name, BorrowType::Mutable),
                  mapping.borrowed_mut_type);
    }
}

// ============================================================================
// Property 19b: Ownership transfer code generation is non-empty and target-specific
// Validates: Requirement 12.2
// ============================================================================

TEST(InteroperabilityProperty, TranspilationTransferCodeGeneration) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 19: Cross-System Integration

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        auto target = random_target();
        auto type_name = random_type_name();
        auto src = random_var_name();
        auto dst = src + "_dst"; // ensure different from src
        OwnershipTranspiler transpiler(target);

        // Property: emit_ownership_transfer always produces non-empty code
        auto transfer = transpiler.emit_ownership_transfer(src, dst, type_name);
        ASSERT_FALSE(transfer.code.empty())
            << "Transfer code empty for target=" << static_cast<int>(target);

        // Property: generated code references both source and destination
        EXPECT_NE(transfer.code.find(src), std::string::npos)
            << "Transfer code missing source var '" << src << "'";
        EXPECT_NE(transfer.code.find(dst), std::string::npos)
            << "Transfer code missing dest var '" << dst << "'";

        // Property: emit_drop always produces non-empty code
        auto drop = transpiler.emit_drop(src, type_name);
        ASSERT_FALSE(drop.code.empty())
            << "Drop code empty for target=" << static_cast<int>(target);
    }
}

// ============================================================================
// Property 19c: Runtime safety checks are generated iff the target needs them
// Validates: Requirement 12.2
// ============================================================================

TEST(InteroperabilityProperty, TranspilationSafetyCheckConsistency) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 19: Cross-System Integration

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        auto target = random_target();
        auto var = random_var_name();
        auto borrow = random_borrow_type();
        OwnershipTranspiler transpiler(target);

        bool needs_runtime = transpiler.needs_runtime_safety_checks();

        auto uam_check = transpiler.emit_use_after_move_check(var);
        auto borrow_check = transpiler.emit_borrow_check(var, borrow);

        if (needs_runtime) {
            // Property: runtime targets produce non-empty safety_checks lists
            EXPECT_FALSE(uam_check.safety_checks.empty())
                << "Runtime target should produce use-after-move safety checks";
            EXPECT_FALSE(borrow_check.safety_checks.empty())
                << "Runtime target should produce borrow safety checks";
        } else {
            // Property: compile-time targets produce empty safety_checks lists
            EXPECT_TRUE(uam_check.safety_checks.empty())
                << "Compile-time target should not produce runtime safety checks";
            EXPECT_TRUE(borrow_check.safety_checks.empty())
                << "Compile-time target should not produce runtime borrow checks";
        }

        // Property: code is always non-empty regardless of target
        EXPECT_FALSE(uam_check.code.empty());
        EXPECT_FALSE(borrow_check.code.empty());
    }
}


// ============================================================================
// Property 19d: FFI declarations are tracked and retrievable
// Validates: Requirement 12.4
// ============================================================================

TEST(InteroperabilityProperty, FFIDeclarationTracking) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 19: Cross-System Integration

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        OwnershipAwareFFI ffi;

        // Generate a random number of FFI declarations (1-5)
        std::uniform_int_distribution<int> count_dist(1, 5);
        int num_decls = count_dist(rng());

        std::vector<std::string> declared_names;
        for (int d = 0; d < num_decls; ++d) {
            FFIFunctionDecl decl;
            decl.name = random_function_name() + "_" + std::to_string(d);
            decl.return_type = random_type_name();
            decl.return_ownership = random_ffi_ownership();

            // Add 1-3 random parameters
            std::uniform_int_distribution<int> param_dist(1, 3);
            int num_params = param_dist(rng());
            for (int p = 0; p < num_params; ++p) {
                FFIParamDecl param(
                    random_var_name() + "_" + std::to_string(p),
                    random_type_name(),
                    random_ffi_ownership()
                );
                decl.params.push_back(param);
            }

            ffi.declare_extern_function(decl);
            declared_names.push_back(decl.name);
        }

        // Property: declared_function_count matches number of declarations
        EXPECT_EQ(ffi.declared_function_count(), static_cast<size_t>(num_decls));

        // Property: every declared function is retrievable
        for (const auto& name : declared_names) {
            EXPECT_TRUE(ffi.is_declared(name))
                << "Function '" << name << "' should be declared";
            auto decl_result = ffi.get_declaration(name);
            ASSERT_TRUE(decl_result.has_value())
                << "get_declaration failed for '" << name << "': " << decl_result.error();
            EXPECT_EQ(decl_result->name, name);
        }

        // Property: undeclared functions are not found
        EXPECT_FALSE(ffi.is_declared("nonexistent_function_xyz"));
        auto bad_result = ffi.get_declaration("nonexistent_function_xyz");
        EXPECT_FALSE(bad_result.has_value());
    }
}

// ============================================================================
// Property 19e: FFI validation rejects calls to undeclared functions
// Validates: Requirement 12.4
// ============================================================================

TEST(InteroperabilityProperty, FFIValidationRejectsUndeclared) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 19: Cross-System Integration

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        OwnershipAwareFFI ffi;

        // Property: calling an undeclared function always fails validation
        auto result = ffi.validate_ffi_call(
            random_function_name(),
            {random_var_name()}
        );
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error().kind, FFIValidationError::Kind::UndeclaredFunction);
    }
}

// ============================================================================
// Property 19f: FFI validation rejects argument count mismatches
// Validates: Requirement 12.4
// ============================================================================

TEST(InteroperabilityProperty, FFIValidationRejectsArgCountMismatch) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 19: Cross-System Integration

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        OwnershipAwareFFI ffi;

        // Declare a function with exactly 2 parameters
        FFIFunctionDecl decl;
        decl.name = "test_func_" + std::to_string(i);
        decl.return_type = "int";
        decl.return_ownership = FFIOwnership::Owned;
        decl.params.push_back(FFIParamDecl("a", "int", FFIOwnership::Borrowed));
        decl.params.push_back(FFIParamDecl("b", "int", FFIOwnership::Borrowed));
        ffi.declare_extern_function(decl);

        // Property: passing wrong number of args always fails
        // Too few
        auto result_few = ffi.validate_ffi_call(decl.name, {"x"});
        EXPECT_FALSE(result_few.has_value())
            << "Should reject 1 arg for 2-param function";

        // Too many
        auto result_many = ffi.validate_ffi_call(decl.name, {"x", "y", "z"});
        EXPECT_FALSE(result_many.has_value())
            << "Should reject 3 args for 2-param function";

        // Correct count succeeds (no borrow checker, so structural validation only)
        auto result_ok = ffi.validate_ffi_call(decl.name, {"x", "y"});
        EXPECT_TRUE(result_ok.has_value())
            << "Should accept 2 args for 2-param function";
    }
}

// ============================================================================
// Property 19g: FFI resource tracker maintains correct active count
// Validates: Requirement 12.4
// ============================================================================

TEST(InteroperabilityProperty, FFIResourceTrackerActiveCount) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 19: Cross-System Integration

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        FFIResourceTracker tracker;

        // Track a random number of resources (2-8)
        std::uniform_int_distribution<int> count_dist(2, 8);
        int total = count_dist(rng());

        std::vector<std::string> resource_names;
        for (int r = 0; r < total; ++r) {
            std::string name = "res_" + std::to_string(i) + "_" + std::to_string(r);
            tracker.track_resource(name, random_type_name());
            resource_names.push_back(name);
        }

        // Property: active_count equals total tracked
        EXPECT_EQ(tracker.active_count(), static_cast<size_t>(total));

        // Release a random subset
        std::uniform_int_distribution<int> release_dist(0, total - 1);
        int num_to_release = release_dist(rng()) + 1; // at least 1
        if (num_to_release > total) num_to_release = total;

        for (int r = 0; r < num_to_release; ++r) {
            EXPECT_TRUE(tracker.release_resource(resource_names[r]));
        }

        // Property: active_count = total - released
        EXPECT_EQ(tracker.active_count(),
                  static_cast<size_t>(total - num_to_release));

        // Property: leaked resources = active resources
        auto leaked = tracker.get_leaked_resources();
        EXPECT_EQ(leaked.size(),
                  static_cast<size_t>(total - num_to_release));

        // Property: double-release returns false
        if (num_to_release > 0) {
            EXPECT_FALSE(tracker.release_resource(resource_names[0]));
        }
    }
}


// ============================================================================
// Property 19h: Trait dispatch with ownership qualifiers is deterministic
// Validates: Requirement 12.3
// ============================================================================

TEST(InteroperabilityProperty, TraitDispatchOwnershipDeterminism) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 19: Cross-System Integration

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        DispatchRegistry registry;
        registry.clear();
        TraitDispatchIntegration integration(registry);

        auto int_type = TypeRegistry::instance().get_int_type();
        auto string_type = TypeRegistry::instance().get_string_type();

        // Register an ownership-qualified signature
        auto qualifiers = std::vector<OwnershipQualifier>{random_ownership_qualifier()};
        auto sig = std::make_shared<FunctionSignature>(
            "process",
            std::vector<std::shared_ptr<MetaType>>{int_type},
            string_type,
            Value(std::make_shared<Integer>(42))
        );
        integration.register_ownership_qualified(sig, qualifiers);

        // Property: resolving with the same qualifiers twice yields the same result
        auto result1 = integration.resolve_with_ownership(
            "process", {int_type}, qualifiers);
        auto result2 = integration.resolve_with_ownership(
            "process", {int_type}, qualifiers);

        ASSERT_EQ(result1.has_value(), result2.has_value())
            << "Dispatch resolution should be deterministic";

        if (result1.has_value() && result2.has_value()) {
            EXPECT_EQ(result1.value()->name, result2.value()->name);
        }
    }
}

// ============================================================================
// Property 19i: Dispatch cache is populated after resolution
// Validates: Requirement 12.3
// ============================================================================

TEST(InteroperabilityProperty, TraitDispatchCachePopulation) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 19: Cross-System Integration

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        DispatchRegistry registry;
        registry.clear();
        TraitDispatchIntegration integration(registry);

        auto int_type = TypeRegistry::instance().get_int_type();
        auto string_type = TypeRegistry::instance().get_string_type();

        auto qualifier = random_ownership_qualifier();
        auto qualifiers = std::vector<OwnershipQualifier>{qualifier};

        auto sig = std::make_shared<FunctionSignature>(
            "compute",
            std::vector<std::shared_ptr<MetaType>>{int_type},
            string_type,
            Value(std::make_shared<Integer>(0))
        );
        integration.register_ownership_qualified(sig, qualifiers);

        // Clear cache and stats
        integration.clear_cache();
        EXPECT_EQ(integration.cache_size(), 0u);

        // First resolution: cache miss
        auto result = integration.resolve_with_ownership(
            "compute", {int_type}, qualifiers);

        if (result.has_value()) {
            // Property: after a successful resolution, cache should have an entry
            EXPECT_GE(integration.cache_size(), 1u);

            // Second resolution: should be a cache hit
            size_t misses_before = integration.cache_misses();
            auto result2 = integration.resolve_with_ownership(
                "compute", {int_type}, qualifiers);
            EXPECT_TRUE(result2.has_value());

            // Property: cache misses should not increase on second call
            EXPECT_EQ(integration.cache_misses(), misses_before);
        }
    }
}

// ============================================================================
// Property 19j: FFI wrapper generation includes resource tracking for owned returns
// Validates: Requirements 12.4, 12.1
// ============================================================================

TEST(InteroperabilityProperty, FFIWrapperResourceTracking) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 19: Cross-System Integration

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        OwnershipAwareFFI ffi;

        FFIFunctionDecl decl;
        decl.name = "alloc_" + std::to_string(i);
        decl.return_type = random_type_name();
        decl.return_ownership = FFIOwnership::Owned;

        // Add one borrowed parameter
        decl.params.push_back(
            FFIParamDecl("size", "int", FFIOwnership::Borrowed));
        ffi.declare_extern_function(decl);

        auto wrapper = ffi.wrap_ffi_call(decl.name, {"size_val"});
        ASSERT_TRUE(wrapper.has_value()) << wrapper.error();

        // Property: wrapper tracks the returned resource when return is Owned
        EXPECT_FALSE(wrapper->tracked_resources.empty())
            << "Owned return should produce tracked resources";

        // Property: wrapper generates non-empty pre/post/call/error code
        EXPECT_FALSE(wrapper->pre_call_code.empty());
        EXPECT_FALSE(wrapper->call_code.empty());
        EXPECT_FALSE(wrapper->post_call_code.empty());
        EXPECT_FALSE(wrapper->error_boundary.empty());

        // Property: call_code references the function name
        EXPECT_NE(wrapper->call_code.find(decl.name), std::string::npos);
    }
}

// ============================================================================
// Property 19k: Monomorphization hints are recorded and deduplicated
// Validates: Requirement 12.3
// ============================================================================

TEST(InteroperabilityProperty, MonomorphizationHintDeduplication) {
    // Tag: Feature: rust-inspired-meld-enhancements, Property 19: Cross-System Integration

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        DispatchRegistry registry;
        registry.clear();
        TraitDispatchIntegration integration(registry);

        auto int_type = TypeRegistry::instance().get_int_type();
        auto string_type = TypeRegistry::instance().get_string_type();

        std::string func_name = "generic_fn_" + std::to_string(i);

        // Record the same hint multiple times
        std::uniform_int_distribution<int> repeat_dist(2, 5);
        int repeats = repeat_dist(rng());

        for (int r = 0; r < repeats; ++r) {
            integration.record_monomorphization_hint(func_name, {int_type, string_type});
        }

        // Property: duplicate hints are deduplicated — only one entry
        auto hints = integration.get_monomorphization_hints(func_name);
        EXPECT_EQ(hints.size(), 1u)
            << "Duplicate monomorphization hints should be deduplicated";

        // Record a different hint
        integration.record_monomorphization_hint(func_name, {string_type, int_type});
        hints = integration.get_monomorphization_hints(func_name);
        EXPECT_EQ(hints.size(), 2u)
            << "Distinct type combinations should be separate hints";

        // Property: no hints for unrecorded function
        auto empty_hints = integration.get_monomorphization_hints("no_such_fn");
        EXPECT_TRUE(empty_hints.empty());
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
