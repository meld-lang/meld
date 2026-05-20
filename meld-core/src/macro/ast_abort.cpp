#include "meld/macro/ast_abort.hpp"
#include <format>

namespace meld::macro {

// ---------------------------------------------------------------------------
// AbortSourceLocation
// ---------------------------------------------------------------------------

compiler::cap::Location AbortSourceLocation::to_cap_location() const {
    return compiler::cap::Location(file, line, column, end_line, end_column);
}

// ---------------------------------------------------------------------------
// AstAbortError
// ---------------------------------------------------------------------------

AstAbortError::AstAbortError(std::string message, AbortSourceLocation location)
    : std::runtime_error(std::format("ast.abort: {}", message))
    , abort_message_(std::move(message))
    , location_(std::move(location))
{}

compiler::cap::CompilationMessage AstAbortError::to_cap_message() const {
    compiler::cap::CompilationMessage msg(
        compiler::cap::MessageSeverity::ERROR,
        "E_MACRO_ABORT",
        abort_message_,
        location_.to_cap_location()
    );
    msg.context = "macro expansion halted by ast.abort";
    return msg;
}

std::string AstAbortError::to_cap_json(bool pretty) const {
    auto msg = to_cap_message();
    auto j = msg.to_json();
    return pretty ? j.dump(2) : j.dump();
}

// ---------------------------------------------------------------------------
// Free functions
// ---------------------------------------------------------------------------

[[noreturn]] void ast_abort(const std::string& message,
                            AbortSourceLocation location) {
    throw AstAbortError(message, std::move(location));
}

[[noreturn]] void ast_abort(const std::string& message,
                            const std::string& file,
                            size_t line,
                            size_t column) {
    throw AstAbortError(message, AbortSourceLocation{file, line, column});
}

} // namespace meld::macro
