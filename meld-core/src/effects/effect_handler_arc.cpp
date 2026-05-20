/// @file effect_handler_arc.cpp
/// @brief Effect handler ARC interaction — implementation
///
/// Requirements: 8.1, 8.2, 8.3, 8.4, 8.5

#include "meld/effects/effect_handler_arc.hpp"
#include <cassert>
#include <stdexcept>

namespace meld::effects {

// ===========================================================================
// EffectHandlerFrame
// ===========================================================================

void EffectHandlerFrame::install() {
    // Captures are already held (strong/weak counts already incremented
    // by the shared_ptr / weak_ptr copies inside CapturedOwn / CapturedLink).
    // Mark the frame as installed so uninstall() knows to release.
    installed_ = true;
}

void EffectHandlerFrame::uninstall() {
    if (!installed_) return;

    // Release all captured references. Clearing the vectors destroys
    // the CapturedOwn / CapturedLink objects, which drops the
    // shared_ptr / weak_ptr copies and decrements strong/weak counts.
    captured_owns_.clear();
    captured_links_.clear();
    installed_ = false;
}

std::unique_ptr<EffectHandlerFrame> EffectHandlerFrame::clone() const {
    auto cloned = std::make_unique<EffectHandlerFrame>();

    // Clone every captured Hold[T] — each clone() call copies the
    // shared_ptr, incrementing strong_count by 1.
    for (const auto& own : captured_owns_) {
        cloned->captured_owns_.push_back(own->clone());
    }

    // Clone every captured View[T] — each clone() call copies the
    // weak_ptr, incrementing weak_count by 1.
    for (const auto& link : captured_links_) {
        cloned->captured_links_.push_back(link->clone());
    }

    cloned->installed_ = installed_;
    return cloned;
}

long EffectHandlerFrame::own_strong_count(size_t index) const {
    if (index >= captured_owns_.size()) {
        throw std::out_of_range("own_strong_count: index out of range");
    }
    return captured_owns_[index]->strong_count();
}

bool EffectHandlerFrame::link_expired(size_t index) const {
    if (index >= captured_links_.size()) {
        throw std::out_of_range("link_expired: index out of range");
    }
    return captured_links_[index]->expired();
}

const std::string& EffectHandlerFrame::own_label(size_t index) const {
    if (index >= captured_owns_.size()) {
        throw std::out_of_range("own_label: index out of range");
    }
    return captured_owns_[index]->label();
}

const std::string& EffectHandlerFrame::link_label(size_t index) const {
    if (index >= captured_links_.size()) {
        throw std::out_of_range("link_label: index out of range");
    }
    return captured_links_[index]->label();
}

// ===========================================================================
// EffectHandlerArcScope
// ===========================================================================

EffectHandlerArcScope::EffectHandlerArcScope(EffectHandlerFrame& frame)
    : frame_(frame) {
    frame_.install();
}

EffectHandlerArcScope::~EffectHandlerArcScope() {
    frame_.uninstall();
}

} // namespace meld::effects
