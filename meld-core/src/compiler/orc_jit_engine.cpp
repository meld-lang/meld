#include "meld/compiler/orc_jit_engine.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>

// ── LLVM headers ────────────────────────────────────────────────────
//
// These are the ORC JIT v2 and support headers required for lazy
// compilation.  The build system must link against the LLVM libraries
// (LLVMOrcJIT, LLVMCore, LLVMSupport, LLVMIRReader, etc.).

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>

namespace meld::compiler {

// ── Helper: stringify an llvm::Error ────────────────────────────────

namespace {

std::string to_string(llvm::Error err) {
    std::string msg;
    llvm::raw_string_ostream os(msg);
    os << err;
    return msg;
}

} // anonymous namespace

// ── PIMPL internals ─────────────────────────────────────────────────
//
// Wraps the LLJIT instance and per-module JITDylib handles so that
// hot_swap_module can remove and re-add individual modules.

struct OrcJitEngine::Impl {
    std::unique_ptr<llvm::orc::LLJIT> jit;

    /// Map from logical module name → JITDylib name used in the session.
    std::unordered_map<std::string, std::string> dylib_names;

    /// Counter used to generate unique JITDylib names for each load.
    uint64_t dylib_counter = 0;

    /// Generate a unique JITDylib name for a module.
    std::string next_dylib_name(const std::string& module_name) {
        return module_name + "." + std::to_string(dylib_counter++);
    }
};

// ── Construction / destruction ──────────────────────────────────────

OrcJitEngine::OrcJitEngine()
    : impl_(std::make_unique<Impl>()) {}

OrcJitEngine::~OrcJitEngine() = default;

// ── initialize() ────────────────────────────────────────────────────
//
// Initializes LLVM native target machinery and creates the LLJIT
// instance with lazy compilation support.
//
// Requirements: 3.1

void OrcJitEngine::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        return; // idempotent
    }

    // Initialize LLVM native target (required once per process).
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    // Build the LLJIT instance.  LLJITBuilder configures lazy
    // compilation layers automatically when available.
    auto jit_builder = llvm::orc::LLJITBuilder();
    auto jit_or_err = jit_builder.create();

    if (!jit_or_err) {
        throw JitError(JitError::Kind::InitializationFailed,
                       "Failed to create ORC JIT: " +
                           to_string(jit_or_err.takeError()));
    }

    impl_->jit = std::move(*jit_or_err);
    initialized_ = true;
}

// ── load_module() ───────────────────────────────────────────────────
//
// Parses bitcode, verifies the module, and adds it to the JIT session
// in its own JITDylib so it can be independently swapped later.
//
// Requirements: 3.1, 3.2

void OrcJitEngine::load_module(const std::string& name,
                               const std::vector<uint8_t>& bitcode) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        throw JitError(JitError::Kind::InitializationFailed,
                       "OrcJitEngine not initialized — call initialize() first");
    }

    if (modules_.count(name)) {
        throw JitError(JitError::Kind::ModuleLoadFailed,
                       "Module '" + name + "' is already loaded; "
                       "use hot_swap_module() to replace it");
    }

    add_module_to_session(name, bitcode);

    // Record metadata for future hot-swap / rollback.
    JitModuleInfo info;
    info.name = name;
    info.bitcode = bitcode;
    info.is_active = true;
    modules_[name] = std::move(info);
}

// ── hot_swap_module() ───────────────────────────────────────────────
//
// Replaces an existing module with updated bitcode.  If the new
// module fails verification the previous module is retained.
//
// Target latency: <200ms for single-module swaps (Req 3.3).
//
// Requirements: 3.2, 3.3, 3.4

void OrcJitEngine::hot_swap_module(const std::string& name,
                                   const std::vector<uint8_t>& bitcode) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        throw JitError(JitError::Kind::InitializationFailed,
                       "OrcJitEngine not initialized");
    }

    auto it = modules_.find(name);
    if (it == modules_.end()) {
        throw JitError(JitError::Kind::ModuleLoadFailed,
                       "Cannot hot-swap unknown module '" + name + "'");
    }

    // Keep a copy of the previous bitcode for rollback.
    std::vector<uint8_t> previous_bitcode = it->second.bitcode;

    // Attempt to remove the old module from the JIT session.
    remove_module_from_session(name);

    try {
        // Add the new module.  add_module_to_session verifies the
        // bitcode before adding — on failure it throws JitError.
        add_module_to_session(name, bitcode);

        // Success — update stored metadata.
        it->second.bitcode = bitcode;
    } catch (const JitError&) {
        // Verification or load of the new module failed.
        // Roll back: re-add the previous working module.
        try {
            add_module_to_session(name, previous_bitcode);
        } catch (...) {
            // If rollback also fails we are in a degraded state.
            // Mark the module as inactive so callers know.
            it->second.is_active = false;
        }
        // Re-throw the original error so the caller can report it.
        throw;
    }
}

// ── execute() ───────────────────────────────────────────────────────
//
// Looks up a symbol in the JIT session and calls it as an int(void)
// entry point.
//
// Requirements: 3.1

int OrcJitEngine::execute(const std::string& entry_point) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        throw JitError(JitError::Kind::InitializationFailed,
                       "OrcJitEngine not initialized");
    }

    // Look up the symbol across all JITDylibs in the session.
    auto sym_or_err = impl_->jit->lookup(entry_point);
    if (!sym_or_err) {
        throw JitError(JitError::Kind::SymbolNotFound,
                       "Symbol '" + entry_point + "' not found: " +
                           to_string(sym_or_err.takeError()));
    }

    // Cast the address to the entry-point function type and call it.
    auto addr = sym_or_err->toPtr<EntryPointFn>();
    if (!addr) {
        throw JitError(JitError::Kind::SymbolNotFound,
                       "Symbol '" + entry_point + "' resolved to null");
    }

    try {
        return addr();
    } catch (const std::exception& ex) {
        throw JitError(JitError::Kind::ExecutionFailed,
                       "Execution of '" + entry_point + "' failed: " +
                           std::string(ex.what()));
    } catch (...) {
        throw JitError(JitError::Kind::ExecutionFailed,
                       "Execution of '" + entry_point +
                           "' failed with unknown exception");
    }
}

// ── Query helpers ───────────────────────────────────────────────────

bool OrcJitEngine::has_module(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return modules_.count(name) > 0;
}

std::vector<std::string> OrcJitEngine::loaded_modules() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(modules_.size());
    for (const auto& [k, _] : modules_) {
        names.push_back(k);
    }
    return names;
}

// ── Internal: add / remove modules from the JIT session ─────────────

void OrcJitEngine::add_module_to_session(const std::string& name,
                                         const std::vector<uint8_t>& bitcode) {
    // Parse the bitcode buffer into an llvm::Module.
    auto mem_buf = llvm::MemoryBuffer::getMemBufferCopy(
        llvm::StringRef(reinterpret_cast<const char*>(bitcode.data()),
                        bitcode.size()),
        name);

    auto ctx = std::make_unique<llvm::LLVMContext>();
    llvm::SMDiagnostic diag;

    auto expected_module =
        llvm::parseBitcodeFile(mem_buf->getMemBufferRef(), *ctx);

    if (!expected_module) {
        throw JitError(JitError::Kind::ModuleLoadFailed,
                       "Failed to parse bitcode for module '" + name +
                           "': " + to_string(expected_module.takeError()));
    }

    auto& module = *expected_module;

    // Verify the module before adding to the session.
    std::string verify_err;
    llvm::raw_string_ostream verify_os(verify_err);
    if (llvm::verifyModule(*module, &verify_os)) {
        throw JitError(JitError::Kind::VerificationFailed,
                       "Module '" + name +
                           "' failed verification: " + verify_err);
    }

    // Create a dedicated JITDylib for this module so it can be
    // independently removed during hot-swap.
    std::string dylib_name = impl_->next_dylib_name(name);

    auto& es = impl_->jit->getExecutionSession();
    auto& dylib = es.createBareJITDylib(dylib_name);

    // Link the main dylib so the module can resolve symbols from
    // previously loaded modules and the process itself.
    auto& main_dylib = impl_->jit->getMainJITDylib();
    dylib.addToLinkOrder(main_dylib);

    // Wrap in a ThreadSafeModule and add to the JITDylib.
    auto tsm = llvm::orc::ThreadSafeModule(std::move(module),
                                           std::move(ctx));

    if (auto err = impl_->jit->addIRModule(dylib, std::move(tsm))) {
        throw JitError(JitError::Kind::ModuleLoadFailed,
                       "Failed to add module '" + name +
                           "' to JIT session: " + to_string(std::move(err)));
    }

    // Record the dylib name for later removal.
    impl_->dylib_names[name] = dylib_name;
}

void OrcJitEngine::remove_module_from_session(const std::string& name) {
    auto dit = impl_->dylib_names.find(name);
    if (dit == impl_->dylib_names.end()) {
        return; // nothing to remove
    }

    auto& es = impl_->jit->getExecutionSession();
    auto* dylib = es.getJITDylibByName(dit->second);
    if (dylib) {
        // Clear all symbols from the dylib.  This effectively
        // "unloads" the module from the session.
        if (auto err = dylib->clear()) {
            // Non-fatal: log and continue.  The dylib may still
            // hold stale symbols but the next load will shadow them.
            llvm::errs() << "Warning: failed to clear JITDylib '"
                         << dit->second << "': " << err << "\n";
            llvm::consumeError(std::move(err));
        }
    }

    impl_->dylib_names.erase(dit);
}

} // namespace meld::compiler
