# Implementation Plan: Meld Manifest & Binary Security

## Overview

Implements the `.meld` manifest sidecar, `.note.meld` binary tombstone, cryptographic signing (Sigstore + traditional), verification modes, the `SandboxProvider` abstract interface, SRT integration, ephemeral policy lifecycle, build-time isolation, and MCP tool hardening. The implementation follows the dependency order: Manifest Format → Tombstone → Bridge Rule → Effect Elision → Signing → Verification → SandboxProvider → SRT Integration → Policy Lifecycle → Build-Time Isolation → MCP Hardening → Diagnostics.

## Tasks

- [x] 1. Set up project structure and build configuration
  - [x] 1.1 Create directory structure and Bazel BUILD files
    - Create `meld-manifest/include/meld/manifest/` and `meld-manifest/src/` directories
    - Create `meld-manifest/tests/` directory
    - Write `meld-manifest/BUILD.bazel` with deps on `//meld-core:api`, `//shared:common`, `@nlohmann_json//:json`, `@googletest//:gtest`, `@rapidcheck//:rapidcheck`
    - Create `meld-manifest/README.md` with package overview
    - _Requirements: 1.1_

  - [x] 1.2 Create Effect enum and core types (`core_types.hpp`)
    - Define `Effect` enum: `Network`, `FileSystemRead`, `FileSystemWrite`, `ProcessExec`, `SystemTime`, `State`
    - Define `EffectBitmask` as compact bitfield encoding
    - Define `ResourceBounds` struct: `allowed_domains`, `read_paths`, `write_paths`
    - Define `SymbolEffectEntry` struct: symbol name → `EffectBitmask` + `ResourceBounds`
    - _Requirements: 1.2, 10.2_

  - [x] 1.3 Write unit tests for core types
    - Test Effect enum values and bitmask operations
    - Test ResourceBounds construction and serialization
    - _Requirements: 1.2, 10.2_


- [x] 2. Implement Manifest Format and Content (Req 1)
  - [x] 2.1 Create Manifest class (`manifest.hpp` / `manifest.cpp`)
    - Define `Manifest` struct: format version, project coordinate (name, version), `code_hash` (SHA-256), map of `SymbolEffectEntry`
    - Implement binary serialization (`serialize() -> std::vector<uint8_t>`) and deserialization (`deserialize(span<uint8_t>) -> Manifest`)
    - Implement `compute_code_hash(binary_path) -> SHA256Hash` for executable segment hashing
    - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5_

  - [x] 2.2 Implement manifest emission in AOT pipeline integration point
    - Define `ManifestEmitter` that accepts compiled symbol table + effect analysis results
    - Emit `.meld` sidecar file co-located with the binary
    - _Requirements: 1.1, 1.6_

  - [x] 2.3 Write property test for manifest round-trip
    - **Property 1: Manifest Round-Trip** — For any valid Manifest, serializing then deserializing SHALL produce an equivalent Manifest
    - **Validates: Requirements 1.2, 1.4**

  - [x] 2.4 Write unit tests for Manifest
    - Test serialization/deserialization of all field types
    - Test code_hash computation against known binary
    - Test format version field presence
    - _Requirements: 1.2, 1.3, 1.4, 1.7_

- [x] 3. Implement Tombstone Binary Section (Req 2)
  - [x] 3.1 Create Tombstone class (`tombstone.hpp` / `tombstone.cpp`)
    - Define `Tombstone` struct: `manifest_hash` (SHA-256), `signature_blob`, `EffectID`
    - Implement `embed_tombstone(binary_path, Tombstone)` for ELF `.note.meld` section writing
    - Implement `embed_tombstone_macho(binary_path, Tombstone)` for Mach-O section writing
    - Implement `read_tombstone(binary_path) -> Tombstone` for extraction
    - _Requirements: 2.1, 2.2, 2.3_

  - [x] 3.2 Implement mutual authentication hash chain
    - `manifest_hash` = SHA-256 of manifest file content
    - `code_hash` in manifest = SHA-256 of binary executable segments
    - Verify: modifying binary breaks code_hash; modifying manifest breaks manifest_hash; modifying both breaks signature
    - _Requirements: 2.4, 2.5_

  - [x] 3.3 Write unit tests for Tombstone
    - Test ELF section embedding and extraction round-trip
    - Test Mach-O section embedding and extraction round-trip
    - Test mutual authentication (tamper detection)
    - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5_

- [x] 4. Checkpoint — Manifest and Tombstone
  - Ensure all tests pass, ask the user if questions arise.

- [x] 5. Implement C/C++ Interop Bridge Rule (Req 3)
  - [x] 5.1 Create BridgeRule checker (`bridge_rule.hpp` / `bridge_rule.cpp`)
    - Detect `@extern("C")` or equivalent FFI annotations in AST
    - Require explicit `@effect` annotation on FFI functions
    - Default untagged FFI to "Full IO" with warning
    - Include FFI functions in manifest symbol-to-effect map
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5_

  - [x] 5.2 Write unit tests for BridgeRule
    - Test tagged FFI function accepted with declared effects
    - Test untagged FFI defaults to Full IO with warning
    - Test manifest includes FFI entries
    - _Requirements: 3.1, 3.2, 3.3, 3.4_

- [x] 6. Implement Global Effect Elision (Req 4)
  - [x] 6.1 Create EffectElision pass (`effect_elision.hpp` / `effect_elision.cpp`)
    - Perform whole-binary call-graph analysis from entry points
    - Identify effects never reachable from any entry point
    - Remove unreachable effects from manifest
    - Only run when `--compilation_mode=opt` (release builds)
    - Re-hash elided manifest and update Tombstone
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_

  - [x] 6.2 Write unit tests for EffectElision
    - Test unreachable effect removal
    - Test that debug builds preserve full effect map
    - Test manifest re-hashing after elision
    - _Requirements: 4.1, 4.2, 4.3, 4.4_

- [x] 7. Checkpoint — Bridge rule and effect elision
  - Ensure all tests pass, ask the user if questions arise.

- [x] 8. Implement Cryptographic Signing (Req 5, 6, 8)
  - [x] 8.1 Create SigningEngine (`signing.hpp` / `signing.cpp`)
    - Implement `sign_manifest(manifest_bytes, private_key) -> SignatureBlob` for traditional key-based signing
    - Implement linked hash architecture: manifest contains code_hash, tombstone contains manifest_hash + signature
    - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5_

  - [x] 8.2 Implement Sigstore integration (`sigstore.hpp` / `sigstore.cpp`)
    - Implement OIDC identity-based keyless signing via Fulcio
    - Generate ephemeral signing keys in-memory, destroy after Rekor logging
    - Produce Sigstore Bundle (signature + Fulcio certificate + Rekor SET)
    - Embed bundle in Tombstone's `signature_blob`
    - _Requirements: 6.1, 6.2, 6.3, 6.4_

  - [x] 8.3 Create `meld-sign` CLI utility (`meld_sign_main.cpp`)
    - Accept binary path and manifest path as inputs
    - Support `--key <path>` for traditional signing
    - Support `--sigstore` for keyless OIDC signing
    - Validate manifest code_hash matches binary before signing
    - Emit structured error on inconsistency
    - _Requirements: 8.1, 8.2, 8.3, 8.5, 8.6_

  - [x] 8.4 Write unit tests for SigningEngine
    - Test traditional key-based sign/verify round-trip
    - Test linked hash chain integrity (tamper any part → verification fails)
    - Test meld-sign CLI argument parsing
    - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 8.1, 8.5_

- [x] 9. Implement Verification Modes (Req 7)
  - [x] 9.1 Create Verifier (`verifier.hpp` / `verifier.cpp`)
    - Implement offline verification: check Sigstore Bundle SET against bundled trust root (no network)
    - Implement online audit mode: re-query Rekor transparency log for revocation
    - On failure: refuse execution, emit structured audit log
    - Read verification mode from `meld.toml` or daemon config
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5_

  - [x] 9.2 Write unit tests for Verifier
    - Test offline verification with valid bundle
    - Test offline verification with tampered bundle
    - Test verification mode configuration
    - _Requirements: 7.1, 7.2, 7.4, 7.5_

- [x] 10. Implement Hardware-Level Integrity (Req 9, optional)
  - [x] 10.1 Create IMA integration (`ima_integration.hpp` / `ima_integration.cpp`)
    - On Linux, optionally register binary hashes with IMA/TPM
    - Opt-in via daemon settings, not enabled by default
    - Support emitting IMA-compatible hash records alongside Tombstone
    - Fall back to userspace verification on unsupported platforms
    - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5_

  - [x] 10.2 Write unit tests for IMA integration
    - Test opt-in configuration
    - Test fallback to userspace verification
    - _Requirements: 9.3, 9.5_

- [x] 11. Checkpoint — Signing, verification, and integrity
  - Ensure all tests pass, ask the user if questions arise.


- [x] 12. Implement SandboxProvider Abstract Interface (Req 10)
  - [x] 12.1 Create SandboxProvider interface (`sandbox_provider.hpp`)
    - Define abstract `SandboxProvider` with pure virtual `launch(SandboxConfig, command) -> SandboxResult`
    - Define `SandboxConfig` struct: `allowed_effects`, `read_only_paths`, `read_write_paths`, `allowed_domains`, `working_directory`
    - Define `SandboxResult` struct: exit code, stdout, stderr, sandbox_failed boolean
    - Implement `create_default()` factory (SRT on macOS/Linux, passthrough fallback)
    - Interface is stateless — each `launch` is independent
    - _Requirements: 10.1, 10.3, 10.4, 10.5, 10.6_

  - [x] 12.2 Write unit tests for SandboxProvider interface
    - Test SandboxConfig construction
    - Test SandboxResult field access
    - Test create_default() returns appropriate provider for host OS
    - _Requirements: 10.1, 10.3, 10.4, 10.5_

- [x] 13. Implement Effect-to-SRT Policy Mapping (Req 11)
  - [x] 13.1 Create MeldSRTProvider (`srt_provider.hpp` / `srt_provider.cpp`)
    - Implement `SandboxProvider` for SRT backend
    - Map `Effect::Network` → `"network": { "mode": "allow", "allowedDomains": [...] }`
    - Map `Effect::FileSystemWrite` → `"filesystem": { "readWrite": [...] }`
    - Map `Effect::FileSystemRead` → `"filesystem": { "readOnly": [...] }`
    - Map `Effect::ProcessExec` → SRT process execution policy
    - Deny-by-default for effects not in `allowed_effects`
    - Always grant read-only access to Meld stdlib and runtime paths
    - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5, 11.6_

  - [x] 13.2 Write property test for effect mapping completeness
    - **Property 2: Effect Mapping Completeness** — For any `SandboxConfig`, every `Effect` in `allowed_effects` SHALL produce a corresponding non-empty SRT policy section, and every `Effect` NOT in `allowed_effects` SHALL produce a deny entry
    - **Validates: Requirements 11.1, 11.2, 11.3, 11.4, 11.5**

  - [x] 13.3 Write unit tests for MeldSRTProvider
    - Test each Effect → SRT policy mapping
    - Test deny-by-default behavior
    - Test stdlib read-only access always granted
    - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5, 11.6_

- [x] 14. Implement Ephemeral Policy Lifecycle (Req 12)
  - [x] 14.1 Create PolicyLifecycleManager (`policy_lifecycle.hpp` / `policy_lifecycle.cpp`)
    - Generate fresh `srt-settings.json` with UUID-based filename in temp directory
    - Derive policy from target binary's `.meld` manifest effect map
    - Create file with restrictive permissions (owner-read-only)
    - Pass policy path to SRT CLI as command-line argument
    - Delete policy file after sandboxed process exits
    - Clean up stale files from previous sessions on startup
    - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6_

  - [x] 14.2 Write unit tests for PolicyLifecycleManager
    - Test unique filename generation
    - Test file permissions (owner-read-only)
    - Test cleanup after process exit
    - Test stale file detection and cleanup
    - _Requirements: 12.1, 12.4, 12.5, 12.6_

- [x] 15. Implement SRT CLI Integration (Req 13)
  - [x] 15.1 Implement SRT CLI invocation (`srt_provider.cpp` additions)
    - Invoke SRT CLI binary (from `$PATH` or `meld.toml` config)
    - Use Seatbelt on macOS, Bubblewrap on Linux (delegated through SRT)
    - If SRT not found: emit diagnostic with install instructions, fall back to unsandboxed with warning
    - Capture SRT exit code and stderr to distinguish sandbox vs application failures
    - Support `--sandbox=off` flag to disable sandboxing for debugging
    - _Requirements: 13.1, 13.2, 13.3, 13.4, 13.5_

  - [x] 15.2 Write unit tests for SRT CLI integration
    - Test SRT CLI invocation with valid policy
    - Test fallback when SRT not found
    - Test sandbox-off flag
    - Test exit code distinction (sandbox failure vs app failure)
    - _Requirements: 13.1, 13.3, 13.4, 13.5_

- [x] 16. Checkpoint — Sandbox provider, SRT mapping, policy lifecycle
  - Ensure all tests pass, ask the user if questions arise.

- [x] 17. Implement Build-Time Isolation (Req 14)
  - [x] 17.1 Create build-time sandbox configuration (`build_sandbox.hpp` / `build_sandbox.cpp`)
    - Third-party targets: zero network, read-only source tree, read-write only to `bazel-out/`, deny ProcessExec
    - First-party targets: relaxed sandbox (filesystem reads within workspace, deny network)
    - Expose as Bazel `exec_properties` attribute on `meld_library` / `meld_binary` rules
    - Allow per-target override
    - Emit structured error when sandbox blocks legitimate operation
    - _Requirements: 14.1, 14.2, 14.3, 14.4, 14.5, 14.6_

  - [x] 17.2 Write unit tests for build-time sandbox
    - Test third-party sandbox config (no network, read-only source)
    - Test first-party sandbox config (relaxed)
    - Test per-target override
    - _Requirements: 14.1, 14.2, 14.3, 14.4, 14.5_

- [x] 18. Implement MCP Tool Hardening (Req 15)
  - [x] 18.1 Create MCP sandbox wrapper (`mcp_sandbox.hpp` / `mcp_sandbox.cpp`)
    - Wrap code-executing MCP tools in SRT sandbox derived from target module's effect manifest
    - Policy at least as restrictive as module's declared effects
    - For multi-module operations: use intersection (most restrictive) of effect sets
    - Reject "disable sandbox" unless `MELD_UNSAFE_NO_SANDBOX=1` env var set before daemon startup
    - Log all sandboxed executions (tool name, target module, effect set, outcome)
    - On sandbox denial: return structured error explaining denied capability and suggesting `@effect` annotation
    - _Requirements: 15.1, 15.2, 15.3, 15.4, 15.5, 15.6_

  - [x] 18.2 Write unit tests for MCP sandbox wrapper
    - Test sandbox policy derivation from effect manifest
    - Test intersection policy for multi-module operations
    - Test rejection of disable-sandbox without env var
    - Test audit logging
    - _Requirements: 15.1, 15.2, 15.3, 15.4, 15.5_

- [x] 19. Implement Sandbox Diagnostics and Observability (Req 16)
  - [x] 19.1 Create SandboxDiagnostics (`sandbox_diagnostics.hpp` / `sandbox_diagnostics.cpp`)
    - On denial: emit structured diagnostic with denied effect, blocked syscall/resource, source module, suggested fix
    - Support `--sandbox-verbose` flag logging all policy decisions (grants and denials)
    - In verbose mode: log full `srt-settings.json` content before each execution
    - Use Structured Diagnostic Frame format (rule_id, ast_selector, context_hash)
    - Emit sandbox statistics summary on graceful shutdown
    - _Requirements: 16.1, 16.2, 16.3, 16.4, 16.5_

  - [x] 19.2 Write unit tests for SandboxDiagnostics
    - Test structured diagnostic format on denial
    - Test verbose mode logging
    - Test SDF format compliance
    - Test statistics summary
    - _Requirements: 16.1, 16.2, 16.3, 16.4, 16.5_

- [x] 20. Checkpoint — Build isolation, MCP hardening, diagnostics
  - Ensure all tests pass, ask the user if questions arise.

- [x] 21. Integration tests
  - [x] 21.1 Write end-to-end manifest/tombstone/signing pipeline test
    - Compile binary → emit manifest → sign → embed tombstone → verify → pass
    - _Requirements: 1.1, 2.1, 5.1, 7.1_

  - [x] 21.2 Write tamper detection integration test
    - Sign binary → tamper binary → verify → fail with correct diagnostic
    - Sign binary → tamper manifest → verify → fail with correct diagnostic
    - _Requirements: 2.4, 5.3, 5.4, 7.4_

  - [x] 21.3 Write sandbox enforcement integration test
    - Create module with `@uses(file_system)` → sandbox allows filesystem → pass
    - Create module with no effects → sandbox denies network → structured error
    - _Requirements: 10.5, 11.5, 16.1_

- [x] 22. Final Checkpoint — Full manifest and security integration
  - Ensure all tests pass, ask the user if questions arise.

- [x] 23. Implement Debug Sidecar Format (Req 17)
  - [x] 23.1 Create MdebugSidecar class (`mdebug_sidecar.hpp` / `mdebug_sidecar.cpp`)
    - Define `MdebugSidecar` struct with: format version, `debug_id`, DWARF section (byte buffer), AST-to-PC index, Ownership State Trace table
    - Define `AstToPcEntry` struct: PC address range (start, end), `ast_selector` (string)
    - Define `OwnershipTraceEntry` struct: instruction offset, reference ID, strong count status, weak count status, lifecycle state (valid/moved/potentially-dangling)
    - Implement `serialize() -> std::vector<uint8_t>` with zstd compression
    - Implement `deserialize(span<uint8_t>) -> MdebugSidecar` with zstd decompression and header validation
    - _Requirements: 17.2, 17.4, 17.5, 17.6, 17.7_

  - [x] 23.2 Implement `debug_id` computation and verification
    - Implement `compute_debug_id(binary_path) -> DebugId` as content-addressable hash of binary's code segments
    - Verify that sidecar's header `debug_id` matches a given `debug_id` for integrity checking
    - Ensure identical builds produce the same `debug_id`
    - _Requirements: 17.6_

  - [x] 23.3 Write property test for MdebugSidecar round-trip
    - **Property 3: MdebugSidecar Round-Trip** — For any valid MdebugSidecar, serializing (with zstd compression) then deserializing SHALL produce an equivalent MdebugSidecar
    - **Validates: Requirements 17.2, 17.7**

  - [x] 23.4 Write unit tests for MdebugSidecar
    - Test serialization/deserialization round-trip with all sections populated
    - Test `debug_id` computation is deterministic for identical binaries
    - Test format version field presence and validation
    - Test zstd compression/decompression
    - Test sidecar is independent of `.meld` manifest (separate concerns)
    - _Requirements: 17.2, 17.4, 17.5, 17.6, 17.7, 17.9_

- [x] 24. Checkpoint — Debug Sidecar Format
  - Ensure MdebugSidecar round-trip works correctly
  - Ensure `debug_id` is deterministic
  - Ask the user if questions arise.

- [x] 25. Update Signing to Combined Integrity Hash (Req 5 update)
  - [x] 25.1 Implement `compute_combined_hash` function
    - Implement `compute_combined_hash(code_hash, manifest_hash, debug_id) -> SHA256Hash` that produces the Combined Integrity Hash
    - Update `SigningEngine::sign` to sign the combined hash instead of raw manifest bytes
    - Update `Verifier::verify` to recompute the combined hash and verify against the signature
    - _Requirements: 5.1, 5.2, 5.6_

  - [x] 25.2 Update existing signing tests for combined hash
    - Update property test (Property 4) to verify that modifying `debug_id` also breaks verification
    - Update tamper detection tests to cover debug_id tampering
    - _Requirements: 5.3, 5.4, 5.5, 5.6_

- [x] 26. Implement `meldn` Notary CLI (Req 8 update)
  - [ ] 26.1 Rename `meld-sign` binary to `meldn` and implement three-verb CLI
    - Rename `meld_sign_main.cpp` → `meldn_main.cpp` and update BUILD.bazel target name
    - Implement `meldn sign` command (existing sign logic, now using combined hash)
    - Implement `meldn verify` command: read Tombstone + manifest, verify combined hash, report pass/fail with details
    - Implement `meldn bundle` command: fetch Rekor SETs and Fulcio certificates, embed as self-contained Sigstore Bundle in Tombstone for air-gapped use
    - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8_

  - [ ] 26.2 Add configurable Sigstore endpoints
    - Support `--fulcio-url` and `--rekor-url` CLI flags
    - Support `meld.toml` `[signing]` section with `fulcio_url` and `rekor_url` fields
    - Update `SigstoreClient` to accept configurable endpoints
    - _Requirements: 6.6, 8.10_

  - [ ] 26.3 Update Bazel integration for `meldn`
    - Update `meld_signed_binary` Bazel rule to invoke `meldn sign` instead of `meld-sign`
    - _Requirements: 8.9_

  - [ ] 26.4 Write unit tests for `meldn` CLI
    - Test `meldn sign` with combined hash
    - Test `meldn verify` pass and fail cases
    - Test `meldn bundle` produces self-contained Sigstore Bundle
    - Test configurable Sigstore endpoints
    - _Requirements: 8.1, 8.5, 8.6, 8.7, 8.8, 8.10_

- [x] 27. Checkpoint — Combined hash and meldn CLI
  - Ensure combined hash signing/verification works
  - Ensure meldn sign/verify/bundle commands work
  - Ask the user if questions arise.

- [x] 28. Implement FirecrackerProvider (Req 18)
  - [ ] 28.1 Create `ExecutionConfig` and `meld.toml` `[execution]` parser
    - Define `ExecutionConfig` struct: `isolation` (process/microvm), `runtime` (firecracker/cloud-hypervisor), `rootfs` (optional path)
    - Parse `[execution]` section from `meld.toml`
    - Update `SandboxProvider::create_default()` to read `ExecutionConfig` and select provider accordingly
    - _Requirements: 18.7, 18.8_

  - [ ] 28.2 Implement `FirecrackerProvider` core (`firecracker_provider.hpp` / `firecracker_provider.cpp`)
    - Implement `SandboxProvider` interface for Firecracker VMM
    - Translate Meld `Effect` values to VM boot parameters and virtio-device restrictions
    - Configure `virtio-net` only when `Effect::Network` is in `allowed_effects`
    - Configure `virtio-blk` with read-only/read-write mounts from `SandboxConfig`
    - Locate VMM binary on `$PATH` or from `ExecutionConfig.runtime`
    - _Requirements: 18.1, 18.2, 18.3_

  - [ ] 28.3 Implement VSOCK guest-host communication bridge
    - Set up VSOCK for guest-host terminal streaming (stdout/stderr)
    - Stream file access requests over VSOCK without exposing host network/filesystem
    - Collect `SandboxResult` from guest exit code and VSOCK output
    - _Requirements: 18.4_

  - [ ] 28.4 Implement minimal "Meld-Base" rootfs generation
    - Define rootfs contents: Meld runtime + standard C++ libraries only
    - Target size: 5–10MB
    - Target boot time: sub-150ms
    - Provide clean rootfs per execution (deterministic `/dev`, `/sys`)
    - _Requirements: 18.5, 18.6_

  - [ ] 28.5 Implement fallback and error handling
    - If VMM binary not found: emit structured diagnostic, fall back to `MeldSRTProvider` with warning
    - Distinguish VMM-level failures (boot failure, kernel panic) from application failures via `sandbox_failed` flag
    - _Requirements: 18.9, 18.10_

  - [ ] 28.6 Write unit tests for FirecrackerProvider
    - Test `ExecutionConfig` parsing from `meld.toml`
    - Test `create_default()` provider selection based on `[execution]` config
    - Test effect-to-virtio mapping (no network effect → no virtio-net)
    - Test fallback when VMM not found
    - Test `sandbox_failed` flag on VMM boot failure
    - _Requirements: 18.1, 18.3, 18.7, 18.8, 18.9, 18.10_

- [x] 29. Add VFS mode to SandboxConfig
  - [ ] 29.1 Extend `SandboxConfig` with `vfs_mode` field
    - Add `bool vfs_mode = false` to `SandboxConfig`
    - When `vfs_mode` is true, `MeldSRTProvider` SHALL configure a tmpfs working directory and redirect writes to it
    - When `vfs_mode` is true, `FirecrackerProvider` SHALL boot the guest with a memory-only rootfs overlay
    - _Requirements: (supports meld-cli Req 22)_

  - [ ] 29.2 Write unit tests for VFS mode
    - Test SRT provider generates tmpfs mount when `vfs_mode = true`
    - Test Firecracker provider uses memory overlay when `vfs_mode = true`
    - _Requirements: (supports meld-cli Req 22)_

- [x] 30. Checkpoint — FirecrackerProvider and VFS mode
  - Ensure provider selection works based on meld.toml
  - Ensure FirecrackerProvider launches VMs correctly
  - Ensure VFS mode redirects writes to memory
  - Ask the user if questions arise.
