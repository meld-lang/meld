#pragma once

#include "command_dispatcher.hpp"
#include "performance_manager.hpp"
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>

namespace meld::cli {

/**
 * Factory function for creating command handlers
 */
using CommandHandlerFactory = std::function<std::unique_ptr<CommandHandler>()>;

/**
 * Lazy command loader for improved startup performance
 */
class LazyCommandLoader {
public:
    explicit LazyCommandLoader(std::shared_ptr<PerformanceManager> performance_manager)
        : performance_manager_(std::move(performance_manager)) {}
    
    /**
     * Register a command handler factory
     */
    void register_factory(const std::string& command, CommandHandlerFactory factory);
    
    /**
     * Get or create a command handler
     */
    CommandHandler* get_handler(const std::string& command);
    
    /**
     * Check if a command is registered
     */
    bool has_command(const std::string& command) const;
    
    /**
     * Get list of all registered commands
     */
    std::vector<std::string> get_commands() const;
    
    /**
     * Preload specific commands
     */
    void preload_commands(const std::vector<std::string>& commands);
    
    /**
     * Preload all commands (for testing or when startup time is not critical)
     */
    void preload_all_commands();
    
    /**
     * Get loading statistics
     */
    struct LoadingStats {
        size_t total_commands = 0;
        size_t loaded_commands = 0;
        std::chrono::milliseconds total_load_time{0};
        std::chrono::milliseconds average_load_time{0};
    };
    
    LoadingStats get_loading_stats() const;

private:
    mutable std::mutex mutex_;
    std::shared_ptr<PerformanceManager> performance_manager_;
    
    std::unordered_map<std::string, CommandHandlerFactory> factories_;
    std::unordered_map<std::string, std::unique_ptr<CommandHandler>> loaded_handlers_;
    
    // Statistics
    mutable std::chrono::milliseconds total_load_time_{0};
    
    CommandHandler* load_handler(const std::string& command);
};

/**
 * Lazy command dispatcher that loads handlers on demand
 */
class LazyCommandDispatcher : public CommandDispatcher {
public:
    explicit LazyCommandDispatcher(std::shared_ptr<ErrorHandler> error_handler,
                                  std::shared_ptr<PerformanceManager> performance_manager)
        : CommandDispatcher(std::move(error_handler))
        , lazy_loader_(std::move(performance_manager)) {}
    
    /**
     * Register a lazy command handler factory
     */
    void register_lazy_handler(const std::string& command, CommandHandlerFactory factory);
    
    /**
     * Override dispatch to use lazy loading
     */
    CommandResult dispatch(const std::vector<std::string>& args) override;
    
    /**
     * Override command listing to include lazy commands
     */
    std::vector<std::string> get_commands() const override;
    
    /**
     * Override command help to support lazy loading
     */
    std::string get_command_help(const std::string& command) const override;

    /**
     * Override general help to include lazy commands
     */
    std::string get_general_help() const override;
    
    /**
     * Override has_command to check lazy commands
     */
    bool has_command(const std::string& command) const override;
    
    /**
     * Preload commonly used commands for better performance
     */
    void preload_common_commands();

protected:
    /**
     * Override find_handler to use lazy loading
     */
    CommandHandler* find_handler(const std::string& command) const override;

private:
    mutable LazyCommandLoader lazy_loader_;
    
    // Cache for frequently used commands
    mutable std::unordered_map<std::string, CommandHandler*> handler_cache_;
    mutable std::mutex cache_mutex_;
};

} // namespace meld::cli