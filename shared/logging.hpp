#pragma once

/**
 * @file logging.hpp
 * @brief Shared logging utilities for all Meld packages
 */

#include <string>
#include <iostream>
#include <memory>

namespace meld::shared {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

/**
 * @brief Shared logger interface for all packages
 */
class Logger {
public:
    static void log(LogLevel level, const std::string& message);
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);
    
    static void set_level(LogLevel level);
    static LogLevel get_level();

private:
    static LogLevel current_level_;
};

} // namespace meld::shared