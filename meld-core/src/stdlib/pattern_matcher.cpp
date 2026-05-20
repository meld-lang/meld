#include "meld/stdlib/pattern_matcher.hpp"

namespace meld::stdlib {

// Implementation notes:
// 
// The PatternMatcher class is primarily template-based, so most of the
// implementation is in the header file. This file is provided for:
// 1. Future non-template helper functions
// 2. Explicit template instantiations if needed
// 3. Maintaining consistent project structure

// Example explicit instantiations for common types (optional):
// template class PatternMatcher<int, std::string>;
// template class PatternMatcher<std::string, int>;

} // namespace meld::stdlib
