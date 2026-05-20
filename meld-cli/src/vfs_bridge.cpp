#include "meld/cli/vfs_bridge.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <array>
#include <random>

#ifdef __linux__
#include <sys/mount.h>
#include <sys/stat.h>
#endif

namespace meld::cli {

namespace {

/// Generate a unique temporary directory name under /tmp
std::filesystem::path make_temp_mount_point() {
    auto tmp = std::filesystem::temp_directory_path();
    // Use random suffix to avoid collisions
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    auto suffix = std::to_string(dist(gen));
    return tmp / ("meld-vfs-" + suffix);
}

/// Run a shell command and return exit code
int run_command(const std::string& cmd) {
    return std::system(cmd.c_str());
}

/// Run a shell command and capture stdout
std::string capture_command(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        result += buffer.data();
    }
    pclose(pipe);
    // Trim trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

}  // namespace

VfsBridge::VfsBridge(std::filesystem::path mount_point, std::string device)
    : mount_point_(std::move(mount_point)),
      device_(std::move(device)),
      active_(true) {}

VfsBridge::~VfsBridge() {
    if (active_) {
        cleanup();
    }
}

VfsBridge::VfsBridge(VfsBridge&& other) noexcept
    : mount_point_(std::move(other.mount_point_)),
      device_(std::move(other.device_)),
      active_(other.active_) {
    other.active_ = false;
}

VfsBridge& VfsBridge::operator=(VfsBridge&& other) noexcept {
    if (this != &other) {
        if (active_) cleanup();
        mount_point_ = std::move(other.mount_point_);
        device_ = std::move(other.device_);
        active_ = other.active_;
        other.active_ = false;
    }
    return *this;
}

std::optional<VfsBridge> VfsBridge::create() {
    auto mount_point = make_temp_mount_point();

    // Create the mount point directory
    std::error_code ec;
    std::filesystem::create_directories(mount_point, ec);
    if (ec) {
        std::cerr << "error: failed to create VFS mount point: "
                  << ec.message() << std::endl;
        return std::nullopt;
    }

#ifdef __linux__
    // Linux: mount tmpfs directly
    int ret = mount("tmpfs", mount_point.c_str(), "tmpfs",
                    0, "size=256m,mode=0700");
    if (ret != 0) {
        std::cerr << "error: failed to mount tmpfs: "
                  << std::strerror(errno) << std::endl;
        std::filesystem::remove(mount_point, ec);
        return std::nullopt;
    }
    return VfsBridge(mount_point);

#elif defined(__APPLE__)
    // macOS: create a RAM disk via hdiutil
    // 256MB = 256 * 2048 = 524288 sectors (512-byte sectors)
    std::string device = capture_command("hdiutil attach -nomount ram://524288");
    if (device.empty()) {
        std::cerr << "error: failed to create RAM disk" << std::endl;
        std::filesystem::remove(mount_point, ec);
        return std::nullopt;
    }

    // Format and mount the RAM disk
    std::string format_cmd = "diskutil erasevolume HFS+ meld-vfs " + device
                             + " >/dev/null 2>&1";
    if (run_command(format_cmd.c_str()) != 0) {
        std::cerr << "error: failed to format RAM disk" << std::endl;
        run_command(("hdiutil detach " + device + " >/dev/null 2>&1").c_str());
        std::filesystem::remove(mount_point, ec);
        return std::nullopt;
    }

    // The volume is auto-mounted at /Volumes/meld-vfs by diskutil
    auto actual_mount = std::filesystem::path("/Volumes/meld-vfs");
    if (!std::filesystem::exists(actual_mount)) {
        std::cerr << "error: RAM disk mount point not found" << std::endl;
        run_command(("hdiutil detach " + device + " >/dev/null 2>&1").c_str());
        std::filesystem::remove(mount_point, ec);
        return std::nullopt;
    }

    // Remove the temp dir — we use the /Volumes mount instead
    std::filesystem::remove(mount_point, ec);
    return VfsBridge(actual_mount, device);

#else
    // Unsupported platform: fall back to a regular temp directory
    // (no memory-only guarantee, but functional)
    std::cerr << "warning: tmpfs not available on this platform, "
                 "using regular temp directory" << std::endl;
    return VfsBridge(mount_point);
#endif
}

void VfsBridge::cleanup() noexcept {
    std::error_code ec;

#ifdef __linux__
    // Unmount tmpfs
    umount2(mount_point_.c_str(), MNT_DETACH);
    std::filesystem::remove_all(mount_point_, ec);

#elif defined(__APPLE__)
    // Detach the RAM disk (also unmounts)
    if (!device_.empty()) {
        run_command(("hdiutil detach " + device_ + " -force >/dev/null 2>&1").c_str());
    }
    // The /Volumes/meld-vfs directory is removed by detach

#else
    // Fallback: just remove the temp directory
    std::filesystem::remove_all(mount_point_, ec);
#endif

    active_ = false;
}

bool VfsBridge::extract_output(const std::filesystem::path& vfs_relative_path,
                               const std::filesystem::path& destination) const {
    if (!active_) return false;

    auto source = mount_point_ / vfs_relative_path;
    if (!std::filesystem::exists(source)) {
        std::cerr << "error: VFS output path not found: "
                  << vfs_relative_path.string() << std::endl;
        return false;
    }

    std::error_code ec;
    // Create destination parent directories if needed
    auto dest_parent = destination.parent_path();
    if (!dest_parent.empty()) {
        std::filesystem::create_directories(dest_parent, ec);
        if (ec) {
            std::cerr << "error: failed to create output directory: "
                      << ec.message() << std::endl;
            return false;
        }
    }

    if (std::filesystem::is_directory(source)) {
        std::filesystem::copy(source, destination,
                              std::filesystem::copy_options::recursive
                              | std::filesystem::copy_options::overwrite_existing,
                              ec);
    } else {
        std::filesystem::copy_file(source, destination,
                                   std::filesystem::copy_options::overwrite_existing,
                                   ec);
    }

    if (ec) {
        std::cerr << "error: failed to extract VFS output: "
                  << ec.message() << std::endl;
        return false;
    }

    return true;
}

}  // namespace meld::cli
