# Meld-Build Design Document

## Overview

The Meld-Build system integrates Bazel as the primary build system for the Meld programming language. This design implements custom Bazel rules, toolchains, and workspace configurations to provide efficient, scalable builds for Meld projects. The system leverages Bazel's incremental compilation, dependency management, and cross-platform capabilities while maintaining seamless integration with existing Meld tooling.

## Architecture

### High-Level Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Meld Source   │    │  Bazel Rules     │    │  Meld Compiler  │
│     Files       │───▶│  & Toolchains    │───▶│   Integration   │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                                │
                                ▼
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│  Build Outputs  │◀───│  Bazel Build     │───▶│  Dependency     │
│ (libs, bins,    │    │    Engine        │    │   Resolution    │
│  tests)         │    └──────────────────┘    └─────────────────┘
└─────────────────┘
```

### Component Architecture

1. **Bazel Rules Layer**: Custom Starlark rules for Meld targets
2. **Toolchain Layer**: Meld compiler integration and platform support
3. **Workspace Layer**: Project configuration and external dependencies
4. **Integration Layer**: IDE and development tool support

## Components and Interfaces

### 1. Meld Bazel Rules

**Core Rules:**
- `meld_library()`: Compiles Meld source files into reusable library artifacts
- `meld_binary()`: Compiles Meld source files into executable binaries
- `meld_test()`: Compiles and runs Meld test files

**Rule Interface:**
```starlark
meld_library(
    name: str,
    srcs: List[Label],
    deps: List[Label] = [],
    compiler_flags: List[str] = [],
    visibility: List[str] = ["//visibility:private"]
)

meld_binary(
    name: str,
    srcs: List[Label],
    main: Label,
    deps: List[Label] = [],
    compiler_flags: List[str] = [],
    data: List[Label] = []
)

meld_test(
    name: str,
    srcs: List[Label],
    deps: List[Label] = [],
    test_data: List[Label] = [],
    timeout: str = "short"
)
```

### 2. Meld Toolchain

**Toolchain Definition:**
- Meld compiler executable location
- Standard library path
- Platform-specific compilation flags
- Cross-compilation support

**Toolchain Interface:**
```starlark
meld_toolchain(
    name: str,
    compiler: Label,
    std_lib: Label,
    target_platform: str,
    compiler_flags: List[str] = []
)
```

### 3. Workspace Configuration

**Repository Rules:**
- `meld_repository()`: Fetches external Meld libraries
- `meld_toolchain_repository()`: Downloads Meld compiler toolchains
- `meld_workspace()`: Parses `meld.toml` and auto-generates targets

**Workspace Setup:**
```starlark
load("@meld_rules//meld:repositories.bzl", "meld_repositories")
load("@meld_rules//meld:toolchains.bzl", "meld_register_toolchains")
load("@meld_rules//meld:defs.bzl", "meld_workspace")

meld_repositories()
meld_register_toolchains()

# Auto-generate targets from meld.toml
meld_workspace(meld_toml = "//:meld.toml")
```

### 3b. `rules_meld` — `meld.toml` Integration

When `bazel_integration = true` in `meld.toml`, the `rules_meld` ruleset provides automatic target generation:

**`meld_workspace` Rule:**
```starlark
meld_workspace(
    name: str = "meld",
    meld_toml: Label,  # Path to meld.toml
)
```

**Behavior:**
1. Parses `meld.toml` during workspace evaluation
2. For each entry in `[dependencies]`, auto-generates a `meld_library` target:
   - Fetches the dependency via Git URL (or registry)
   - Sets `compiler_flags` to include `--allow-effects=<effects>` from the `allow` array
   - If no `allow` array, passes `--allow-effects=` (empty) to enforce pure-only
3. Generates a `meld_binary` or `meld_library` target for the project itself using `[project]` and `[targets]` sections
4. Auto-generated targets can be overridden by explicit BUILD file definitions

**Example `meld.toml` → Generated Targets:**
```toml
# meld.toml
[dependencies]
http-client = { git = "https://github.com/meld-pkg/http-client.git", version = "2.1.0", allow = ["net"] }
json = { git = "https://github.com/meld-pkg/json.git", version = "1.0.0" }
```

Generates (in memory):
```starlark
# Auto-generated — do not edit
meld_library(
    name = "http-client",
    srcs = ["@http-client//:srcs"],
    compiler_flags = ["--allow-effects=net"],
)

meld_library(
    name = "json",
    srcs = ["@json//:srcs"],
    compiler_flags = ["--allow-effects="],  # Pure — no effects
)
```

### 4. Build Actions

**Compilation Action:**
- Input: Meld source files, dependencies
- Tool: Meld compiler
- Output: Compiled artifacts (object files, libraries)
- Flags: Compiler options, optimization levels

**Linking Action:**
- Input: Compiled objects, libraries
- Tool: System linker or Meld linker
- Output: Executable binaries
- Flags: Linker options, library paths

## Data Models

### Build Target Model
```
MeldTarget {
    name: string
    type: TargetType (library, binary, test)
    sources: List<SourceFile>
    dependencies: List<MeldTarget>
    compiler_flags: List<string>
    output_artifacts: List<Artifact>
}
```

### Dependency Graph Model
```
DependencyNode {
    target: MeldTarget
    direct_deps: List<DependencyNode>
    transitive_deps: List<DependencyNode>
    build_order: int
}
```

### Compilation Context Model
```
CompilationContext {
    source_files: List<SourceFile>
    include_paths: List<Path>
    library_paths: List<Path>
    compiler_flags: List<string>
    target_platform: Platform
}
```
## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system-essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property Reflection

After reviewing all properties identified in the prework, several can be consolidated to eliminate redundancy:

- Properties 2.1, 2.2, 2.3 (rule parameter acceptance) can be combined into a single comprehensive property about rule interface validation
- Properties 1.3 and 1.4 (library and binary artifact generation) can be combined into a general artifact generation property
- Properties 4.1 and 4.4 (test compilation/execution and dependency ordering) overlap with general build ordering properties
- Properties 6.1, 6.2, 6.3 (build configurations) can be consolidated into a single configuration application property

### Core Properties

**Property 1: Compiler invocation consistency**
*For any* Meld target with source files, building the target should invoke the Meld compiler with the correct source files and compilation flags
**Validates: Requirements 1.1, 2.5**

**Property 2: Incremental build correctness**
*For any* set of Meld source files, when only a subset is modified, rebuilding should only recompile the modified files and their transitive dependents
**Validates: Requirements 1.2, 3.4**

**Property 3: Artifact generation completeness**
*For any* valid Meld target definition, building the target should produce the expected output artifacts (libraries, binaries, or test executables) in the correct format
**Validates: Requirements 1.3, 1.4**

**Property 4: Cross-compilation toolchain selection**
*For any* target platform specification, the build system should select and use the appropriate toolchain for that platform
**Validates: Requirements 1.5, 5.4**

**Property 5: Rule interface validation**
*For any* Meld rule invocation with valid parameters (sources, dependencies, flags), the rule should accept and correctly process all specified parameters
**Validates: Requirements 2.1, 2.2, 2.3**

**Property 6: Dependency resolution ordering**
*For any* set of interdependent Meld targets, the build system should compile targets in an order that respects all dependency relationships
**Validates: Requirements 2.4, 3.2, 3.5, 4.4**

**Property 7: Import-based dependency discovery**
*For any* Meld source file containing import statements, the build system should automatically include all imported modules in the dependency graph
**Validates: Requirements 3.1**

**Property 8: Test execution and reporting**
*For any* Meld test target, running the test should compile the test code, execute it, and report results with pass/fail status and timing information
**Validates: Requirements 4.1, 4.2**

**Property 9: Test data availability**
*For any* test target with specified test data files, those files should be accessible in the test runtime environment during execution
**Validates: Requirements 4.5**

**Property 10: External dependency management**
*For any* declared external Meld library, the build system should download the library and make it available for import by dependent targets
**Validates: Requirements 5.3, 5.5**

**Property 11: Compiler configuration consistency**
*For any* specified Meld compiler location, all Meld compilation tasks should use that compiler executable
**Validates: Requirements 5.2**

**Property 12: Build configuration application**
*For any* build configuration (debug, release, or custom), the build system should apply the appropriate compiler flags and settings to all compilation tasks
**Validates: Requirements 6.1, 6.2, 6.3, 6.4, 6.5**

**Property 13: Tool integration execution**
*For any* integrated development tool (formatter, linter, custom tool), the build system should execute the tool with correct parameters and report results
**Validates: Requirements 7.2, 7.3, 7.5**

**Property 14: Debug information preservation**
*For any* debug build configuration, the generated artifacts should contain debug symbols and source mapping information
**Validates: Requirements 7.4**

**Property 15: `meld.toml` dependency target generation**
*For any* `meld.toml` file with a `[dependencies]` section, `meld_workspace` should auto-generate a `meld_library` target for each declared dependency with correct source references and compiler flags
**Validates: Requirements 8.1, 8.2**

**Property 16: Effect Firewall CLI flag piping**
*For any* dependency with an `allow` array in `meld.toml`, the generated `meld_library` target should include `--allow-effects=<effects>` in its compiler flags; dependencies without `allow` should pass `--allow-effects=` (empty)
**Validates: Requirements 8.3, 8.4**

**Property 17: `meld.toml` change invalidation**
*For any* modification to `meld.toml`, the build system should invalidate and regenerate all affected auto-generated targets on the next build
**Validates: Requirements 8.5**

**Property 18: Single source of truth consistency**
*For any* project with a `meld.toml`, the compiler should read project metadata, dependencies, effect permissions, and targets exclusively from `meld.toml` without requiring duplicate configuration
**Validates: Requirements 9.1, 9.2, 9.3, 9.4, 9.5**

## Error Handling

### Build Errors
- **Compilation Failures**: Clear error messages with source location information
- **Dependency Cycles**: Detection and reporting of circular dependencies with cycle visualization
- **Missing Dependencies**: Informative errors when required dependencies cannot be resolved
- **Toolchain Errors**: Clear messages when required tools or toolchains are unavailable

### Configuration Errors
- **Invalid Rule Parameters**: Validation and error reporting for malformed BUILD file rules
- **Workspace Configuration**: Clear errors for invalid WORKSPACE configurations
- **Toolchain Registration**: Informative messages for toolchain registration failures

### Runtime Errors
- **Test Failures**: Detailed test failure reporting with stack traces and timing
- **Resource Access**: Clear errors when test data or resources are unavailable
- **Platform Compatibility**: Informative errors for unsupported target platforms

## Testing Strategy

### Dual Testing Approach

The Meld-Build system requires both unit testing and property-based testing to ensure comprehensive correctness validation:

**Unit Testing:**
- Specific examples of BUILD file parsing and rule validation
- Integration points between Bazel rules and Meld compiler
- Error condition handling for malformed configurations
- Platform-specific toolchain selection scenarios

**Property-Based Testing:**
- Universal properties that should hold across all valid build configurations
- Dependency resolution correctness across randomly generated dependency graphs
- Incremental build behavior verification with arbitrary source file modifications
- Cross-platform build consistency across different target platforms

### Property-Based Testing Framework

**Framework Selection:** We will use **Hypothesis** (Python) for property-based testing of the Bazel rules implementation, as Bazel rules are implemented in Starlark/Python.

**Configuration Requirements:**
- Each property-based test MUST run a minimum of 100 iterations
- Each property-based test MUST be tagged with a comment explicitly referencing the correctness property from this design document
- Tag format: `**Feature: meld-build, Property {number}: {property_text}**`
- Each correctness property MUST be implemented by a SINGLE property-based test

### Test Implementation Requirements

**Unit Tests:**
- Test specific BUILD file configurations and rule parameter combinations
- Verify error handling for invalid configurations and missing dependencies
- Test toolchain registration and selection logic
- Validate workspace initialization and external dependency resolution

**Property-Based Tests:**
- Generate random Meld project structures and verify build correctness
- Test dependency resolution with randomly generated dependency graphs
- Verify incremental build behavior with arbitrary file modification patterns
- Test cross-compilation with random target platform combinations
- Validate configuration application across random build flag combinations

### Integration Testing

**End-to-End Scenarios:**
- Complete project builds from workspace initialization to binary execution
- Multi-target builds with complex dependency relationships
- Cross-platform builds with different toolchain configurations
- Test execution workflows with various test data configurations

**Performance Testing:**
- Build time measurements for incremental vs. full builds
- Memory usage validation for large project builds
- Parallel build efficiency verification