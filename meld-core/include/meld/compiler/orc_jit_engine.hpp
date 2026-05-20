#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace meld::compiler {

/**
 * Error type for JIT module load and verification failures.
 *
 * Thrown when a bitcode module cannot be loaded, verified, or linked
 * into the ORC JIT session.
 *
 * Requirements: 3.1, 3.4
 */
class JitError : public std::runtime_error {
public:
    enum class Kind {
        InitializationFailed,   ///< LLVM target or JIT setup failure
        ModuleLoadFailed,       ///< Bitcode parsing or deserialization error
        VerificationFailed,     ///< llvm::verifyModule rejected the module
        SymbolNotFound,         ///< Entry point lookup failed
        ExecutionFailed         ///< Runtime error during JIT execution
    };

    JitError(Kind kind, const std::string& message)
        : std::runtime_error(message), kind_(kind) {}

    Kind kind() const noexcept { return kind_; }

private:
    Kind kind_;
};

/**
 * Metadata for a loaded JIT module.
 */
struct JitModuleInfo {
    std::string name;
    std::vector<uint8_t> bitcode;   ///< Retained for rollback on hot-swap failure
    bool is_active = true;
};

/**
 * LLVM ORC JIT engine for Tier 2 (meld dev) hot-reload execution.
 *
 * Wraps LLVM's ORC JIT v2 API with lazy compilation support.
 * Provides module loading, hot-swapping (<200ms latency target),
 * and entry-point execution.
 *
 * On verification failure the engine retains the previous working
 * module and reports the error via JitError.
 *
 * Requirements: 3.1, 3.2, 3.4
 */
class OrcJitEngine {
public:
    /// Function signature for JIT entry points: int main(void).
    using EntryPointFn = int (*)();

    OrcJitEngine();
    ~OrcJitEngine();

    // Non-copyable, non-movable (owns LLVM resources)
    OrcJitEngine(const OrcJitEngine&) = delete;
    OrcJitEngine& operator=(const OrcJitEngine&) = delete;
    OrcJitEngine(OrcJitEngine&&) = delete;
    OrcJitEngine& operator=(OrcJitEngine&&) = delete;

    /**
     * Initialize the LLVM ORC JIT with lazy compilation support.
     *
     * Must be called before any load/execute operations.
     * Initializes LLVM native target, creates the JIT session,
     * and configures lazy compilation layers.
     *
     * @throws JitError on initialization failure.
     */
    void initialize();

    /**
     * Load an LLVM bitcode module into the JIT session.
     *
     * The module is verified before being added. On verification
     * failure a JitError is thrown and no module is loaded.
     *
     * @param name     Logical module name (used for hot-swap lookup).
     * @param bitcode  Raw LLVM bitcode bytes (.bc content).
     * @throws JitError on load or verification failure.
     *
     * Requirements: 3.1, 3.2
     */
    void load_module(const std::string& name,
                     const std::vector<uint8_t>& bitcode);

    /**
     * Hot-swap an existing module with updated bitcode.
     *
     * Removes the old module and loads the replacement. If the new
     * module fails verification the previous working module is
     * retained and a JitError is thrown.
     *
     * Target latency: <200ms for single-module swaps (Req 3.3).
     *
     * @param name     Name of the module to replace (must exist).
     * @param bitcode  Updated LLVM bitcode bytes.
     * @throws JitError if the module is unknown or the new bitcode
     *         fails verification.
     *
     * Requirements: 3.2, 3.3, 3.4
     */
    void hot_swap_module(const std::string& name,
                         const std::vector<uint8_t>& bitcode);

    /**
     * Look up and call a JIT-compiled entry point.
     *
     * @param entry_point  Symbol name of the function to execute.
     * @return The integer return value of the entry point.
     * @throws JitError if the symbol is not found or execution fails.
     *
     * Requirements: 3.1
     */
    int execute(const std::string& entry_point);

    /**
     * Check whether the engine has been initialized.
     */
    bool is_initialized() const noexcept { return initialized_; }

    /**
     * Check whether a named module is currently loaded.
     */
    bool has_module(const std::string& name) const;

    /**
     * Get the names of all currently loaded modules.
     */
    std::vector<std::string> loaded_modules() const;

private:
    bool initialized_ = false;
    mutable std::mutex mutex_;

    /// Loaded module metadata keyed by logical name.
    std::unordered_map<std::string, JitModuleInfo> modules_;

    /// Opaque pointer to LLVM ORC JIT internals (PIMPL).
    struct Impl;
    std::unique_ptr<Impl> impl_;

    /// Internal: add a verified bitcode buffer to the JIT session.
    void add_module_to_session(const std::string& name,
                               const std::vector<uint8_t>& bitcode);

    /// Internal: remove a module from the JIT session by name.
    void remove_module_from_session(const std::string& name);
};

} // namespace meld::compiler
