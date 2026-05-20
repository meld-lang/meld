#include "meld/manifest/manifest.hpp"
#include "meld/manifest/signing.hpp"
#include "meld/manifest/tombstone.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void print_usage() {
    std::cerr << "Usage: meld-sign <binary> <manifest> [options]\n"
              << "Options:\n"
              << "  --key <path>    Sign with private key (traditional)\n"
              << "  --sigstore      Sign with Sigstore (keyless OIDC)\n"
              << "  --help          Show this help message\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    std::filesystem::path binary_path;
    std::filesystem::path manifest_path;
    std::filesystem::path key_path;
    bool use_sigstore = false;

    // Parse positional args
    binary_path = argv[1];
    manifest_path = argv[2];

    // Parse options
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--key" && i + 1 < argc) {
            key_path = argv[++i];
        } else if (arg == "--sigstore") {
            use_sigstore = true;
        } else if (arg == "--help") {
            print_usage();
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage();
            return 1;
        }
    }

    if (key_path.empty() && !use_sigstore) {
        std::cerr << "Error: must specify --key <path> or --sigstore\n";
        return 1;
    }

    // Validate binary exists
    if (!std::filesystem::exists(binary_path)) {
        std::cerr << "Error: binary not found: " << binary_path << "\n";
        return 1;
    }

    // Read manifest and validate code_hash (Req 8.5)
    std::ifstream ifs(manifest_path, std::ios::binary);
    if (!ifs.is_open()) {
        std::cerr << "Error: cannot open manifest: " << manifest_path << "\n";
        return 1;
    }
    std::vector<uint8_t> manifest_bytes((std::istreambuf_iterator<char>(ifs)),
                                         std::istreambuf_iterator<char>());
    ifs.close();

    auto manifest = meld::manifest::deserialize_manifest(manifest_bytes);
    if (!manifest) {
        std::cerr << "Error: cannot parse manifest\n";
        return 1;
    }

    auto actual_hash = meld::manifest::compute_code_hash(binary_path);
    if (actual_hash != manifest->code_hash) {
        std::cerr << "Error: binary/manifest inconsistency (Req 8.6)\n"
                  << "  Expected code_hash: " << manifest->code_hash << "\n"
                  << "  Actual code_hash:   " << actual_hash << "\n";
        return 1;
    }

    // Sign
    meld::manifest::SigningEngine engine;
    meld::manifest::SigningResult result;

    if (use_sigstore) {
        result = engine.sign_with_sigstore(manifest_bytes);
    } else {
        result = engine.sign_with_key(manifest_bytes, key_path);
    }

    if (!result.success) {
        std::cerr << "Error: signing failed: " << result.error_message << "\n";
        return 1;
    }

    // Create and embed tombstone
    meld::manifest::Tombstone ts;
    ts.manifest_hash = meld::manifest::compute_file_hash(manifest_path);
    ts.signature_blob = result.signature_blob;
    ts.effect_id = actual_hash;

    if (!meld::manifest::embed_tombstone(binary_path, ts)) {
        std::cerr << "Error: failed to embed tombstone\n";
        return 1;
    }

    std::cout << "Signed successfully: " << binary_path << "\n";
    return 0;
}
