#include "meld/cli/cli_core.hpp"
#include "meld/cli/lazy_command_loader.hpp"
#include "meld/cli/compiler_module.hpp"
#include "meld/cli/interpreter_module.hpp"
#include "meld/cli/project_module.hpp"
#include "meld/cli/dev_tools_module.hpp"
#include "meld/cli/lsp_module.hpp"
#include "meld/cli/help_module.hpp"
#include "meld/cli/debug_orchestrator_module.hpp"
#include "meld/cli/sign_module.hpp"
#include "meld/cli/daemon_module.hpp"
#include "meld/cli/vm_module.hpp"
#include "meld/cli/update_module.hpp"
#include "meld/cli/module_command.hpp"
#include "meld/cli/mcp_server_module.hpp"
#include "meld/cli/package_module.hpp"
#include "meld/cli/test_module.hpp"
#include "meld/cli/shell_integration_module.hpp"
#include "meld/cli/explain_module.hpp"
#include "meld/cli/fix_module.hpp"
#include "meld/cli/guide_module.hpp"
#include "meld/cli/graph_module.hpp"
#include "meld/cli/doctor_module.hpp"
#include "meld/cli/check_module.hpp"
#include <iostream>
#include <csignal>
#include <cstdlib>

namespace meld::cli {

// Global pointer for signal handling
static CliCore* g_cli_instance = nullptr;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    if (g_cli_instance) {
        std::cerr << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
        
        // Emergency cleanup first (signal-safe operations only)
        g_cli_instance->get_resource_manager().emergency_cleanup();
        
        // Then full shutdown
        g_cli_instance->shutdown();
        std::exit(signal);
    }
}

CliCore::CliCore(const CliConfig& config) : config_(config) {
    error_handler_ = std::make_shared<ErrorHandler>();
    config_manager_ = std::make_shared<ConfigManager>();
    performance_manager_ = std::make_shared<PerformanceManager>();
    resource_manager_ = std::make_shared<ResourceManager>();
    
    // Use lazy command dispatcher for better startup performance
    auto lazy_dispatcher = std::make_shared<LazyCommandDispatcher>(error_handler_, performance_manager_);
    dispatcher_ = lazy_dispatcher;
}

bool CliCore::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Start performance monitoring
    performance_manager_->start_startup_timer();
    
    try {
        // Set up logging
        setup_logging();
        
        // Load configuration and apply performance settings
        load_configuration();
        apply_performance_configuration();
        
        // Register built-in command handlers (lazy loading)
        register_builtin_handlers();
        
        // Set up signal handlers
        setup_signal_handlers();
        
        // Register shutdown handlers
        resource_manager_->register_shutdown_handler([this]() {
            performance_manager_->stop_background_cleanup();
        });
        
        initialized_ = true;
        performance_manager_->end_startup_timer();
        return true;
    } catch (const std::exception& e) {
        error_handler_->report_internal_error("Failed to initialize CLI: " + std::string(e.what()));
        return false;
    }
}

int CliCore::run(int argc, char* argv[]) {
    std::vector<std::string> args = convert_args(argc, argv);
    return run(args);
}

int CliCore::run(const std::vector<std::string>& args) {
    if (!initialize()) {
        return 1;
    }
    
    // Start command timing
    performance_manager_->start_command_timer();
    
    // Create scope guard for cleanup
    ScopeGuard cleanup_guard([this]() {
        performance_manager_->end_command_timer();
        performance_manager_->update_memory_usage();
    });
    
    // Handle special commands that don't go through normal dispatch
    int special_result = handle_special_commands(args);
    if (special_result >= 0) {
        return special_result;
    }
    
    // Check memory limits before executing command
    if (!resource_manager_->check_memory_limit()) {
        error_handler_->report_internal_error("Memory limit exceeded");
        return 1;
    }
    
    // Dispatch to appropriate command handler
    CommandResult result = dispatcher_->dispatch(args);
    
    switch (result) {
        case CommandResult::Success:
            return 0;
        case CommandResult::Error:
            return 1;
        case CommandResult::InvalidArguments:
            return 2;
        case CommandResult::NotFound:
            return 127; // Command not found (similar to shell convention)
        default:
            return 1;
    }
}

void CliCore::shutdown() {
    if (!initialized_) {
        return;
    }
    
    // Shutdown resource manager (this will trigger all cleanup)
    resource_manager_->shutdown();
    
    // Clean up managers in reverse order of initialization
    dispatcher_.reset();
    performance_manager_.reset();
    resource_manager_.reset();
    config_manager_.reset();
    error_handler_.reset();
    
    initialized_ = false;
}

void CliCore::register_builtin_handlers() {
    auto lazy_dispatcher = std::dynamic_pointer_cast<LazyCommandDispatcher>(dispatcher_);
    if (!lazy_dispatcher) {
        // Fallback to regular registration if not using lazy dispatcher
        register_handlers_eagerly();
        return;
    }
    
    // Register lazy command factories for better startup performance
    lazy_dispatcher->register_lazy_handler("build", []() {
        return std::make_unique<CompilerModule>();
    });
    
    lazy_dispatcher->register_lazy_handler("run", []() {
        return std::make_unique<InterpreterModule>();
    });
    
    lazy_dispatcher->register_lazy_handler("new", [this]() {
        return std::make_unique<ProjectModule>(error_handler_);
    });
    
    lazy_dispatcher->register_lazy_handler("test", [this]() {
        return std::make_unique<ProjectModule>(error_handler_);
    });
    
    lazy_dispatcher->register_lazy_handler("clean", [this]() {
        return std::make_unique<ProjectModule>(error_handler_);
    });
    
    lazy_dispatcher->register_lazy_handler("lsp", []() {
        return std::make_unique<LspModule>();
    });
    
    lazy_dispatcher->register_lazy_handler("fmt", []() {
        return std::make_unique<DevToolsModule>();
    });
    
    lazy_dispatcher->register_lazy_handler("debug", []() {
        return std::make_unique<DebugOrchestratorModule>();
    });
    
    lazy_dispatcher->register_lazy_handler("sign", []() {
        return std::make_unique<SignModule>();
    });
    
    lazy_dispatcher->register_lazy_handler("daemon", []() {
        return std::make_unique<DaemonModule>();
    });
    
    lazy_dispatcher->register_lazy_handler("vm", []() {
        return std::make_unique<VmModule>();
    });
    
    lazy_dispatcher->register_lazy_handler("update", []() {
        return std::make_unique<UpdateModule>();
    });
    
    lazy_dispatcher->register_lazy_handler("mcp", []() {
        return std::make_unique<McpServerModule>();
    });
    
    lazy_dispatcher->register_lazy_handler("module", []() {
        return std::make_unique<ModuleCommandHandler>();
    });
    
    lazy_dispatcher->register_lazy_handler("profile", []() {
        return std::make_unique<DevToolsModule>();
    });
    
    lazy_dispatcher->register_lazy_handler("completion", [this]() {
        return std::make_unique<ShellIntegrationModule>(error_handler_);
    });
    
    lazy_dispatcher->register_lazy_handler("explain", []() {
        return std::make_unique<ExplainModule>();
    });
    
    lazy_dispatcher->register_lazy_handler("fix", []() {
        return std::make_unique<FixModule>();
    });
    
    lazy_dispatcher->register_lazy_handler("guide", []() {
        return std::make_unique<GuideModule>();
    });
    
    lazy_dispatcher->register_lazy_handler("graph", []() {
        return std::make_unique<GraphModule>();
    });
    
    lazy_dispatcher->register_lazy_handler("doctor", []() {
        return std::make_unique<DoctorModule>();
    });
    
    lazy_dispatcher->register_lazy_handler("check", []() {
        return std::make_unique<CheckModule>();
    });
    
    // Register help and version commands eagerly (they're lightweight and commonly used)
    auto help_module = std::make_unique<HelpModule>(dispatcher_, error_handler_);
    help_module->set_version(config_.version, "development build");
    
    HelpModule* help_ptr = help_module.get();
    dispatcher_->register_handler(std::move(help_module));
    
    auto version_module = std::make_unique<VersionModule>(help_ptr);
    dispatcher_->register_handler(std::move(version_module));
    
    // Preload commonly used commands for better performance
    lazy_dispatcher->preload_common_commands();
    
    // TODO: Register other lazy command handlers for:
    // - mcp
    // - format
    // - lint
    // - install/publish/search (package management)
    // - debug/profile
    // - config
    // - completion
}

void CliCore::setup_signal_handlers() {
    g_cli_instance = this;
    
    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
#ifndef _WIN32
    std::signal(SIGHUP, signal_handler);
    std::signal(SIGQUIT, signal_handler);
#endif
}

void CliCore::setup_logging() {
    // TODO: Set up proper logging system
    // For now, just set context in error handler
    error_handler_->add_context("app_name", config_.app_name);
    error_handler_->add_context("version", config_.version);
}

void CliCore::load_configuration() {
    // Load all configuration sources
    config_manager_->load_all_configs();
    
    // Apply configuration to CLI settings
    auto log_level = config_manager_->get_config("log_level");
    if (log_level.has_value()) {
        config_.log_level = log_level.value();
    }
    
    auto enable_colors = config_manager_->get_config("enable_colors");
    if (enable_colors.has_value()) {
        config_.enable_colors = (enable_colors.value() == "true" || enable_colors.value() == "1");
    }
}

void CliCore::apply_performance_configuration() {
    // Apply performance-related configuration
    auto cache_size = config_manager_->get_config("cache_size");
    if (cache_size.has_value()) {
        try {
            size_t size = std::stoull(cache_size.value());
            performance_manager_->set_cache_size(size);
        } catch (...) {
            // Invalid cache size, use default
        }
    }
    
    auto cache_ttl = config_manager_->get_config("cache_ttl_minutes");
    if (cache_ttl.has_value()) {
        try {
            int minutes = std::stoi(cache_ttl.value());
            performance_manager_->set_cache_ttl(std::chrono::minutes(minutes));
        } catch (...) {
            // Invalid TTL, use default
        }
    }
    
    auto memory_limit = config_manager_->get_config("memory_limit_mb");
    if (memory_limit.has_value()) {
        try {
            size_t mb = std::stoull(memory_limit.value());
            resource_manager_->set_memory_limit(mb * 1024 * 1024);
        } catch (...) {
            // Invalid memory limit, use default
        }
    }
    
    auto batch_size = config_manager_->get_config("batch_size");
    if (batch_size.has_value()) {
        try {
            size_t size = std::stoull(batch_size.value());
            performance_manager_->set_batch_size(size);
        } catch (...) {
            // Invalid batch size, use default
        }
    }
    
    auto max_threads = config_manager_->get_config("max_threads");
    if (max_threads.has_value()) {
        try {
            size_t threads = std::stoull(max_threads.value());
            performance_manager_->set_max_concurrent_operations(threads);
        } catch (...) {
            // Invalid thread count, use default
        }
    }
}

std::vector<std::string> CliCore::convert_args(int argc, char* argv[]) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {  // Skip program name
        args.emplace_back(argv[i]);
    }
    return args;
}

int CliCore::handle_special_commands(const std::vector<std::string>& args) {
    if (args.empty()) {
        // No command specified - show general help
        std::cout << dispatcher_->get_general_help() << std::endl;
        return 0;
    }
    
    const std::string& command = args[0];
    
    // Handle version command
    if (command == "version" || command == "--version" || command == "-v") {
        std::cout << config_.app_name << " version " << config_.version << std::endl;
        return 0;
    }
    
    // Handle global help flags
    if (command == "--help" || command == "-h") {
        std::cout << dispatcher_->get_general_help() << std::endl;
        return 0;
    }
    
    // Not a special command
    return -1;
}

void CliCore::register_handlers_eagerly() {
    // Fallback method for non-lazy registration
    dispatcher_->register_handler(std::make_unique<CompilerModule>());
    dispatcher_->register_handler(std::make_unique<InterpreterModule>());
    dispatcher_->register_handler(std::make_unique<ProjectModule>(error_handler_));
    dispatcher_->register_handler(std::make_unique<LspModule>());
    
    auto help_module = std::make_unique<HelpModule>(dispatcher_, error_handler_);
    help_module->set_version(config_.version, "development build");
    
    HelpModule* help_ptr = help_module.get();
    dispatcher_->register_handler(std::move(help_module));
    
    auto version_module = std::make_unique<VersionModule>(help_ptr);
    dispatcher_->register_handler(std::move(version_module));
}

} // namespace meld::cli