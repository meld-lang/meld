#include "meld/cli/fix_module.hpp"
#include "meld/compiler/cap.hpp"
#include <iostream>
#include <filesystem>

namespace meld::cli {

using namespace meld::compiler::cap;

// A single actionable fix derived from a diagnostic + suggestion pair
struct FixAction {
    std::string diagnostic_code;
    std::string diagnostic_message;
    std::string fix_description;
    std::string fix_type;
    std::string file;
    size_t line;
    size_t column;
    size_t end_line;
    size_t end_column;
    std::string replacement_text;
    int confidence;  // 0-100
};

static std::string fix_type_str(FixType t) {
    switch (t) {
        case FixType::REPLACE_TEXT: return "replace";
        case FixType::INSERT_TEXT: return "insert";
        case FixType::DELETE_TEXT: return "delete";
        case FixType::ADD_IMPORT: return "add_import";
        case FixType::RENAME_SYMBOL: return "rename";
        case FixType::ADD_TYPE_ANNOTATION: return "add_type";
        case FixType::EXTRACT_FUNCTION: return "extract_function";
        case FixType::INLINE_VARIABLE: return "inline";
    }
    return "unknown";
}

static int confidence_pct(ConfidenceLevel c) {
    switch (c) {
        case ConfidenceLevel::LOW: return 25;
        case ConfidenceLevel::MEDIUM: return 55;
        case ConfidenceLevel::HIGH: return 80;
        case ConfidenceLevel::VERY_HIGH: return 95;
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════

FixModule::FixModule()
    : BaseCommandHandler("fix", "Generate a fix plan for diagnostics") {}

CommandResult FixModule::execute(const CommandArgs& args) {
    // JSON is the default output; --text for human-readable
    bool text = args.flags.count("text") > 0 || args.options.count("text") > 0;
    bool plan = args.flags.count("plan") > 0 || args.options.count("plan") > 0;

    // Find the file path (first positional that isn't a flag)
    std::string file_path;
    for (const auto& arg : args.positional) {
        if (arg.substr(0, 2) != "--") {
            file_path = arg;
            break;
        }
    }
    // Also check positional for flags
    for (const auto& arg : args.positional) {
        if (arg == "--text") text = true;
        if (arg == "--plan") plan = true;
    }
    bool json = !text;

    if (file_path.empty()) {
        std::cerr << "Usage: meld fix --plan <file.meld> [--text]\n";
        return CommandResult::InvalidArguments;
    }

    if (!plan) {
        std::cerr << "Usage: meld fix --plan <file.meld> [--text]\n"
                  << "The --plan flag is required (auto-apply not yet supported).\n";
        return CommandResult::InvalidArguments;
    }

    // Check file exists
    if (!std::filesystem::exists(file_path)) {
        std::cerr << "File not found: " << file_path << "\n";
        return CommandResult::Error;
    }

    // Compile and collect diagnostics
    CompilerAgentProtocol cap;
    auto result = cap.compile_file(std::filesystem::path(file_path));

    // Extract fix actions from diagnostics that have suggestions
    std::vector<FixAction> actions;
    for (const auto& msg : result.messages) {
        for (const auto& suggestion : msg.suggestions) {
            actions.push_back({
                msg.code,
                msg.message,
                suggestion.description,
                fix_type_str(suggestion.type),
                suggestion.location.file.empty() ? file_path : suggestion.location.file,
                suggestion.location.line,
                suggestion.location.column,
                suggestion.location.end_line,
                suggestion.location.end_column,
                suggestion.replacement_text,
                confidence_pct(suggestion.confidence)
            });
        }
    }

    if (json) {
        print_plan_json(file_path, actions);
    } else {
        print_plan_human(file_path, actions);
    }
    return CommandResult::Success;
}

void FixModule::print_plan_human(const std::string& file,
                                  const std::vector<FixAction>& actions) const {
    if (actions.empty()) {
        std::cout << "No fixes available for " << file << "\n";
        return;
    }

    std::cout << "Fix plan for " << file << " (" << actions.size() << " actions):\n\n";
    for (size_t i = 0; i < actions.size(); ++i) {
        const auto& a = actions[i];
        std::cout << "  " << (i + 1) << ". [" << a.diagnostic_code << "] "
                  << a.fix_description << "\n";
        std::cout << "     at " << a.file << ":" << a.line << ":" << a.column << "\n";
        std::cout << "     action: " << a.fix_type << "\n";
        if (!a.replacement_text.empty()) {
            std::cout << "     text: " << a.replacement_text << "\n";
        }
        std::cout << "     confidence: " << a.confidence << "%\n\n";
    }
}

void FixModule::print_plan_json(const std::string& file,
                                 const std::vector<FixAction>& actions) const {
    std::cout << "{\n";
    std::cout << "  \"file\": \"" << file << "\",\n";
    std::cout << "  \"ok\": " << (actions.empty() ? "true" : "false") << ",\n";
    std::cout << "  \"actions\": [\n";
    for (size_t i = 0; i < actions.size(); ++i) {
        const auto& a = actions[i];
        if (i > 0) std::cout << ",\n";
        std::cout << "    {\n";
        std::cout << "      \"diagnostic_code\": \"" << a.diagnostic_code << "\",\n";
        std::cout << "      \"diagnostic_message\": \"" << a.diagnostic_message << "\",\n";
        std::cout << "      \"description\": \"" << a.fix_description << "\",\n";
        std::cout << "      \"type\": \"" << a.fix_type << "\",\n";
        std::cout << "      \"location\": {\n";
        std::cout << "        \"file\": \"" << a.file << "\",\n";
        std::cout << "        \"line\": " << a.line << ",\n";
        std::cout << "        \"column\": " << a.column << ",\n";
        std::cout << "        \"end_line\": " << a.end_line << ",\n";
        std::cout << "        \"end_column\": " << a.end_column << "\n";
        std::cout << "      },\n";
        std::cout << "      \"replacement_text\": \"" << a.replacement_text << "\",\n";
        std::cout << "      \"confidence\": " << a.confidence << "\n";
        std::cout << "    }";
    }
    std::cout << "\n  ]\n";
    std::cout << "}\n";
}

std::string FixModule::get_help() const {
    return R"(Usage: meld fix --plan <file.meld> [--text]

Generate a structured fix plan for all diagnostics in a file.
Output is JSON by default (for AI agents). Use --text for human-readable.

Flags:
  --plan     Emit the fix plan (required; auto-apply not yet supported)
  --text     Output as human-readable plain text

The fix plan lists every actionable repair the compiler can suggest,
with exact locations, replacement text, and confidence scores.

Examples:
  meld fix --plan src/main.meld              JSON plan (default)
  meld fix --plan --text src/main.meld       Human-readable plan

JSON output format:
  {
    "file": "src/main.meld",
    "ok": false,
    "actions": [{
      "diagnostic_code": "E001",
      "description": "Add variable declaration",
      "type": "insert",
      "location": { "file": "...", "line": 5, "column": 1 },
      "replacement_text": "val x = ...",
      "confidence": 80
    }]
  })";
}

std::string FixModule::get_usage() const {
    return "meld fix --plan <file.meld> [--text]";
}

std::vector<std::string> FixModule::get_completions(const std::string& partial) const {
    return {"--plan", "--text"};
}

bool FixModule::validate_args(const CommandArgs& args, std::string& error_message) const {
    // Accept any args — execute() handles validation with better messages
    return true;
}

} // namespace meld::cli
