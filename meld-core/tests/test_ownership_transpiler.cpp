#include <gtest/gtest.h>
#include "meld/compiler/ownership_transpiler.hpp"

using namespace meld::compiler;

// ============================================================================
// C++ ownership mapping tests
// ============================================================================

class CppOwnershipTranspilerTest : public ::testing::Test {
protected:
    OwnershipTranspiler transpiler{OwnershipTarget::Cpp};
};

TEST_F(CppOwnershipTranspilerTest, OwnedMapsToUniquePtr) {
    EXPECT_EQ(transpiler.map_owned_type("Widget"), "std::unique_ptr<Widget>");
    EXPECT_EQ(transpiler.map_owned_type("int"), "std::unique_ptr<int>");
}

TEST_F(CppOwnershipTranspilerTest, ImmutableBorrowMapsToConstRef) {
    auto result = transpiler.map_borrowed_type("Widget", BorrowType::Immutable);
    EXPECT_EQ(result, "const Widget&");
}

TEST_F(CppOwnershipTranspilerTest, MutableBorrowMapsToRef) {
    auto result = transpiler.map_borrowed_type("Widget", BorrowType::Mutable);
    EXPECT_EQ(result, "Widget&");
}

TEST_F(CppOwnershipTranspilerTest, TransferUsesStdMove) {
    auto result = transpiler.emit_ownership_transfer("src", "dst", "Widget");
    EXPECT_NE(result.code.find("std::move(src)"), std::string::npos);
    EXPECT_NE(result.code.find("std::unique_ptr<Widget>"), std::string::npos);
    EXPECT_NE(result.code.find("dst"), std::string::npos);
}

TEST_F(CppOwnershipTranspilerTest, DropUsesReset) {
    auto result = transpiler.emit_drop("obj", "Widget");
    EXPECT_NE(result.code.find("obj.reset()"), std::string::npos);
}

TEST_F(CppOwnershipTranspilerTest, NoRuntimeSafetyChecks) {
    EXPECT_FALSE(transpiler.needs_runtime_safety_checks());
}

TEST_F(CppOwnershipTranspilerTest, RequiredImportsIncludeMemory) {
    auto imports = transpiler.get_required_imports();
    EXPECT_FALSE(imports.empty());
    bool has_memory = false;
    for (const auto& imp : imports) {
        if (imp == "<memory>") has_memory = true;
    }
    EXPECT_TRUE(has_memory);
}

// ============================================================================
// Java ownership mapping tests
// ============================================================================

class JavaOwnershipTranspilerTest : public ::testing::Test {
protected:
    OwnershipTranspiler transpiler{OwnershipTarget::Java};
};

TEST_F(JavaOwnershipTranspilerTest, OwnedMapsToAnnotation) {
    EXPECT_EQ(transpiler.map_owned_type("Widget"), "@Owned Widget");
}

TEST_F(JavaOwnershipTranspilerTest, ImmutableBorrowMapsToAnnotation) {
    auto result = transpiler.map_borrowed_type("Widget", BorrowType::Immutable);
    EXPECT_EQ(result, "@Borrowed Widget");
}

TEST_F(JavaOwnershipTranspilerTest, MutableBorrowMapsToAnnotation) {
    auto result = transpiler.map_borrowed_type("Widget", BorrowType::Mutable);
    EXPECT_EQ(result, "@BorrowedMut Widget");
}

TEST_F(JavaOwnershipTranspilerTest, TransferNullsOriginal) {
    auto result = transpiler.emit_ownership_transfer("src", "dst", "Widget");
    EXPECT_NE(result.code.find("dst = src"), std::string::npos);
    EXPECT_NE(result.code.find("src = null"), std::string::npos);
}

TEST_F(JavaOwnershipTranspilerTest, DropUsesTryFinally) {
    auto result = transpiler.emit_drop("obj", "Widget");
    EXPECT_NE(result.code.find("try"), std::string::npos);
    EXPECT_NE(result.code.find("finally"), std::string::npos);
    EXPECT_NE(result.code.find("obj = null"), std::string::npos);
}

TEST_F(JavaOwnershipTranspilerTest, NeedsRuntimeSafetyChecks) {
    EXPECT_TRUE(transpiler.needs_runtime_safety_checks());
}

TEST_F(JavaOwnershipTranspilerTest, UseAfterMoveCheckThrows) {
    auto result = transpiler.emit_use_after_move_check("obj");
    EXPECT_NE(result.code.find("== null"), std::string::npos);
    EXPECT_NE(result.code.find("IllegalStateException"), std::string::npos);
    EXPECT_FALSE(result.safety_checks.empty());
}

TEST_F(JavaOwnershipTranspilerTest, BorrowCheckCallsRuntime) {
    auto result = transpiler.emit_borrow_check("obj", BorrowType::Mutable);
    EXPECT_NE(result.code.find("MeldOwnership.checkBorrow"), std::string::npos);
    EXPECT_FALSE(result.required_imports.empty());
}

// ============================================================================
// Rust ownership mapping tests
// ============================================================================

class RustOwnershipTranspilerTest : public ::testing::Test {
protected:
    OwnershipTranspiler transpiler{OwnershipTarget::Rust};
};

TEST_F(RustOwnershipTranspilerTest, OwnedMapsToPlainType) {
    // Rust owns by default — no wrapper needed
    EXPECT_EQ(transpiler.map_owned_type("Widget"), "Widget");
}

TEST_F(RustOwnershipTranspilerTest, ImmutableBorrowMapsToSharedRef) {
    EXPECT_EQ(transpiler.map_borrowed_type("Widget", BorrowType::Immutable), "&Widget");
}

TEST_F(RustOwnershipTranspilerTest, MutableBorrowMapsToMutRef) {
    EXPECT_EQ(transpiler.map_borrowed_type("Widget", BorrowType::Mutable), "&mut Widget");
}

TEST_F(RustOwnershipTranspilerTest, TransferIsNaturalMove) {
    auto result = transpiler.emit_ownership_transfer("src", "dst", "Widget");
    EXPECT_NE(result.code.find("let dst"), std::string::npos);
    EXPECT_NE(result.code.find("= src"), std::string::npos);
    // Rust move doesn't need std::move or null-out
    EXPECT_EQ(result.code.find("null"), std::string::npos);
    EXPECT_EQ(result.code.find("std::move"), std::string::npos);
}

TEST_F(RustOwnershipTranspilerTest, DropCallsDrop) {
    auto result = transpiler.emit_drop("obj", "Widget");
    EXPECT_NE(result.code.find("drop(obj)"), std::string::npos);
}

TEST_F(RustOwnershipTranspilerTest, NoRuntimeSafetyChecks) {
    EXPECT_FALSE(transpiler.needs_runtime_safety_checks());
}

TEST_F(RustOwnershipTranspilerTest, NoRequiredImports) {
    auto imports = transpiler.get_required_imports();
    EXPECT_TRUE(imports.empty());
}

// ============================================================================
// WebAssembly ownership mapping tests
// ============================================================================

class WasmOwnershipTranspilerTest : public ::testing::Test {
protected:
    OwnershipTranspiler transpiler{OwnershipTarget::Wasm};
};

TEST_F(WasmOwnershipTranspilerTest, OwnedMapsToI32Pointer) {
    EXPECT_EQ(transpiler.map_owned_type("Widget"), "i32");
}

TEST_F(WasmOwnershipTranspilerTest, BorrowedMapsToI32Pointer) {
    EXPECT_EQ(transpiler.map_borrowed_type("Widget", BorrowType::Immutable), "i32");
    EXPECT_EQ(transpiler.map_borrowed_type("Widget", BorrowType::Mutable), "i32");
}

TEST_F(WasmOwnershipTranspilerTest, TransferZerosSource) {
    auto result = transpiler.emit_ownership_transfer("src", "dst", "Widget");
    EXPECT_NE(result.code.find("local.get $src"), std::string::npos);
    EXPECT_NE(result.code.find("local.set $dst"), std::string::npos);
    EXPECT_NE(result.code.find("i32.const 0"), std::string::npos);
    EXPECT_NE(result.code.find("local.set $src"), std::string::npos);
}

TEST_F(WasmOwnershipTranspilerTest, DropCallsFree) {
    auto result = transpiler.emit_drop("obj", "Widget");
    EXPECT_NE(result.code.find("call $free"), std::string::npos);
    EXPECT_NE(result.code.find("local.get $obj"), std::string::npos);
}

TEST_F(WasmOwnershipTranspilerTest, NeedsRuntimeSafetyChecks) {
    EXPECT_TRUE(transpiler.needs_runtime_safety_checks());
}

TEST_F(WasmOwnershipTranspilerTest, UseAfterMoveCheckTraps) {
    auto result = transpiler.emit_use_after_move_check("ptr");
    EXPECT_NE(result.code.find("i32.eqz"), std::string::npos);
    EXPECT_NE(result.code.find("__meld_trap_use_after_move"), std::string::npos);
}

TEST_F(WasmOwnershipTranspilerTest, BorrowCheckCallsRuntime) {
    auto result = transpiler.emit_borrow_check("ptr", BorrowType::Immutable);
    EXPECT_NE(result.code.find("__meld_check_borrow"), std::string::npos);
}

// ============================================================================
// Cross-language safety guarantee tests
// ============================================================================

class CrossLanguageSafetyTest : public ::testing::Test {};

TEST_F(CrossLanguageSafetyTest, CompileTimeTargetsSkipRuntimeChecks) {
    OwnershipTranspiler cpp_t(OwnershipTarget::Cpp);
    OwnershipTranspiler rust_t(OwnershipTarget::Rust);

    EXPECT_FALSE(cpp_t.needs_runtime_safety_checks());
    EXPECT_FALSE(rust_t.needs_runtime_safety_checks());

    // Compile-time targets produce comments, not runtime code
    auto cpp_check = cpp_t.emit_use_after_move_check("x");
    EXPECT_NE(cpp_check.code.find("compile time"), std::string::npos);
    EXPECT_TRUE(cpp_check.safety_checks.empty());

    auto rust_check = rust_t.emit_use_after_move_check("x");
    EXPECT_NE(rust_check.code.find("compile time"), std::string::npos);
    EXPECT_TRUE(rust_check.safety_checks.empty());
}

TEST_F(CrossLanguageSafetyTest, RuntimeTargetsGenerateSafetyChecks) {
    OwnershipTranspiler java_t(OwnershipTarget::Java);
    OwnershipTranspiler wasm_t(OwnershipTarget::Wasm);

    EXPECT_TRUE(java_t.needs_runtime_safety_checks());
    EXPECT_TRUE(wasm_t.needs_runtime_safety_checks());

    auto java_check = java_t.emit_use_after_move_check("x");
    EXPECT_FALSE(java_check.safety_checks.empty());

    auto wasm_check = wasm_t.emit_use_after_move_check("x");
    EXPECT_FALSE(wasm_check.safety_checks.empty());
}

TEST_F(CrossLanguageSafetyTest, TypeMappingIsConsistentAcrossTargets) {
    // Every target should produce a non-empty mapping for a given type
    std::vector<OwnershipTarget> targets = {
        OwnershipTarget::Cpp, OwnershipTarget::Java,
        OwnershipTarget::Rust, OwnershipTarget::Wasm
    };

    for (auto target : targets) {
        OwnershipTranspiler t(target);
        auto mapping = t.get_type_mapping("MyType");
        EXPECT_FALSE(mapping.owned_type.empty())
            << "Empty owned_type for target " << static_cast<int>(target);
        EXPECT_FALSE(mapping.borrowed_type.empty())
            << "Empty borrowed_type for target " << static_cast<int>(target);
        EXPECT_FALSE(mapping.borrowed_mut_type.empty())
            << "Empty borrowed_mut_type for target " << static_cast<int>(target);
    }
}
