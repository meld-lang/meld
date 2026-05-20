#pragma once

#include <set>
#include <string>

namespace meld::parser {

// Parser context for effect name resolution during parsing.
// Populated during the initial pass that collects @effect trait definitions.
// Used by the parser to disambiguate Effect.operation(args) from regular method calls.
struct ParserContext {
    std::set<std::string> known_effect_names;

    bool is_effect_name(const std::string& name) const {
        return known_effect_names.count(name) > 0;
    }
};

} // namespace meld::parser
