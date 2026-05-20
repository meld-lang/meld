#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace meld::cli {

/// RAII helper that creates a memory-only tmpfs mount point as a working
/// directory for VFS bridge mode (Req 22.1–22.3).
///
/// On Linux: `mount -t tmpfs tmpfs <mount_point>`
/// On macOS: `diskutil erasevolume HFS+ <label> $(hdiutil attach -nomount ram://...)`
///
/// The mount is destroyed when the VfsBridge object goes out of scope.
class VfsBridge {
public:
    /// Create and mount a memory-only filesystem.
    /// Returns nullopt if the mount fails.
    static std::optional<VfsBridge> create();

    ~VfsBridge();

    // Move-only (RAII resource)
    VfsBridge(VfsBridge&& other) noexcept;
    VfsBridge& operator=(VfsBridge&& other) noexcept;
    VfsBridge(const VfsBridge&) = delete;
    VfsBridge& operator=(const VfsBridge&) = delete;

    /// Path to the memory-only mount point (use as working directory).
    const std::filesystem::path& mount_point() const { return mount_point_; }

    /// Copy specified output files from the tmpfs to a real filesystem path.
    /// Used by --vfs-output (Req 22.4).
    bool extract_output(const std::filesystem::path& vfs_relative_path,
                        const std::filesystem::path& destination) const;

private:
    explicit VfsBridge(std::filesystem::path mount_point, std::string device = "");

    void cleanup() noexcept;

    std::filesystem::path mount_point_;
    std::string device_;  // macOS: device path for detach
    bool active_{true};
};

}  // namespace meld::cli
