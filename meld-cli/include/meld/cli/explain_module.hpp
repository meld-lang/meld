#pragma once

#include "command_handler.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace meld::cli {

/**
 * A single diagnostic entry in the registry.
 * Rich enough for agents to act on without human interpretation.
 */
struct DiagnosticEntry {
    std::string code;
    std::string title;
    std::string description;
    std::vector<std::string> causes;
    std::string fix_pattern;
    std::string example_bad;
    std::string example_good;
    std::string category;  // "error", "warning", "info"
};

/**
 * Scalable registry of all diagnostic codes.
 * New codes are added here — the module logic doesn't change.
 */
class DiagnosticRegistry {
public:
    static const DiagnosticRegistry& instance();

    const DiagnosticEntry* lookup(const std::string& code) const;
    std::vector<const DiagnosticEntry*> all() const;
    std::vector<const DiagnosticEntry*> by_category(const std::string& category) const;

private:
    DiagnosticRegistry();
    void register_entry(DiagnosticEntry entry);

    std::unordered_map<std::string, DiagnosticEntry> entries_;
};

/**
 * Handles the `meld explain` subcommand.
 * Looks up diagnostic codes and prints rich, agent-actionable explanations.
 *
 * Usage:
 *   meld explain E001          Plain text explanation
 *   meld explain E001 --json   Structured JSON for agents
 *   meld explain --list        List all known codes
 */
class ExplainModule : public BaseCommandHandler {
public:
    ExplainModule();
    ~ExplainModule() = default;

    CommandResult execute(const CommandArgs& args) override;
    std::string get_help() const override;
    std::string get_usage() const override;
    std::vector<std::string> get_completions(const std::string& partial) const override;
    bool validate_args(const CommandArgs& args, std::string& error_message) const override;

private:
    void print_human(const DiagnosticEntry& entry) const;
    void print_json(const DiagnosticEntry& entry) const;
    void print_list(bool json) const;
};

} // namespace meld::cli
