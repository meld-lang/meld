# Bazel Command Reference

## Overview

This document provides a comprehensive reference for Bazel commands used in the Meld mono-repository, including equivalents for common CMake operations.

## Command Equivalents

### CMake to Bazel Translation

| Task | CMake (Old) | Bazel (New) |
|------|-------------|-------------|
| **Configure** | `cmake ..` | Automatic (no explicit step) |
| **Build All** | `cmake --build .` | `bazel build //...` |
| **Build Target** | `cmake --build . --target meld` | `bazel build //packages/meld-lang:meld` |
| **Parallel Build** | `cmake --build . --parallel 8` | `bazel build //... --jobs=8` |
| **Clean** | `make clean` | `bazel clean` |
| **Full Clean** | `rm -rf build/` | `bazel clean --expunge` |
| **Run Tests** | `ctest` | `bazel test //...` |
| **Test Output** | `ctest --output-on-failure` | `bazel test //... --test_output=errors` |
| **Install** | `make install` | `bazel run //packages/meld-lang:install` |
| **Debug Build** | `cmake -DCMAKE_BUILD_TYPE=Debug` | `bazel build --compilation_mode=dbg //...` |
| **Release Build** | `cmake -DCMAKE_BUILD_TYPE=Release` | `bazel build --compilation_mode=opt //...` |

## Core Commands

### Build Commands

```bash
# Build everything in the workspace
bazel build //...

# Build all targets in a package
bazel build //packages/meld-lang:all

# Build specific target
bazel build //packages/meld-lang:meld

# Build with specific configuration
bazel build --config=release //packages/meld-lang:meld

# Build with custom flags
bazel build --cxxopt=-DDEBUG //packages/meld-lang:meld

# Build and show progress
bazel build //... --show_progress

# Build with verbose output
bazel build //... --verbose_failures
```

### Test Commands

```bash
# Run all tests
bazel test //...

# Run tests in specific package
bazel test //packages/meld-lang:tests

# Run specific test
bazel test //packages/meld-lang:primitives_test

# Run tests with output
bazel test //... --test_output=all

# Run tests with summary
bazel test //... --test_summary=detailed

# Run tests in parallel
bazel test //... --test_jobs=8

# Run tests with timeout
bazel test //... --test_timeout=300
```

### Run Commands

```bash
# Run executable
bazel run //packages/meld-lang:meld

# Run with arguments
bazel run //packages/meld-lang:meld -- examples/hello.meld

# Run example
bazel run //packages/meld-lang:ceylon_initialization_demo

# Run with environment variables
bazel run --action_env=DEBUG=1 //packages/meld-lang:meld
```

### Query Commands

```bash
# List all targets
bazel query //...

# List targets in package
bazel query //packages/meld-lang:all

# Show dependencies
bazel query "deps(//packages/meld-lang:meld)"

# Show reverse dependencies
bazel query "rdeps(//..., //packages/meld-lang:kernel)"

# Find tests
bazel query "kind(cc_test, //...)"

# Find binaries
bazel query "kind(cc_binary, //...)"

# Show build files
bazel query --output=build //packages/meld-lang:meld
```

## Package-Specific Commands

### Meld-Lang (Core Language)

```bash
# Build core libraries
bazel build //packages/meld-lang:kernel
bazel build //packages/meld-lang:parser
bazel build //packages/meld-lang:compiler
bazel build //packages/meld-lang:meta
bazel build //packages/meld-lang:types
bazel build //packages/meld-lang:stdlib
bazel build //packages/meld-lang:serialization
bazel build //packages/meld-lang:macro

# Build main executable
bazel build //packages/meld-lang:meld

# Run compiler
bazel run //packages/meld-lang:meld -- input.meld

# Run all tests
bazel test //packages/meld-lang:tests

# Run specific test suites
bazel test //packages/meld-lang:kernel_tests
bazel test //packages/meld-lang:parser_tests
bazel test //packages/meld-lang:compiler_tests

# Run examples
bazel run //packages/meld-lang:ceylon_initialization_demo
bazel run //packages/meld-lang:function_signature_demo
bazel run //packages/meld-lang:extension_demo
bazel run //packages/meld-lang:pipeline_demo
bazel run //packages/meld-lang:regex_demo
bazel run //packages/meld-lang:collection_demo
```

### Meld-LSP-Server (Language Server)

```bash
# Build LSP server
bazel build //packages/meld-lsp-server:server

# Run LSP server
bazel run //packages/meld-lsp-server:server

# Test LSP server
bazel test //packages/meld-lsp-server:tests
```

### Meld-Build (Build Tools)

```bash
# Build build tool
bazel build //packages/meld-build:build-tool

# Run build tool
bazel run //packages/meld-build:build-tool

# Test build tool
bazel test //packages/meld-build:tests
```

### Meld-MCP-Server (Model Context Protocol)

```bash
# Build MCP server
bazel build //packages/meld-mcp-server:server

# Run MCP server
bazel run //packages/meld-mcp-server:server

# Test MCP server
bazel test //packages/meld-mcp-server:tests
```

## Configuration and Debugging

### Build Configurations

```bash
# Debug build (equivalent to CMAKE_BUILD_TYPE=Debug)
bazel build --compilation_mode=dbg //...

# Optimized build (equivalent to CMAKE_BUILD_TYPE=Release)
bazel build --compilation_mode=opt //...

# Fast build (minimal optimization)
bazel build --compilation_mode=fastbuild //...

# Custom configuration (defined in .bazelrc)
bazel build --config=dev //...
bazel build --config=release //...
```

### Debugging Commands

```bash
# Show detailed build information
bazel build --verbose_failures //packages/meld-lang:meld

# Show all executed commands
bazel build --subcommands //packages/meld-lang:meld

# Explain why target was rebuilt
bazel build --explain=explain.log //packages/meld-lang:meld

# Show dependency analysis
bazel build --verbose_explanations //packages/meld-lang:meld

# Profile build performance
bazel build --profile=profile.json //packages/meld-lang:meld
```

### Cache Management

```bash
# Clean build outputs (keeps external dependencies)
bazel clean

# Clean everything including external dependencies
bazel clean --expunge

# Show cache statistics
bazel info

# Show repository cache location
bazel info repository_cache

# Show output base location
bazel info output_base
```

## Advanced Usage

### Parallel Builds

```bash
# Use all available CPU cores
bazel build --jobs=auto //...

# Limit to specific number of jobs
bazel build --jobs=4 //...

# Limit memory usage
bazel build --local_ram_resources=8192 //...

# Limit CPU resources
bazel build --local_cpu_resources=4 //...
```

### Cross-Compilation

```bash
# Build for different platform
bazel build --platforms=@platforms//os:windows //packages/meld-lang:meld

# Build with specific toolchain
bazel build --crosstool_top=//toolchain:my_toolchain //...
```

### External Dependencies

```bash
# Show external dependencies
bazel query --output=build @boost//:boost

# Update external dependencies
bazel sync

# Show dependency tree
bazel query "deps(//packages/meld-lang:meld)" --output=graph
```

### Testing Options

```bash
# Run tests with coverage
bazel coverage //packages/meld-lang:tests

# Run tests under sanitizer
bazel test --config=asan //packages/meld-lang:tests

# Run tests with custom test environment
bazel test --test_env=GTEST_COLOR=1 //packages/meld-lang:tests

# Run flaky tests multiple times
bazel test --flaky_test_attempts=3 //packages/meld-lang:tests

# Run tests with filter
bazel test --test_filter="*Parser*" //packages/meld-lang:tests
```

## Workspace Commands

### Validation and Information

```bash
# Validate workspace
python tools/validate_workspace.py

# Show workspace information
bazel info workspace

# Show Bazel version
bazel version

# Show build configuration
bazel config

# Show available configurations
bazel help startup_options
```

### Package Management

```bash
# List all packages
bazel query //...

# Show package dependencies
bazel query "deps(//packages/meld-lang:all)"

# Find unused dependencies
bazel query "kind(cc_library, //...) except deps(//packages/meld-lang:meld)"

# Show package size
bazel query --output=minrank //packages/meld-lang:all
```

## Performance Optimization

### Build Performance

```bash
# Enable disk cache
echo "build --disk_cache=~/.cache/bazel" >> .bazelrc.user

# Enable remote cache (if available)
echo "build --remote_cache=grpc://cache-server:port" >> .bazelrc.user

# Optimize for incremental builds
echo "build --experimental_reuse_sandbox_directories" >> .bazelrc.user

# Use faster linker (if available)
echo "build --linkopt=-fuse-ld=lld" >> .bazelrc.user
```

### Memory Optimization

```bash
# Limit Bazel server memory
echo "startup --host_jvm_args=-Xmx2g" >> .bazelrc.user

# Limit build memory
echo "build --local_ram_resources=HOST_RAM*0.75" >> .bazelrc.user

# Reduce concurrent actions
echo "build --jobs=HOST_CPUS*0.5" >> .bazelrc.user
```

## Troubleshooting Commands

### Common Issues

```bash
# Fix "Target not found" errors
bazel query //...

# Fix dependency issues
bazel clean && bazel build //...

# Fix external dependency issues
bazel clean --expunge && bazel build //...

# Check for circular dependencies
bazel query "somepath(//packages/meld-lang:kernel, //packages/meld-lang:kernel)"

# Verify build file syntax
bazel query --output=build //packages/meld-lang:all
```

### Debugging Build Failures

```bash
# Show detailed error messages
bazel build --verbose_failures //packages/meld-lang:meld

# Show compiler commands
bazel build --subcommands //packages/meld-lang:meld

# Run single action for debugging
bazel build --strategy=CppCompile=standalone //packages/meld-lang:meld

# Keep temporary files for inspection
bazel build --sandbox_debug //packages/meld-lang:meld
```

## IDE Integration

### VS Code

```bash
# Generate compile_commands.json for IntelliSense
bazel run @hedron_compile_commands//:refresh_all

# Build current file
bazel build //packages/meld-lang:$(basename $PWD)
```

### CLion

```bash
# Import Bazel project
# File -> Import Bazel Project -> Select workspace root

# Sync project
bazel sync
```

## Useful Aliases

Add these to your shell configuration:

```bash
# Common build commands
alias bb='bazel build'
alias bt='bazel test'
alias br='bazel run'
alias bq='bazel query'

# Package-specific shortcuts
alias bmeld='bazel build //packages/meld-lang:meld'
alias tmeld='bazel test //packages/meld-lang:tests'
alias rmeld='bazel run //packages/meld-lang:meld'

# Workspace commands
alias bclean='bazel clean'
alias binfo='bazel info'
alias bversion='bazel version'
```

## References

- [Bazel Documentation](https://bazel.build/docs)
- [Bazel C++ Rules](https://bazel.build/reference/be/c-cpp)
- [Bazel Query Language](https://bazel.build/query/language)
- [Bazel Best Practices](https://bazel.build/concepts/best-practices)