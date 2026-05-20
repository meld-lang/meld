#pragma once

/// @file effect_handler_arc.hpp
/// @brief Effect handler ARC interaction for Hold[T]/View[T] capture semantics
///
/// Models the ARC behavior when effect handlers capture Hold[T] and View[T]
/// references from their enclosing scope:
///
///   - Hold[T] capture: increments strong_count on install, decrements on exit
///   - View[T] capture: increments weak_count on install, decrements on exit;
///     still requires upgrade via `if val`/`match` inside handler body
///   - Multi-shot continuation clone: cloning copies all captured Hold[T]
///     (incrementing strong_count per clone); discarding decrements
///
/// Requirements: 8.1, 8.2, 8.3, 8.4, 8.5

#include "meld/std/mem.hpp"
#include "meld/types/memory.hpp"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace meld::effects {

// ---------------------------------------------------------------------------
// CapturedOwn — type-erased wrapper for a captured Hold[T] reference
// ---------------------------------------------------------------------------

/// Abstract base for type-erased captured Hold[T] references.
/// Each capture holds a copy of the shared_ptr, which increments
/// strong_count on construction and decrements on destruction.
class CapturedOwnBase {
public:
    virtual ~CapturedOwnBase() = default;

    /// Clone this capture (increments strong_count again).
    virtual std::unique_ptr<CapturedOwnBase> clone() const = 0;

    /// Current strong_count of the referenced object.
    virtual long strong_count() const = 0;

    /// A label for debugging / diagnostics.
    virtual const std::string& label() const = 0;
};

/// Concrete typed capture for Hold[T].
template <typename T>
class CapturedOwn : public CapturedOwnBase {
    static_assert(std::is_base_of_v<types::ManagedObject, T>,
                  "CapturedOwn<T> requires T to derive from ManagedObject");

public:
    CapturedOwn(const std_mem::Own<T>& source, std::string lbl)
        : ptr_(source.raw()), label_(std::move(lbl)) {}

    std::unique_ptr<CapturedOwnBase> clone() const override {
        return std::make_unique<CapturedOwn<T>>(ptr_, label_);
    }

    long strong_count() const override {
        return ptr_ ? static_cast<long>(ptr_.use_count()) : 0;
    }

    const std::string& label() const override { return label_; }

    /// Access the underlying shared_ptr.
    const std::shared_ptr<T>& raw() const { return ptr_; }

private:
    /// Private constructor used by clone().
    CapturedOwn(std::shared_ptr<T> ptr, std::string lbl)
        : ptr_(std::move(ptr)), label_(std::move(lbl)) {}

    std::shared_ptr<T> ptr_;   // holds a strong reference (copy semantics)
    std::string label_;

    friend class CapturedOwn;  // allow clone's private ctor
};

// ---------------------------------------------------------------------------
// CapturedLink — type-erased wrapper for a captured View[T] reference
// ---------------------------------------------------------------------------

/// Abstract base for type-erased captured View[T] references.
/// Each capture holds a copy of the weak_ptr, which increments
/// weak_count on construction and decrements on destruction.
class CapturedLinkBase {
public:
    virtual ~CapturedLinkBase() = default;

    /// Clone this capture.
    virtual std::unique_ptr<CapturedLinkBase> clone() const = 0;

    /// Whether the referenced object is still alive.
    virtual bool expired() const = 0;

    /// A label for debugging / diagnostics.
    virtual const std::string& label() const = 0;
};

/// Concrete typed capture for View[T].
template <typename T>
class CapturedLink : public CapturedLinkBase {
    static_assert(std::is_base_of_v<types::ManagedObject, T>,
                  "CapturedLink<T> requires T to derive from ManagedObject");

public:
    CapturedLink(const std_mem::Link<T>& source, std::string lbl)
        : weak_(source.raw()), label_(std::move(lbl)) {}

    std::unique_ptr<CapturedLinkBase> clone() const override {
        return std::make_unique<CapturedLink<T>>(weak_, label_);
    }

    bool expired() const override { return weak_.expired(); }

    const std::string& label() const override { return label_; }

    /// Access the underlying weak_ptr.
    const types::WeakRef<T>& raw() const { return weak_; }

private:
    /// Private constructor used by clone().
    CapturedLink(types::WeakRef<T> weak, std::string lbl)
        : weak_(std::move(weak)), label_(std::move(lbl)) {}

    types::WeakRef<T> weak_;
    std::string label_;
};

// ---------------------------------------------------------------------------
// EffectHandlerFrame — captures Hold[T] and View[T] from enclosing scope
// ---------------------------------------------------------------------------

/// Represents the captured reference state of an installed effect handler.
///
/// On install (construction / install()):
///   - Each captured Hold[T] increments strong_count (copy semantics).
///   - Each captured View[T] increments weak_count.
///
/// On scope exit (destruction / uninstall()):
///   - Each captured Hold[T] decrements strong_count.
///   - Each captured View[T] decrements weak_count.
///
/// Multi-shot continuation clone:
///   - clone() copies the frame, incrementing strong_count for every
///     captured Hold[T] and weak_count for every captured View[T].
///   - Discarding the clone (destruction) decrements accordingly.
class EffectHandlerFrame {
public:
    EffectHandlerFrame() = default;
    ~EffectHandlerFrame() = default;

    // Move-only to prevent accidental implicit copies.
    EffectHandlerFrame(EffectHandlerFrame&&) noexcept = default;
    EffectHandlerFrame& operator=(EffectHandlerFrame&&) noexcept = default;
    EffectHandlerFrame(const EffectHandlerFrame&) = delete;
    EffectHandlerFrame& operator=(const EffectHandlerFrame&) = delete;

    // -- Capture API --------------------------------------------------------

    /// Capture an Hold[T] reference (increments strong_count via copy).
    template <typename T>
    void capture_own(const std_mem::Own<T>& own, const std::string& label = "") {
        captured_owns_.push_back(
            std::make_unique<CapturedOwn<T>>(own, label));
    }

    /// Capture a View[T] reference (increments weak_count via copy).
    template <typename T>
    void capture_link(const std_mem::Link<T>& link, const std::string& label = "") {
        captured_links_.push_back(
            std::make_unique<CapturedLink<T>>(link, label));
    }

    // -- Install / Uninstall ------------------------------------------------

    /// Install the handler frame. Captures are already held via the
    /// capture_own / capture_link calls (strong/weak counts already
    /// incremented). This method marks the frame as installed.
    void install();

    /// Uninstall the handler frame. Releases all captured references
    /// (decrements strong/weak counts). After this call the frame is
    /// empty and must not be reused.
    void uninstall();

    /// Whether the frame is currently installed.
    bool is_installed() const { return installed_; }

    // -- Multi-shot continuation clone --------------------------------------

    /// Clone this frame for a multi-shot continuation. Every captured
    /// Hold[T] is copied (strong_count +1 per clone). Every captured
    /// View[T] is copied (weak_count +1 per clone).
    std::unique_ptr<EffectHandlerFrame> clone() const;

    // -- Introspection ------------------------------------------------------

    /// Number of captured Hold[T] references.
    size_t own_capture_count() const { return captured_owns_.size(); }

    /// Number of captured View[T] references.
    size_t link_capture_count() const { return captured_links_.size(); }

    /// Strong count of the i-th captured Hold[T].
    long own_strong_count(size_t index) const;

    /// Whether the i-th captured View[T] is expired.
    bool link_expired(size_t index) const;

    /// Label of the i-th captured Hold[T].
    const std::string& own_label(size_t index) const;

    /// Label of the i-th captured View[T].
    const std::string& link_label(size_t index) const;

private:
    std::vector<std::unique_ptr<CapturedOwnBase>> captured_owns_;
    std::vector<std::unique_ptr<CapturedLinkBase>> captured_links_;
    bool installed_ = false;
};

// ---------------------------------------------------------------------------
// RAII scope guard for EffectHandlerFrame
// ---------------------------------------------------------------------------

/// Installs an EffectHandlerFrame on construction and uninstalls on
/// destruction, ensuring captured references are properly released
/// even if an exception is thrown.
class EffectHandlerArcScope {
public:
    explicit EffectHandlerArcScope(EffectHandlerFrame& frame);
    ~EffectHandlerArcScope();

    // Non-copyable, non-movable.
    EffectHandlerArcScope(const EffectHandlerArcScope&) = delete;
    EffectHandlerArcScope& operator=(const EffectHandlerArcScope&) = delete;
    EffectHandlerArcScope(EffectHandlerArcScope&&) = delete;
    EffectHandlerArcScope& operator=(EffectHandlerArcScope&&) = delete;

private:
    EffectHandlerFrame& frame_;
};

} // namespace meld::effects
