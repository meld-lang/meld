# Meld Mono-Repository Troubleshooting Guide

## Overview

This guide helps resolve common issues when working with the Meld mono-repository and Bazel build system.

## Installation Issues

### Bazel Installation Problems

#### "bazel: command not found"

**Symptoms:**
```bash
$ bazel version
bash: bazel: command not found
```

**Solutions:**

**Windows:**
```powershell
# Check if Bazel is in PATH
$env:PATH -split ';' | Select-String bazel

# Install via Chocolatey
choco install bazel

# Install via Scoop
scoop install bazel

# Manual installation
# Download from https://github.com/bazelbuild/bazel/releases/tag/9.0.0
# Add to PATH
```

**Linux/macOS:**
```bash
# Check if Bazel is in PATH
which bazel

# Install via package manager
# Ubuntu/Debian
sudo apt install bazel

# macOS
brew install bazel

# Or use Bazelisk (recommended)
npm install -g @bazel/bazelisk
```

#### Wrong Bazel Version

**Symptoms:**
```bash
$ bazel version
Build label: 8.0.0
ERROR: This workspace requires Bazel 9.0.0
```

**Solutions:**
```bash
# Update Bazel
# Windows (Chocolatey)
choco upgrade bazel

# Linux/macOS (Homebrew)
brew upgrade bazel

# Or download specific version
wget https://github.com/bazelbuild/bazel/releases/download/9.0.0/bazel-9.0.0-linux-x86_64
chmod +x bazel-9.0.0-linux-x86_64
sudo mv bazel-9.0.0-linux-x86_64 /usr/local/bin/bazel
```

### Compiler Issues

#### C++23 Not Supported

**Symptoms:**
```bash
ERROR: C++23 features are not supported by this compiler
```

**Solutions:**

**Check compiler version:**
```bash
# GCC (need 13.0+)
gcc --version

# Clang (need 16.0+)
clang --version

# MSVC (need 19.35+)
cl.exe
```

**Update compiler:**

**Ubuntu/Debian:**
```bash
# Add GCC 13 repository
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install gcc-13 g++-13

# Set as default
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100
```

**macOS:**
```bash
# Install latest Clang via Xcode
xcode-select --install

# Or via Homebrew
brew install llvm
```

**Windows:**
```powershell
# Install Visual Studio 2022 17.5 or later
# Or Visual Studio Build Tools 2022
```

## Build Issues

### External Dependency Failures

#### Network/Download Issues

**Symptoms:**
```bash
ERROR: Failed to fetch external dependency: boost
ERROR: Repository rule http_archive failed
```

**Solutions:**

**Check network connectivity:**
```bash
# Test connectivity
curl -I https://github.com

# Check proxy settings
echo $HTTP_PROXY
echo $HTTPS_PROXY
```

**Configure proxy (if needed):**
```bash
# Add to .bazelrc.user
echo "build --action_env=HTTP_PROXY=$HTTP_PROXY" >> .bazelrc.user
echo "build --action_env=HTTPS_PROXY=$HTTPS_PROXY" >> .bazelrc.user
```

**Clear cache and retry:**
```bash
bazel clean --expunge
bazel build //...
```

#### Dependency Version Conflicts

**Symptoms:**
```bash
ERROR: Multiple versions of dependency 'boost' found
ERROR: Version conflict in external dependencies
```

**Solutions:**

**Check MODULE.bazel:**
```python
# Ensure consistent versions
bazel_dep(name = "boost", version = "1.83.0.bzl.2")  # Use exact version
```

**Clear dependency cache:**
```bash
bazel clean --expunge
bazel sync
bazel build //...
```

### Build Target Issues

#### Target Not Found

**Symptoms:**
```bash
ERROR: no such target '//packages/meld-lang:nonexistent'
```

**Solutions:**

**List available targets:**
```bash
# List all targets
bazel query //...

# List targets in specific package
bazel query //packages/meld-lang:all

# Search for target by name
bazel query //... | grep meld
```

**Check BUILD.bazel file:**
```bash
# Verify target exists in BUILD file
cat packages/meld-core/BUILD.bazel | grep -A5 "name = \"target_name\""
```

#### Circular Dependency

**Symptoms:**
```bash
ERROR: Circular dependency between targets
ERROR: cycle in dependency graph
```

**Solutions:**

**Find the cycle:**
```bash
# Check for cycles
bazel query "somepath(//packages/meld-lang:kernel, //packages/meld-lang:kernel)"

# Show dependency graph
bazel query "deps(//packages/meld-lang:meld)" --output=graph > deps.dot
```

**Fix dependency structure:**
- Move shared code to a common library
- Break circular dependencies by refactoring
- Use forward declarations instead of includes

### Compilation Errors

#### Missing Headers

**Symptoms:**
```bash
fatal error: 'boost/spirit/x3.hpp' file not found
fatal error: 'meld/kernel/primitives.hpp' file not found
```

**Solutions:**

**Check include paths:**
```python
# In BUILD.bazel, ensure proper includes
cc_library(
    name = "my_library",
    srcs = ["src/file.cpp"],
    hdrs = ["include/header.hpp"],
    includes = ["include"],  # Add this
    deps = ["@boost//:spirit"],
)
```

**Verify external dependencies:**
```bash
# Check if Boost is available
bazel query @boost//:all

# Check dependency configuration
cat MODULE.bazel | grep boost
```

#### Linker Errors

**Symptoms:**
```bash
undefined reference to 'boost::system::error_category::name()'
undefined reference to 'meld::kernel::Symbol::Symbol()'
```

**Solutions:**

**Check library dependencies:**
```python
# Ensure all required libraries are linked
cc_binary(
    name = "meld",
    srcs = ["src/main.cpp"],
    deps = [
        ":kernel",
        ":parser",
        "@boost//:system",  # Add missing dependencies
    ],
)
```

**Verify library targets:**
```bash
# Check if library target exists
bazel query //packages/meld-lang:kernel

# Check library contents
bazel query "deps(//packages/meld-lang:kernel)"
```

## Test Issues

### Test Failures

#### Test Not Found

**Symptoms:**
```bash
ERROR: no such target '//packages/meld-lang:missing_test'
```

**Solutions:**

**List test targets:**
```bash
# Find all tests
bazel query "kind(cc_test, //...)"

# Find tests in specific package
bazel query "kind(cc_test, //packages/meld-lang:all)"
```

**Check test configuration:**
```python
# Ensure test is properly defined
cc_test(
    name = "my_test",
    srcs = ["tests/my_test.cpp"],
    deps = [
        ":library_under_test",
        "@googletest//:gtest_main",
    ],
)
```

#### Test Execution Failures

**Symptoms:**
```bash
FAILED: //packages/meld-lang:kernel_test
Test failed with exit code 1
```

**Solutions:**

**Run with verbose output:**
```bash
# Show test output
bazel test //packages/meld-lang:kernel_test --test_output=all

# Show detailed test summary
bazel test //packages/meld-lang:kernel_test --test_summary=detailed
```

**Debug test execution:**
```bash
# Run test directly
bazel run //packages/meld-lang:kernel_test

# Run with debugger
bazel run --run_under="gdb --args" //packages/meld-lang:kernel_test
```

### Property-Based Test Issues

#### Hypothesis/QuickCheck Failures

**Symptoms:**
```bash
FAILED: Property test found counterexample
Falsifying example: input = {...}
```

**Solutions:**

**Analyze counterexample:**
1. Examine the failing input
2. Determine if it's a valid test case
3. Check if the property is correctly specified
4. Verify the implementation handles the case

**Common fixes:**
- Add input validation to exclude invalid cases
- Fix the implementation to handle edge cases
- Refine the property specification
- Add preconditions to the property

## Performance Issues

### Slow Builds

#### Initial Build Takes Too Long

**Symptoms:**
- First build takes 30+ minutes
- External dependencies downloading slowly

**Solutions:**

**Enable caching:**
```bash
# Add to .bazelrc.user
echo "build --disk_cache=~/.cache/bazel" >> .bazelrc.user

# Use remote cache if available
echo "build --remote_cache=grpc://cache-server:port" >> .bazelrc.user
```

**Optimize build settings:**
```bash
# Limit parallel jobs if memory constrained
echo "build --jobs=4" >> .bazelrc.user

# Limit memory usage
echo "build --local_ram_resources=8192" >> .bazelrc.user
```

#### Incremental Builds Still Slow

**Symptoms:**
- Small changes trigger large rebuilds
- Build doesn't seem incremental

**Solutions:**

**Check dependency structure:**
```bash
# Find what depends on changed file
bazel query "rdeps(//..., //packages/meld-lang:changed_target)"

# Optimize dependencies to reduce coupling
```

**Use faster linker:**
```bash
# Add to .bazelrc (Linux)
echo "build --linkopt=-fuse-ld=lld" >> .bazelrc.user
```

### Memory Issues

#### Out of Memory During Build

**Symptoms:**
```bash
ERROR: OutOfMemoryError: Java heap space
ERROR: Build failed due to memory constraints
```

**Solutions:**

**Increase JVM memory:**
```bash
# Add to .bazelrc.user
echo "startup --host_jvm_args=-Xmx4g" >> .bazelrc.user
```

**Limit build parallelism:**
```bash
# Reduce concurrent jobs
echo "build --jobs=2" >> .bazelrc.user

# Limit RAM usage
echo "build --local_ram_resources=HOST_RAM*0.5" >> .bazelrc.user
```

**Use swap space (Linux):**
```bash
# Add swap file
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

## IDE Integration Issues

### VS Code Problems

#### IntelliSense Not Working

**Symptoms:**
- No code completion
- Red squiggles on valid code
- "Include file not found" errors

**Solutions:**

**Generate compile_commands.json:**
```bash
# Install hedron_compile_commands
# Add to MODULE.bazel:
bazel_dep(name = "hedron_compile_commands", version = "0.5.2")

# Generate compilation database
bazel run @hedron_compile_commands//:refresh_all
```

**Configure VS Code:**
```json
// In .vscode/settings.json
{
    "C_Cpp.default.compileCommands": "${workspaceFolder}/compile_commands.json",
    "C_Cpp.default.cppStandard": "c++23"
}
```

#### Bazel Extension Issues

**Symptoms:**
- Bazel commands not working in VS Code
- Build targets not showing

**Solutions:**

**Install Bazel extension:**
1. Open Extensions (Ctrl+Shift+X)
2. Search for "Bazel"
3. Install official Bazel extension

**Configure extension:**
```json
// In .vscode/settings.json
{
    "bazel.executable": "bazel",
    "bazel.buildifierExecutable": "buildifier"
}
```

### CLion Problems

#### Project Import Issues

**Symptoms:**
- CLion doesn't recognize Bazel project
- Build configurations missing

**Solutions:**

**Import as Bazel project:**
1. File → Import Bazel Project
2. Select workspace root directory
3. Choose "Import project view file" or "Import from scratch"

**Configure project view:**
```
# Create .bazelproject file
directories:
  packages/meld-lang
  packages/meld-lsp-server
  packages/meld-build
  packages/meld-mcp-server

targets:
  //packages/meld-lang:all
  //packages/meld-lsp-server:all
  //packages/meld-build:all
  //packages/meld-mcp-server:all

additional_languages:
  c++
```

## Workspace Issues

### File Structure Problems

#### Missing Files After Migration

**Symptoms:**
- Files not found in expected locations
- Build targets reference non-existent files

**Solutions:**

**Verify migration:**
```bash
# Check if files were moved correctly
find packages/meld-lang -name "*.cpp" | head -10
find packages/meld-lang -name "*.hpp" | head -10

# Compare with original structure
ls -la meld/src/
ls -la packages/meld-core/src/
```

**Update file references:**
```bash
# Find and update include paths
grep -r "include.*meld/" packages/meld-core/
# Update to use new paths
```

#### Permission Issues

**Symptoms:**
```bash
ERROR: Permission denied accessing file
ERROR: Cannot create output directory
```

**Solutions:**

**Fix file permissions:**
```bash
# Make files readable
chmod -R 644 packages/

# Make directories executable
find packages/ -type d -exec chmod 755 {} \;

# Make scripts executable
find . -name "*.sh" -exec chmod +x {} \;
```

**Check disk space:**
```bash
# Check available space
df -h

# Clean Bazel cache if needed
bazel clean --expunge
```

## Getting Help

### Diagnostic Commands

```bash
# Show Bazel configuration
bazel info

# Show build configuration
bazel config

# Show workspace status
python tools/validate_workspace.py

# Show target information
bazel query --output=build //packages/meld-lang:meld

# Show dependency tree
bazel query "deps(//packages/meld-lang:meld)" --output=graph
```

### Logging and Debugging

```bash
# Enable verbose logging
bazel build --verbose_failures --subcommands //packages/meld-lang:meld

# Profile build performance
bazel build --profile=profile.json //packages/meld-lang:meld

# Explain build decisions
bazel build --explain=explain.log //packages/meld-lang:meld

# Debug external dependencies
bazel build --verbose_explanations //packages/meld-lang:meld
```

### Community Support

- **GitHub Issues**: Report bugs and ask questions
- **Bazel Documentation**: https://bazel.build/docs
- **Stack Overflow**: Tag questions with `bazel` and `meld-lang`
- **Team Chat**: Internal Slack/Discord channels

### Emergency Recovery

If the workspace becomes completely broken:

```bash
# Nuclear option: clean everything
bazel clean --expunge
rm -rf ~/.cache/bazel

# Re-validate workspace
python tools/validate_workspace.py

# Rebuild from scratch
bazel build //...
```

## Prevention Tips

1. **Regular validation**: Run `python tools/validate_workspace.py` regularly
2. **Keep Bazel updated**: Use the version specified in `.bazelversion`
3. **Monitor disk space**: Bazel cache can grow large
4. **Use version control**: Commit working states before major changes
5. **Test incrementally**: Build and test after each change
6. **Document changes**: Update BUILD files when adding/removing files

---

**Note**: If you encounter an issue not covered here, please document the problem and solution for future reference.