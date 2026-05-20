#pragma once

/**
 * @file common.hpp
 * @brief Common utilities and definitions for all Meld packages
 */

#include <string>
#include <vector>
#include <memory>

namespace meld::shared {

/**
 * @brief Common result type for operations that can fail
 */
template<typename T>
class Result {
public:
    static Result success(T value) {
        return Result(std::move(value), true, "");
    }
    
    static Result error(const std::string& message) {
        return Result(T{}, false, message);
    }
    
    bool is_success() const { return success_; }
    bool is_error() const { return !success_; }
    
    const T& value() const { return value_; }
    const std::string& error_message() const { return error_message_; }

private:
    Result(T value, bool success, const std::string& error_message)
        : value_(std::move(value)), success_(success), error_message_(error_message) {}
    
    T value_;
    bool success_;
    std::string error_message_;
};

/**
 * @brief Common string utilities
 */
class StringUtils {
public:
    static std::vector<std::string> split(const std::string& str, char delimiter);
    static std::string join(const std::vector<std::string>& parts, const std::string& separator);
    static std::string trim(const std::string& str);
    static bool starts_with(const std::string& str, const std::string& prefix);
    static bool ends_with(const std::string& str, const std::string& suffix);
};

/**
 * @brief Common file utilities
 */
class FileUtils {
public:
    static bool exists(const std::string& path);
    static std::string read_file(const std::string& path);
    static bool write_file(const std::string& path, const std::string& content);
    static std::string get_extension(const std::string& path);
    static std::string get_directory(const std::string& path);
};

} // namespace meld::shared