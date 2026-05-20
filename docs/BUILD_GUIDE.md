# Meld Mono-Repository Build Guide

## Prerequisites

### Bazel Installation

This project uses Bazel 9.0.0 with bzlmod for building. Install Bazel using one of the following methods:

#### Windows
```powershell
# Using Chocolatey
choco install bazel

# Using Scoop
scoop install bazel

# Manual installation
# Download from https://github.com/bazelbuild/bazel/releases/tag/9.0.0
```

#### Linux/macOS
```bash
# Using Bazelisk (recommended)
npm install -g @bazel/bazelisk

# Or download directly
wget https://github.com/bazelbuild/bazel/releases/download/9.0.0/bazel-9.0.0-linux-x86_64
chmod +x bazel-9.0.0-linux-x86_64
sudo mv bazel-9.0.0-linux-x86_64 /usr/local/bin/bazel
```

### System Requirements

- **C++23** compatible compiler:
  - **GCC** 13.0 or later
  - **Clang** 16.0 or later  
  - **MSVC** 19.35 (Visual Studio 2022 17.5) or later

## Workspace Validation

After installing Bazel, validate the workspace setup:

```bash
python tools/validate_workspace.py
```

## Basic Build Commands

```bash
# Build all packages
bazel build //packages:all

# Build specific package
bazel build //packages/meld-lang:all

# Run tests
bazel test //packages/meld-lang:tests

# Run executables
bazel run //packages/meld-lang:meld

# Clean build artifacts
bazel clean
```

## External Dependencies

The workspace automatically manages these external dependencies via bzlmod:

- **Boost** 1.83.0 (Spirit X3, System)
- **GoogleTest** 1.14.0 (for testing)
- **RTTR** v0.9.6 (reflection library)
- **nlohmann/json** v3.11.3 (JSON serialization)

## Configuration Files

- **MODULE.bazel**: Defines external dependencies and module settings (bzlmod)
- **.bazelrc**: Build configuration options and compiler flags
- **.bazelversion**: Specifies required Bazel version (9.0.0)
- **BUILD.bazel**: Root build configuration

## Package Structure

```
packages/
├── meld-core/          # Core language implementation
├── meld-lsp-server/    # Language Server Protocol
├── meld-build/         # Build tools
└── meld-mcp-server/    # Model Context Protocol server
```

## Migration from CMake

If you're migrating from the old CMake-based build system, see the comprehensive guides:

- **[Migration Guide](MIGRATION_GUIDE.md)**: Complete migration instructions
- **[Bazel Commands](BAZEL_COMMANDS.md)**: CMake to Bazel command equivalents
- **[Troubleshooting](TROUBLESHOOTING.md)**: Common issues and solutions
- **[Developer Onboarding](DEVELOPER_ONBOARDING.md)**: Setup guide for new developers

## Troubleshooting

### Common Issues

1. **Bazel not found**: Ensure Bazel is installed and in your PATH
2. **C++23 not supported**: Update your compiler to a supported version
3. **External dependency failures**: Check internet connection and proxy settings

For detailed troubleshooting, see the **[Troubleshooting Guide](TROUBLESHOOTING.md)**.

### Getting Help

- Run `bazel help` for command documentation
- Check build logs with `bazel build --verbose_failures`
- Use `bazel query` to inspect build targets
- See **[Bazel Commands](BAZEL_COMMANDS.md)** for complete command reference