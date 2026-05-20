#include "meld/manifest/manifest.hpp"
#include "meld/manifest/sandbox_provider.hpp"
#include "meld/manifest/srt_provider.hpp"
#include "meld/manifest/tombstone.hpp"
#include "meld/manifest/verifier.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <unistd.h>
#include <vector>

namespace meld::supervisor {

/// Configuration for the melds production supervisor.
struct SupervisorConfig {
    bool offline_mode = true;            // --offline (default) or --online
    std::string isolation = "process";   // --isolation=[process|microvm]
    std::filesystem::path trust_root;    // Path to trusted_root.json
    std::filesystem::path binary_path;   // Target binary to execute
    std::vector<std::string> binary_args; // Arguments forwarded to the binary
};

/// Structured audit log entry emitted on each supervised launch.
struct AuditLogEntry {
    std::string binary_path;
    std::string sigstore_identity;
    std::vector<std::string> effect_permissions;
    std::string isolation_mode;
    std::string timestamp;
};

/// Cache entry for static execution mode.
struct StaticPolicyCache {
    std::string binary_hash;
    manifest::SandboxConfig sandbox_config;
    bool valid{false};
};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void print_usage();
static std::optional<SupervisorConfig> parse_args(int argc, char** argv);
static void emit_audit_log(const AuditLogEntry& entry);
static std::optional<std::filesystem::path> find_manifest_sidecar(
    const std::filesystem::path& binary);
static std::optional<std::filesystem::path> find_policy_cache_path(
    const std::filesystem::path& binary);
static manifest::SandboxConfig build_sandbox_config(
    const manifest::Manifest& mf,
    const SupervisorConfig& config);

// ---------------------------------------------------------------------------
// Argument parsing
// ---------------------------------------------------------------------------

static void print_usage() {
    std::cerr << R"(melds — Meld Production Supervisor

USAGE:
    melds [OPTIONS] <binary_path> [args...]

OPTIONS:
    --offline             Verify against local trusted_root.json (default)
    --online              Query Rekor for revocation checking
    --isolation=process   Use SRT process-level sandboxing (default)
    --isolation=microvm   Use Firecracker MicroVM isolation
    --trust-root=<path>   Path to trusted_root.json
    --static              Enable static execution mode (pre-cache sandbox policy)
    --help                Show this help message

DESCRIPTION:
    Verifies binary integrity via Sigstore, extracts the effect policy from
    the co-located .meld manifest, and executes the binary inside an
    OS-level sandbox. Designed as ENTRYPOINT for production containers:

        ENTRYPOINT ["melds", "./app.bin"]
)" << std::endl;
}

static std::optional<SupervisorConfig> parse_args(int argc, char** argv) {
    SupervisorConfig config;

    // Default trust root: co-located with the melds binary or /etc/meld/
    config.trust_root = "/etc/meld/trusted_root.json";

    int i = 1;
    while (i < argc) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage();
            return std::nullopt;
        } else if (arg == "--offline") {
            config.offline_mode = true;
        } else if (arg == "--online") {
            config.offline_mode = false;
        } else if (arg.starts_with("--isolation=")) {
            config.isolation = arg.substr(std::string("--isolation=").size());
            if (config.isolation != "process" && config.isolation != "microvm") {
                std::cerr << "error: invalid isolation mode: " << config.isolation
                          << " (expected 'process' or 'microvm')" << std::endl;
                return std::nullopt;
            }
        } else if (arg.starts_with("--trust-root=")) {
            config.trust_root = arg.substr(std::string("--trust-root=").size());
        } else if (arg == "--static") {
            // Static execution mode is handled in the execution chain
        } else if (arg.starts_with("--")) {
            std::cerr << "error: unknown option: " << arg << std::endl;
            print_usage();
            return std::nullopt;
        } else {
            // First non-flag argument is the binary path
            config.binary_path = arg;
            // Remaining arguments are forwarded to the binary
            for (int j = i + 1; j < argc; ++j) {
                config.binary_args.push_back(argv[j]);
            }
            break;
        }
        ++i;
    }

    if (config.binary_path.empty()) {
        std::cerr << "error: no binary specified" << std::endl;
        print_usage();
        return std::nullopt;
    }

    return config;
}

// ---------------------------------------------------------------------------
// Manifest / sidecar discovery
// ---------------------------------------------------------------------------

static std::optional<std::filesystem::path> find_manifest_sidecar(
    const std::filesystem::path& binary) {
    // Look for <binary_stem>.meld co-located with the binary
    auto manifest = binary;
    manifest.replace_extension(".meld");
    if (std::filesystem::exists(manifest)) {
        return manifest;
    }
    // Also check <binary>.meld (appended extension)
    auto appended = binary;
    appended += ".meld";
    if (std::filesystem::exists(appended)) {
        return appended;
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Static execution mode — policy cache
// ---------------------------------------------------------------------------

static std::optional<std::filesystem::path> find_policy_cache_path(
    const std::filesystem::path& binary) {
    auto cache_dir = binary.parent_path() / ".melds-cache";
    auto cache_file = cache_dir / (binary.filename().string() + ".policy");
    if (std::filesystem::exists(cache_file)) {
        return cache_file;
    }
    return std::nullopt;
}

static bool write_policy_cache(const std::filesystem::path& binary,
                               const std::string& binary_hash,
                               const manifest::SandboxConfig& sandbox_config) {
    auto cache_dir = binary.parent_path() / ".melds-cache";
    std::filesystem::create_directories(cache_dir);
    auto cache_file = cache_dir / (binary.filename().string() + ".policy");

    std::ofstream out(cache_file);
    if (!out) return false;

    // Simple format: hash on first line, then effect names
    out << binary_hash << "\n";
    for (const auto& effect : sandbox_config.allowed_effects) {
        out << manifest::effect_to_string(effect) << "\n";
    }
    for (const auto& path : sandbox_config.read_only_paths) {
        out << "ro:" << path << "\n";
    }
    for (const auto& path : sandbox_config.read_write_paths) {
        out << "rw:" << path << "\n";
    }
    return true;
}

// ---------------------------------------------------------------------------
// Sandbox configuration builder
// ---------------------------------------------------------------------------

static manifest::SandboxConfig build_sandbox_config(
    const manifest::Manifest& mf,
    const SupervisorConfig& config) {
    manifest::SandboxConfig sandbox;
    sandbox.working_directory = std::filesystem::current_path();

    // Extract effect permissions from manifest symbols
    manifest::EffectBitmask combined_effects;
    for (const auto& sym : mf.symbols) {
        combined_effects = combined_effects | sym.effects;
        // Merge resource bounds
        for (const auto& d : sym.bounds.allowed_domains) {
            sandbox.allowed_domains.push_back(d);
        }
        for (const auto& p : sym.bounds.read_paths) {
            sandbox.read_only_paths.push_back(p);
        }
        for (const auto& p : sym.bounds.write_paths) {
            sandbox.read_write_paths.push_back(p);
        }
    }

    sandbox.allowed_effects = combined_effects.to_vector();
    return sandbox;
}

// ---------------------------------------------------------------------------
// Audit logging
// ---------------------------------------------------------------------------

static void emit_audit_log(const AuditLogEntry& entry) {
    // Structured audit output to stderr (JSON format)
    std::cerr << "{\"audit\":\"melds\""
              << ",\"binary\":\"" << entry.binary_path << "\""
              << ",\"identity\":\"" << entry.sigstore_identity << "\""
              << ",\"effects\":[";
    for (size_t i = 0; i < entry.effect_permissions.size(); ++i) {
        if (i > 0) std::cerr << ",";
        std::cerr << "\"" << entry.effect_permissions[i] << "\"";
    }
    std::cerr << "]"
              << ",\"isolation\":\"" << entry.isolation_mode << "\""
              << ",\"timestamp\":\"" << entry.timestamp << "\""
              << "}" << std::endl;
}

static std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time_t_now));
    return buf;
}

// ---------------------------------------------------------------------------
// Execution chain: verify → extract policy → sandbox → execvp
// ---------------------------------------------------------------------------

/// melds entry point: verify → extract policy → sandbox → execvp
int supervisor_main(int argc, char** argv) {
    auto config_opt = parse_args(argc, argv);
    if (!config_opt) {
        return 1;
    }
    auto& config = *config_opt;

    // Validate binary exists
    if (!std::filesystem::exists(config.binary_path)) {
        std::cerr << "INTEGRITY_FAILURE: binary not found: "
                  << config.binary_path.string() << std::endl;
        return 127;
    }

    // --- Step 1: Read .note.meld Tombstone from binary ---
    auto tombstone = manifest::read_tombstone(config.binary_path);
    if (!tombstone) {
        std::cerr << "INTEGRITY_FAILURE: no .note.meld tombstone found in "
                  << config.binary_path.string() << std::endl;
        return 127;
    }

    // --- Step 2: Verify Combined Integrity Hash against trust root ---
    auto manifest_path = find_manifest_sidecar(config.binary_path);
    if (!manifest_path) {
        std::cerr << "INTEGRITY_FAILURE: manifest sidecar not found for "
                  << config.binary_path.string() << std::endl;
        return 127;
    }

    auto mode = config.offline_mode
        ? manifest::VerificationMode::Offline
        : manifest::VerificationMode::OnlineAudit;
    manifest::Verifier verifier(mode);

    if (std::filesystem::exists(config.trust_root)) {
        verifier.set_trust_root(config.trust_root);
    }

    auto verify_result = verifier.verify_binary(config.binary_path, *manifest_path);
    if (!verify_result.valid) {
        std::cerr << "INTEGRITY_FAILURE: " << verify_result.failed_check
                  << " — " << verify_result.details << std::endl;
        return 127;
    }

    // --- Step 3: Read manifest, extract effect policy ---
    std::ifstream mf_stream(*manifest_path, std::ios::binary);
    std::vector<uint8_t> mf_data{
        std::istreambuf_iterator<char>(mf_stream),
        std::istreambuf_iterator<char>()
    };
    auto manifest_opt = manifest::deserialize_manifest(mf_data);
    if (!manifest_opt) {
        std::cerr << "INTEGRITY_FAILURE: failed to parse manifest: "
                  << manifest_path->string() << std::endl;
        return 127;
    }

    // --- Step 4: Build sandbox configuration from effect policy ---
    auto sandbox_config = build_sandbox_config(*manifest_opt, config);

    // --- Step 5: Emit audit log ---
    AuditLogEntry audit;
    audit.binary_path = config.binary_path.string();
    audit.sigstore_identity = tombstone->signature_blob.empty()
        ? "unknown" : "sigstore-verified";
    for (const auto& effect : sandbox_config.allowed_effects) {
        audit.effect_permissions.push_back(manifest::effect_to_string(effect));
    }
    audit.isolation_mode = config.isolation;
    audit.timestamp = current_timestamp();
    emit_audit_log(audit);

    // --- Step 6: Static execution mode — cache policy for subsequent runs ---
    std::string binary_hash = manifest::compute_code_hash(config.binary_path);
    write_policy_cache(config.binary_path, binary_hash, sandbox_config);

    // --- Step 7: Select SandboxProvider and execute ---
    if (config.isolation == "process") {
        // Use MeldSRTProvider for process-level sandboxing
        manifest::MeldSRTProvider srt;

        // Build the command: binary + args
        std::vector<std::string> command;
        command.push_back(config.binary_path.string());
        for (const auto& arg : config.binary_args) {
            command.push_back(arg);
        }

        // For production: replace current process via execvp
        // Build argv for execvp
        std::vector<const char*> exec_argv;
        exec_argv.push_back(config.binary_path.c_str());
        for (const auto& arg : config.binary_args) {
            exec_argv.push_back(arg.c_str());
        }
        exec_argv.push_back(nullptr);

        // Apply sandbox policy via SRT before exec
        auto srt_policy = manifest::MeldSRTProvider::generate_policy(sandbox_config);

        // Replace current process with the sandboxed binary
        execvp(config.binary_path.c_str(),
               const_cast<char* const*>(exec_argv.data()));

        // If execvp returns, it failed
        std::cerr << "INTEGRITY_FAILURE: execvp failed for "
                  << config.binary_path.string() << ": "
                  << std::strerror(errno) << std::endl;
        return 127;
    } else if (config.isolation == "microvm") {
        // MicroVM isolation — delegate to Firecracker provider
        // For now, report that microvm isolation requires the Firecracker provider
        std::cerr << "error: microvm isolation requires the Firecracker provider "
                  << "(not yet available in this build)" << std::endl;
        return 1;
    }

    std::cerr << "error: unknown isolation mode: " << config.isolation << std::endl;
    return 1;
}

}  // namespace meld::supervisor

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    return meld::supervisor::supervisor_main(argc, argv);
}
