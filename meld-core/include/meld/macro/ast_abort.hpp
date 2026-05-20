#pragma once

#include "meld/compiler/cap.hpp"
#include <string>
#include <stdexcept>
#include <optional>

namespace meld::macro {

/// Source location captured at the point where ast.abort is called.
struct AbortSourceLocation {
    std::string file;
    size_t line = 0;
    size_t column = 0;
    size_t end_line = 0;
    size_t end_column = 0;

    /// Convert to a CAP Location for structured error output.
    compiler::cap::Location to_cap_location() const;
};

/// Exception thrown by ast.abort to halt macro expansion.
///
/// This is caught by the macro expander to produce a structured
/// compiler error through the Compiler-Agent Protocol (CAP).
/// It is NOT a general-purpose exception — it specifically models
/// the "macro author signals an error" control flow.
class AstAbortError : public std::runtime_error {
public:
    AstAbortError(std::string message, AbortSourceLocation location);

    /// The user-provided error message.
    const std::string& abort_message() const { return abort_message_; }

    /// The source location where ast.abort was called.
    const AbortSourceLocation& source_location() const { return location_; }

    /// Produce a CAP CompilationMessage for structured JSON output.
    compiler::cap::CompilationMessage to_cap_message() const;

    /// Produce structured JSON string for the Compiler-Agent Protocol.
    std::string to_cap_json(bool pretty = false) const;

private:
    std::string abort_message_;
    AbortSourceLocation location_;
};

/// The ast.abort API — halts macro expansion with a structured error.
///
/// Usage (from C++ macro code):
///   ast_abort("@Getter must be applied to a field inside a class",
///             AbortSourceLocation{"example.meld", 10, 5});
///
/// This throws an AstAbortError which the macro expander catches
/// and converts into a CAP-compatible structured compiler error.
[[noreturn]] void ast_abort(const std::string& message,
                            AbortSourceLocation location = {});

/// Convenience overload with individual location parameters.
[[noreturn]] void ast_abort(const std::string& message,
                            const std::string& file,
                            size_t line,
                            size_t column = 0);

} // namespace meld::macro
