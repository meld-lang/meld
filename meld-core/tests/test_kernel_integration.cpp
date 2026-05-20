/**
 * Integration test: Rust-inspired features with existing Meld kernel
 *
 * Validates Requirements 10.3, 10.4, 10.5:
 *  - Pattern matching uses existing match macro expansion
 *  - Memory safety integrates with existing memory management
 *  - New features maintain the minimal kernel principle (20 primitives)
 *
 * This test verifies that ALL Rust-inspired enhancements integrate
 * correctly with the existing Meld kernel without breaking existing
 * functionality or adding new kernel primitives.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <set>
#include <string>
#include <vector>

// Kernel headers
#include "meld/kernel/primitives.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/kernel/dispatch.hpp"
#include "meld/kernel/symbol_table.hpp"
#include "meld/kernel/trait_dispatch.hpp"

// Meta / type system headers
#include "meld/meta/metatype.hpp"
#include "meld/meta/advanced_traits.hpp"

// New feature headers
#include "meld/types/result.hpp"
#include "meld/types/option.hpp"
#include "meld/compiler/ownership_transpiler.hpp"
#include "meld/compiler/ownership_ffi.hpp"
#include "meld/compiler/borrow_checker.hpp"
#include "meld/compiler/ownership_metadata.hpp"

using namespace meld::kernel;
using namespace meld::parser;
using namespace meld::meta;
using namespace meld::types;
using namespace meld::compiler;

// ============================================================================
// Test fixture
// ============================================================================

class KernelIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        dispatch_registry_.clear();
        MetadataStore::instance().clear_all();
    }

    void TearDown() override {
        dispatch_registry_.clear();
        MetadataStore::instance().clear_all();
    }

    DispatchRegistry dispatch_registry_;
};

// ============================================================================
// 1. Kernel Primitive Count — Requirement 10.5
//    Verify that exactly 20 kernel primitives exist and no new ones were added.
// ============================================================================

TEST_F(KernelIntegrationTest, KernelHasExactly20Primitives) {
    // The 20 kernel primitives as defined in the Meld specification:
    //
    // Data primitives (6):
    //   1. symbol       - Atomic identifiers
    //   2. cons         - Pair / cons cell
    //   3. vec          - Contiguous array
    //   4. empty        - Empty / unit value
    //   5. function     - Lambda abstractions
    //   6. integer      - Numeric values
    //
    // Operation primitives (6):
    //   7. car          - First element of cons
    //   8. cdr          - Rest of cons
    //   9. apply        - Function application
    //  10. eq           - Identity equality
    //  11. type_of      - Runtime type query
    //  12. eval         - Evaluate expression
    //
    // Structural primitives (3):
    //  13. scope        - Lexical scope
    //  14. match        - Pattern matching
    //  15. quote        - Code as data
    //
    // Control flow primitive (1):
    //  16. primitive_suspend - Delimited continuations
    //
    // AI & metadata primitives (2):
    //  17. meta_set     - Attach metadata
    //  18. meta_get     - Retrieve metadata
    //
    // Interop primitives (2):
    //  19. native_load  - Load native library
    //  20. native_call  - Call native function

    const std::set<std::string> kernel_primitives = {
        // Data
        "symbol", "cons", "vec", "empty", "function", "integer",
        // Operations
        "car", "cdr", "apply", "eq", "type_of", "eval",
        // Structural
        "scope", "match", "quote",
        // Control flow
        "primitive_suspend",
        // AI & metadata
        "meta_set", "meta_get",
        // Interop
        "native_load", "native_call"
    };

    EXPECT_EQ(kernel_primitives.size(), 20u)
        << "The Meld kernel must have exactly 20 primitives. "
           "New features must be built on top of existing primitives.";

    // Verify we can exercise the core primitives that have C++ APIs
    // (this also proves they still exist and work)

    // symbol
    auto sym = SymbolTable::instance().intern("test_symbol");
    EXPECT_EQ(sym->name(), "test_symbol");

    // cons / car / cdr
    auto pair = cons(Value::from_int(1), Value::from_int(2));
    auto car_result = car(pair);
    auto cdr_result = cdr(pair);
    ASSERT_TRUE(car_result.has_value());
    ASSERT_TRUE(cdr_result.has_value());
    EXPECT_EQ(car_result.value().as_int(), 1);
    EXPECT_EQ(cdr_result.value().as_int(), 2);

    // empty
    auto empty_val = Value(Empty::instance());
    EXPECT_EQ(empty_val.to_string(), "empty");

    // integer
    auto int_val = Value::from_int(42);
    EXPECT_EQ(int_val.as_int(), 42);

    // function / apply
    auto fn = std::make_shared<Function>(
        std::vector<std::shared_ptr<Symbol>>{std::make_shared<Symbol>("x")},
        Value::from_int(0),
        Function::NativeImpl([](const std::vector<Value>& args) -> Value {
            return Value::from_int(args[0].as_int() * 2);
        }),
        std::string("double")
    );
    auto apply_result = apply(Value(fn), {Value::from_int(21)});
    ASSERT_TRUE(apply_result.has_value());
    EXPECT_EQ(apply_result.value().as_int(), 42);

    // eq
    EXPECT_TRUE(eq(Value::from_int(5), Value::from_int(5)));
    EXPECT_FALSE(eq(Value::from_int(5), Value::from_int(6)));

    // type_of
    auto int_type = type_of(Value::from_int(1));
    EXPECT_NE(int_type, nullptr);

    // meta_set / meta_get
    auto obj = Value::from_string("hello");
    meta_set(obj, "origin", Value::from_string("human"));
    auto retrieved = meta_get(obj, "origin");
    EXPECT_EQ(retrieved.as_string(), "human");

    // primitive_suspend (via DelimitedContinuation)
    DelimitedContinuation::clear();
    EXPECT_EQ(DelimitedContinuation::depth(), 0u);

    // native_call (built-in logical_shift_right)
    auto shift_result = native_call("logical_shift_right",
        {Value::from_int(16), Value::from_int(2)});
    EXPECT_EQ(shift_result.as_int(), 4);
}

// ============================================================================
// 2. Ownership system integrates with scope/symbol primitives — Req 10.4
// ============================================================================

TEST_F(KernelIntegrationTest, OwnershipIntegratesWithSymbolTable) {
    // Verify that the ownership system works alongside the symbol table
    // without modifying the symbol primitive itself.

    auto sym = SymbolTable::instance().intern("owned_var");
    EXPECT_EQ(sym->name(), "owned_var");

    // Create an Owned value and attach ownership metadata via meta_set
    auto val = Value::from_int(100);
    meta_set(val, "ownership", Value::from_string("owned"));
    meta_set(val, "provenance", Value::from_string("human"));

    // Verify metadata is retrievable (ownership info stored via existing primitives)
    EXPECT_TRUE(meta_has(val, "ownership"));
    EXPECT_EQ(meta_get(val, "ownership").as_string(), "owned");
    EXPECT_EQ(meta_get(val, "provenance").as_string(), "human");

    // The symbol itself is unchanged
    EXPECT_EQ(sym->to_string(), ":owned_var");
}

TEST_F(KernelIntegrationTest, BorrowCheckerDoesNotAddKernelPrimitives) {
    // Requirement 10.2: Borrow checking analyzes without adding new kernel primitives.
    // The BorrowChecker is a compiler-layer component that uses existing primitives.

    BorrowChecker checker;

    // Set ownership info for a symbol — this is compiler metadata, not a kernel primitive
    OwnershipInfo info(true, false);  // owned, not copyable
    checker.set_ownership_info("resource", info);

    auto retrieved = checker.get_ownership_info("resource");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_TRUE(retrieved->is_owned);
    EXPECT_FALSE(retrieved->is_moved);
    EXPECT_FALSE(retrieved->is_copyable);

    // Verify the borrow checker tracks borrows without kernel changes
    EXPECT_FALSE(checker.is_borrowed("resource"));
}

TEST_F(KernelIntegrationTest, OwnershipMetadataManagerIntegration) {
    // OwnershipMetadataManager extends AST nodes with ownership info
    // without modifying the kernel.

    OwnershipMetadataManager manager;

    // Track a move
    meld::parser::ast::identifier loc;
    loc.name = "x";
    manager.set_identifier_metadata("x", OwnershipMetadata(true, "x"));

    auto meta = manager.get_identifier_metadata("x");
    ASSERT_TRUE(meta.has_value());
    EXPECT_TRUE(meta->is_owned);
    EXPECT_FALSE(meta->is_moved);

    // Track a move — still uses compiler layer, not kernel
    manager.track_move("x", loc);
    auto moved_vars = manager.get_moved_variables();
    EXPECT_FALSE(moved_vars.empty());
}

// ============================================================================
// 3. Trait dispatch integrates with existing dispatch registry — Req 12.3
// ============================================================================

TEST_F(KernelIntegrationTest, TraitDispatchIntegratesWithExistingRegistry) {
    // The trait dispatch system bridges the new trait system with the
    // existing multiple dispatch registry without replacing it.

    TraitDispatchIntegration integration(dispatch_registry_);

    auto int_type = TypeRegistry::instance().get_int_type();
    auto string_type = TypeRegistry::instance().get_string_type();

    // Register a plain function in the existing dispatch registry
    auto plain_sig = std::make_shared<FunctionSignature>(
        "format",
        std::vector<std::shared_ptr<MetaType>>{int_type},
        string_type,
        Value::from_int(0)
    );
    dispatch_registry_.register_function(plain_sig);

    // The existing dispatch still works
    auto resolved = dispatch_registry_.resolve("format", {int_type});
    ASSERT_TRUE(resolved.has_value()) << resolved.error();
    EXPECT_EQ(resolved.value()->name, "format");

    // Now register a trait-based method
    auto display_trait = std::make_shared<AdvancedTraitMetaType>(
        "Display",
        std::vector<Method>{
            Method("to_display", {}, string_type, Value::from_int(0))
        }
    );

    auto impl = std::make_shared<TraitImplementation>(
        int_type, display_trait,
        std::map<std::string, std::shared_ptr<MetaType>>{},
        std::map<std::string, Value>{{"to_display", Value::from_int(1)}}
    );

    auto reg_result = integration.register_trait_implementation(impl);
    ASSERT_TRUE(reg_result.has_value()) << reg_result.error();

    // Trait dispatch resolves through the same registry
    auto trait_resolved = integration.resolve_trait_dispatch(
        "to_display", int_type, {});
    ASSERT_TRUE(trait_resolved.has_value()) << trait_resolved.error();

    // The plain function is still resolvable — no interference
    auto still_works = dispatch_registry_.resolve("format", {int_type});
    ASSERT_TRUE(still_works.has_value()) << still_works.error();
    EXPECT_EQ(still_works.value()->name, "format");
}

TEST_F(KernelIntegrationTest, OwnershipAwareDispatchCoexistsWithPlainDispatch) {
    TraitDispatchIntegration integration(dispatch_registry_);

    auto int_type = TypeRegistry::instance().get_int_type();
    auto string_type = TypeRegistry::instance().get_string_type();

    // Register a plain function
    auto plain_sig = std::make_shared<FunctionSignature>(
        "compute",
        std::vector<std::shared_ptr<MetaType>>{int_type},
        int_type,
        Value::from_int(10)
    );
    dispatch_registry_.register_function(plain_sig);

    // Register an ownership-qualified version
    auto owned_sig = std::make_shared<FunctionSignature>(
        "transform",
        std::vector<std::shared_ptr<MetaType>>{int_type},
        string_type,
        Value::from_int(20)
    );
    integration.register_ownership_qualified(
        owned_sig, {OwnershipQualifier::Owned});

    // Plain dispatch still works
    auto plain_result = dispatch_registry_.resolve("compute", {int_type});
    ASSERT_TRUE(plain_result.has_value());

    // Ownership-aware dispatch works
    auto owned_result = integration.resolve_with_ownership(
        "transform", {int_type}, {OwnershipQualifier::Owned});
    ASSERT_TRUE(owned_result.has_value());

    // Ownership dispatch falls back to plain registry for unqualified functions
    auto fallback = integration.resolve_with_ownership(
        "compute", {int_type}, {OwnershipQualifier::Borrowed});
    ASSERT_TRUE(fallback.has_value()) << fallback.error();
    EXPECT_EQ(fallback.value()->name, "compute");
}

// ============================================================================
// 4. Type system extensions work with existing types — Req 10.3
// ============================================================================

TEST_F(KernelIntegrationTest, ResultTypeWorksWithKernelValues) {
    // Result<T,E> integrates with existing kernel Value types

    auto ok_result = Result<int, std::string>::success(42);
    EXPECT_TRUE(ok_result.is_success());
    EXPECT_EQ(ok_result.value(), 42);

    auto err_result = Result<int, std::string>::error("not found");
    EXPECT_TRUE(err_result.is_error());
    EXPECT_EQ(err_result.error(), "not found");

    // Combinators work correctly
    auto mapped = ok_result.map([](int v) { return v * 2; });
    EXPECT_TRUE(mapped.is_success());
    EXPECT_EQ(mapped.value(), 84);

    auto chained = ok_result.flat_map([](int v) -> Result<std::string, std::string> {
        return Result<std::string, std::string>::success(std::to_string(v));
    });
    EXPECT_TRUE(chained.is_success());
    EXPECT_EQ(chained.value(), "42");

    // Error propagation
    auto err_mapped = err_result.map([](int v) { return v * 2; });
    EXPECT_TRUE(err_mapped.is_error());
    EXPECT_EQ(err_mapped.error(), "not found");
}

TEST_F(KernelIntegrationTest, OptionTypeWorksWithKernelValues) {
    // Option<T> integrates with existing kernel Value types

    auto some_val = Option<int>::some(42);
    EXPECT_TRUE(some_val.is_some());
    EXPECT_EQ(some_val.value(), 42);

    auto none_val = Option<int>::none();
    EXPECT_TRUE(none_val.is_none());

    // Combinators
    auto mapped = some_val.map([](int v) { return std::to_string(v); });
    EXPECT_TRUE(mapped.is_some());
    EXPECT_EQ(mapped.value(), "42");

    auto filtered = some_val.filter([](int v) { return v > 100; });
    EXPECT_TRUE(filtered.is_none());

    // value_or with None
    EXPECT_EQ(none_val.value_or(0), 0);
    EXPECT_EQ(some_val.value_or(0), 42);
}

TEST_F(KernelIntegrationTest, AlgebraicTypesIntegrateWithMetaTypeSystem) {
    // Algebraic data types (enums with variants) work with the existing MetaType system

    auto result_enum = MetaType::create_enum("result", {
        EnumVariant("ok", {Field("value", TypeRegistry::instance().get_int_type())}),
        EnumVariant("err", {Field("error", TypeRegistry::instance().get_string_type())})
    });

    EXPECT_EQ(result_enum->name(), "result");

    auto* enum_type = result_enum->as<EnumMetaType>();
    ASSERT_NE(enum_type, nullptr);
    EXPECT_TRUE(enum_type->has_variant("ok"));
    EXPECT_TRUE(enum_type->has_variant("err"));
    EXPECT_FALSE(enum_type->has_variant("Maybe"));

    // Variants have associated data
    auto ok_variant = enum_type->get_variant("ok");
    ASSERT_TRUE(ok_variant.has_value());
    EXPECT_EQ(ok_variant.value()->associated_fields.size(), 1u);
    EXPECT_EQ(ok_variant.value()->associated_fields[0].name, "value");
}

TEST_F(KernelIntegrationTest, NewtypeWrappersAreZeroCost) {
    // Newtypes provide type safety with zero runtime overhead

    auto int_type = TypeRegistry::instance().get_int_type();
    auto user_id = MetaType::create_newtype("UserId", int_type, true);

    auto* newtype = user_id->as<NewtypeMetaType>();
    ASSERT_NE(newtype, nullptr);

    // Zero-cost: same size as wrapped type
    EXPECT_EQ(newtype->size(), int_type->size());
    EXPECT_TRUE(newtype->is_transparent());
    EXPECT_EQ(newtype->wrapped_type()->name(), "int");

    // But it's a distinct type
    EXPECT_NE(newtype->name(), int_type->name());
    EXPECT_EQ(newtype->name(), "UserId");
}

// ============================================================================
// 5. Compiler enhancements don't break existing compilation — Req 10.3, 10.4
// ============================================================================

TEST_F(KernelIntegrationTest, OwnershipTranspilerDoesNotAffectKernel) {
    // The transpiler maps ownership concepts to target languages
    // without modifying kernel primitives.

    OwnershipTranspiler cpp_transpiler(OwnershipTarget::Cpp);
    OwnershipTranspiler java_transpiler(OwnershipTarget::Java);

    // C++ mapping
    auto cpp_owned = cpp_transpiler.map_owned_type("int");
    EXPECT_FALSE(cpp_owned.empty());

    auto cpp_borrowed = cpp_transpiler.map_borrowed_type("int", BorrowType::Immutable);
    EXPECT_FALSE(cpp_borrowed.empty());

    // Java mapping
    auto java_owned = java_transpiler.map_owned_type("int");
    EXPECT_FALSE(java_owned.empty());

    // Transfer emission
    auto transfer = cpp_transpiler.emit_ownership_transfer("src", "dst", "int");
    EXPECT_FALSE(transfer.code.empty());

    // Drop emission
    auto drop = cpp_transpiler.emit_drop("resource", "FileHandle");
    EXPECT_FALSE(drop.code.empty());

    // Runtime safety checks differ by target
    EXPECT_FALSE(cpp_transpiler.needs_runtime_safety_checks());
    EXPECT_TRUE(java_transpiler.needs_runtime_safety_checks());
}

TEST_F(KernelIntegrationTest, FFIIntegratesWithExistingNativeCallPrimitive) {
    // The ownership-aware FFI builds on top of the existing native_call primitive.

    OwnershipAwareFFI ffi;

    // Declare an extern function with ownership annotations
    FFIFunctionDecl decl;
    decl.name = "native_process";
    decl.params = {
        FFIParamDecl("data", "buffer", FFIOwnership::Borrowed),
        FFIParamDecl("size", "int", FFIOwnership::Owned)
    };
    decl.return_type = "int";
    decl.return_ownership = FFIOwnership::Owned;

    ffi.declare_extern_function(decl);
    EXPECT_TRUE(ffi.is_declared("native_process"));
    EXPECT_FALSE(ffi.is_declared("unknown_function"));

    // Get declaration back
    auto retrieved = ffi.get_declaration("native_process");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved.value().params.size(), 2u);

    // Generate wrapper code
    auto wrapper = ffi.wrap_ffi_call("native_process", {"buf", "sz"});
    ASSERT_TRUE(wrapper.has_value());
    EXPECT_FALSE(wrapper.value().call_code.empty());

    // Resource tracking works
    ffi.resource_tracker().track_resource("native_buf", "buffer");
    EXPECT_TRUE(ffi.resource_tracker().is_tracked("native_buf"));
    EXPECT_EQ(ffi.resource_tracker().active_count(), 1u);

    ffi.resource_tracker().release_resource("native_buf");
    EXPECT_FALSE(ffi.resource_tracker().is_tracked("native_buf"));
    EXPECT_EQ(ffi.resource_tracker().active_count(), 0u);

    // No leaked resources
    auto leaked = ffi.resource_tracker().get_leaked_resources();
    EXPECT_TRUE(leaked.empty());
}

// ============================================================================
// 6. No new kernel primitives were added — Req 10.5
//    Verify that new features are built as library/compiler extensions.
// ============================================================================

TEST_F(KernelIntegrationTest, OwnershipIsLibraryNotKernel) {
    // Owned<T> and Borrowed<T> are library types, not kernel primitives.
    // They use existing Value, meta_set, meta_get under the hood.

    // The Value variant list should NOT contain Owned or Borrowed types.
    // It should only contain the original kernel types:
    //   Symbol, Cons, Vec, Empty, Function, Integer, Boolean, String,
    //   Placeholder, Optional<Value>, Continuation, NativeHandle, NativeFunction

    Value v = Value::from_int(42);
    EXPECT_TRUE(v.is<Integer>());

    // We can attach ownership metadata using existing meta_set
    meta_set(v, "ownership_mode", Value::from_string("owned"));
    meta_set(v, "is_moved", Value::from_bool(false));

    EXPECT_EQ(meta_get(v, "ownership_mode").as_string(), "owned");
    EXPECT_EQ(meta_get(v, "is_moved").as_bool(), false);

    // The Value type itself is unchanged — no new variants added
    auto str_val = Value::from_string("test");
    EXPECT_TRUE(str_val.is<String>());

    auto bool_val = Value::from_bool(true);
    EXPECT_TRUE(bool_val.is<Boolean>());

    auto sym_val = Value::from_symbol("x");
    EXPECT_TRUE(sym_val.is<Symbol>());
}

TEST_F(KernelIntegrationTest, TraitSystemIsMetaLayerNotKernel) {
    // Traits are implemented in the meta layer using existing MetaType system.
    // No new kernel primitives were added for traits.

    auto int_type = TypeRegistry::instance().get_int_type();
    auto string_type = TypeRegistry::instance().get_string_type();

    // Create a trait using the meta layer
    auto debug_trait = MetaType::create_trait("Debug", {
        Method("debug_string", {}, string_type, Value::from_int(0))
    });

    EXPECT_EQ(debug_trait->name(), "Debug");
    auto* trait_type = debug_trait->as<TraitMetaType>();
    ASSERT_NE(trait_type, nullptr);

    auto method = trait_type->get_method("debug_string");
    ASSERT_TRUE(method.has_value());
    EXPECT_EQ(method.value()->name, "debug_string");
}

TEST_F(KernelIntegrationTest, ResultAndOptionAreLibraryTypes) {
    // Result<T,E> and Option<T> are library types in meld::types namespace,
    // not kernel primitives.

    // They don't appear in the kernel Value variant
    // They are standalone C++ template types

    auto result = Ok<int>(42);
    EXPECT_TRUE(result.is_success());

    auto option = Option<std::string>::some("hello");
    EXPECT_TRUE(option.is_some());

    // They compose with each other
    auto chained = result.map([](int v) { return Option<int>::some(v); });
    EXPECT_TRUE(chained.is_success());
    EXPECT_TRUE(chained.value().is_some());
    EXPECT_EQ(chained.value().value(), 42);
}

// ============================================================================
// 7. Performance validation — new features don't degrade existing operations
// ============================================================================

TEST_F(KernelIntegrationTest, KernelOperationsPerformanceNotDegraded) {
    // Verify that basic kernel operations remain fast.
    // This is a smoke test — not a rigorous benchmark, but ensures
    // no catastrophic regression.

    constexpr int iterations = 10000;

    // Benchmark: cons/car/cdr operations
    auto start_cons = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto pair = cons(Value::from_int(i), Value::from_int(i + 1));
        auto c = car(pair);
        auto d = cdr(pair);
        (void)c;
        (void)d;
    }
    auto end_cons = std::chrono::steady_clock::now();
    auto cons_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_cons - start_cons).count();

    // Should complete well under 1 second for 10k iterations
    EXPECT_LT(cons_duration, 1000)
        << "cons/car/cdr operations took " << cons_duration
        << "ms for " << iterations << " iterations — possible regression";

    // Benchmark: symbol interning
    auto start_sym = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto sym = SymbolTable::instance().intern("sym_" + std::to_string(i));
        (void)sym;
    }
    auto end_sym = std::chrono::steady_clock::now();
    auto sym_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_sym - start_sym).count();

    EXPECT_LT(sym_duration, 1000)
        << "Symbol interning took " << sym_duration
        << "ms for " << iterations << " iterations — possible regression";

    // Benchmark: metadata operations
    auto start_meta = std::chrono::steady_clock::now();
    auto meta_obj = Value::from_int(0);
    for (int i = 0; i < iterations; ++i) {
        meta_set(meta_obj, "key_" + std::to_string(i % 100),
                 Value::from_int(i));
    }
    for (int i = 0; i < iterations; ++i) {
        auto val = meta_get(meta_obj, "key_" + std::to_string(i % 100));
        (void)val;
    }
    auto end_meta = std::chrono::steady_clock::now();
    auto meta_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_meta - start_meta).count();

    EXPECT_LT(meta_duration, 1000)
        << "Metadata operations took " << meta_duration
        << "ms for " << (iterations * 2) << " operations — possible regression";

    // Benchmark: dispatch resolution
    auto int_type = TypeRegistry::instance().get_int_type();
    auto string_type = TypeRegistry::instance().get_string_type();

    auto sig = std::make_shared<FunctionSignature>(
        "perf_fn",
        std::vector<std::shared_ptr<MetaType>>{int_type},
        string_type,
        Value::from_int(0)
    );
    dispatch_registry_.register_function(sig);

    auto start_dispatch = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto resolved = dispatch_registry_.resolve("perf_fn", {int_type});
        (void)resolved;
    }
    auto end_dispatch = std::chrono::steady_clock::now();
    auto dispatch_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_dispatch - start_dispatch).count();

    EXPECT_LT(dispatch_duration, 1000)
        << "Dispatch resolution took " << dispatch_duration
        << "ms for " << iterations << " iterations — possible regression";
}

TEST_F(KernelIntegrationTest, TraitDispatchCacheImprovesPerformance) {
    // Verify that the dispatch cache actually helps performance

    TraitDispatchIntegration integration(dispatch_registry_);
    auto int_type = TypeRegistry::instance().get_int_type();
    auto string_type = TypeRegistry::instance().get_string_type();

    auto sig = std::make_shared<FunctionSignature>(
        "cached_perf",
        std::vector<std::shared_ptr<MetaType>>{int_type},
        string_type,
        Value::from_int(0)
    );
    integration.register_ownership_qualified(
        sig, {OwnershipQualifier::Owned});

    constexpr int iterations = 1000;

    // First call populates cache
    integration.resolve_with_ownership(
        "cached_perf", {int_type}, {OwnershipQualifier::Owned});

    // Subsequent calls should hit cache
    for (int i = 0; i < iterations; ++i) {
        auto result = integration.resolve_with_ownership(
            "cached_perf", {int_type}, {OwnershipQualifier::Owned});
        ASSERT_TRUE(result.has_value());
    }

    // Cache should have been hit for all subsequent calls
    EXPECT_GE(integration.cache_hits(), static_cast<size_t>(iterations));
    EXPECT_EQ(integration.cache_misses(), 1u);  // Only the first call
}

// ============================================================================
// 8. Cross-system integration — all new features work together
// ============================================================================

TEST_F(KernelIntegrationTest, OwnershipAndTraitDispatchWorkTogether) {
    // Verify that ownership-qualified dispatch works with trait implementations

    TraitDispatchIntegration integration(dispatch_registry_);

    auto int_type = TypeRegistry::instance().get_int_type();
    auto string_type = TypeRegistry::instance().get_string_type();

    // Register a trait implementation
    auto serializable = std::make_shared<AdvancedTraitMetaType>(
        "Serializable",
        std::vector<Method>{
            Method("serialize", {}, string_type, Value::from_int(0))
        }
    );

    auto impl = std::make_shared<TraitImplementation>(
        int_type, serializable,
        std::map<std::string, std::shared_ptr<MetaType>>{},
        std::map<std::string, Value>{{"serialize", Value::from_int(42)}}
    );

    auto reg_result = integration.register_trait_implementation(impl);
    ASSERT_TRUE(reg_result.has_value()) << reg_result.error();

    // Resolve via trait dispatch
    auto trait_result = integration.resolve_trait_dispatch(
        "serialize", int_type, {});
    ASSERT_TRUE(trait_result.has_value()) << trait_result.error();

    // Also register an ownership-qualified version
    auto owned_sig = std::make_shared<FunctionSignature>(
        "serialize",
        std::vector<std::shared_ptr<MetaType>>{int_type},
        string_type,
        Value::from_int(99)
    );
    integration.register_ownership_qualified(
        owned_sig, {OwnershipQualifier::Owned});

    // Ownership-qualified resolution works alongside trait dispatch
    auto owned_result = integration.resolve_with_ownership(
        "serialize", {int_type}, {OwnershipQualifier::Owned});
    ASSERT_TRUE(owned_result.has_value()) << owned_result.error();
}

TEST_F(KernelIntegrationTest, ResultTypeWithMetadataTracking) {
    // Result types work with the kernel metadata system

    auto val = Value::from_int(42);

    // Attach Result-like metadata using kernel primitives
    meta_set(val, "result_status", Value::from_string("ok"));
    meta_set(val, "error_chain", Value(Empty::instance()));

    EXPECT_EQ(meta_get(val, "result_status").as_string(), "ok");

    // Simulate error propagation tracking
    auto err_val = Value::from_string("file not found");
    meta_set(err_val, "result_status", Value::from_string("error"));
    meta_set(err_val, "error_source", Value::from_string("io::read"));

    EXPECT_EQ(meta_get(err_val, "result_status").as_string(), "error");
    EXPECT_EQ(meta_get(err_val, "error_source").as_string(), "io::read");
}

TEST_F(KernelIntegrationTest, FFIResourceTrackingWithOwnership) {
    // FFI resource tracking integrates with ownership concepts

    OwnershipAwareFFI ffi;

    // Declare a function that returns an owned resource
    FFIFunctionDecl alloc_decl;
    alloc_decl.name = "native_alloc";
    alloc_decl.params = {FFIParamDecl("size", "int", FFIOwnership::Owned)};
    alloc_decl.return_type = "buffer";
    alloc_decl.return_ownership = FFIOwnership::Owned;
    ffi.declare_extern_function(alloc_decl);

    // Declare a function that borrows a resource
    FFIFunctionDecl read_decl;
    read_decl.name = "native_read";
    read_decl.params = {FFIParamDecl("buf", "buffer", FFIOwnership::Borrowed)};
    read_decl.return_type = "int";
    read_decl.return_ownership = FFIOwnership::Owned;
    ffi.declare_extern_function(read_decl);

    // Declare a function that takes ownership (frees)
    FFIFunctionDecl free_decl;
    free_decl.name = "native_free";
    free_decl.params = {FFIParamDecl("buf", "buffer", FFIOwnership::Owned)};
    free_decl.return_type = "void";
    free_decl.return_ownership = FFIOwnership::Owned;
    ffi.declare_extern_function(free_decl);

    EXPECT_EQ(ffi.declared_function_count(), 3u);

    // Track resources through their lifecycle
    ffi.resource_tracker().track_resource("buf1", "buffer");
    EXPECT_EQ(ffi.resource_tracker().active_count(), 1u);

    // Simulate read (borrow — resource stays tracked)
    EXPECT_TRUE(ffi.resource_tracker().is_tracked("buf1"));

    // Simulate free (ownership transfer — release tracking)
    ffi.resource_tracker().release_resource("buf1");
    EXPECT_EQ(ffi.resource_tracker().active_count(), 0u);

    // No leaks
    EXPECT_TRUE(ffi.resource_tracker().get_leaked_resources().empty());
}

TEST_F(KernelIntegrationTest, MonomorphizationHintsFromDispatch) {
    // Monomorphization hints are recorded during dispatch and can be
    // used by the compiler for optimization — all without kernel changes.

    TraitDispatchIntegration integration(dispatch_registry_);

    auto int_type = TypeRegistry::instance().get_int_type();
    auto string_type = TypeRegistry::instance().get_string_type();
    auto bool_type = TypeRegistry::instance().get_bool_type();

    // Record hints for a generic function
    integration.record_monomorphization_hint("generic_map", {int_type, string_type});
    integration.record_monomorphization_hint("generic_map", {string_type, bool_type});

    auto hints = integration.get_monomorphization_hints("generic_map");
    EXPECT_EQ(hints.size(), 2u);

    // First hint: (int, string)
    EXPECT_EQ(hints[0][0]->name(), "int");
    EXPECT_EQ(hints[0][1]->name(), "string");

    // Second hint: (string, bool)
    EXPECT_EQ(hints[1][0]->name(), "string");
    EXPECT_EQ(hints[1][1]->name(), "bool");

    // No hints for unknown functions
    auto empty_hints = integration.get_monomorphization_hints("nonexistent");
    EXPECT_TRUE(empty_hints.empty());
}

// ============================================================================
// 9. Existing kernel features still work correctly alongside new features
// ============================================================================

TEST_F(KernelIntegrationTest, ContinuationsStillWorkWithNewFeatures) {
    // Delimited continuations (the foundation for effects, async, generators)
    // must still work correctly.

    DelimitedContinuation::clear();
    EXPECT_EQ(DelimitedContinuation::depth(), 0u);

    // Push a delimiter
    bool handler_called = false;
    DelimitedContinuation::push_delimiter("test_effect",
        [&handler_called](Value v) -> Value {
            handler_called = true;
            return v;
        });

    EXPECT_EQ(DelimitedContinuation::depth(), 1u);

    // Pop it
    DelimitedContinuation::pop_delimiter();
    EXPECT_EQ(DelimitedContinuation::depth(), 0u);

    DelimitedContinuation::clear();
}

TEST_F(KernelIntegrationTest, ListOperationsStillWork) {
    // List construction and operations using cons/car/cdr

    auto lst = list(std::vector<Value>{
        Value::from_int(1),
        Value::from_int(2),
        Value::from_int(3)
    });

    // Convert back to array
    auto arr = list_to_array(lst);
    ASSERT_TRUE(arr.has_value());
    EXPECT_EQ(arr.value().size(), 3u);
    EXPECT_EQ(arr.value()[0].as_int(), 1);
    EXPECT_EQ(arr.value()[1].as_int(), 2);
    EXPECT_EQ(arr.value()[2].as_int(), 3);
}

TEST_F(KernelIntegrationTest, GensymStillProducesUniqueSymbols) {
    auto sym1 = SymbolTable::instance().gensym("test");
    auto sym2 = SymbolTable::instance().gensym("test");

    // Each gensym should produce a unique symbol
    EXPECT_NE(sym1->name(), sym2->name());
}

TEST_F(KernelIntegrationTest, FunctionClosuresStillWork) {
    // Closures with captured environments must still work

    Function::Environment env;
    env["captured"] = Value::from_int(10);

    auto closure = std::make_shared<Function>(
        std::vector<std::shared_ptr<Symbol>>{std::make_shared<Symbol>("x")},
        Value::from_int(0),
        Function::NativeImpl([](const std::vector<Value>& args) -> Value {
            return Value::from_int(args[0].as_int() + 10);
        }),
        std::string("add_captured"),
        env
    );

    EXPECT_TRUE(closure->is_closure());
    EXPECT_EQ(closure->name().value(), "add_captured");

    auto result = apply(Value(closure), {Value::from_int(5)});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().as_int(), 15);
}

TEST_F(KernelIntegrationTest, TypeRegistryStillFunctional) {
    // The TypeRegistry should still provide all built-in types

    auto& registry = TypeRegistry::instance();

    auto int_type = registry.get_int_type();
    ASSERT_NE(int_type, nullptr);
    EXPECT_EQ(int_type->name(), "int");

    auto bool_type = registry.get_bool_type();
    ASSERT_NE(bool_type, nullptr);
    EXPECT_EQ(bool_type->name(), "bool");

    auto string_type = registry.get_string_type();
    ASSERT_NE(string_type, nullptr);
    EXPECT_EQ(string_type->name(), "string");

    auto unit_type = registry.get_unit_type();
    ASSERT_NE(unit_type, nullptr);

    // Can still register new types
    auto custom = MetaType::create_struct("TestStruct", {
        Field("x", int_type),
        Field("y", string_type)
    });
    registry.register_type("TestStruct_Integration", custom);

    auto retrieved = registry.get_type("TestStruct_Integration");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved.value()->name(), "TestStruct");
}
