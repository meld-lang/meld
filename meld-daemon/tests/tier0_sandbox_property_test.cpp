#include "meld/daemon/tier0_sandbox.hpp"
#include "meld/daemon/semantic_model.hpp"

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <future>

namespace meld::daemon {
namespace {

/// Property 15: Tier 0 Sandbox Isolation
/// For any script, the sandbox SHALL have NO ability to modify the SemanticModel.
RC_GTEST_PROP(Tier0SandboxProperty, SandboxIsolation, ()) {
    SemanticModel model;

    // Add a file to the model
    FileSemantics sem;
    sem.path = "src/test.meld";
    auto node = std::make_shared<ASTNode>();
    node->kind = "function_definition";
    node->name = "test_fn";
    node->type_info = "() -> Int";
    sem.ast = node;
    model.update_file("src/test.meld", std::move(sem));

    auto count_before = model.file_count();
    auto diags_before = model.get_all_diagnostics().size();

    // Generate a random script
    auto script = *rc::gen::nonEmpty<std::string>();

    // Execute it
    Tier0Sandbox sandbox(model);
    sandbox.execute(script);

    // Model must be unchanged
    RC_ASSERT(model.file_count() == count_before);
    RC_ASSERT(model.get_all_diagnostics().size() == diags_before);
    RC_ASSERT(model.has_file("src/test.meld"));
}

/// Property 16: Tier 0 Resource Limits
/// For any script that triggers MEMORY_EXCEED, the sandbox SHALL terminate
/// and return a diagnostic.
RC_GTEST_PROP(Tier0SandboxProperty, ResourceLimitsEnforced, ()) {
    SemanticModel model;
    Tier0Sandbox sandbox(model);

    auto result = sandbox.execute("MEMORY_EXCEED");

    RC_ASSERT(!result.success);
    RC_ASSERT(result.diagnostics.has_value());
    RC_ASSERT(!result.diagnostics->empty());
}

/// Property 17: Tier 0 Concurrent Independence
/// For any two concurrent executions, a crash in one SHALL NOT affect the other.
RC_GTEST_PROP(Tier0SandboxProperty, ConcurrentIndependence, ()) {
    SemanticModel model;

    auto run_ok = [&model]() {
        Tier0Sandbox sandbox(model);
        return sandbox.execute("fn ok() -> Int { 1 }");
    };

    auto run_fail = [&model]() {
        Tier0Sandbox sandbox(model);
        return sandbox.execute("RUNTIME_ERROR");
    };

    auto f1 = std::async(std::launch::async, run_ok);
    auto f2 = std::async(std::launch::async, run_fail);

    auto r1 = f1.get();
    auto r2 = f2.get();

    // The OK execution must succeed regardless of the failing one
    RC_ASSERT(r1.success);
    RC_ASSERT(!r2.success);
}

}  // namespace
}  // namespace meld::daemon
