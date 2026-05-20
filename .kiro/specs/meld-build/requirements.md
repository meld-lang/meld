# Requirements Document

## Introduction

This specification defines the requirements for implementing Bazel build system support for the Meld programming language. The goal is to make Bazel the default build system for Meld projects, providing efficient, scalable, and reproducible builds with proper dependency management, incremental compilation, and cross-platform support.

This spec also incorporates the mono-repo workspace organization requirements (formerly `.kiro/specs/meld-monorepo/`, now retired). Requirements 12–18 cover transforming the workspace into a Bazel-based mono-repository with separate packages for meld-core, meld-lsp-server, meld-build, and meld-mcp-server.

## Glossary

- **Bazel**: Google's open-source build and test tool that supports multiple languages and platforms
- **BUILD File**: Bazel configuration file that defines build targets and their dependencies
- **Workspace**: Root directory of a Bazel project containing a WORKSPACE file
- **Target**: A buildable unit in Bazel (library, binary, test, etc.)
- **Rule**: A function that defines how to build a specific type of target
- **Meld Compiler**: The compiler that transforms Meld source code into executable binaries
- **Starlark**: The configuration language used in Bazel BUILD files
- **Toolchain**: A set of tools and configurations needed to build for a specific platform
- **Mono_Repo**: A single repository containing multiple related packages (meld-core, meld-lsp-server, meld-build, meld-mcp-server) with shared Bazel configuration
- **Package**: A self-contained unit of code with its own BUILD file and build configuration within the mono-repo

## Requirements

### Requirement 1

**User Story:** As a Meld developer, I want to build my Meld projects using Bazel, so that I can benefit from fast, incremental builds and proper dependency management.

#### Acceptance Criteria

1. WHEN a developer runs `bazel build` on a Meld target, THE Bazel Build System SHALL compile the Meld source files using the Meld compiler
2. WHEN Meld source files are modified, THE Bazel Build System SHALL perform incremental compilation of only the changed files and their dependents
3. WHEN building Meld libraries, THE Bazel Build System SHALL generate appropriate library artifacts that can be consumed by other Meld targets
4. WHEN building Meld binaries, THE Bazel Build System SHALL produce executable files for the target platform
5. WHERE cross-compilation is specified, THE Bazel Build System SHALL build Meld code for the target platform using appropriate toolchains

### Requirement 2

**User Story:** As a Meld developer, I want to define Meld build targets using Bazel rules, so that I can specify dependencies and build configurations declaratively.

#### Acceptance Criteria

1. WHEN defining a Meld library target, THE Bazel Build System SHALL accept source files, dependencies, and compilation flags as parameters
2. WHEN defining a Meld binary target, THE Bazel Build System SHALL accept source files, dependencies, and a main entry point
3. WHEN defining a Meld test target, THE Bazel Build System SHALL accept test source files and their dependencies
4. WHEN specifying inter-target dependencies, THE Bazel Build System SHALL ensure proper build ordering and dependency resolution
5. WHERE compilation flags are specified, THE Bazel Build System SHALL pass them to the Meld compiler during build

> **Cross-reference:** The `meld_dev_server` rule for ORC JIT hot-reload development is defined in `.kiro/specs/meld-compiler/requirements.md` Req 9.3. The full set of Bazel rules across both specs is: `meld_library`, `meld_binary`, `meld_test` (this spec), and `meld_dev_server` (meld-compile).

### Requirement 3

**User Story:** As a Meld developer, I want Bazel to automatically discover and manage Meld source file dependencies, so that builds are reliable and complete.

#### Acceptance Criteria

1. WHEN a Meld file imports another Meld module, THE Bazel Build System SHALL automatically include the imported module in the build dependency graph
2. WHEN Meld source files reference external libraries, THE Bazel Build System SHALL ensure those libraries are built before the dependent targets
3. WHEN circular dependencies are detected, THE Bazel Build System SHALL report clear error messages indicating the dependency cycle
4. WHEN dependency information changes, THE Bazel Build System SHALL invalidate affected build artifacts and rebuild as necessary
5. WHERE transitive dependencies exist, THE Bazel Build System SHALL resolve and build the complete dependency chain

### Requirement 4

**User Story:** As a Meld developer, I want to run Meld tests through Bazel, so that I can integrate testing into my build workflow.

#### Acceptance Criteria

1. WHEN running `bazel test` on Meld test targets, THE Bazel Build System SHALL compile and execute the test code
2. WHEN test execution completes, THE Bazel Build System SHALL report test results including pass/fail status and execution time
3. WHEN tests fail, THE Bazel Build System SHALL display detailed failure information and exit with appropriate error codes
4. WHEN test dependencies are specified, THE Bazel Build System SHALL ensure all dependencies are built before test execution
5. WHERE test data files are required, THE Bazel Build System SHALL make them available to the test runtime environment

### Requirement 5

**User Story:** As a Meld project maintainer, I want to configure Bazel workspace settings for Meld projects, so that the build system can locate tools and dependencies.

#### Acceptance Criteria

1. WHEN initializing a Meld project, THE Bazel Build System SHALL create a WORKSPACE file with Meld-specific configurations
2. WHEN the Meld compiler location is specified, THE Bazel Build System SHALL use that compiler for all Meld compilation tasks
3. WHEN external Meld libraries are declared, THE Bazel Build System SHALL download and make them available for import
4. WHEN toolchain configurations are defined, THE Bazel Build System SHALL use appropriate tools for different target platforms
5. WHERE repository rules are specified, THE Bazel Build System SHALL fetch external dependencies according to the rules

### Requirement 6

**User Story:** As a Meld developer, I want Bazel to support different build configurations for Meld code, so that I can optimize builds for development, testing, and production.

#### Acceptance Criteria

1. WHEN building in debug mode, THE Bazel Build System SHALL compile Meld code with debug symbols and reduced optimizations
2. WHEN building in release mode, THE Bazel Build System SHALL compile Meld code with full optimizations and minimal debug information
3. WHEN custom build configurations are defined, THE Bazel Build System SHALL apply the specified compiler flags and settings
4. WHEN platform-specific configurations are needed, THE Bazel Build System SHALL select appropriate settings based on the target platform
5. WHERE conditional compilation is required, THE Bazel Build System SHALL support preprocessor definitions and feature flags

### Requirement 7

**User Story:** As a Meld developer, I want Bazel to integrate with existing Meld tooling, so that I can use familiar development workflows.

#### Acceptance Criteria

1. WHEN generating IDE project files, THE Bazel Build System SHALL create configurations that work with Meld language servers and editors
2. WHEN using Meld formatting tools, THE Bazel Build System SHALL support running formatters as part of the build process
3. WHEN using Meld linting tools, THE Bazel Build System SHALL integrate static analysis into the build pipeline
4. WHEN debugging Meld applications, THE Bazel Build System SHALL preserve debug information and source mappings
5. WHERE custom tools are specified, THE Bazel Build System SHALL allow integration of additional Meld development tools

### Requirement 8

**User Story:** As a Meld developer, I want `rules_meld` to parse my `meld.toml` file and auto-generate build targets, so that I don't maintain duplicate dependency information in both `meld.toml` and BUILD files.

#### Acceptance Criteria

1. WHEN `bazel_integration = true` is set in `meld.toml`, THE `rules_meld` ruleset SHALL parse `meld.toml` during workspace evaluation
2. WHEN dependencies are declared in `meld.toml`, THE `rules_meld` ruleset SHALL auto-generate `meld_library` targets in memory for each dependency
3. WHEN a dependency specifies an `allow` array in `meld.toml`, THE `rules_meld` ruleset SHALL pipe those effect permissions to the Meld compiler via CLI flags (e.g., `--allow-effects=net,fs`)
4. WHEN a dependency has no `allow` array, THE `rules_meld` ruleset SHALL pass `--allow-effects=` (empty) to enforce pure-only compilation for that dependency
5. WHEN `meld.toml` changes, THE Bazel Build System SHALL invalidate affected targets and regenerate them on the next build

### Requirement 9

**User Story:** As a Meld developer, I want `meld.toml` to be the single source of truth for my project configuration, so that all build tools read from one canonical file.

#### Acceptance Criteria

1. WHEN the Meld compiler is invoked, IT SHALL read project metadata (name, version, entry point) from `meld.toml`
2. WHEN resolving dependencies, THE Meld compiler SHALL use the `[dependencies]` section of `meld.toml` for Git URL resolution and version pinning
3. WHEN enforcing the Effect Firewall, THE Meld compiler SHALL use the `allow` arrays from `meld.toml` to validate dependency effect usage
4. WHEN selecting compilation targets, THE Meld compiler SHALL use the `[targets]` section of `meld.toml`
5. WHEN `meld.toml` is absent, THE Meld compiler SHALL treat the project as a single-file script with no dependencies and default settings

### Requirement 10: Version-Aware Dependency Isolation

**User Story:** As a Meld developer, I want each unique dependency version to be a distinct Bazel repository with its own version hash, so that multiple versions of the same library can coexist safely in a single binary.

#### Acceptance Criteria

1. WHEN `meld.toml` declares multiple versions of the same dependency (via `[[overrides]]` or transitive resolution), THE `rules_meld` ruleset SHALL create a distinct Bazel repository for each version (e.g., `@com_google_cloud_http_v1`, `@com_google_cloud_http_v2`)
2. THE `MeldInfo` Bazel provider SHALL carry a `version_hash` field for each `meld_library` target, derived deterministically from the dependency's coordinate (group, artifact, version)
3. WHEN invoking the Meld compiler for a `meld_library` target, THE `rules_meld` ruleset SHALL pass the `version_hash` via a `--version-hash=<hash>` flag so the compiler can apply version-aware symbol mangling
4. THE full dependency closure SHALL be enforced — no implicit version overrides; if two targets require incompatible versions, the build SHALL fail with a clear diagnostic identifying the conflict
5. WHEN a dependency is private (transitive), THE `rules_meld` ruleset SHALL NOT expose its symbols to consumers unless the intermediate library explicitly re-exports them

> **Cross-reference:** The compiler's symbol mangling behavior is specified in `.kiro/specs/meld-compiler/requirements.md` Requirement 12.


### Requirement 11: Link-Time Effect Validation

**User Story:** As a Meld developer, I want the build system to verify effect declarations across library boundaries at link time, so that a dependency cannot introduce undeclared effects that bypass compile-time checks in separately compiled modules.

#### Acceptance Criteria

1. WHEN the Meld compiler compiles a `meld_library` target, IT SHALL emit an effect summary alongside the `.bc` output listing all effects the module performs (directly and transitively)
2. WHEN the `meld_binary` rule links multiple `meld_library` targets, THE linker phase SHALL merge the effect summaries from all transitive dependencies
3. IF a transitive dependency's effect summary includes an effect not declared in the consumer's `meld.toml` `allow` array for that dependency, THEN THE build SHALL fail with a structured error identifying the dependency, the undeclared effect, and the call chain that introduces it
4. THE effect summary format SHALL be a lightweight sidecar (e.g., JSON or binary map of `symbol → effect bitmask`) co-located with the `.bc` output
5. WHEN `--compilation_mode=opt` is set, THE linker phase SHALL perform the effect merge check before applying ThinLTO, so violations are caught before expensive optimization

> **Note:** This is a simplified version covering the build-time merge check. The full `.meld` manifest sidecar format (binary-serialized, signed, with resource bounds) will be specified when §3.1 of REQUIREMENTS.md is addressed. At that point, this requirement will be expanded to use the manifest as the authoritative effect summary rather than the lightweight sidecar.

> **Cross-reference:** Compile-time effect checking is specified in `.kiro/specs/meld-core/requirements.md` (Req 99–118) and `.kiro/specs/meld-compiler/requirements.md` Requirement 5.


---

> **Note:** Requirements 12–18 below were merged from the retired `.kiro/specs/meld-monorepo/` spec. They cover the Bazel mono-repo workspace organization.

### Requirement 12: Mono-Repo Package Organization

**User Story:** As a developer, I want the workspace organized as a mono-repo with separate packages, so that I can work on different components independently while maintaining shared dependencies.

#### Acceptance Criteria

1. WHEN the transformation is complete, THE Workspace SHALL contain the following packages: meld-core, meld-compile, meld-cli, meld-async, meld-test, meld-build, meld-lsp-server, meld-mcp-server, meld-daemon, meld-manifest, meld-port, and vscode-meld
2. WHEN a developer builds any package, THE Bazel Build System SHALL resolve dependencies automatically between packages
3. WHEN examining the workspace structure, THE Workspace SHALL have a clear separation between package-specific code and shared utilities
4. WHEN building the entire workspace, THE Bazel Build System SHALL build all packages in the correct dependency order
5. WHERE a package depends on another package, THE Bazel Build System SHALL enforce proper dependency declarations

### Requirement 13: Mono-Repo Bazel Configuration

**User Story:** As a developer, I want Bazel build configuration for all packages, so that I can have consistent, reproducible builds across all components.

#### Acceptance Criteria

1. WHEN building any package, THE Bazel Build System SHALL use hermetic builds with declared dependencies
2. WHEN the WORKSPACE file is processed, THE Bazel Build System SHALL configure all external dependencies (Boost, GoogleTest, etc.)
3. WHEN a BUILD file is processed, THE Bazel Build System SHALL define appropriate targets for libraries, binaries, and tests
4. WHEN running tests, THE Bazel Build System SHALL execute all test targets and report results
5. WHERE C++23 features are used, THE Bazel Build System SHALL configure appropriate compiler flags and toolchain settings

### Requirement 14: Core Language Package Preservation

**User Story:** As a developer, I want the existing meld language implementation preserved in the meld-core package, so that all current functionality remains available.

#### Acceptance Criteria

1. WHEN the meld-core package is built, THE Bazel Build System SHALL compile all existing source files from the current meld directory
2. WHEN tests are run for meld-core, THE Bazel Build System SHALL execute all existing test files and maintain current test coverage
3. WHEN the meld-core package is examined, THE Package SHALL contain the parser, compiler, runtime, and standard library components
4. WHEN building meld-core, THE Bazel Build System SHALL link against required dependencies (Boost Spirit X3, etc.)
5. WHERE existing CMake configuration exists, THE Bazel Build System SHALL replicate equivalent build settings

### Requirement 15: Placeholder Packages for Future Components

**User Story:** As a developer, I want placeholder packages for future components, so that I can develop these components in the future.

#### Acceptance Criteria

1. WHEN each placeholder package is created, THE Package SHALL contain a basic BUILD file with minimal structure
2. WHEN examining placeholder packages, THE Package SHALL include appropriate directory structure for future development
3. WHEN building placeholder packages, THE Bazel Build System SHALL successfully build minimal stub implementations
4. WHERE dependencies exist between packages, THE Bazel Build System SHALL allow proper dependency declarations
5. WHEN the workspace is built, THE Bazel Build System SHALL include all placeholder packages in the build process

### Requirement 16: Cross-Package Dependency Management

**User Story:** As a developer, I want proper dependency management between packages, so that changes in one package correctly trigger rebuilds in dependent packages.

#### Acceptance Criteria

1. WHEN meld-core is modified, THE Bazel Build System SHALL rebuild dependent packages (meld-lsp-server, meld-build, meld-mcp-server)
2. WHEN external dependencies are updated, THE Bazel Build System SHALL propagate changes to all affected packages
3. WHEN dependency graphs are analyzed, THE Bazel Build System SHALL prevent circular dependencies between packages
4. WHERE shared utilities exist, THE Bazel Build System SHALL allow multiple packages to depend on common libraries
5. WHEN incremental builds are performed, THE Bazel Build System SHALL only rebuild changed components and their dependents

### Requirement 17: Documentation Preservation

**User Story:** As a developer, I want the workspace to maintain existing documentation and specifications, so that project knowledge is preserved during the transformation.

#### Acceptance Criteria

1. WHEN the transformation is complete, THE Workspace SHALL preserve all existing documentation files in appropriate locations
2. WHEN package-specific documentation exists, THE Documentation SHALL be moved to the relevant package directory
3. WHEN workspace-level documentation is needed, THE Documentation SHALL remain at the workspace root
4. WHERE existing specs reference file paths, THE Specs SHALL be updated to reflect the new package structure
5. WHEN developers examine packages, THE Package SHALL include relevant README and documentation files

### Requirement 18: Build Tooling Migration

**User Story:** As a developer, I want build scripts and tooling updated for the new structure, so that existing workflows continue to function.

#### Acceptance Criteria

1. WHEN build scripts are executed, THE Scripts SHALL work with the new Bazel-based structure
2. WHEN CI/CD pipelines run, THE Pipelines SHALL successfully build and test all packages
3. WHEN development tools are used, THE Tools SHALL recognize the new package structure
4. WHERE existing build commands exist, THE Commands SHALL be updated or replaced with Bazel equivalents
5. WHEN developers run common tasks, THE Tasks SHALL have clear Bazel command equivalents documented
