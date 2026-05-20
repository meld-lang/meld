#include "meld/stdlib/regex.hpp"

namespace meld::stdlib {

// Implementation notes:
//
// The Regex class is primarily implemented in the header file using
// C++ standard library <regex>. This file is provided for:
// 1. Future non-inline implementations
// 2. Platform-specific regex engine integration
// 3. Maintaining consistent project structure

// Future enhancements could include:
// - Integration with PCRE or RE2 for better performance
// - Named capture group support
// - More sophisticated multiline handling
// - Unicode property support
// - Regex optimization and caching

} // namespace meld::stdlib
