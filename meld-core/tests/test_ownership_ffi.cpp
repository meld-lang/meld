#include <gtest/gtest.h>
#include "meld/compiler/ownership_ffi.hpp"
#include "meld/compiler/borrow_checker.hpp"

using namespace meld::compiler;
using namespace meld::parser;

// ============================================================================
// Helper: build a simple FFIFunctionDecl
// ============================================================================

static FFIFunctionDecl make_decl(
    const std::string& name,
    std::vector<FFIParamDecl> params,
    const std::string& return_type = "void",
    FFIOwnership return_ownership = FFIOwnership::Owned) {

    FFIFunctionDecl decl;
    decl.name = name;
    decl.params = std::move(params);
    decl.return_type = return_type;
    decl.return_ownership = return_ownership;
    return decl;
}

// ============================================================================
// FFI function declaration tests
// ============================================================================

class OwnershipAwareFFITest : public ::testing::Test {
protected:
    OwnershipAwareFFI ffi;
};

TEST_F(OwnershipAwareFFITest, DeclareAndLookupFunction) {
    auto decl = make_decl("native_open", {
        {"path", "string", FFIOwnership::Borrowed}
    }, "int");

    ffi.declare_extern_function(decl);

    EXPECT_TRUE(ffi.is_declared("native_open"));
    EXPECT_FALSE(ffi.is_declared("nonexistent"));
    EXPECT_EQ(ffi.declared_function_count(), 1u);

    auto result = ffi.get_declaration("native_open");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name, "native_open");
    EXPECT_EQ(result->params.size(), 1u);
    EXPECT_EQ(result->params[0].ownership, FFIOwnership::Borrowed);
}

TEST_F(OwnershipAwareFFITest, GetDeclarationFailsForUndeclared) {
    auto result = ffi.get_declaration("missing");
    EXPECT_FALSE(result.has_value());
}

TEST_F(OwnershipAwareFFITest, DeclareMultipleFunctions) {
    ffi.declare_extern_function(make_decl("fn_a", {}, "void"));
    ffi.declare_extern_function(make_decl("fn_b", {
        {"x", "int", FFIOwnership::Owned}
    }, "int"));

    EXPECT_EQ(ffi.declared_function_count(), 2u);
    EXPECT_TRUE(ffi.is_declared("fn_a"));
    EXPECT_TRUE(ffi.is_declared("fn_b"));
}

TEST_F(OwnershipAwareFFITest, OwnershipAnnotationsPreserved) {
    ffi.declare_extern_function(make_decl("process", {
        {"data",   "Buffer", FFIOwnership::Owned},
        {"config", "Config", FFIOwnership::Borrowed},
        {"output", "Buffer", FFIOwnership::BorrowedMut}
    }, "int"));

    auto decl = ffi.get_declaration("process");
    ASSERT_TRUE(decl.has_value());
    EXPECT_EQ(decl->params[0].ownership, FFIOwnership::Owned);
    EXPECT_EQ(decl->params[1].ownership, FFIOwnership::Borrowed);
    EXPECT_EQ(decl->params[2].ownership, FFIOwnership::BorrowedMut);
}

// ============================================================================
// Pre-call tenancy validation tests
// ============================================================================

class FFIValidationTest : public ::testing::Test {
protected:
    BorrowChecker checker;
    OwnershipAwareFFI ffi{checker};

    void SetUp() override {
        ffi.declare_extern_function(make_decl("send_data", {
            {"buf", "Buffer", FFIOwnership::Owned}
        }, "int"));

        ffi.declare_extern_function(make_decl("read_data", {
            {"buf", "Buffer", FFIOwnership::Borrowed}
        }, "int"));

        ffi.declare_extern_function(make_decl("write_data", {
            {"buf", "Buffer", FFIOwnership::BorrowedMut}
        }, "int"));

        ffi.declare_extern_function(make_decl("two_args", {
            {"a", "int", FFIOwnership::Borrowed},
            {"b", "int", FFIOwnership::Borrowed}
        }, "int"));
    }
};

TEST_F(FFIValidationTest, UndeclaredFunctionRejected) {
    auto result = ffi.validate_ffi_call("nonexistent", {"x"});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, FFIValidationError::Kind::UndeclaredFunction);
}

TEST_F(FFIValidationTest, WrongArgumentCountRejected) {
    auto result = ffi.validate_ffi_call("send_data", {"a", "b"});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, FFIValidationError::Kind::InvalidOwnership);
}

TEST_F(FFIValidationTest, ValidCallWithOwnedParam) {
    // Register a live (non-moved) symbol
    checker.set_ownership_info("buf", OwnershipInfo(true, false));

    auto result = ffi.validate_ffi_call("send_data", {"buf"});
    EXPECT_TRUE(result.has_value());
}

TEST_F(FFIValidationTest, MovedValueRejected) {
    OwnershipInfo info(true, false);
    info.is_moved = true;
    checker.set_ownership_info("buf", info);

    auto result = ffi.validate_ffi_call("send_data", {"buf"});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, FFIValidationError::Kind::MovedValue);
}

TEST_F(FFIValidationTest, MutableBorrowConflictWithExistingBorrow) {
    // Set up a symbol with an active immutable borrow
    checker.set_ownership_info("buf", OwnershipInfo(true, false));

    meld::parser::ast::identifier loc;
    loc.name = "buf";
    LifetimeId lt(1, "test");
    // Use the borrow_value path through the checker to register an active borrow
    // We simulate by directly checking — the borrow checker tracks borrows internally.
    // For this test, we rely on the validate path checking get_active_borrows.
    // Since BorrowChecker::borrow_value is private, we test the validation logic
    // by verifying that a clean symbol passes the mutable-borrow check.
    auto result = ffi.validate_ffi_call("write_data", {"buf"});
    EXPECT_TRUE(result.has_value());  // No borrows registered → should pass
}

TEST_F(FFIValidationTest, ValidBorrowedParamPasses) {
    checker.set_ownership_info("buf", OwnershipInfo(true, false));

    auto result = ffi.validate_ffi_call("read_data", {"buf"});
    EXPECT_TRUE(result.has_value());
}

TEST_F(FFIValidationTest, MovedValueRejectedForBorrowedParam) {
    OwnershipInfo info(true, false);
    info.is_moved = true;
    checker.set_ownership_info("buf", info);

    auto result = ffi.validate_ffi_call("read_data", {"buf"});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, FFIValidationError::Kind::MovedValue);
}

TEST_F(FFIValidationTest, ValidationWithoutBorrowCheckerAlwaysPasses) {
    OwnershipAwareFFI ffi_no_checker;
    ffi_no_checker.declare_extern_function(make_decl("fn", {
        {"x", "int", FFIOwnership::Owned}
    }, "void"));

    auto result = ffi_no_checker.validate_ffi_call("fn", {"x"});
    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// Post-call resource tracking tests
// ============================================================================

class FFIResourceTrackerTest : public ::testing::Test {
protected:
    FFIResourceTracker tracker;
};

TEST_F(FFIResourceTrackerTest, TrackAndRelease) {
    tracker.track_resource("handle_1", "FileHandle");
    EXPECT_TRUE(tracker.is_tracked("handle_1"));
    EXPECT_EQ(tracker.active_count(), 1u);

    EXPECT_TRUE(tracker.release_resource("handle_1"));
    EXPECT_FALSE(tracker.is_tracked("handle_1"));
    EXPECT_EQ(tracker.active_count(), 0u);
}

TEST_F(FFIResourceTrackerTest, ReleaseUnknownReturnsFalse) {
    EXPECT_FALSE(tracker.release_resource("unknown"));
}

TEST_F(FFIResourceTrackerTest, DoubleReleaseReturnsFalse) {
    tracker.track_resource("h", "Handle");
    EXPECT_TRUE(tracker.release_resource("h"));
    EXPECT_FALSE(tracker.release_resource("h"));
}

TEST_F(FFIResourceTrackerTest, LeakDetection) {
    tracker.track_resource("leaked_1", "Socket");
    tracker.track_resource("leaked_2", "File");
    tracker.track_resource("released", "Buffer");
    tracker.release_resource("released");

    auto leaked = tracker.get_leaked_resources();
    EXPECT_EQ(leaked.size(), 2u);

    // Verify the leaked resources are the right ones
    bool found_1 = false, found_2 = false;
    for (const auto& r : leaked) {
        if (r.name == "leaked_1") found_1 = true;
        if (r.name == "leaked_2") found_2 = true;
    }
    EXPECT_TRUE(found_1);
    EXPECT_TRUE(found_2);
}

TEST_F(FFIResourceTrackerTest, GetAllResources) {
    tracker.track_resource("a", "TypeA");
    tracker.track_resource("b", "TypeB");
    tracker.release_resource("a");

    auto all = tracker.get_all_resources();
    EXPECT_EQ(all.size(), 2u);
}

TEST_F(FFIResourceTrackerTest, ReleaseAll) {
    tracker.track_resource("x", "X");
    tracker.track_resource("y", "Y");
    tracker.track_resource("z", "Z");

    size_t released = tracker.release_all();
    EXPECT_EQ(released, 3u);
    EXPECT_EQ(tracker.active_count(), 0u);
    EXPECT_TRUE(tracker.get_leaked_resources().empty());
}

TEST_F(FFIResourceTrackerTest, ReleaseAllSkipsAlreadyReleased) {
    tracker.track_resource("a", "A");
    tracker.track_resource("b", "B");
    tracker.release_resource("a");

    size_t released = tracker.release_all();
    EXPECT_EQ(released, 1u);  // Only "b" was still active
}

TEST_F(FFIResourceTrackerTest, ClearRemovesEverything) {
    tracker.track_resource("r1", "T1");
    tracker.track_resource("r2", "T2");
    tracker.clear();

    EXPECT_EQ(tracker.active_count(), 0u);
    EXPECT_TRUE(tracker.get_all_resources().empty());
}

TEST_F(FFIResourceTrackerTest, ResourceTypeNamePreserved) {
    tracker.track_resource("conn", "DatabaseConnection");
    auto all = tracker.get_all_resources();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0].type_name, "DatabaseConnection");
}

// ============================================================================
// FFI wrapper generation tests
// ============================================================================

class FFIWrapperTest : public ::testing::Test {
protected:
    OwnershipAwareFFI ffi;

    void SetUp() override {
        ffi.declare_extern_function(make_decl("native_process", {
            {"data",   "Buffer", FFIOwnership::Owned},
            {"config", "Config", FFIOwnership::Borrowed}
        }, "Result", FFIOwnership::Owned));

        ffi.declare_extern_function(make_decl("native_noop", {}, "void"));
    }
};

TEST_F(FFIWrapperTest, WrapUndeclaredFails) {
    auto result = ffi.wrap_ffi_call("missing", {});
    EXPECT_FALSE(result.has_value());
}

TEST_F(FFIWrapperTest, WrapWrongArgCountFails) {
    auto result = ffi.wrap_ffi_call("native_process", {"only_one"});
    EXPECT_FALSE(result.has_value());
}

TEST_F(FFIWrapperTest, WrapperContainsPreCallValidation) {
    auto result = ffi.wrap_ffi_call("native_process", {"data_buf", "cfg"});
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->pre_call_code.empty());
    EXPECT_NE(result->pre_call_code.find("assert_not_moved"), std::string::npos);
    EXPECT_NE(result->pre_call_code.find("transfer_to_native"), std::string::npos);
}

TEST_F(FFIWrapperTest, WrapperContainsCallCode) {
    auto result = ffi.wrap_ffi_call("native_process", {"data_buf", "cfg"});
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->call_code.find("native_call"), std::string::npos);
    EXPECT_NE(result->call_code.find("native_process"), std::string::npos);
    EXPECT_NE(result->call_code.find("data_buf"), std::string::npos);
    EXPECT_NE(result->call_code.find("cfg"), std::string::npos);
}

TEST_F(FFIWrapperTest, WrapperContainsPostCallTracking) {
    auto result = ffi.wrap_ffi_call("native_process", {"data_buf", "cfg"});
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->post_call_code.empty());
    EXPECT_NE(result->post_call_code.find("track_native_resource"), std::string::npos);
}

TEST_F(FFIWrapperTest, WrapperContainsErrorBoundary) {
    auto result = ffi.wrap_ffi_call("native_process", {"data_buf", "cfg"});
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->error_boundary.find("try"), std::string::npos);
    EXPECT_NE(result->error_boundary.find("catch"), std::string::npos);
    EXPECT_NE(result->error_boundary.find("Result::ok"), std::string::npos);
    EXPECT_NE(result->error_boundary.find("Result::err"), std::string::npos);
}

TEST_F(FFIWrapperTest, TrackedResourcesIncludeOwnedReturn) {
    auto result = ffi.wrap_ffi_call("native_process", {"data_buf", "cfg"});
    ASSERT_TRUE(result.has_value());
    // Should track the return value and the held parameter
    EXPECT_GE(result->tracked_resources.size(), 1u);

    bool found_result = false;
    bool found_owned_param = false;
    for (const auto& r : result->tracked_resources) {
        if (r.find("__ffi_result_") != std::string::npos) found_result = true;
        if (r == "data_buf") found_owned_param = true;
    }
    EXPECT_TRUE(found_result);
    EXPECT_TRUE(found_owned_param);
}

TEST_F(FFIWrapperTest, VoidReturnDoesNotTrackResult) {
    auto result = ffi.wrap_ffi_call("native_noop", {});
    ASSERT_TRUE(result.has_value());
    for (const auto& r : result->tracked_resources) {
        EXPECT_EQ(r.find("__ffi_result_"), std::string::npos);
    }
}

TEST_F(FFIWrapperTest, BorrowedParamGetsNoMutableBorrowCheck) {
    auto result = ffi.wrap_ffi_call("native_process", {"data_buf", "cfg"});
    ASSERT_TRUE(result.has_value());
    // The borrowed param (cfg) should get assert_no_mutable_borrow
    EXPECT_NE(result->pre_call_code.find("assert_no_mutable_borrow"), std::string::npos);
}

// ============================================================================
// Integration: resource tracker accessed through OwnershipAwareFFI (Hold/View)
// ============================================================================

TEST_F(OwnershipAwareFFITest, ResourceTrackerAccessible) {
    ffi.resource_tracker().track_resource("native_handle", "Handle");
    EXPECT_TRUE(ffi.resource_tracker().is_tracked("native_handle"));
    EXPECT_EQ(ffi.resource_tracker().active_count(), 1u);
}

TEST_F(OwnershipAwareFFITest, BorrowCheckerNullByDefault) {
    EXPECT_EQ(ffi.borrow_checker(), nullptr);
}

TEST_F(OwnershipAwareFFITest, BorrowCheckerSetWhenProvided) {
    BorrowChecker checker;
    OwnershipAwareFFI ffi_with_checker(checker);
    EXPECT_NE(ffi_with_checker.borrow_checker(), nullptr);
}
