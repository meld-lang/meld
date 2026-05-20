# meld-manifest

Meld Manifest & Binary Security package — provides the `.meld` manifest sidecar,
`.note.meld` binary tombstone, cryptographic signing (Sigstore + traditional),
verification modes, the `SandboxProvider` abstract interface, SRT integration,
ephemeral policy lifecycle, build-time isolation, and MCP tool hardening.

## Components

- **Manifest** — Binary-serialized sidecar with symbol→effect map and code_hash
- **Tombstone** — Non-loadable ELF/Mach-O section linking binary to manifest+signature
- **BridgeRule** — FFI effect enforcement for `@extern("C")` functions
- **EffectElision** — Whole-binary optimization stripping unreachable effects
- **SigningEngine** — Traditional key-based and Sigstore keyless signing
- **Verifier** — Offline and online verification modes
- **SandboxProvider** — Abstract interface for OS-level sandbox enforcement
- **MeldSRTProvider** — SRT backend mapping effects to sandbox policies
- **PolicyLifecycle** — Ephemeral srt-settings.json generation and cleanup
- **BuildSandbox** — Zero-trust build-time isolation for third-party deps
- **McpSandbox** — MCP tool hardening via effect-derived sandboxing
- **SandboxDiagnostics** — Structured diagnostics for sandbox denials

## Usage

```
meld-sign <binary> <manifest> [--key <path> | --sigstore]
```
