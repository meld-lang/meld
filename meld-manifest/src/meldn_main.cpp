// meldn_main.cpp — Meld Notary CLI (Req 8)
// Three-verb CLI: meldn sign, meldn verify, meldn bundle

#include "meld/manifest/signing.hpp"
#include "meld/manifest/verifier.hpp"
#include "meld/manifest/tombstone.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

static void print_usage() {
    std::cerr << "Usage: meldn <command> [options]\n"
              << "\nCommands:\n"
              << "  sign   <binary>              Sign a binary and emit Tombstone\n"
              << "  verify <binary>              Verify a signed binary\n"
              << "  bundle <binary> [--output]   Bundle Sigstore certificates for air-gapped use\n";
}

static int cmd_sign(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "meldn sign: missing binary path\n";
        return 1;
    }
    auto binary_path = args[0];
    if (!fs::exists(binary_path)) {
        std::cerr << "meldn sign: file not found: " << binary_path << "\n";
        return 1;
    }

    meld::manifest::Signer signer;
    auto result = signer.sign_binary(binary_path);
    if (!result) {
        std::cerr << "meldn sign: " << result.error() << "\n";
        return 1;
    }

    std::cout << "Signed: " << binary_path << "\n"
              << "Tombstone: " << result->tombstone_path << "\n"
              << "Hash: " << result->combined_hash << "\n";
    return 0;
}

static int cmd_verify(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "meldn verify: missing binary path\n";
        return 1;
    }
    auto binary_path = args[0];

    meld::manifest::Verifier verifier;
    auto result = verifier.verify_binary(binary_path);
    if (!result) {
        std::cerr << "FAIL: " << result.error() << "\n";
        return 1;
    }

    std::cout << "PASS: " << binary_path << "\n"
              << "  Hash: " << result->combined_hash << "\n"
              << "  Signed by: " << result->signer_identity << "\n";
    return 0;
}

static int cmd_bundle(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "meldn bundle: missing binary path\n";
        return 1;
    }
    auto binary_path = args[0];
    auto output = (args.size() > 1) ? args[1] : binary_path + ".bundle";

    meld::manifest::Verifier verifier;
    auto result = verifier.create_bundle(binary_path, output);
    if (!result) {
        std::cerr << "meldn bundle: " << result.error() << "\n";
        return 1;
    }

    std::cout << "Bundle created: " << output << "\n";
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];
    std::vector<std::string> args(argv + 2, argv + argc);

    if (command == "sign")   return cmd_sign(args);
    if (command == "verify") return cmd_verify(args);
    if (command == "bundle") return cmd_bundle(args);

    std::cerr << "meldn: unknown command '" << command << "'\n";
    print_usage();
    return 1;
}
