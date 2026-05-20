#pragma once
#include "meld/parser/ast.hpp"
#include <string>
#include <vector>

namespace meld::compiler {

struct HoldTypeInferenceResult {
    struct Hint {
        std::string variable_name;
        std::string inferred_type;
        size_t line = 0;
        size_t column = 0;
    };
    bool success = true;
    std::vector<Hint> hints;
    std::vector<Hint> inferences;
    std::vector<Hint> guest_rule_inferences;
};

class HoldTypeInferencePass {
public:
    HoldTypeInferenceResult run(const std::vector<parser::ast::expression>&, const auto&, const std::string& = "") {
        return {};
    }
};

} // namespace meld::compiler
