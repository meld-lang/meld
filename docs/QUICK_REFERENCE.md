# Meld Mono-Repository Quick Reference

## Essential Commands

### Build Commands
```bash
# Build everything
bazel build //...

# Build Meld compiler
bazel build //packages/meld-lang:meld

# Build with debug info
bazel build --compilation_mode=dbg //packages/meld-lang:meld

# Build optimized
bazel build --compilation_mode=opt //packages/meld-lang:meld
```

### Test Commands
```bash
# Run all tests
bazel test //...

# Run Meld tests
bazel test //packages/meld-lang:tests

# Run with output
bazel test --test_output=all //packages/meld-lang:tests

# Run specific test
bazel test //packages/meld-lang:primitives_test
```

### Run Commands
```bash
# Run Meld compiler
bazel run //packages/meld-lang:meld

# Run with file
bazel run //packages/meld-lang:meld -- examples/hello.meld

# Run example
bazel run //packages/meld-lang:ceylon_initialization_demo
```

### Query Commands
```bash
# List all targets
bazel query //...

# List package targets
bazel query //packages/meld-lang:all

# Show dependencies
bazel query "deps(//packages/meld-lang:meld)"

# Find tests
bazel query "kind(cc_test, //...)"
```

### Maintenance Commands
```bash
# Clean build outputs
bazel clean

# Clean everything
bazel clean --expunge

# Validate workspace
python tools/validate_workspace.py

# Show info
bazel info
```

## Package Structure

```
packages/
├── meld-core/          # Core language (compiler, parser, runtime)
├── meld-lsp-server/    # Language Server Protocol
├── meld-build/         # Build tools and utilities
└── meld-mcp-server/    # Model Context Protocol server
```

## Key Build Targets

### Meld-Lang
```bash
# Libraries
//packages/meld-lang:kernel
//packages/meld-lang:parser
//packages/meld-lang:compiler
//packages/meld-lang:meta
//packages/meld-lang:types
//packages/meld-lang:stdlib
//packages/meld-lang:serialization
//packages/meld-lang:macro

# Executable
//packages/meld-lang:meld

# Tests
//packages/meld-lang:tests
//packages/meld-lang:kernel_tests
//packages/meld-lang:parser_tests
//packages/meld-lang:compiler_tests

# Examples
//packages/meld-lang:ceylon_initialization_demo
//packages/meld-lang:function_signature_demo
//packages/meld-lang:extension_demo
//packages/meld-lang:pipeline_demo
```

### Other Packages
```bash
# LSP Server
//packages/meld-lsp-server:server
//packages/meld-lsp-server:tests

# Build Tools
//packages/meld-build:build-tool
//packages/meld-build:tests

# MCP Server
//packages/meld-mcp-server:server
//packages/meld-mcp-server:tests
```

## Configuration Files

- **MODULE.bazel**: External dependencies (Boost, GoogleTest, RTTR, JSON)
- **.bazelrc**: Build configuration (C++23, compiler flags, optimizations)
- **.bazelversion**: Required Bazel version (9.0.0)
- **BUILD.bazel**: Build target definitions

## CMake to Bazel Equivalents

| CMake | Bazel |
|-------|-------|
| `cmake ..` | Automatic |
| `cmake --build .` | `bazel build //...` |
| `make meld` | `bazel build //packages/meld-lang:meld` |
| `ctest` | `bazel test //...` |
| `make clean` | `bazel clean` |
| `./build/meld` | `bazel run //packages/meld-lang:meld` |

## Useful Aliases

Add to your shell configuration:

```bash
# Bazel shortcuts
alias bb='bazel build'
alias bt='bazel test'
alias br='bazel run'
alias bq='bazel query'
alias bc='bazel clean'

# Meld-specific
alias bmeld='bazel build //packages/meld-lang:meld'
alias tmeld='bazel test //packages/meld-lang:tests'
alias rmeld='bazel run //packages/meld-lang:meld'

# Development
alias validate='python tools/validate_workspace.py'
alias buildall='bazel build //...'
alias testall='bazel test //...'
```

## IDE Setup

### VS Code
1. Install Bazel extension
2. Generate compile commands: `bazel run @hedron_compile_commands//:refresh_all`
3. Configure C++ standard to C++23

### CLion
1. Import as Bazel project
2. Create `.bazelproject` file with package directories and targets

## Troubleshooting

### Common Issues
- **Bazel not found**: Install Bazel 9.0.0
- **C++23 not supported**: Update compiler (GCC 13+, Clang 16+, MSVC 19.35+)
- **Build failures**: Run `bazel clean --expunge` and retry
- **Slow builds**: Enable disk cache in `.bazelrc.user`

### Debug Commands
```bash
# Verbose build output
bazel build --verbose_failures //packages/meld-lang:meld

# Show executed commands
bazel build --subcommands //packages/meld-lang:meld

# Profile build
bazel build --profile=profile.json //packages/meld-lang:meld
```

## External Dependencies

Managed via MODULE.bazel:
- **Boost** 1.83.0 (Spirit X3, System)
- **GoogleTest** 1.14.0 (testing framework)
- **RTTR** v0.9.6 (reflection library)
- **nlohmann/json** v3.11.3 (JSON serialization)

## Documentation

- **[Migration Guide](MIGRATION_GUIDE.md)**: Complete migration from CMake
- **[Bazel Commands](BAZEL_COMMANDS.md)**: Comprehensive command reference
- **[Troubleshooting](TROUBLESHOOTING.md)**: Common issues and solutions
- **[Developer Onboarding](DEVELOPER_ONBOARDING.md)**: Setup for new developers
- **[Build Guide](BUILD_GUIDE.md)**: Detailed build instructions

## Getting Help

- **Workspace validation**: `python tools/validate_workspace.py`
- **Bazel help**: `bazel help <command>`
- **Target information**: `bazel query --output=build <target>`
- **Dependency tree**: `bazel query "deps(<target>)" --output=graph`

---

**Quick Start**: `bazel build //... && bazel test //... && bazel run //packages/meld-lang:meld`