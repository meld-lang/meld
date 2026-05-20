# Meld Manifest & Binary Security Requirements

## Introduction

This specification defines the requirements for the `.meld` manifest sidecar, the `.note.meld` binary tombstone section, cryptographic signing infrastructure, production artifact integrity, and runtime sandbox enforcement for the Meld programming language. Together these components form a chain of trust from compilation through deployment: the compiler emits an effect manifest alongside each binary, the manifest is cryptographically signed, a tombstone is embedded in the binary linking it to the manifest, the runtime daemon verifies the chain before execution, and the sandbox enforces the declared effects at the OS level.

This spec covers REQUIREMENTS.md sections §3.1 (Manifest Sidecar), §3.2 (Binary Tombstone), §3.3 (C/C++ Interop Bridge Rule), §3.4 (Global Effect Elision), §4.1–4.5 (Cryptographic Signing & Verification), §5.1 (SRT Integration), §5.2 (SandboxProvider Interface), §5.3 (Build-Time Isolation), and §5.4 (MCP Tool Hardening).

This spec also incorporates the runtime sandbox requirements (formerly `.kiro/specs/meld-sandbox/`, now retired). Requirements 10–16 cover the `SandboxProvider` interface, SRT integration, ephemeral policy lifecycle, build-time isolation, and MCP tool hardening.

**Cross-references:**
- `.kiro/specs/meld-build/requirements.md` — Requirement 11 (link-time effect validation, simplified sidecar) will be upgraded to use this manifest format
- `.kiro/specs/meld-compiler/requirements.md` — Requirement 12 (version-aware symbol mangling), Requirement 8 (AOT pipeline)
- `.kiro/specs/meld-core/requirements.md` (Req 99–118) — `@effect` / `@uses` annotation system defines the compile-time effect model
- `.kiro/specs/meld-build/requirements.md` — Bazel rules (Req 1–6) orchestrate builds; build-time isolation (Req 14) wraps those builds in a sandbox
- `.kiro/specs/meld-mcp-server/requirements.md` — MCP tools (Req 1–7) are wrapped in sandbox enforcement (Req 15)
- `.kiro/specs/meld-async/requirements.md` — Isolate system (Req 7) provides process-level isolation; the sandbox adds OS-level capability restriction on top

## Glossary

- **Manifest**: A binary-serialized sidecar file (`.meld` extension) co-located with the compiled binary, containing a map of `Symbol Name → Effect Bitmask + Resource Bounds` for every exported symbol
- **Effect_Bitmask**: A compact bitfield encoding the set of effects (`Network`, `FileSystemRead`, `FileSystemWrite`, `ProcessExec`, `SystemTime`, `State`) that a symbol may perform
- **Resource_Bounds**: Per-effect constraints specifying allowed domains, file paths, or capability limits (e.g., allowed network domains, read-only vs read-write paths)
- **Tombstone**: A non-loadable ELF (`.note.meld`) or Mach-O section embedded in the compiled binary containing `manifest_hash`, `signature_blob`, `EffectID`, and `debug_id`
- **Debug_ID**: A unique identifier (UUID or content-addressable hash) embedded in the Tombstone that links the binary to its `.mdebug` debug sidecar, enabling the daemon to locate and load shadow symbols for crash analysis and debugging
- **Mdebug_Sidecar**: A compressed archive file (`.mdebug` extension) co-located with the binary or stored in a symbol cache, containing stripped DWARF data, AST-to-PC mapping, and Ownership State Traces for the binary
- **EffectID**: A unique identifier embedded in the Tombstone, used by the daemon to look up the SRT (Sandbox Runtime) policy for the binary
- **Code_Hash**: A SHA-256 hash (or Merkle Root of executable segments) of the binary's loadable code, stored in the manifest for integrity verification
- **Manifest_Hash**: A SHA-256 hash of the manifest file, stored in the Tombstone for cross-verification
- **Signature_Blob**: A cryptographic signature over the manifest, stored in the Tombstone
- **Sigstore_Bundle**: A composite artifact containing the signature, a short-lived Fulcio certificate, and a Rekor Signed Entry Timestamp (SET), enabling keyless identity-based signing
- **Fulcio**: Sigstore's certificate authority that issues short-lived certificates bound to OIDC identity (GitHub Actions, GitLab CI, Google)
- **Rekor**: Sigstore's transparency log that records signing events for auditability
- **meldn**: The Meld Notary — a standalone CLI utility (`meldn sign`, `meldn verify`, `meldn bundle`) that produces signed tombstones from a binary and manifest, verifies binary integrity, and prepares air-gapped bundles
- **MeldDaemon**: The long-lived runtime process that verifies manifest/tombstone integrity before spawning executables
- **Global_Effect_Elision**: A whole-binary optimization pass that strips unreachable effects from the manifest during production builds
- **Bridge_Rule**: The policy that any `extern "C"` FFI function must be explicitly tagged with `@effect`; untagged FFI defaults to "Full IO"
- **SandboxProvider**: An abstract C++ interface in `libmeld_core` that translates a set of Meld `Effect` values and resource constraints into an OS-specific sandbox configuration and launches a sandboxed process
- **Effect**: An enum in `libmeld_core` representing a category of side effect: `Network`, `FileSystemRead`, `FileSystemWrite`, `ProcessExec`, `SystemTime`, `State`
- **SandboxConfig**: A C++ struct containing the set of allowed effects, read-only mount paths, read-write mount paths, allowed network domains, and working directory for a sandboxed execution
- **SandboxResult**: A C++ struct containing exit code, stdout, stderr, and a boolean indicating whether the sandbox itself failed
- **MeldSRTProvider**: The default `SandboxProvider` implementation that generates Anthropic SRT policy files and delegates execution to the SRT CLI
- **SRT**: Anthropic's Sandbox Runtime — a cross-platform sandboxing tool using Seatbelt (`sandbox-exec`) on macOS and Bubblewrap (`bwrap`) on Linux
- **srt-settings.json**: An ephemeral JSON policy file generated per execution, consumed by the SRT CLI, and deleted after execution completes
- **Build_Time_Isolation**: A zero-trust sandbox applied to all third-party dependency builds, granting no network access and read-only source tree access
- **MCP_Tool_Hardening**: The policy that all MCP tool executions are automatically wrapped in an SRT sandbox derived from the target module's effect manifest

## Requirements

### Requirement 1: Manifest Format and Content

**User Story:** As a Meld developer, I want the compiler to emit a structured manifest alongside my binary, so that consumers and the runtime can understand the security contract without needing source code.

#### Acceptance Criteria

1. WHEN the AOT_Pipeline produces a native binary, THE Compiler SHALL emit a `.meld` manifest sidecar file co-located with the binary
2. THE manifest SHALL contain a binary-serialized map of `Symbol Name → Effect Bitmask + Resource Bounds` for every exported symbol in the binary
3. THE manifest SHALL include a `code_hash` field containing a SHA-256 hash (or Merkle Root) of the binary's executable segments
4. THE manifest SHALL include a format version number for forward compatibility
5. THE manifest SHALL include the project coordinate (name, version) from `meld.toml`
6. WHEN a consumer imports a dependency, THE consumer's toolchain SHALL read the dependency's manifest to understand its effect contract without requiring source code
7. THE manifest format SHALL be compact enough that reading it adds negligible overhead to build and startup times

### Requirement 2: Tombstone Binary Section

**User Story:** As a release engineer, I want a non-loadable section embedded in the binary that links it to its manifest and signature, so that integrity can be verified from the binary alone without external metadata files.

#### Acceptance Criteria

1. WHEN the AOT_Pipeline produces a native binary, THE Compiler SHALL embed a `.note.meld` section (ELF) or equivalent Mach-O section in the binary
2. THE Tombstone section SHALL contain: `manifest_hash` (SHA-256 of the manifest), `signature_blob` (cryptographic signature), `EffectID` (unique identifier for SRT policy lookup), and `debug_id` (unique identifier linking the binary to its `.mdebug` sidecar)
3. THE Tombstone section SHALL be non-loadable — it SHALL NOT consume runtime memory or affect execution
4. THE Tombstone SHALL enable mutual authentication: modifying the binary breaks the code_hash in the manifest; modifying the manifest breaks the manifest_hash in the Tombstone; modifying both breaks the cross-hash
5. THE effect tracking metadata in the Tombstone SHALL add zero runtime instructions — it is purely compile-time metadata
6. THE `debug_id` SHALL be a deterministic content-addressable hash derived from the binary's code content, so that identical builds produce the same `debug_id` and can share `.mdebug` sidecars

### Requirement 3: C/C++ Interop Bridge Rule

**User Story:** As a Meld developer using FFI, I want the compiler to enforce effect declarations on foreign functions, so that native code cannot silently bypass the Effect Firewall.

#### Acceptance Criteria

1. WHEN a function is declared with `@extern("C")` or equivalent FFI annotation, THE Compiler SHALL require an explicit `@effect` annotation declaring the function's side effects
2. IF an `@extern` function has no `@effect` annotation, THEN THE Compiler SHALL treat it as "Full IO" — the most restrictive assumption requiring all effect permissions
3. THE Compiler SHALL emit a warning when defaulting an FFI function to "Full IO", suggesting the developer add an explicit `@effect` annotation
4. THE manifest SHALL include FFI functions and their declared (or defaulted) effects in the symbol-to-effect map
5. THE "Full IO" default SHALL require the consumer to grant all effect permissions for the dependency containing the untagged FFI function

### Requirement 4: Global Effect Elision (Production Optimization)

**User Story:** As a release engineer, I want the production build to strip unreachable effects from the manifest, so that the runtime policy is as restrictive as possible without manual tuning.

#### Acceptance Criteria

1. WHEN `--compilation_mode=opt` is set (or `meld build --release`), THE Compiler SHALL perform whole-binary call-graph analysis to identify effects that are never reachable from any entry point
2. THE Compiler SHALL remove unreachable effects from the final manifest, producing a minimal effect policy
3. THE Global_Effect_Elision pass SHALL run after ThinLTO cross-module inlining, so that inlined code is included in the reachability analysis
4. THE Global_Effect_Elision pass SHALL NOT remove effects from debug builds, preserving the full effect map for development diagnostics
5. THE elided manifest SHALL be re-hashed and the Tombstone updated with the new `manifest_hash`

### Requirement 5: Cryptographic Signing (Combined Integrity Hash)

**User Story:** As a release engineer, I want the manifest and binary to be cryptographically signed with mutual integrity guarantees covering code, effects, and debug identity, so that tampering with any artifact is detectable.

#### Acceptance Criteria

1. THE signing process SHALL produce a `signature_blob` over a Combined Integrity Hash: `H_total = Hash(code_hash + manifest_hash + debug_id)`, using the signer's private key (or ephemeral key via Sigstore)
2. THE manifest SHALL contain the `code_hash` of the binary, and the Tombstone SHALL contain the `manifest_hash` of the manifest and the `debug_id`, creating a linked hash chain that includes the debug sidecar identity
3. IF the binary is modified without updating the manifest, THE `code_hash` in the manifest SHALL not match, and verification SHALL fail
4. IF the manifest is modified without re-signing, THE `signature_blob` in the Tombstone SHALL not verify, and verification SHALL fail
5. IF both are modified without access to the signing key, THE signature SHALL not verify against the new manifest content
6. IF the `debug_id` in the Tombstone is modified (e.g., to point to a different `.mdebug` sidecar), THE Combined Integrity Hash SHALL not match the `signature_blob`, and verification SHALL fail

### Requirement 6: Sigstore / Cosign Integration (Keyless Signing)

**User Story:** As a CI/CD engineer, I want keyless, identity-based signing for Meld binaries, so that I don't need to manage long-lived signing keys.

#### Acceptance Criteria

1. THE `meldn` utility SHALL support the Sigstore protocol for keyless signing via OIDC identity (GitHub Actions, GitLab CI, Google)
2. WHEN using Sigstore mode, THE `meldn` utility SHALL request a short-lived certificate from Fulcio bound to the caller's OIDC identity
3. THE `meldn` utility SHALL generate ephemeral signing keys in-memory and destroy them after the signature is logged in Rekor
4. THE `meldn` utility SHALL produce a Sigstore Bundle (signature + Fulcio certificate + Rekor SET) and embed it in the Tombstone's `signature_blob`
5. THE `meldn` utility SHALL also support traditional key-based signing (private key file or HSM) as an alternative to Sigstore
6. THE `meldn` utility SHALL support configurable Fulcio and Rekor endpoints via `--fulcio-url` and `--rekor-url` flags (or `meld.toml` `[signing]` section), enabling private Sigstore instances for air-gapped infrastructures

### Requirement 7: Verification Modes

**User Story:** As a Meld developer, I want the runtime to verify binary integrity before execution, with both offline and online verification options.

#### Acceptance Criteria

1. THE MeldDaemon (or `meld run` pre-execution check) SHALL verify the Tombstone signature before spawning a process
2. IN offline mode, THE verifier SHALL check the Sigstore Bundle's SET against the bundled Sigstore trust root without network access
3. IN online audit mode, THE verifier SHALL re-query the Rekor transparency log to check for certificate revocation or compromise
4. IF any verification check fails (signature mismatch, hash mismatch, revoked certificate), THE daemon SHALL refuse to spawn the process and emit a structured audit log entry
5. THE verification mode (offline vs online) SHALL be configurable via `meld.toml` or daemon configuration

### Requirement 8: `meldn` Notary Utility

**User Story:** As a release engineer, I want a standalone notary tool with sign, verify, and bundle commands that integrates into my build pipeline, so that I can sign, verify, and prepare air-gapped bundles for Meld binaries as post-build steps.

#### Acceptance Criteria

1. THE `meldn` utility SHALL be a standalone CLI binary with three primary commands: `meldn sign`, `meldn verify`, and `meldn bundle`
2. WHEN `meldn sign` is invoked with a binary path and manifest path, IT SHALL produce a signed Tombstone by computing the Combined Integrity Hash (`code_hash + manifest_hash + debug_id`) and signing it
3. THE `meldn sign` command SHALL support `--key <path>` for traditional private key signing
4. THE `meldn sign` command SHALL support `--sigstore` for keyless OIDC-based signing (default mode)
5. THE `meldn sign` command SHALL validate that the manifest's `code_hash` matches the provided binary before signing, and emit a structured error if they are inconsistent
6. WHEN `meldn verify` is invoked with a binary path, IT SHALL read the Tombstone and co-located manifest, verify the Combined Integrity Hash against the `signature_blob`, and report pass/fail with details of any failed check
7. THE `meldn verify` command SHALL support `--offline` (verify against bundled trust root) and `--online` (re-query Rekor for revocation) modes
8. WHEN `meldn bundle` is invoked with a signed binary, IT SHALL fetch the necessary Rekor SETs and Fulcio certificates and embed them into the binary's Tombstone as a self-contained Sigstore Bundle, preparing the binary for offline verification by `melds` in air-gapped environments
9. THE `meldn` utility SHALL integrate into the Bazel post-processing phase via a `meld_signed_binary` rule that automatically invokes `meldn sign` on release builds
10. THE `meldn` utility SHALL support configurable Fulcio and Rekor endpoints via `--fulcio-url` and `--rekor-url` flags (or `meld.toml` `[signing]` section), enabling private Sigstore instances

### Requirement 9: Hardware-Level Integrity (Optional, High-Security)

**User Story:** As a security engineer deploying Meld in high-security environments, I want optional hardware-backed integrity verification, so that the OS kernel itself refuses to execute tampered binaries.

#### Acceptance Criteria

1. ON Linux, THE MeldDaemon SHALL optionally register binary hashes with IMA (Integrity Measurement Architecture) / TPM
2. WHEN IMA integration is enabled, THE Linux kernel SHALL refuse to `execve()` a binary whose on-disk state doesn't match the measured hash
3. THE IMA integration SHALL be opt-in and configured via daemon settings, not enabled by default
4. THE `meldn` utility SHALL support emitting IMA-compatible hash records alongside the Tombstone
5. ON platforms without IMA/TPM support, THE daemon SHALL fall back to userspace verification (Requirement 7)


---

> **Note:** Requirement 17 below was added to address the Shadow Debugging gap identified in NEW_REQUIREMENTS_2.md §5.

### Requirement 17: Debug Sidecar Format (`.mdebug`)

**User Story:** As a Meld developer, I want production binaries to be stripped of debug symbols while retaining full debugging capability through a separate sidecar, so that deployed binaries are small and fast while crash analysis and debugging remain possible.

#### Acceptance Criteria

1. WHEN `meld build --release` produces a native binary, THE AOT_Pipeline SHALL emit a `.mdebug` sidecar file co-located with the binary (e.g., `myapp.mdebug` alongside `myapp`)
2. THE `.mdebug` sidecar SHALL be a compressed archive (zstd compression) containing three sections: DWARF debug data (stripped from the production binary), an AST-to-PC index, and an Ownership State Trace table
3. THE DWARF section SHALL contain the full debug information (source locations, function names, type descriptions, local variables) that was stripped from the release binary
4. THE AST-to-PC index SHALL map PC address ranges to `ast_selector` paths (JSONPath-like references to AST nodes), enabling the daemon and MCP tools to correlate machine code locations with AST-level constructs
5. THE Ownership State Trace table SHALL contain the compile-time-computed ARC state (strong count status, weak count status, lifecycle state: valid/moved/potentially-dangling) for each reference at each instruction boundary, derived from the ARC injection analysis pass (`.kiro/specs/meld-compiler/requirements.md` Req 20–22)
6. THE `.mdebug` sidecar SHALL include a `debug_id` field in its header that matches the `debug_id` in the binary's Tombstone section, enabling the daemon to verify that a sidecar corresponds to a specific binary
7. THE `.mdebug` format SHALL include a version number for forward compatibility
8. WHEN `meld build --debug` is invoked (debug builds), THE AOT_Pipeline SHALL NOT emit a `.mdebug` sidecar — debug symbols remain inline in the binary as today
9. THE `.mdebug` sidecar SHALL be independent of the `.meld` manifest — the manifest handles effect/security metadata, the sidecar handles debug metadata; they are separate concerns linked through the Tombstone

> **Cross-reference:** The AOT pipeline emission of `.mdebug` is specified in `.kiro/specs/meld-compiler/requirements.md` Req 32. The daemon's sidecar resolution is specified in `.kiro/specs/meld-daemon/requirements.md` Req 10. The `meld debug` orchestrator that loads sidecars is specified in `.kiro/specs/meld-cli/requirements.md` Req 20.

---

> **Note:** Requirements 10–16 below were merged from the retired `.kiro/specs/meld-sandbox/` spec. They cover runtime enforcement and OS-level sandboxing.

### Requirement 10: SandboxProvider Abstract Interface

**User Story:** As a runtime developer, I want a pluggable sandbox interface that translates Meld effects into OS-specific isolation, so that the daemon and build system can enforce effect policies without being coupled to a single sandboxing technology.

#### Acceptance Criteria

1. THE `SandboxProvider` SHALL be an abstract C++ interface in `libmeld_core` with a pure virtual method `launch(SandboxConfig config, std::vector<std::string> command) -> SandboxResult`
2. THE `SandboxProvider` SHALL define an `Effect` enum with values: `Network`, `FileSystemRead`, `FileSystemWrite`, `ProcessExec`, `SystemTime`, `State`

> **Cross-reference:** The C++ `Effect` enum maps from the Meld-level effect traits defined in meld-core Req 81 (`console`, `file_system`, `network`, `random`, `time`) but uses finer granularity for OS-level sandboxing. For example, the Meld `file_system` trait maps to both `FileSystemRead` and `FileSystemWrite` in the sandbox layer. Effects like `ProcessExec` and `State` are sandbox-specific and have no direct Meld-level trait counterpart.
3. THE `SandboxProvider` SHALL define a `SandboxConfig` struct containing: `allowed_effects` (set of `Effect`), `read_only_paths` (list of filesystem paths), `read_write_paths` (list of filesystem paths), `allowed_domains` (list of network domains), and `working_directory` (path)
4. THE `SandboxProvider` SHALL define a `SandboxResult` struct containing: exit code, stdout, stderr, and a boolean indicating whether the sandbox itself failed (as opposed to the sandboxed process)
5. THE `SandboxProvider` SHALL provide a `create_default()` factory function that selects the best available provider for the host OS (SRT on macOS/Linux, with a passthrough fallback on unsupported platforms)
6. THE `SandboxProvider` interface SHALL be stateless — each `launch` call is independent and carries its full configuration

### Requirement 11: Effect-to-SRT Policy Mapping

**User Story:** As a runtime developer, I want a well-defined mapping from Meld effects to SRT policy fields, so that the sandbox policy accurately reflects the permissions declared in the effect manifest.

#### Acceptance Criteria

1. THE `MeldSRTProvider` SHALL map `Effect::Network` to `"network": { "mode": "allow", "allowedDomains": [...] }` in the SRT policy, where `allowedDomains` is populated from the manifest's `Resource Bounds`
2. THE `MeldSRTProvider` SHALL map `Effect::FileSystemWrite` to `"filesystem": { "readWrite": [...] }` in the SRT policy, populated from the manifest's allowed write paths
3. THE `MeldSRTProvider` SHALL map `Effect::FileSystemRead` to `"filesystem": { "readOnly": [...] }` in the SRT policy, populated from the manifest's allowed read paths
4. THE `MeldSRTProvider` SHALL map `Effect::ProcessExec` to the SRT process execution policy, restricting which binaries can be spawned
5. WHEN an effect is NOT present in the `SandboxConfig.allowed_effects`, THE `MeldSRTProvider` SHALL deny the corresponding capability in the SRT policy (deny-by-default)
6. THE `MeldSRTProvider` SHALL always grant read-only access to the Meld standard library and runtime paths regardless of the effect set

### Requirement 12: Ephemeral Policy Lifecycle

**User Story:** As a security engineer, I want each execution to use a unique, ephemeral sandbox policy that is never reused, so that stale or tampered policies cannot be exploited.

#### Acceptance Criteria

1. WHEN the MeldDaemon receives an execution request (build, test, or run), THE daemon SHALL generate a fresh `srt-settings.json` file with a unique filename (e.g., UUID-based) in a temporary directory
2. THE generated `srt-settings.json` SHALL be derived from the target binary's `.meld` manifest effect map and the `SandboxConfig` constructed by the daemon
3. THE daemon SHALL pass the `srt-settings.json` path to the SRT CLI as a command-line argument when launching the sandboxed process
4. WHEN the sandboxed process exits (success or failure), THE daemon SHALL delete the `srt-settings.json` file
5. IF the daemon crashes or is killed, THE daemon SHALL clean up stale policy files from previous sessions on next startup
6. THE `srt-settings.json` file SHALL be created with restrictive file permissions (owner-read-only) to prevent tampering by the sandboxed process

### Requirement 13: SRT CLI Integration

**User Story:** As a Meld developer, I want the sandbox to use the SRT CLI as its execution backend, so that I get battle-tested OS-level isolation without the Meld project maintaining its own sandbox kernel code.

#### Acceptance Criteria

1. THE `MeldSRTProvider` SHALL invoke the SRT CLI binary (assumed on `$PATH` or configured via `meld.toml`) to launch sandboxed processes
2. THE `MeldSRTProvider` SHALL use Seatbelt (`sandbox-exec`) on macOS and Bubblewrap (`bwrap`) on Linux as the underlying OS isolation mechanism, delegated through SRT
3. IF the SRT CLI is not found on the system, THE `MeldSRTProvider` SHALL emit a structured diagnostic explaining how to install SRT (via NPM or Homebrew) and fall back to unsandboxed execution with a prominent warning
4. THE `MeldSRTProvider` SHALL capture the SRT CLI's exit code and stderr to distinguish sandbox-level failures (e.g., policy violation) from application-level failures
5. THE `MeldSRTProvider` SHALL support a `--sandbox=off` flag (or `meld.toml` setting) that disables sandboxing entirely for debugging, with a warning emitted to stderr

### Requirement 14: Build-Time Isolation

**User Story:** As a Meld developer, I want all third-party dependency builds to run in a zero-trust sandbox, so that a compromised build script cannot exfiltrate data or modify my source tree.

#### Acceptance Criteria

1. WHEN `rules_meld` compiles a third-party `meld_library` target, THE build system SHALL wrap the compiler invocation in a sandbox with zero network access and read-only access to the source tree
2. THE build-time sandbox SHALL grant read-write access only to the Bazel output directory (`bazel-out/`) for that target
3. THE build-time sandbox SHALL deny `ProcessExec` for the sandboxed compiler, preventing it from spawning arbitrary subprocesses (except the compiler itself)
4. WHEN a first-party (workspace-local) target is compiled, THE build system SHALL apply a relaxed sandbox that permits filesystem reads within the workspace but still denies network access
5. THE build-time sandbox configuration SHALL be defined as a Bazel `exec_properties` attribute on `meld_library` / `meld_binary` rules, allowing per-target override
6. IF the sandbox blocks a legitimate build operation, THE build system SHALL emit a structured error explaining which effect was denied and how to grant it via `meld.toml`

### Requirement 15: MCP Tool Hardening

**User Story:** As a security engineer, I want all MCP tool executions to be automatically sandboxed, so that an AI agent cannot use MCP tools to escape the effect boundaries of the code it's operating on.

#### Acceptance Criteria

1. WHEN the MCP server executes a tool that runs code (e.g., `verify_test`, `dry_run_patch`, `apply_patch`), THE MCP server SHALL wrap the execution in an SRT sandbox derived from the target module's effect manifest
2. THE MCP sandbox policy SHALL be at least as restrictive as the target module's declared effects — the tool SHALL NOT gain capabilities beyond what the module declares
3. WHEN an MCP tool operates on multiple modules with different effect sets, THE MCP server SHALL use the intersection (most restrictive) of their effect sets as the sandbox policy
4. THE MCP server SHALL reject any tool invocation that requests "dangerously disable sandbox" unless the user has provided an explicit physical override (e.g., environment variable `MELD_UNSAFE_NO_SANDBOX=1` set before daemon startup)
5. THE MCP server SHALL log all sandboxed tool executions with the tool name, target module, applied effect set, and outcome (success/failure/sandbox-violation) for audit purposes
6. WHEN a sandboxed MCP tool execution is denied by the sandbox (e.g., attempted network access not in the effect set), THE MCP server SHALL return a structured error to the agent explaining the denied capability and suggesting the appropriate `@effect` annotation

### Requirement 16: Sandbox Diagnostics and Observability

**User Story:** As a Meld developer, I want clear diagnostics when the sandbox blocks an operation, so that I can understand whether I need to add an `@effect` annotation or whether something unexpected is happening.

#### Acceptance Criteria

1. WHEN the sandbox denies an operation, THE daemon SHALL emit a structured diagnostic containing: the denied effect category, the specific system call or resource that was blocked, the source module whose manifest was used, and a suggested fix (e.g., "add `@effect(net)` to function `foo`")
2. THE daemon SHALL support a `--sandbox-verbose` flag that logs all sandbox policy decisions (grants and denials) for debugging
3. WHEN running in verbose mode, THE daemon SHALL log the full generated `srt-settings.json` content before each execution
4. THE sandbox diagnostics SHALL use the same Structured Diagnostic Frame format (rule_id, ast_selector, context_hash) as compiler diagnostics for consistency
5. THE daemon SHALL emit a summary of sandbox statistics (total executions, denials, policy generation time) when shutting down gracefully

---

> **Note:** Requirement 18 below was added to address the MicroVM (Tier 3) hardware isolation gap identified in RUNTIME_REQUIREMENTS.md §4.

### Requirement 18: MicroVM Provider (Firecracker — Tier 3 Hardware Isolation)

**User Story:** As a platform engineer deploying Meld in multi-tenant or zero-trust environments, I want a hardware-level isolation tier using MicroVMs, so that AI-generated code is physically isolated from the host kernel and other tenants, protecting against kernel-level exploits that process-level sandboxing cannot prevent.

#### Acceptance Criteria

1. THE `FirecrackerProvider` SHALL implement the `SandboxProvider` interface (Req 10), providing hardware-level isolation as an alternative to the `MeldSRTProvider` (process-level)
2. THE `FirecrackerProvider` SHALL use Firecracker or Cloud Hypervisor as the Virtual Machine Monitor (VMM) to boot a minimal guest kernel for each execution
3. THE `FirecrackerProvider` SHALL translate Meld `Effect` values into guest kernel boot parameters or virtio-device restrictions (e.g., no `virtio-net` if `Effect::Network` is absent from `allowed_effects`)
4. THE `FirecrackerProvider` SHALL use VSOCK for guest-host communication, streaming terminal output (`stdout`/`stderr`) and file access without exposing the host network or filesystem to the guest unless explicitly permitted by the effect policy
5. THE `FirecrackerProvider` SHALL boot a minimal "Meld-Base" rootfs image (target size 5–10MB) containing only the Meld runtime and standard C++ libraries, optimized for sub-150ms boot times
6. THE `FirecrackerProvider` SHALL provide a perfectly clean rootfs for each execution, ensuring deterministic global system state (including `/dev` and `/sys`)
7. THE `SandboxProvider::create_default()` factory function (Req 10.5) SHALL select the provider based on the `[execution]` section in `meld.toml`: `isolation = "process"` (default) selects `MeldSRTProvider`, `isolation = "microvm"` selects `FirecrackerProvider`
8. THE `meld.toml` `[execution]` section SHALL support the following fields: `isolation` (enum: `"process"` or `"microvm"`, default `"process"`), `runtime` (string: `"firecracker"` or `"cloud-hypervisor"`, default `"firecracker"`), and `rootfs` (optional path to a custom rootfs image)
9. WHEN the configured VMM binary is not found on the system, THE `FirecrackerProvider` SHALL emit a structured diagnostic explaining how to install it and fall back to the `MeldSRTProvider` with a prominent warning
10. THE `FirecrackerProvider` SHALL return a `SandboxResult` with `sandbox_failed = true` if the VMM fails to boot or the guest kernel panics, distinguishing VMM-level failures from application-level failures

> **Cross-reference:** The full host-side orchestration for Firecracker MicroVMs (VM lifecycle, TAP provisioning, VSOCK streaming, Lima bridge) is specified in `.kiro/specs/meld-supervisor/requirements.md`. The guest-side PID 1 binary (`meldi`) is specified in `.kiro/specs/meld-init/requirements.md`. This requirement defines the `SandboxProvider` interface implementation; the supervisor and init specs define the operational binaries.

---

> **Note:** Requirement 19 below was added to address the Tier 0 (JIT/Wasm) sandbox for AI agent scripts from UPDATES-2.md.

### Requirement 19: Tier 0 Sandbox Provider (JIT/Wasm — Agent Script Isolation)

**User Story:** As a runtime developer, I want a lightweight in-process sandbox provider for ephemeral agent scripts, so that the daemon can execute `execute_script` MCP tool invocations with memory and time isolation without the overhead of OS-level sandboxing or MicroVM boot.

#### Acceptance Criteria

1. THE `Tier0Provider` SHALL implement a sandbox interface compatible with the `SandboxProvider` pattern (Req 10) but optimized for in-process JIT execution rather than OS-level process isolation
2. THE `Tier0Provider` SHALL NOT use SRT, Seatbelt, Bubblewrap, or any OS-level sandboxing mechanism — isolation is achieved through JIT/Wasm memory sandboxing and the absence of injected I/O capabilities
3. THE `Tier0Provider` SHALL provide the sandboxed script with NO filesystem access, NO network access, and NO raw system calls — the only I/O available is through RPC bindings injected by the daemon
4. THE `Tier0Provider` SHALL enforce memory limits per script execution (configurable, default 16MB) by constraining the JIT/Wasm linear memory allocation
5. THE `Tier0Provider` SHALL enforce execution time limits per script (configurable, default 5000ms) by monitoring elapsed time and terminating the JIT/Wasm execution if exceeded
6. THE `Tier0Provider` SHALL achieve sandbox creation and teardown in under 1ms, enabling the daemon to handle thousands of ephemeral script executions per second
7. THE `Tier0Provider` SHALL support two backend implementations: LLVM ORC JIT (compiles Meld to native code in-memory) and WebAssembly via Wasmtime (compiles Meld to Wasm bytecode), selectable via `meld.toml` `[daemon.tier0]` section (`backend = "jit"` or `backend = "wasm"`, default `"jit"`)
8. THE `SandboxProvider::create_default()` factory function (Req 10.5) SHALL NOT select the `Tier0Provider` — it is used exclusively by the daemon's `Tier0Sandbox` component (`.kiro/specs/meld-daemon/requirements.md` Req 13) and is not a general-purpose execution provider
9. THE `Tier0Provider` SHALL be stateless per invocation — each script execution creates a fresh sandbox with no shared state from previous executions

> **Cross-reference:** The daemon's `Tier0Sandbox` component that uses this provider is defined in `.kiro/specs/meld-daemon/requirements.md` Req 13. The `execute_script` MCP tool is defined in `.kiro/specs/meld-mcp-server/requirements.md` Req 15. The JIT compilation path is defined in `.kiro/specs/meld-compiler/requirements.md`.


---

> **Note:** Requirement 20 below was added to address the Finch container isolation backend from UPDATES-3.md, providing OCI container support alongside the existing MicroVM and SRT providers.

### Requirement 20: Finch Container Provider (OCI — Tier 3 Container Isolation)

**User Story:** As a platform engineer deploying Meld in Kubernetes or container-native environments, I want a container-based isolation tier using Finch/containerd, so that Meld binaries can be executed inside OCI containers with the same `meldi` PID 1 lifecycle and effect enforcement as MicroVMs, enabling Kubernetes-compatible deployments.

#### Acceptance Criteria

1. THE `FinchProvider` SHALL implement the `SandboxProvider` interface (Req 10), providing container-level isolation as an alternative to the `MeldSRTProvider` (process-level) and `FirecrackerProvider` (hardware-level)
2. THE `FinchProvider` SHALL launch OCI containers via `finch run` (macOS) or `nerdctl run` (Linux) with `meldi` as the container entrypoint (`--entrypoint /meldi`)
3. THE `FinchProvider` SHALL mount the compiled application binary read-only into the container (`-v ./app.bin:/app.bin:ro`) and the `meldi` binary read-only (`-v ~/.meld/rt/meldi:/meldi:ro`)
4. THE `FinchProvider` SHALL apply container security options: `--read-only` filesystem and `--security-opt no-new-privileges`
5. THE `FinchProvider` SHALL create a Unix Domain Socket on the host (e.g., `/tmp/meld/app_<id>.sock`) and mount it into the container at `/run/meld/control.sock` for the control plane between `melds` and `meldi`
6. THE `FinchProvider` SHALL translate Meld `Effect` values into container capabilities: no `--network` flag if `Effect::Network` is absent, volume mounts for `Effect::FileSystemRead`/`Effect::FileSystemWrite` with appropriate read-only or read-write permissions
7. THE `FinchProvider` SHALL use the base image specified by the `base_image` setting in `meld.toml` `[isolation]` section (Req 21): `alpine:latest` for the Alpine base or `scratch` for the empty void
8. ON macOS, THE `FinchProvider` SHALL route container execution through the Lima VM (`meld-vm`) where containerd is running
9. ON Linux, THE `FinchProvider` SHALL invoke `nerdctl` directly against the host's containerd instance, bypassing Lima entirely
10. THE `FinchProvider` SHALL return a `SandboxResult` with `sandbox_failed = true` if the container fails to start or `meldi` fails to connect to the UDS within the timeout, distinguishing container-level failures from application-level failures
11. THE `SandboxProvider::create_default()` factory function (Req 10.5) SHALL select the `FinchProvider` when `meld.toml` `[isolation]` section has `local = "finch"` or `production = "finch"`

> **Cross-reference:** The full host-side orchestration for Finch containers is specified in `.kiro/specs/meld-supervisor/requirements.md` Req 13 (Finch Container Provider) and Req 14 (Container UDS Control Plane). The guest-side PID 1 binary (`meldi`) with UDS transport detection is specified in `.kiro/specs/meld-init/requirements.md` Req 5. The `[isolation]` configuration block is defined in `.kiro/specs/meld-supervisor/requirements.md` Req 11.

---

> **Note:** Requirement 21 below was added to address the `base_image` manifest switch from UPDATES-3.md, allowing developers to choose between Alpine (debugging) and scratch (zero-trust) base images.

### Requirement 21: Base Image Configuration

**User Story:** As a platform engineer, I want to configure whether the sandbox guest environment uses an Alpine base image (with shell and debugging tools) or a scratch base image (empty void for maximum security), so that I can trade off debuggability against attack surface depending on the deployment environment.

#### Acceptance Criteria

1. THE `meld.toml` `[isolation]` section SHALL support a `base_image` field accepting values `"alpine"` or `"scratch"` (default: `"alpine"`)
2. WHEN `base_image` is set to `"alpine"`, THE `FinchProvider` (Req 20) SHALL use the `alpine:latest` OCI image, providing a standard Linux environment with busybox, shell (`/bin/sh`), and musl libc for debugging and dynamic linking
3. WHEN `base_image` is set to `"scratch"`, THE `FinchProvider` SHALL use the `scratch` OCI image (0 bytes), providing an empty void with no shell, no utilities, and no dynamic linker — the binary must be 100% statically linked
4. WHEN `base_image` is set to `"alpine"` for the `FirecrackerProvider` (Req 18), THE provider SHALL use the standard Alpine-based `alpine-rootfs.ext4` block device
5. WHEN `base_image` is set to `"scratch"` for the `FirecrackerProvider`, THE provider SHALL use a minimal ext4 block device containing only `meldi` and no Alpine userland
6. THE `SandboxConfig` struct (Req 10.3) SHALL be extended with a `base_image` field (enum: `Alpine`, `Scratch`) so that providers can read the configured base image
7. WHEN `base_image` is set to `"scratch"`, THE `SandboxProvider` SHALL verify that the target binary has no dynamic library dependencies before launching the sandbox, and emit a structured error if dynamic dependencies are detected

> **Cross-reference:** The `[isolation]` configuration block is defined in `.kiro/specs/meld-supervisor/requirements.md` Req 11 and Req 15 (Base Image Manifest Switch). The `SandboxConfig` struct is defined in Req 10.3 of this spec.
