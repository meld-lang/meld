#pragma once
#include "meld/parser/ast.hpp"
#include <string>
#include <vector>

namespace meld::compiler {

struct ViewAccessDiagnostic {
    enum class Level { Info, Warning, Error };
    Level level = Level::Info;
    std::string message;
    std::string code;
    std::string source_file;
    size_t line = 0;
    size_t column = 0;
};

struct ViewAccessResult {
    bool success = true;
    std::vector<ViewAccessDiagnostic> diagnostics;
};

class ViewAccessPass {
public:
    ViewAccessResult run(const std::vector<parser::ast::expression>&, const auto&, const std::string& = "") {
        return {};
    }
};

} // namespace meld::compiler
