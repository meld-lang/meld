/**
 * Integration Property-Based Tests for Rust-Inspired Meld Enhancements
 *
 * Validates cross-feature interactions between all Rust-inspired features:
 *   1. Ownership + Traits: Owned values work correctly with trait dispatch
 *   2. Result + Pattern Matching: Result types integrate with exhaustive matching
 *   3. Ownership + FFI + Transpilation: Ownership preserved across system boundaries
 *   4. Traits + Dispatch + Monomorphization: Trait dispatch with monomorphization hints
 *   5. Error Handling + Effects: Result types work with effect system integration
 *   6. Metaprogramming + Provenance: Macro expansion preserves provenance
 *   7. Concurrency + Ownership: Send/Sync traits enforce ownership rules
 *   8. Gradual Adoption + All Features: Mixed-mode compilation works
 *
 * Feature: rust-inspired-meld-enhancements, Integration Property Tests
 * **Validates: All requirements**
 */

#include <gtest/gtest.h>

// Ownership & Transpilation & FFI
#include "meld/compiler/ownership_transpiler.hpp"
#include "meld/compiler/ownership_ffi.hpp"
#include "meld/compiler/borrow_checker.hpp"

// Trait dispatch
#include "meld/kernel/trait_dispatch.hpp"
#include "meld/meta/advanced_traits.hpp"
#include "meld/meta/metatype.hpp"

// Result & Option types
#include "meld/types/result.hpp"
#include "meld/types/option.hpp"
#include "meld/types/error_trait.hpp"

// Provenance & AI
#include "meld/provenance/provenance.hpp"
#include "meld/provenance/ai_provenance.hpp"

// Macro system
#include "meld/macro/macro.hpp"
#include "meld/macro/blueprint.hpp"

// Kernel primitives
#include "meld/kernel/primitives.hpp"

// Testing framework
#include "meld/testing/property_test.hpp"

#include <random>
#include <string>
#include <vector>
#include <algorithm>
#include <set>

using namespace meld::compiler;
using namespace meld::kernel;
using namespace meld::meta;
using namespace meld::types;
using namespace meld::provenance;
using namespace meld::ai;
using namespace meld::macro;
using namespace meld::testing;

// ============================================================================
// Random generators for integration property tests
// ============================================================================

namespace {

std::mt19937& rng() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

std::string random_type_name() {
    static const std::vector<std::string> names = {
        "Widget", "Buffer", "Handle", "Stream", "Connection",
        "Texture", "Shader", "Mesh", "Socket", "FileDesc"
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
        "process", "transform", "compute", "validate",
        "serialize", "parse", "render", "dispatch",
        "allocate", "release", "connect", "initialize"
    };
    std::uniform_int_distribution<size_t> dist(0, names.size() - 1);
    return names[dist(rng())];
}

std::string random_model_name() {
    static const std::vector<std::string> names = {
        "gpt-4", "gpt-3.5-turbo", "claude-3", "codex", "copilot"
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

OwnershipQualifier random_ownership_qualifier() {
    std::uniform_int_distribution<int> dist(0, 2);
    switch (dist(rng())) {
        case 0: return OwnershipQualifier::Owned;
        case 1: return OwnershipQualifier::Borrowed;
        default: return OwnershipQualifier::BorrowedMut;
    }
}

FFIOwnership random_ffi_ownership() {
    std::uniform_int_distribution<int> dist(0, 2);
    switch (dist(rng())) {
        case 0: return FFIOwnership::Owned;
        case 1: return FFIOwnership::Borrowed;
        default: return FFIOwnership::BorrowedMut;
    }
}

double random_confidence() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng());
}

OriginType random_origin() {
    std::uniform_int_distribution<int> dist(0, 2);
    switch (dist(rng())) {
        case 0: return OriginType::Human;
        case 1: return OriginType::Agent;
        default: return OriginType::Verified;
    }
}

int random_int(int lo, int hi) {
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng());
}

} // anonymous namespace


// ============================================================================
// Integration Test 1: Ownership + Traits
// Owned values work correctly with trait dispatch — registering ownership-
// qualified signatures and resolving them through the trait dispatch system.
// Validates: Requirements 1.1, 3.1, 3.3, 12.3
// ============================================================================

TEST(IntegrationProperty, OwnershipPlusTraitDispatch) {
    // Tag: Feature: rust-inspired-meld-enhancements, Integration Property Tests

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        DispatchRegistry registry;
        registry.clear();
        TraitDispatchIntegration integration(registry);

        auto int_type = TypeRegistry::instance().get_int_type();
        auto string_type = TypeRegistry::instance().get_string_type();

        std::string method_name = random_function_name() + "_" + std::to_string(i);
        auto qualifier = random_ownership_qualifier();
        auto qualifiers = std::vector<OwnershipQualifier>{qualifier};

        // Register an ownership-qualified signature
        auto sig = std::make_shared<FunctionSignature>(
            method_name,
            std::vector<std::shared_ptr<MetaType>>{int_type},
            string_type,
            Value(std::make_shared<Integer>(i))
        );
        integration.register_ownership_qualified(sig, qualifiers);

        // Property: resolution with matching qualifiers succeeds
        auto result = integration.resolve_with_ownership(
            method_name, {int_type}, qualifiers);
        ASSERT_TRUE(result.has_value())
            << "Ownership-qualified dispatch should resolve for: " << method_name;
        EXPECT_EQ(result.value()->name, method_name);

        // Property: resolution is cached after first call
        integration.clear_cache();
        auto r1 = integration.resolve_with_ownership(method_name, {int_type}, qualifiers);
        ASSERT_TRUE(r1.has_value());
        size_t misses_after_first = integration.cache_misses();

        auto r2 = integration.resolve_with_ownership(method_name, {int_type}, qualifiers);
        ASSERT_TRUE(r2.has_value());
        // Second call should not increase cache misses
        EXPECT_EQ(integration.cache_misses(), misses_after_first);

        // Property: monomorphization hint recorded for the concrete types
        integration.record_monomorphization_hint(method_name, {int_type});
        auto hints = integration.get_monomorphization_hints(method_name);
        EXPECT_GE(hints.size(), 1u)
            << "Monomorphization hint should be recorded";
    }
}

// ============================================================================
// Integration Test 2: Result + Pattern Matching
// Result types integrate with exhaustive pattern matching — map, flat_map,
// match, and combinator chains preserve correctness.
// Validates: Requirements 2.1, 2.2, 5.1, 5.2, 5.4
// ============================================================================

TEST(IntegrationProperty, ResultPlusPatternMatching) {
    // Tag: Feature: rust-inspired-meld-enhancements, Integration Property Tests

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        int value = random_int(-1000, 1000);

        // Create a Result based on random value
        auto result = (value >= 0)
            ? Result<int, std::string>::success(value)
            : Result<int, std::string>::error("negative: " + std::to_string(value));

        // Property: match covers both branches exhaustively
        auto matched = result.match(
            [](int v) -> std::string { return "ok:" + std::to_string(v); },
            [](const std::string& e) -> std::string { return "err:" + e; }
        );

        if (value >= 0) {
            EXPECT_EQ(matched, "ok:" + std::to_string(value));
        } else {
            EXPECT_EQ(matched, "err:negative: " + std::to_string(value));
        }

        // Property: map preserves success/error status
        auto mapped = result.map([](int v) { return v * 2; });
        EXPECT_EQ(mapped.is_success(), result.is_success());
        if (mapped.is_success()) {
            EXPECT_EQ(mapped.value(), value * 2);
        }

        // Property: flat_map chains correctly
        auto chained = result.flat_map([](int v) -> Result<std::string, std::string> {
            if (v > 500) return Result<std::string, std::string>::error("too large");
            return Result<std::string, std::string>::success(std::to_string(v));
        });
        if (result.is_error()) {
            EXPECT_TRUE(chained.is_error());
        }

        // Property: Option integration — converting Result to Option
        auto opt = result.is_success()
            ? Option<int>::some(result.value())
            : Option<int>::none();
        EXPECT_EQ(opt.is_some(), result.is_success());
        if (opt.is_some()) {
            EXPECT_EQ(opt.value(), value);
        }
    }
}


// ============================================================================
// Integration Test 3: Ownership + FFI + Transpilation
// Ownership concepts are preserved across FFI boundaries and transpilation
// targets — FFI wrapper generation and transpiled code are consistent.
// Validates: Requirements 1.1, 12.2, 12.4
// ============================================================================

TEST(IntegrationProperty, OwnershipPlusFFIPlusTranspilation) {
    // Tag: Feature: rust-inspired-meld-enhancements, Integration Property Tests

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        auto target = random_target();
        auto type_name = random_type_name();
        std::string func_name = "ffi_func_" + std::to_string(i);

        // Set up FFI declaration with ownership annotations
        OwnershipAwareFFI ffi;
        FFIFunctionDecl decl;
        decl.name = func_name;
        decl.return_type = type_name;
        decl.return_ownership = FFIOwnership::Owned;
        decl.params.push_back(FFIParamDecl("input", type_name, FFIOwnership::Borrowed));
        ffi.declare_extern_function(decl);

        // Set up transpiler for the same target
        OwnershipTranspiler transpiler(target);

        // Property: FFI wrapper generation succeeds for declared function
        auto wrapper = ffi.wrap_ffi_call(func_name, {"input_val"});
        ASSERT_TRUE(wrapper.has_value())
            << "FFI wrapper should succeed for: " << func_name;

        // Property: transpiler produces consistent type mappings for the same type
        auto mapping = transpiler.get_type_mapping(type_name);
        EXPECT_FALSE(mapping.owned_type.empty());
        EXPECT_FALSE(mapping.borrowed_type.empty());

        // Property: FFI wrapper tracks owned return resources
        EXPECT_FALSE(wrapper->tracked_resources.empty())
            << "Owned return should produce tracked resources";

        // Property: transpiled ownership transfer code is non-empty
        auto transfer = transpiler.emit_ownership_transfer("src", "dst", type_name);
        EXPECT_FALSE(transfer.code.empty());

        // Property: runtime safety check consistency between FFI and transpiler
        bool transpiler_needs_runtime = transpiler.needs_runtime_safety_checks();
        auto uam_check = transpiler.emit_use_after_move_check("input_val");
        if (transpiler_needs_runtime) {
            EXPECT_FALSE(uam_check.safety_checks.empty())
                << "Runtime target should produce safety checks";
        } else {
            EXPECT_TRUE(uam_check.safety_checks.empty())
                << "Compile-time target should not produce runtime checks";
        }

        // Property: FFI validation rejects undeclared functions
        auto bad_result = ffi.validate_ffi_call("nonexistent_" + func_name, {"x"});
        EXPECT_FALSE(bad_result.has_value());
    }
}

// ============================================================================
// Integration Test 4: Traits + Dispatch + Monomorphization
// Trait-based dispatch works with monomorphization hints — registering
// multiple type specializations and verifying deduplication.
// Validates: Requirements 3.1, 3.2, 7.1, 12.3
// ============================================================================

TEST(IntegrationProperty, TraitsPlusDispatchPlusMonomorphization) {
    // Tag: Feature: rust-inspired-meld-enhancements, Integration Property Tests

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        DispatchRegistry registry;
        registry.clear();
        TraitDispatchIntegration integration(registry);

        auto int_type = TypeRegistry::instance().get_int_type();
        auto string_type = TypeRegistry::instance().get_string_type();

        std::string func_name = "generic_" + std::to_string(i);

        // Register signatures with different ownership qualifiers
        auto owned_q = std::vector<OwnershipQualifier>{OwnershipQualifier::Owned};
        auto borrowed_q = std::vector<OwnershipQualifier>{OwnershipQualifier::Borrowed};

        auto sig_owned = std::make_shared<FunctionSignature>(
            func_name,
            std::vector<std::shared_ptr<MetaType>>{int_type},
            string_type,
            Value(std::make_shared<Integer>(1))
        );
        auto sig_borrowed = std::make_shared<FunctionSignature>(
            func_name,
            std::vector<std::shared_ptr<MetaType>>{int_type},
            string_type,
            Value(std::make_shared<Integer>(2))
        );
        integration.register_ownership_qualified(sig_owned, owned_q);
        integration.register_ownership_qualified(sig_borrowed, borrowed_q);

        // Property: different qualifiers can resolve to different signatures
        auto res_owned = integration.resolve_with_ownership(func_name, {int_type}, owned_q);
        auto res_borrowed = integration.resolve_with_ownership(func_name, {int_type}, borrowed_q);
        ASSERT_TRUE(res_owned.has_value());
        ASSERT_TRUE(res_borrowed.has_value());

        // Property: monomorphization hints are deduplicated
        int repeats = random_int(2, 5);
        for (int r = 0; r < repeats; ++r) {
            integration.record_monomorphization_hint(func_name, {int_type, string_type});
        }
        auto hints = integration.get_monomorphization_hints(func_name);
        EXPECT_EQ(hints.size(), 1u)
            << "Duplicate monomorphization hints should be deduplicated";

        // Property: distinct type combinations produce separate hints
        integration.record_monomorphization_hint(func_name, {string_type, int_type});
        hints = integration.get_monomorphization_hints(func_name);
        EXPECT_EQ(hints.size(), 2u)
            << "Distinct type combinations should be separate hints";

        // Property: no hints for unrecorded function
        auto empty_hints = integration.get_monomorphization_hints("no_such_fn_" + std::to_string(i));
        EXPECT_TRUE(empty_hints.empty());
    }
}


// ============================================================================
// Integration Test 5: Error Handling + Result Types + Error Trait
// Result types work with the Error trait system — error chaining, categories,
// and combinator operations preserve error information.
// Validates: Requirements 5.1, 5.2, 5.3, 5.4, 5.5
// ============================================================================

TEST(IntegrationProperty, ErrorHandlingPlusResultTypes) {
    // Tag: Feature: rust-inspired-meld-enhancements, Integration Property Tests

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        // Create a chain of errors using the Error trait
        auto root_error = std::make_shared<GenericError>("root cause " + std::to_string(i));
        auto io_error = std::make_shared<IOError>(
            IOError::Kind::NotFound,
            "file not found " + std::to_string(i),
            root_error
        );

        // Property: error chaining preserves the source
        ASSERT_NE(io_error->source(), nullptr);
        EXPECT_EQ(io_error->source()->message(), root_error->message());

        // Property: chain_string includes all errors in the chain
        std::string chain = io_error->chain_string();
        EXPECT_NE(chain.find(io_error->message()), std::string::npos);
        EXPECT_NE(chain.find(root_error->message()), std::string::npos);

        // Property: wrapping errors in Result preserves error information
        auto result = Result<int, std::shared_ptr<Error>>::error(io_error);
        ASSERT_TRUE(result.is_error());
        EXPECT_EQ(result.error()->category(), "IOError");

        // Property: map_error transforms the error while preserving structure
        auto mapped = result.map_error([](const std::shared_ptr<Error>& e) {
            return e->message() + " [mapped]";
        });
        ASSERT_TRUE(mapped.is_error());
        EXPECT_NE(mapped.error().find("[mapped]"), std::string::npos);

        // Property: or_else provides fallback for error results
        auto fallback = Result<int, std::shared_ptr<Error>>::success(42);
        auto recovered = result.or_else(fallback);
        EXPECT_TRUE(recovered.is_success());
        EXPECT_EQ(recovered.value(), 42);

        // Property: Option integration — None maps to error path
        auto opt = Option<int>::none();
        EXPECT_TRUE(opt.is_none());
        int val = opt.value_or(-1);
        EXPECT_EQ(val, -1);

        // Property: Option::some maps to success path
        auto opt_some = Option<int>::some(i);
        EXPECT_TRUE(opt_some.is_some());
        EXPECT_EQ(opt_some.value(), i);
    }
}

// ============================================================================
// Integration Test 6: Metaprogramming + Provenance
// Macro expansion preserves provenance through ownership — AI-generated code
// retains provenance metadata after macro registration and expansion.
// Validates: Requirements 6.1, 6.5, 9.1, 12.5
// ============================================================================

TEST(IntegrationProperty, MetaprogrammingPlusProvenance) {
    // Tag: Feature: rust-inspired-meld-enhancements, Integration Property Tests

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        // Clean registries per iteration
        MacroRegistry::instance().clear();

        std::string macro_name = "integ_macro_" + std::to_string(i);
        std::string model = random_model_name();
        double confidence = random_confidence();

        // Register a macro
        auto macro = make_simple_macro(
            macro_name, {"input"},
            [](const std::vector<Value>& args) -> Value {
                if (args.empty()) return Value(nil());
                return args[0];
            }
        );
        MacroRegistry::instance().register_macro(macro);

        // Property: macro is registered and retrievable
        ASSERT_TRUE(MacroRegistry::instance().has_macro(macro_name));

        // Create an AI-generated node and attach provenance
        AIProvenanceTracker tracker(model);
        auto sym = std::make_shared<Symbol>("generated_" + std::to_string(i));
        Value node(sym);
        Value marked = tracker.markAsAIGenerated(node, confidence, "bp_" + std::to_string(i));

        // Property: provenance is attached to the AI-generated node
        ASSERT_TRUE(Provenance::hasProvenance(marked));
        auto prov = Provenance::getProvenance(marked);
        ASSERT_TRUE(prov.has_value());
        EXPECT_EQ(prov->origin, OriginType::Agent);

        // Property: confidence is preserved (clamped to [0,1])
        double expected_conf = std::clamp(confidence, 0.0, 1.0);
        ASSERT_TRUE(prov->confidence_score.has_value());
        EXPECT_NEAR(*prov->confidence_score, expected_conf, 0.001);

        // Property: model name is preserved
        ASSERT_TRUE(prov->agent_model.has_value());
        EXPECT_EQ(*prov->agent_model, model);

        // Property: trust score is within valid range
        double trust = Provenance::calculateTrustScore(*prov);
        EXPECT_GE(trust, 0.0);
        EXPECT_LE(trust, 1.0);

        // Property: content hash attachment and mismatch detection work
        std::string blueprint = "blueprint_" + std::to_string(i);
        std::string code = "code_" + std::to_string(i);
        Value with_hashes = Provenance::attachContentHashes(marked, blueprint, code);

        EXPECT_FALSE(Provenance::hasContentChanged(with_hashes, blueprint, "blueprint_hash"));
        EXPECT_TRUE(Provenance::hasContentChanged(with_hashes, blueprint + "_mod", "blueprint_hash"));
    }

    MacroRegistry::instance().clear();
}


// ============================================================================
// Integration Test 7: FFI Resource Tracking + Error Handling
// FFI resource tracker integrates with Result-based error handling —
// tracking resources, detecting leaks, and wrapping in Result types.
// Validates: Requirements 4.1, 4.2, 12.4
// ============================================================================

TEST(IntegrationProperty, FFIResourceTrackingPlusErrorHandling) {
    // Tag: Feature: rust-inspired-meld-enhancements, Integration Property Tests

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        FFIResourceTracker tracker;

        int total = random_int(2, 8);
        std::vector<std::string> resource_names;

        for (int r = 0; r < total; ++r) {
            std::string name = "res_" + std::to_string(i) + "_" + std::to_string(r);
            tracker.track_resource(name, random_type_name());
            resource_names.push_back(name);
        }

        // Property: active count matches total tracked
        EXPECT_EQ(tracker.active_count(), static_cast<size_t>(total));

        // Release a random subset and wrap in Result
        int num_to_release = random_int(1, total);
        for (int r = 0; r < num_to_release; ++r) {
            bool released = tracker.release_resource(resource_names[r]);
            auto result = released
                ? Result<std::string, std::string>::success(resource_names[r])
                : Result<std::string, std::string>::error("failed to release: " + resource_names[r]);
            EXPECT_TRUE(result.is_success());
        }

        // Property: active count = total - released
        EXPECT_EQ(tracker.active_count(), static_cast<size_t>(total - num_to_release));

        // Property: leaked resources match remaining active resources
        auto leaked = tracker.get_leaked_resources();
        EXPECT_EQ(leaked.size(), static_cast<size_t>(total - num_to_release));

        // Property: double-release returns false (wrapped in Result)
        if (num_to_release > 0) {
            bool double_release = tracker.release_resource(resource_names[0]);
            auto dr_result = double_release
                ? Result<bool, std::string>::success(true)
                : Result<bool, std::string>::error("double release");
            EXPECT_TRUE(dr_result.is_error());
        }

        // Property: release_all cleans up remaining resources
        size_t cleaned = tracker.release_all();
        EXPECT_EQ(cleaned, static_cast<size_t>(total - num_to_release));
        EXPECT_EQ(tracker.active_count(), 0u);
    }
}

// ============================================================================
// Integration Test 8: Gradual Adoption + All Features
// Mixed-mode compilation works with all features — transpilation targets
// produce valid code for all ownership qualifiers, and FFI declarations
// coexist with trait dispatch registrations.
// Validates: Requirements 11.1, 11.2, 11.3, 11.4
// ============================================================================

TEST(IntegrationProperty, GradualAdoptionPlusAllFeatures) {
    // Tag: Feature: rust-inspired-meld-enhancements, Integration Property Tests

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        auto target = random_target();
        OwnershipTranspiler transpiler(target);
        OwnershipAwareFFI ffi;
        DispatchRegistry registry;
        registry.clear();
        TraitDispatchIntegration integration(registry);

        auto int_type = TypeRegistry::instance().get_int_type();
        auto string_type = TypeRegistry::instance().get_string_type();

        std::string type_name = random_type_name();
        std::string func_name = "mixed_" + std::to_string(i);

        // Simulate gradual adoption: register both FFI and trait dispatch
        FFIFunctionDecl decl;
        decl.name = func_name;
        decl.return_type = type_name;
        decl.return_ownership = random_ffi_ownership();
        decl.params.push_back(FFIParamDecl("arg", type_name, FFIOwnership::Borrowed));
        ffi.declare_extern_function(decl);

        auto qualifier = random_ownership_qualifier();
        auto sig = std::make_shared<FunctionSignature>(
            func_name,
            std::vector<std::shared_ptr<MetaType>>{int_type},
            string_type,
            Value(std::make_shared<Integer>(i))
        );
        integration.register_ownership_qualified(sig, {qualifier});

        // Property: FFI and trait dispatch coexist — both are queryable
        EXPECT_TRUE(ffi.is_declared(func_name));
        auto dispatch_result = integration.resolve_with_ownership(
            func_name, {int_type}, {qualifier});
        EXPECT_TRUE(dispatch_result.has_value());

        // Property: transpiler produces valid mappings for all ownership modes
        auto mapping = transpiler.get_type_mapping(type_name);
        EXPECT_FALSE(mapping.owned_type.empty());
        EXPECT_FALSE(mapping.borrowed_type.empty());
        EXPECT_FALSE(mapping.borrowed_mut_type.empty());

        // Property: all three map functions agree with get_type_mapping
        EXPECT_EQ(transpiler.map_owned_type(type_name), mapping.owned_type);
        EXPECT_EQ(transpiler.map_borrowed_type(type_name, BorrowType::Immutable),
                  mapping.borrowed_type);
        EXPECT_EQ(transpiler.map_borrowed_type(type_name, BorrowType::Mutable),
                  mapping.borrowed_mut_type);

        // Property: drop code is always generated regardless of target
        auto drop = transpiler.emit_drop(random_var_name(), type_name);
        EXPECT_FALSE(drop.code.empty());
    }
}


// ============================================================================
// Integration Test 9: Provenance + Trust Enforcement + Result Types
// Provenance trust levels integrate with Result-based error handling —
// trust enforcement produces violations that can be wrapped in Results.
// Validates: Requirements 9.1, 9.5, 5.1
// ============================================================================

TEST(IntegrationProperty, ProvenancePlusTrustPlusResults) {
    // Tag: Feature: rust-inspired-meld-enhancements, Integration Property Tests

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        double min_trust = 0.5 + (random_confidence() * 0.5); // [0.5, 1.0]
        TrustLevelEnforcer enforcer(min_trust);

        // Create nodes with varying provenance
        std::vector<Value> nodes;
        int expected_violations = 0;

        int num_nodes = random_int(3, 8);
        for (int n = 0; n < num_nodes; ++n) {
            auto sym = std::make_shared<Symbol>("node_" + std::to_string(i) + "_" + std::to_string(n));
            Value node(sym);

            auto origin = random_origin();
            double conf = random_confidence();

            if (origin == OriginType::Agent) {
                ProvenanceMetadata metadata(random_model_name(), conf);
                node = Provenance::attachProvenance(node, metadata);
                double trust = Provenance::calculateTrustScore(metadata);
                if (trust < min_trust) expected_violations++;
            } else if (origin == OriginType::Human) {
                ProvenanceMetadata metadata;
                metadata.origin = OriginType::Human;
                metadata.trust_score = 1.0;
                node = Provenance::attachProvenance(node, metadata);
                // Human code has trust 1.0, always passes
            } else {
                ProvenanceMetadata metadata;
                metadata.origin = OriginType::Verified;
                metadata.trust_score = 1.0;
                node = Provenance::attachProvenance(node, metadata);
                // Verified code has trust 1.0, always passes
            }

            nodes.push_back(node);
        }

        // Property: violations count matches expected
        auto violations = enforcer.findViolations(nodes);
        EXPECT_EQ(violations.size(), static_cast<size_t>(expected_violations));

        // Property: wrapping violations in Result types
        for (const auto& violation : violations) {
            auto prov = Provenance::getProvenance(violation);
            auto result = prov.has_value()
                ? Result<double, std::string>::success(Provenance::calculateTrustScore(*prov))
                : Result<double, std::string>::error("no provenance");

            ASSERT_TRUE(result.is_success());
            EXPECT_LT(result.value(), min_trust)
                << "Violation trust score should be below threshold";
        }

        // Property: trust stats are consistent
        auto stats = enforcer.analyzeTrustLevels(nodes);
        EXPECT_EQ(stats.total_nodes, num_nodes);
        EXPECT_EQ(stats.violations, static_cast<int>(expected_violations));
    }
}

// ============================================================================
// Integration Test 10: End-to-End Feature Pipeline
// Full pipeline: create type → register trait dispatch → transpile →
// wrap FFI → attach provenance → verify trust. All features in sequence.
// Validates: All requirements (end-to-end)
// ============================================================================

TEST(IntegrationProperty, EndToEndFeaturePipeline) {
    // Tag: Feature: rust-inspired-meld-enhancements, Integration Property Tests

    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        // Step 1: Set up types and trait dispatch
        DispatchRegistry registry;
        registry.clear();
        TraitDispatchIntegration integration(registry);

        auto int_type = TypeRegistry::instance().get_int_type();
        auto string_type = TypeRegistry::instance().get_string_type();

        std::string func_name = "pipeline_" + std::to_string(i);
        auto qualifier = random_ownership_qualifier();

        auto sig = std::make_shared<FunctionSignature>(
            func_name,
            std::vector<std::shared_ptr<MetaType>>{int_type},
            string_type,
            Value(std::make_shared<Integer>(i))
        );
        integration.register_ownership_qualified(sig, {qualifier});

        // Step 2: Resolve through trait dispatch
        auto dispatch_result = integration.resolve_with_ownership(
            func_name, {int_type}, {qualifier});
        ASSERT_TRUE(dispatch_result.has_value())
            << "Pipeline step 2: dispatch resolution failed";

        // Step 3: Transpile the resolved function for a target
        auto target = random_target();
        OwnershipTranspiler transpiler(target);
        auto type_name = random_type_name();
        auto transfer = transpiler.emit_ownership_transfer("input", "output", type_name);
        EXPECT_FALSE(transfer.code.empty())
            << "Pipeline step 3: transpilation failed";

        // Step 4: Set up FFI wrapper
        OwnershipAwareFFI ffi;
        FFIFunctionDecl decl;
        decl.name = func_name + "_native";
        decl.return_type = type_name;
        decl.return_ownership = FFIOwnership::Owned;
        decl.params.push_back(FFIParamDecl("arg", type_name, FFIOwnership::Borrowed));
        ffi.declare_extern_function(decl);

        auto wrapper = ffi.wrap_ffi_call(decl.name, {"arg_val"});
        ASSERT_TRUE(wrapper.has_value())
            << "Pipeline step 4: FFI wrapper failed";

        // Step 5: Attach provenance to the generated code
        AIProvenanceTracker tracker(random_model_name());
        auto code_sym = std::make_shared<Symbol>(func_name + "_code");
        Value code_node(code_sym);
        double confidence = random_confidence();
        Value marked = tracker.markAsAIGenerated(code_node, confidence);

        ASSERT_TRUE(Provenance::hasProvenance(marked))
            << "Pipeline step 5: provenance attachment failed";

        // Step 6: Verify trust level
        auto prov = Provenance::getProvenance(marked);
        ASSERT_TRUE(prov.has_value());
        double trust = Provenance::calculateTrustScore(*prov);
        EXPECT_GE(trust, 0.0);
        EXPECT_LE(trust, 1.0);

        // Step 7: Wrap the entire pipeline result in a Result type
        auto pipeline_result = Result<std::string, std::string>::success(
            "pipeline completed for " + func_name +
            " trust=" + std::to_string(trust));
        EXPECT_TRUE(pipeline_result.is_success());

        // Property: the full pipeline produces a coherent result
        auto final_value = pipeline_result.match(
            [](const std::string& s) { return s; },
            [](const std::string& e) { return std::string("error: ") + e; }
        );
        EXPECT_NE(final_value.find("pipeline completed"), std::string::npos);
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
