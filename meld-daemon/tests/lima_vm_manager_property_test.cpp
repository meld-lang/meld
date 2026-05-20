#include "meld/daemon/lima_vm_manager.hpp"

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <filesystem>
#include <memory>

namespace meld::daemon {
namespace {

namespace fs = std::filesystem;

/// Helper: create a LimaVmManager with mock executor for testing.
struct MockVmEnv {
    fs::path lock_path;
    bool vm_running{false};
    int start_count{0};
    int stop_count{0};

    std::unique_ptr<LimaVmManager> create() {
        lock_path = fs::temp_directory_path() /
                    ("meld_prop_vmm_" + std::to_string(reinterpret_cast<uintptr_t>(this)) + ".lock");
        fs::remove(lock_path);

        auto mgr = std::make_unique<LimaVmManager>(std::chrono::minutes(5));
        mgr->set_lock_path(lock_path.string());
        mgr->set_command_executor(
            [this](const std::string& cmd, std::string& output) -> int {
                if (cmd.find("limactl ls") != std::string::npos) {
                    output = vm_running ? "Running" : "Stopped";
                    return 0;
                }
                if (cmd.find("limactl start") != std::string::npos) {
                    vm_running = true;
                    start_count++;
                    output = "Started";
                    return 0;
                }
                if (cmd.find("limactl stop") != std::string::npos) {
                    vm_running = false;
                    stop_count++;
                    output = "Stopped";
                    return 0;
                }
                return 0;
            });
        return mgr;
    }

    ~MockVmEnv() {
        std::error_code ec;
        fs::remove(lock_path, ec);
    }
};

/// Property 18: Lima VM Single Instance
/// For any number of ensure_running() calls from a single daemon,
/// exactly one VM start should occur.
RC_GTEST_PROP(LimaVmManagerProperty, SingleInstanceStart, ()) {
    if (!LimaVmManager::is_required()) {
        RC_SUCCEED("Lima not required on this platform");
    }

    MockVmEnv env;
    auto mgr = env.create();

    auto num_calls = *rc::gen::inRange(1, 10);
    for (int i = 0; i < num_calls; ++i) {
        mgr->ensure_running();
    }

    // Only one start should have occurred
    RC_ASSERT(env.start_count == 1);
    RC_ASSERT(env.vm_running);
}

/// Property 19: Lima VM Last-Daemon Shutdown
/// The last daemon to shut down SHALL stop the VM.
/// Non-last daemons SHALL NOT stop the VM.
RC_GTEST_PROP(LimaVmManagerProperty, LastDaemonShutdown, ()) {
    if (!LimaVmManager::is_required()) {
        RC_SUCCEED("Lima not required on this platform");
    }

    MockVmEnv env;
    auto mgr = env.create();

    mgr->ensure_running();
    RC_ASSERT(env.vm_running);

    auto is_last = *rc::gen::arbitrary<bool>();
    mgr->set_other_daemons_running(!is_last);
    mgr->shutdown();

    if (is_last) {
        RC_ASSERT(!env.vm_running);
        RC_ASSERT(env.stop_count == 1);
    } else {
        RC_ASSERT(env.vm_running);
        RC_ASSERT(env.stop_count == 0);
    }
}

/// Property 20: Lima VM Platform Skip
/// On Linux, all Lima lifecycle management SHALL be skipped.
RC_GTEST_PROP(LimaVmManagerProperty, PlatformSkip, ()) {
#ifdef __linux__
    MockVmEnv env;
    auto mgr = env.create();

    RC_ASSERT(!LimaVmManager::is_required());

    mgr->ensure_running();
    RC_ASSERT(env.start_count == 0);

    mgr->shutdown();
    RC_ASSERT(env.stop_count == 0);

    auto status = mgr->check_status();
    RC_ASSERT(status.running);  // Linux: native KVM always available
#else
    RC_SUCCEED("Not on Linux — skip platform test");
#endif
}

}  // namespace
}  // namespace meld::daemon
