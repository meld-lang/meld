#pragma once

#include "command_dispatcher.hpp"
#include "config_manager.hpp"
#include "error_handler.hpp"
#include "performance_manager.hpp"
#include "resource_manager.hpp"
#include <memory>
#include <string>
#include <vector>

namespace meld::cli {

/**
 * CLI application configuration
 */
struct CliConfig {
    std::string app_name = "meld";
    std::string version = "0.1.0";
    bool enable_colors = true;
    bool enable_logging = true;
    std::string log_level = "info";
};

/**
 * Main CLI application core
 */
class CliCore {
public:
    explicit CliCore(const CliConfig& config = CliConfig{});
    ~CliCore() = default;

    /**
     * Initialize the CLI application
     */
    bool initialize();

    /**
     * Run the CLI application with command line arguments
     */
    int run(int argc, char* argv[]);

    /**
     * Run the CLI application with string arguments
     */
    int run(const std::vector<std::string>& args);

    /**
     * Shutdown the CLI application
     */
    void shutdown();

    /**
     * Get the command dispatcher
     */
    CommandDispatcher& get_dispatcher() { return *dispatcher_; }

    /**
     * Get the configuration manager
     */
    ConfigManager& get_config_manager() { return *config_manager_; }

    /**
     * Get the error handler
     */
    ErrorHandler& get_error_handler() { return *error_handler_; }

    /**
     * Get the performance manager
     */
    PerformanceManager& get_performance_manager() { return *performance_manager_; }

    /**
     * Get the resource manager
     */
    ResourceManager& get_resource_manager() { return *resource_manager_; }

    /**
     * Register built-in command handlers
     */
    void register_builtin_handlers();

    /**
     * Set up signal handlers for graceful shutdown
     */
    void setup_signal_handlers();

private:
    CliConfig config_;
    std::shared_ptr<ErrorHandler> error_handler_;
    std::shared_ptr<ConfigManager> config_manager_;
    std::shared_ptr<CommandDispatcher> dispatcher_;
    std::shared_ptr<PerformanceManager> performance_manager_;
    std::shared_ptr<ResourceManager> resource_manager_;
    bool initialized_ = false;

    // Helper methods
    void setup_logging();
    void load_configuration();
    void apply_performance_configuration();
    void register_handlers_eagerly();
    std::vector<std::string> convert_args(int argc, char* argv[]);
    int handle_special_commands(const std::vector<std::string>& args);
};

} // namespace meld::cli