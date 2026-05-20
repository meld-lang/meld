#pragma once

#include "command_handler.hpp"
#include "meld/manifest/signing.hpp"
#include "meld/manifest/tombstone.hpp"
#include "meld/manifest/verifier.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace meld::cli {

/// Options for the sign subcommand.
struct SignOptions {
    std::optional<std::filesystem::path> key_path;  // --key <path>
    bool sigstore = true;                            // default mode
    std::optional<std::string> fulcio_url;           // --fulcio-url
    std::optional<std::string> rekor_url;            // --rekor-url
};

/// Options for the verify subcommand.
struct VerifyOptions {
    bool offline = true;   // --offline (default)
    bool online = false;   // --online
};

/**
 * Handles the `meld sign` subcommand.
 * Wraps the meldn notary library to provide sign, verify, and bundle
 * operations as first-class CLI commands.
 *
 * Requirements: 23.1–23.13
 */
class SignModule : public BaseCommandHandler {
public:
    SignModule();
    ~SignModule() override = default;

    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

private:
    /// Sign a compiled binary (default: Sigstore keyless).
    CommandResult handle_sign(const std::filesystem::path& binary, const SignOptions& opts,
                              bool json_output);

    /// Verify binary integrity (tombstone + manifest chain).
    CommandResult handle_verify(const std::filesystem::path& binary, const VerifyOptions& opts,
                                bool json_output);

    /// Prepare air-gapped Sigstore bundle.
    CommandResult handle_bundle(const std::filesystem::path& binary, bool json_output);

    /// Merge CLI flags with meld.toml [signing] section defaults.
    SignOptions load_signing_config(const CommandArgs& args) const;

    /// Locate the co-located .meld manifest sidecar for a binary.
    std::optional<std::filesystem::path> find_manifest(const std::filesystem::path& binary) const;

    manifest::SigningEngine signing_engine_;
    manifest::Verifier verifier_;
};

} // namespace meld::cli
