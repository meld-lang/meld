#include "meld/cli/help_module.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace meld::cli {

HelpModule::HelpModule(std::shared_ptr<CommandDispatcher> dispatcher, 
                      std::shared_ptr<ErrorHandler> error_handler)
    : BaseCommandHandler("help", "Show help information for commands")
    , dispatcher_(dispatcher)
    , error_handler_(error_handler)
    , version_("0.1.0")
    , build_info_("development") {
}

CommandResult HelpModule::execute(const CommandArgs& args) {
    if (args.positional.empty()) {
        // Show general help
        std::cout << get_general_help() << std::endl;
        return CommandResult::Success;
    }

    const std::string& command = args.positional[0];
    
    // Check for special help topics
    if (command == "commands") {
        std::cout << format_command_list() << std::endl;
        return CommandResult::Success;
    }
    
    if (command == "examples") {
        std::cout << format_usage_examples() << std::endl;
        return CommandResult::Success;
    }

    // Get help for specific command
    std::string help_text = get_command_help(command);
    if (help_text.empty()) {
        // Command not found - suggest alternatives
        std::vector<std::string> suggestions = suggest_commands(command);
        std::string error_msg = format_helpful_error(
            "Command '" + command + "' not found",
            suggestions
        );
        std::cerr << error_msg << std::endl;
        return CommandResult::NotFound;
    }

    std::cout << help_text << std::endl;
    return CommandResult::Success;
}

std::string HelpModule::get_help() const {
    return R"(Usage: meld help [command|topic]

Show help information for commands or topics.

Arguments:
  command    Show help for a specific command
  topic      Show help for a specific topic

Available topics:
  commands   List all available commands
  examples   Show usage examples

Examples:
  meld help              Show general help
  meld help compile      Show help for compile command
  meld help commands     List all commands
  meld help examples     Show usage examples)";
}

std::string HelpModule::get_usage() const {
    return "meld help [command|topic]";
}

std::vector<std::string> HelpModule::get_completions(const std::string& partial) const {
    std::vector<std::string> completions;
    
    // Add available commands
    std::vector<std::string> commands = dispatcher_->get_commands();
    for (const auto& command : commands) {
        if (command.find(partial) == 0) {
            completions.push_back(command);
        }
    }
    
    // Add help topics
    std::vector<std::string> topics = {"commands", "examples"};
    for (const auto& topic : topics) {
        if (topic.find(partial) == 0) {
            completions.push_back(topic);
        }
    }
    
    return completions;
}

bool HelpModule::validate_args(const CommandArgs& args, std::string& error_message) const {
    // Help command accepts 0 or 1 positional arguments
    if (args.positional.size() > 1) {
        error_message = "Too many arguments. Expected: meld help [command]";
        return false;
    }
    return true;
}

std::string HelpModule::get_version_info() const {
    std::ostringstream oss;
    oss << "meld version " << version_;
    if (!build_info_.empty()) {
        oss << " (" << build_info_ << ")";
    }
    oss << std::endl;
    oss << "Meld Programming Language CLI" << std::endl;
    oss << "Copyright (c) 2024 Meld Language Team" << std::endl;
    return oss.str();
}

std::string HelpModule::get_general_help() const {
    // Single source of truth: delegate to the dispatcher's general help
    return dispatcher_->get_general_help();
}

std::string HelpModule::get_command_help(const std::string& command) const {
    // Provide built-in detailed help for commands that may not yet have
    // registered handlers, or where the help module augments the handler's help.
    if (command == "vm") {
        return R"(Usage: meld vm <subcommand> [options]

Manage the background Lima VM (macOS).

On macOS, Meld uses a Lima virtual machine to provide Linux kernel features
(KVM, containerd) required by the Firecracker micro-VM isolation backend.
The `meld vm` commands wrap `limactl` operations behind a clean interface.

Subcommands:
  status     Show current VM status
  start      Start the Lima VM
  stop       Stop the Lima VM
  restart    Restart the Lima VM
  shell      Open a shell inside the VM
  prune      Remove unused VM data and reclaim disk space
  logs       Stream or display VM logs

Flags:
  --json         Output in JSON format (status, logs)
  --no-follow    Print current logs and exit without following (logs)

Examples:
  meld vm status                  Show whether the VM is running
  meld vm status --json           Machine-readable VM status
  meld vm start                   Start the Lima VM
  meld vm stop                    Gracefully stop the VM
  meld vm restart                 Stop then start the VM
  meld vm shell                   Open an interactive shell in the VM
  meld vm prune                   Remove unused VM data
  meld vm logs                    Follow VM log output
  meld vm logs --no-follow        Print current logs and exit
  meld vm logs --json             Stream logs in JSON format)";
    }

    if (command == "profile") {
        return R"(Usage: meld profile [options] <file.meld>

Profile CPU, memory, and call traces for a Meld program.

Runs the specified program under the AST interpreter with profiling
instrumentation enabled, then writes a report to the specified output
file or stdout.

Flags:
  --cpu                 Enable CPU profiling (default: on)
  --memory              Enable memory profiling (default: on)
  --trace               Trace all function calls
  --duration=<seconds>  Maximum profiling duration (default: 30)
  --output=<path>       Write profile data to file (default: stdout)

Examples:
  meld profile app.meld                   Profile with defaults
  meld profile --trace app.meld           Include call traces
  meld profile --output=prof.json app.meld Write to file
  meld profile --duration=60 server.meld  Profile for 60 seconds)";
    }

    if (command == "config") {
        return R"(Usage: meld config <subcommand> [options]

Read and write CLI configuration values.

Configuration is resolved with the following precedence (highest first):
  1. Command-line arguments
  2. Environment variables (MELD_*)
  3. Project-local config (.meld/config.toml)
  4. Project-global config
  5. User-global config (~/.config/meld/config.toml)
  6. System-global config (/etc/meld/config.toml)

Subcommands:
  get <key>              Read a configuration value
  set <key> <value>      Write a configuration value (project-local)
  list                   Show all resolved values with sources

Flags:
  --global               Target user-global config instead of project-local

Examples:
  meld config get default_target          Read a value
  meld config set default_target jvm      Set project-local value
  meld config --global set editor vim     Set user-global value
  meld config list                        Show all config with sources)";
    }

    if (command == "dev") {
        return R"(Usage: meld dev [options]

Start the ORC JIT hot-reload development server (Tier 2 execution).

Watches for .meld source file changes via ibazel and hot-swaps updated
modules into the running JIT session with sub-200ms latency.

Flags:
  --port <port>    Listen on the specified port for IPC signals

Examples:
  meld dev                Start the dev server in the current project
  meld dev --port 8080    Start with a custom IPC port)";
    }

    if (command == "init") {
        return R"(Usage: meld init [options]

Initialize a Meld project in the current directory.

Creates a meld.toml and src/main.meld in the current directory without
creating a new parent directory. Errors if meld.toml already exists.

Flags:
  --lib    Create a library project (src/module.meld instead of src/main.meld)

Examples:
  meld init              Initialize an executable project
  meld init --lib        Initialize a library project)";
    }

    if (command == "audit") {
        return R"(Usage: meld audit [options]

Display the effect permission tree for all dependencies.

Parses the project AST and all transitive dependency ASTs to build a tree
of requested algebraic effects, showing which packages request which I/O
permissions (fs.read, fs.write, net, console, etc.).

Flags:
  --json    Output the effect tree in machine-readable JSON format

Examples:
  meld audit             Show effect tree in human-readable format
  meld audit --json      Machine-readable effect tree)";
    }

    if (!is_valid_command(command)) {
        return "";
    }
    
    std::string help = dispatcher_->get_command_help(command);

    // Augment `run` help with --isolation flag documentation
    if (command == "run" && !help.empty() &&
        help.find("--isolation") == std::string::npos) {
        help += R"(

Isolation Override:
  --isolation=<backend>   Override the isolation backend for this execution.
                          Valid backends:
                            srt      - Process-level SRT sandbox (default)
                            finch    - Finch/containerd container isolation
                            microvm  - Firecracker micro-VM isolation
                          This flag overrides the [isolation].local setting
                          in meld.toml for a single run.)";
    }

    return help;
}

std::vector<std::string> HelpModule::suggest_commands(const std::string& invalid_command) const {
    return dispatcher_->suggest_commands(invalid_command);
}

std::string HelpModule::format_helpful_error(const std::string& error_message, 
                                           const std::vector<std::string>& suggestions) const {
    std::ostringstream oss;
    oss << "Error: " << error_message << std::endl;
    
    if (!suggestions.empty()) {
        oss << std::endl << "Did you mean:" << std::endl;
        for (const auto& suggestion : suggestions) {
            oss << "  meld " << suggestion << std::endl;
        }
    }
    
    oss << std::endl << "Use 'meld help' to see all available commands." << std::endl;
    
    return oss.str();
}

void HelpModule::set_version(const std::string& version, const std::string& build_info) {
    version_ = version;
    build_info_ = build_info;
}

std::string HelpModule::format_command_list() const {
    std::ostringstream oss;
    oss << "Available Meld CLI Commands:" << std::endl;
    oss << std::endl;
    
    std::vector<std::string> commands = dispatcher_->get_commands();
    std::sort(commands.begin(), commands.end());
    
    // Group commands by category (categories and commands alphabetized)
    std::vector<std::pair<std::string, std::vector<std::string>>> categories = {
        {"Debugging", {"debug", "profile"}},
        {"Editor Integration", {"lsp", "mcp"}},
        {"Execution", {"build", "dev", "run"}},
        {"Infrastructure", {"daemon", "vm"}},
        {"Project Management", {"audit", "fmt", "init", "module", "new", "test"}},
        {"Security & Signing", {"sign"}},
        {"Shell & Help", {"completion", "config", "help", "update", "version"}}
    };
    
    for (const auto& [category, category_commands] : categories) {
        bool has_commands = false;
        std::ostringstream category_oss;
        
        for (const auto& command : category_commands) {
            if (std::find(commands.begin(), commands.end(), command) != commands.end()) {
                if (!has_commands) {
                    category_oss << category << ":" << std::endl;
                    has_commands = true;
                }
                
                std::string description = "No description available";
                if (dispatcher_->has_command(command)) {
                    // Get brief description from help text
                    std::string help = dispatcher_->get_command_help(command);
                    size_t newline_pos = help.find('\n');
                    if (newline_pos != std::string::npos) {
                        description = help.substr(0, newline_pos);
                        if (description.find("Usage:") == 0) {
                            description = "Command usage available";
                        }
                    }
                }
                
                category_oss << "  " << std::left << std::setw(12) << command << description << std::endl;
            }
        }
        
        if (has_commands) {
            oss << category_oss.str() << std::endl;
        }
    }
    
    // Add any uncategorized commands
    std::vector<std::string> uncategorized;
    for (const auto& command : commands) {
        bool found = false;
        for (const auto& [category, category_commands] : categories) {
            if (std::find(category_commands.begin(), category_commands.end(), command) != category_commands.end()) {
                found = true;
                break;
            }
        }
        if (!found) {
            uncategorized.push_back(command);
        }
    }
    
    if (!uncategorized.empty()) {
        oss << "Other Commands:" << std::endl;
        for (const auto& command : uncategorized) {
            oss << "  " << command << std::endl;
        }
        oss << std::endl;
    }
    
    oss << "Use 'meld help <command>' for detailed information about a specific command." << std::endl;
    
    return oss.str();
}

std::string HelpModule::format_usage_examples() const {
    return R"(Meld CLI Usage Examples:

Execution:
  meld run hello.meld                       # Interpret a Meld program (Tier 1)
  meld run hello.meld arg1 arg2             # Pass arguments to the program
  meld run -i                               # Start interactive REPL
  meld run -i prelude.meld                  # Load file then drop into REPL
  meld run --watch server.meld              # Re-execute on file saves
  meld run --debug app.meld                 # Debug with DAP (VS Code/Neovim)
  meld run --strict app.bin                 # Run with production-grade verification
  meld run --vfs app.meld                   # Dry run with ephemeral filesystem
  meld run --isolation=microvm app.bin      # Override isolation backend
  meld dev                                  # Start ORC JIT hot-reload dev server
  meld build                                # AOT compile to native binary
  meld build --release                      # Optimized production build
  meld build --transpile go                 # Transpile to Go source

Project Management:
  meld new my-project                       # Scaffold a new executable project
  meld new --lib my-library                 # Scaffold a new library project
  meld init                                 # Initialize project in current directory
  meld test                                 # Discover and run @test functions
  meld test --filter "auth.*"               # Run tests matching a regex
  meld test --parallel                      # Run tests concurrently
  meld fmt                                  # Format all .meld files in project
  meld fmt --check src/main.meld            # Check formatting without changes
  meld mod fetch                            # Download Git dependencies
  meld mod update                           # Resolve latest dependency versions
  meld mod clean                            # Purge dependency cache
  meld audit                                # Show effect permissions per dependency
  meld audit --json                         # Machine-readable effect tree

Debugging:
  meld debug --run ./app.bin                # Launch binary under LLDB/GDB
  meld debug --attach 1234                  # Attach debugger to running process
  meld debug --run ./app.bin --break main.meld:42  # Set breakpoint before launch
  meld profile app.meld                     # Profile CPU and memory usage
  meld profile --trace app.meld             # Include function call traces
  meld profile --output=prof.json app.meld  # Write profile data to file

Security & Signing:
  meld sign app.bin                         # Sign binary with Sigstore
  meld sign --verify app.bin                # Verify binary integrity
  meld sign --bundle app.bin                # Prepare air-gapped Sigstore bundle

Infrastructure:
  meld daemon start                         # Start the meldd daemon
  meld daemon status                        # Query daemon state
  meld daemon logs                          # Tail daemon log output
  meld vm status                            # Show Lima VM status (macOS)
  meld vm start                             # Boot the meld-vm Alpine host
  meld vm shell                             # Drop into VM root shell

Editor Integration:
  meld lsp                                  # Start LSP server (stdio)
  meld mcp                                  # Start MCP server (stdio)

Shell & Help:
  meld completion bash                      # Generate Bash completion script
  meld completion zsh                       # Generate Zsh completion script
  meld config get default_target            # Read a configuration value
  meld config set default_target jvm        # Set project-local config
  meld config --global set editor vim       # Set user-global config
  meld help                                 # Show general help
  meld help run                             # Show help for a specific command
  meld version                              # Show version information

For more detailed information about any command, use 'meld help <command>'.)";
}

bool HelpModule::is_valid_command(const std::string& command) const {
    return dispatcher_->has_command(command);
}

// VersionModule implementation

VersionModule::VersionModule(HelpModule* help_module)
    : BaseCommandHandler("version", "Display version information")
    , help_module_(help_module) {
}

CommandResult VersionModule::execute(const CommandArgs& args) {
    std::cout << help_module_->get_version_info();
    return CommandResult::Success;
}

std::string VersionModule::get_help() const {
    return R"(Usage: meld version

Display version information for the Meld CLI.

This command shows:
- Meld CLI version number
- Build information
- Copyright notice

Examples:
  meld version                              # Show version info
  meld --version                            # Same as above
  meld -v                                   # Short form)";
}

std::string VersionModule::get_usage() const {
    return "meld version";
}

} // namespace meld::cli