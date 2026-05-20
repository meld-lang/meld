#include "meld/types/option.hpp"
#include "meld/types/result.hpp"

namespace meld::types {

// Implementation is mostly in the header (template-based)
// This file exists for any non-template implementations if needed

// Transpose implementation
template<typename T, typename E>
Result<Option<T>, E> transpose(const Option<Result<T, E>>& opt) {
    if (opt.is_some()) {
        const auto& result = opt.value();
        if (result.is_success()) {
            return Result<Option<T>, E>::success(Option<T>::some(result.value()));
        } else {
            return Result<Option<T>, E>::error(result.error());
        }
    } else {
        return Result<Option<T>, E>::success(Option<T>::none());
    }
}

} // namespace meld::types
