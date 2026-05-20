#pragma once

#include "meld/kernel/primitives.hpp"
#include "meld/meta/metatype.hpp"
#include <string>
#include <memory>
#include <vector>
#include <optional>
#include <source_location>

namespace meld::types {

/**
 * Error trait - Base interface for all error types
 * 
 * Provides a common interface for error handling, including:
 * - Error messages and descriptions
 * - Error chaining and source tracking
 * - Debug information and stack traces
 * - Error categorization
 */
class Error {
public:
    virtual ~Error() = default;
    
    /**
     * Get a human-readable error message
     */
    virtual std::string message() const = 0;
    
    /**
     * Get a detailed description of the error
     */
    virtual std::string description() const {
        return message();
    }
    
    /**
     * Get the source error that caused this error (if any)
     */
    virtual std::shared_ptr<Error> source() const {
        return nullptr;
    }
    
    /**
     * Get the error category/type
     */
    virtual std::string category() const {
        return "Error";
    }
    
    /**
     * Get debug information including stack trace
     */
    virtual std::string debug_info() const {
        return message();
    }
    
    /**
     * Check if this error is of a specific type
     */
    template<typename T>
    bool is() const {
        return dynamic_cast<const T*>(this) != nullptr;
    }
    
    /**
     * Try to downcast to a specific error type
     */
    template<typename T>
    const T* as() const {
        return dynamic_cast<const T*>(this);
    }
    
    /**
     * Get the full error chain as a string
     */
    std::string chain_string() const {
        std::string result = message();
        auto src = source();
        while (src) {
            result += "\nCaused by: " + src->message();
            src = src->source();
        }
        return result;
    }
    
    /**
     * Get all errors in the chain
     */
    std::vector<std::shared_ptr<Error>> chain() const {
        std::vector<std::shared_ptr<Error>> result;
        auto current = source();
        while (current) {
            result.push_back(current);
            current = current->source();
        }
        return result;
    }
};

/**
 * Standard error types
 */

// Generic error with a message
class GenericError : public Error {
public:
    explicit GenericError(std::string msg) 
        : message_(std::move(msg)) {}
    
    std::string message() const override {
        return message_;
    }
    
    std::string category() const override {
        return "GenericError";
    }
    
private:
    std::string message_;
};

// IO error for file and network operations
class IOError : public Error {
public:
    enum class Kind {
        NotFound,
        PermissionDenied,
        ConnectionRefused,
        ConnectionReset,
        Timeout,
        Other
    };
    
    IOError(Kind kind, std::string msg, std::shared_ptr<Error> source = nullptr)
        : kind_(kind), message_(std::move(msg)), source_(std::move(source)) {}
    
    std::string message() const override {
        return message_;
    }
    
    std::string category() const override {
        return "IOError";
    }
    
    std::shared_ptr<Error> source() const override {
        return source_;
    }
    
    Kind kind() const { return kind_; }
    
    std::string kind_string() const {
        switch (kind_) {
            case Kind::NotFound: return "NotFound";
            case Kind::PermissionDenied: return "PermissionDenied";
            case Kind::ConnectionRefused: return "ConnectionRefused";
            case Kind::ConnectionReset: return "ConnectionReset";
            case Kind::Timeout: return "Timeout";
            case Kind::Other: return "Other";
        }
        return "Unknown";
    }
    
    std::string description() const override {
        return kind_string() + ": " + message_;
    }
    
private:
    Kind kind_;
    std::string message_;
    std::shared_ptr<Error> source_;
};

// Parse error for parsing operations
class ParseError : public Error {
public:
    ParseError(std::string msg, size_t line, size_t column)
        : message_(std::move(msg)), line_(line), column_(column) {}
    
    std::string message() const override {
        return message_;
    }
    
    std::string category() const override {
        return "ParseError";
    }
    
    std::string description() const override {
        return "Parse error at line " + std::to_string(line_) + 
               ", column " + std::to_string(column_) + ": " + message_;
    }
    
    size_t line() const { return line_; }
    size_t column() const { return column_; }
    
private:
    std::string message_;
    size_t line_;
    size_t column_;
};

// Validation error for input validation
class ValidationError : public Error {
public:
    ValidationError(std::string field, std::string msg)
        : field_(std::move(field)), message_(std::move(msg)) {}
    
    std::string message() const override {
        return message_;
    }
    
    std::string category() const override {
        return "ValidationError";
    }
    
    std::string description() const override {
        return "Validation error for field '" + field_ + "': " + message_;
    }
    
    const std::string& field() const { return field_; }
    
private:
    std::string field_;
    std::string message_;
};

// Network error for network operations
class NetworkError : public Error {
public:
    enum class Kind {
        ConnectionFailed,
        Timeout,
        InvalidResponse,
        Other
    };
    
    NetworkError(Kind kind, std::string msg, std::shared_ptr<Error> source = nullptr)
        : kind_(kind), message_(std::move(msg)), source_(std::move(source)) {}
    
    std::string message() const override {
        return message_;
    }
    
    std::string category() const override {
        return "NetworkError";
    }
    
    std::shared_ptr<Error> source() const override {
        return source_;
    }
    
    Kind kind() const { return kind_; }
    
private:
    Kind kind_;
    std::string message_;
    std::shared_ptr<Error> source_;
};

// Database error for database operations
class DatabaseError : public Error {
public:
    DatabaseError(std::string msg, std::optional<int> error_code = std::nullopt)
        : message_(std::move(msg)), error_code_(error_code) {}
    
    std::string message() const override {
        return message_;
    }
    
    std::string category() const override {
        return "DatabaseError";
    }
    
    std::string description() const override {
        if (error_code_) {
            return "Database error (code " + std::to_string(*error_code_) + "): " + message_;
        }
        return "Database error: " + message_;
    }
    
    std::optional<int> error_code() const { return error_code_; }
    
private:
    std::string message_;
    std::optional<int> error_code_;
};

/**
 * Error builder for creating complex errors with chaining
 */
class ErrorBuilder {
public:
    ErrorBuilder(std::string message) : message_(std::move(message)) {}
    
    ErrorBuilder& with_source(std::shared_ptr<Error> source) {
        source_ = std::move(source);
        return *this;
    }
    
    ErrorBuilder& with_category(std::string category) {
        category_ = std::move(category);
        return *this;
    }
    
    std::shared_ptr<Error> build() {
        // Create a custom error with the builder parameters
        class CustomError : public Error {
        public:
            CustomError(std::string msg, std::string cat, std::shared_ptr<Error> src)
                : message_(std::move(msg)), category_(std::move(cat)), source_(std::move(src)) {}
            
            std::string message() const override { return message_; }
            std::string category() const override { return category_; }
            std::shared_ptr<Error> source() const override { return source_; }
            
        private:
            std::string message_;
            std::string category_;
            std::shared_ptr<Error> source_;
        };
        
        return std::make_shared<CustomError>(
            std::move(message_),
            category_.value_or("Error"),
            std::move(source_)
        );
    }
    
private:
    std::string message_;
    std::optional<std::string> category_;
    std::shared_ptr<Error> source_;
};

/**
 * Helper functions for creating errors
 */
inline std::shared_ptr<Error> make_error(std::string message) {
    return std::make_shared<GenericError>(std::move(message));
}

inline std::shared_ptr<IOError> make_io_error(
    IOError::Kind kind,
    std::string message,
    std::shared_ptr<Error> source = nullptr
) {
    return std::make_shared<IOError>(kind, std::move(message), std::move(source));
}

inline std::shared_ptr<ParseError> make_parse_error(
    std::string message,
    size_t line,
    size_t column
) {
    return std::make_shared<ParseError>(std::move(message), line, column);
}

inline std::shared_ptr<ValidationError> make_validation_error(
    std::string field,
    std::string message
) {
    return std::make_shared<ValidationError>(std::move(field), std::move(message));
}

} // namespace meld::types
