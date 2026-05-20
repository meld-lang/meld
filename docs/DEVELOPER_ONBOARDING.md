# Developer Onboarding Guide

## Welcome to Meld Development!

This guide helps new developers get up and running with the Meld mono-repository. Follow these steps to set up your development environment and start contributing.

## Prerequisites

### System Requirements

- **Operating System**: Windows 10+, macOS 10.15+, or Linux (Ubuntu 20.04+)
- **Memory**: 8GB RAM minimum, 16GB recommended
- **Storage**: 10GB free space for dependencies and build cache
- **Network**: Stable internet connection for downloading dependencies

### Required Software

#### 1. Git
```bash
# Verify Git installation
git --version

# If not installed:
# Windows: Download from https://git-scm.com/
# macOS: xcode-select --install
# Linux: sudo apt install git
```

#### 2. C++23 Compatible Compiler

**Windows:**
- Visual Studio 2022 17.5+ or Visual Studio Build Tools 2022
- Download from: https://visualstudio.microsoft.com/

**macOS:**
- Xcode 14.3+ or Xcode Command Line Tools
```bash
xcode-select --install
```

**Linux (Ubuntu/Debian):**
```bash
# Install GCC 13
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install gcc-13 g++-13

# Set as default
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100

# Verify installation
gcc --version  # Should show 13.0+
```

#### 3. Bazel 9.0.0

**Windows:**
```powershell
# Using Chocolatey (recommended)
choco install bazel

# Using Scoop
scoop install bazel

# Verify installation
bazel version
```

**macOS:**
```bash
# Using Homebrew
brew install bazel

# Verify installation
bazel version
```

**Linux:**
```bash
# Using Bazelisk (recommended)
npm install -g @bazel/bazelisk

# Or download directly
wget https://github.com/bazelbuild/bazel/releases/download/9.0.0/bazel-9.0.0-linux-x86_64
chmod +x bazel-9.0.0-linux-x86_64
sudo mv bazel-9.0.0-linux-x86_64 /usr/local/bin/bazel

# Verify installation
bazel version
```

## Repository Setup

### 1. Clone the Repository

```bash
# Clone the repository
git clone https://github.com/your-org/meld-monorepo.git
cd meld-monorepo

# Verify you're in the right place
ls -la
# Should see: MODULE.bazel, .bazelrc, packages/, docs/, etc.
```

### 2. Validate Workspace

```bash
# Run workspace validation
python tools/validate_workspace.py

# Expected output:
# ✅ All configuration files present
# ✅ All required directories created
# ✅ Bazel installation verified
# ✅ C++23 compiler available
```

### 3. Initial Build

```bash
# Build everything (this will take 10-30 minutes on first run)
bazel build //...

# If successful, you should see:
# INFO: Build completed successfully
```

### 4. Run Tests

```bash
# Run all tests to verify everything works
bazel test //...

# Expected output:
# //packages/meld-lang:tests                    PASSED
# //packages/meld-lsp-server:tests             PASSED
# //packages/meld-build:tests                  PASSED
# //packages/meld-mcp-server:tests             PASSED
```

## Development Environment Setup

### IDE Configuration

#### VS Code (Recommended)

1. **Install Extensions:**
   - Bazel (official)
   - C/C++ (Microsoft)
   - C++ TestMate
   - GitLens

2. **Generate Compilation Database:**
```bash
# Add to MODULE.bazel if not present:
bazel_dep(name = "hedron_compile_commands", version = "0.5.2")

# Generate compile_commands.json
bazel run @hedron_compile_commands//:refresh_all
```

3. **Configure Settings:**
```json
// .vscode/settings.json
{
    "C_Cpp.default.compileCommands": "${workspaceFolder}/compile_commands.json",
    "C_Cpp.default.cppStandard": "c++23",
    "C_Cpp.default.intelliSenseMode": "gcc-x64",
    "bazel.executable": "bazel",
    "files.associations": {
        "*.meld": "cpp"
    }
}
```

4. **Configure Tasks:**
```json
// .vscode/tasks.json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Build All",
            "type": "shell",
            "command": "bazel",
            "args": ["build", "//..."],
            "group": "build",
            "presentation": {
                "echo": true,
                "reveal": "always",
                "focus": false,
                "panel": "shared"
            }
        },
        {
            "label": "Test All",
            "type": "shell",
            "command": "bazel",
            "args": ["test", "//..."],
            "group": "test"
        },
        {
            "label": "Build Meld",
            "type": "shell",
            "command": "bazel",
            "args": ["build", "//packages/meld-lang:meld"],
            "group": "build"
        }
    ]
}
```

#### CLion

1. **Import Project:**
   - File → Import Bazel Project
   - Select workspace root directory
   - Choose "Import from scratch"

2. **Create Project View:**
```
# .bazelproject
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

build_flags:
  --cxxopt=-std=c++23
```

### Shell Configuration

Add these aliases to your shell configuration (`.bashrc`, `.zshrc`, etc.):

```bash
# Bazel shortcuts
alias bb='bazel build'
alias bt='bazel test'
alias br='bazel run'
alias bq='bazel query'
alias bc='bazel clean'

# Meld-specific shortcuts
alias bmeld='bazel build //packages/meld-lang:meld'
alias tmeld='bazel test //packages/meld-lang:tests'
alias rmeld='bazel run //packages/meld-lang:meld'

# Development helpers
alias validate='python tools/validate_workspace.py'
alias buildall='bazel build //...'
alias testall='bazel test //...'
```

## Understanding the Codebase

### Repository Structure

```
meld-monorepo/
├── packages/                    # All packages
│   ├── meld-core/              # Core language implementation
│   │   ├── include/meld/       # Public headers
│   │   ├── src/                # Implementation
│   │   ├── tests/              # Unit tests
│   │   ├── examples/           # Example programs
│   │   └── BUILD.bazel         # Build configuration
│   ├── meld-lsp-server/        # Language Server Protocol
│   ├── meld-build/             # Build tools
│   └── meld-mcp-server/        # Model Context Protocol server
├── tools/                      # Shared build tools and scripts
├── docs/                       # Documentation
├── third_party/                # External dependencies (managed by Bazel)
├── MODULE.bazel                # Dependency management
├── .bazelrc                    # Build configuration
└── BUILD.bazel                 # Root build file
```

### Key Components

#### Meld-Lang Package

**Core Libraries:**
- **Kernel** (`src/kernel/`): Core primitives, operations, symbol table
- **Meta** (`src/meta/`): Type system, reflection, type registration
- **Types** (`src/types/`): Type instances, nullable system, collections
- **Parser** (`src/parser/`): Lexer, parser, AST generation
- **Compiler** (`src/compiler/`): Type checker, IR, code generation
- **Macro** (`src/macro/`): Macro system and decorators
- **Standard Library** (`src/stdlib/`): Built-in functions and utilities
- **Serialization** (`src/serialization/`): JSON serialization support

**Build Targets:**
```bash
# Core libraries
bazel build //packages/meld-lang:kernel
bazel build //packages/meld-lang:parser
bazel build //packages/meld-lang:compiler

# Main executable
bazel build //packages/meld-lang:meld

# All tests
bazel test //packages/meld-lang:tests
```

### Development Workflow

#### 1. Making Changes

```bash
# Create feature branch
git checkout -b feature/my-new-feature

# Make your changes
# Edit files in packages/meld-core/src/

# Build affected targets
bazel build //packages/meld-lang:kernel

# Run tests
bazel test //packages/meld-lang:kernel_tests
```

#### 2. Adding New Files

When adding new source files, update the corresponding BUILD.bazel:

```python
# packages/meld-core/BUILD.bazel
cc_library(
    name = "kernel",
    srcs = [
        "src/kernel/primitives.cpp",
        "src/kernel/operations.cpp",
        "src/kernel/new_file.cpp",  # Add new file here
    ],
    hdrs = [
        "include/meld/kernel/primitives.hpp",
        "include/meld/kernel/operations.hpp",
        "include/meld/kernel/new_file.hpp",  # Add new header here
    ],
    # ... rest of configuration
)
```

#### 3. Adding Tests

```python
# Add test to appropriate test target
cc_test(
    name = "kernel_tests",
    srcs = [
        "tests/kernel/primitives_test.cpp",
        "tests/kernel/operations_test.cpp",
        "tests/kernel/new_file_test.cpp",  # Add new test here
    ],
    deps = [
        ":kernel",
        "@googletest//:gtest_main",
    ],
)
```

#### 4. Running Examples

```bash
# List available examples
bazel query "kind(cc_binary, //packages/meld-lang:all)" | grep demo

# Run specific example
bazel run //packages/meld-lang:ceylon_initialization_demo

# Run with arguments
bazel run //packages/meld-lang:meld -- examples/hello.meld
```

## Common Development Tasks

### Building and Testing

```bash
# Build specific component
bazel build //packages/meld-lang:parser

# Build with debug information
bazel build --compilation_mode=dbg //packages/meld-lang:meld

# Run tests with output
bazel test --test_output=all //packages/meld-lang:parser_tests

# Run specific test
bazel test //packages/meld-lang:primitives_test

# Run tests with coverage
bazel coverage //packages/meld-lang:tests
```

### Debugging

```bash
# Build with debug symbols
bazel build --compilation_mode=dbg //packages/meld-lang:meld

# Run under debugger
bazel run --run_under="gdb --args" //packages/meld-lang:meld

# Run test under debugger
bazel run --run_under="gdb --args" //packages/meld-lang:primitives_test
```

### Performance Analysis

```bash
# Profile build performance
bazel build --profile=profile.json //packages/meld-lang:meld

# Analyze profile (requires bazel-profile tool)
bazel analyze-profile profile.json

# Build with optimization
bazel build --compilation_mode=opt //packages/meld-lang:meld
```

## Code Style and Standards

### C++ Guidelines

1. **Follow C++23 best practices**
2. **Use modern C++ features** (concepts, ranges, modules when available)
3. **Prefer RAII** for resource management
4. **Use smart pointers** instead of raw pointers
5. **Follow Google C++ Style Guide** with project-specific modifications

### Naming Conventions

```cpp
// Namespaces: lowercase with underscores
namespace meld::kernel {}

// Classes: PascalCase
class SymbolTable {};

// Functions: camelCase
void processSymbol();

// Variables: camelCase
int symbolCount = 0;

// Constants: UPPER_CASE
const int MAX_SYMBOLS = 1000;

// Files: lowercase with underscores
// primitives.hpp, symbol_table.cpp
```

### Documentation

```cpp
/**
 * @brief Brief description of the class/function
 * 
 * Detailed description explaining the purpose,
 * behavior, and any important notes.
 * 
 * @param param1 Description of parameter
 * @param param2 Description of parameter
 * @return Description of return value
 * 
 * @example
 * ```cpp
 * SymbolTable table;
 * table.addSymbol("example");
 * ```
 */
class SymbolTable {
    // Implementation
};
```

## Testing Guidelines

### Unit Tests

```cpp
#include <gtest/gtest.h>
#include "meld/kernel/primitives.hpp"

namespace meld::kernel {

class PrimitivesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }
    
    void TearDown() override {
        // Cleanup code
    }
};

TEST_F(PrimitivesTest, SymbolCreation) {
    Symbol symbol("test");
    EXPECT_EQ(symbol.name(), "test");
    EXPECT_TRUE(symbol.isValid());
}

TEST_F(PrimitivesTest, SymbolComparison) {
    Symbol symbol1("test");
    Symbol symbol2("test");
    Symbol symbol3("other");
    
    EXPECT_EQ(symbol1, symbol2);
    EXPECT_NE(symbol1, symbol3);
}

} // namespace meld::kernel
```

### Property-Based Tests

```cpp
#include <gtest/gtest.h>
#include <random>
#include "meld/kernel/primitives.hpp"

namespace meld::kernel {

TEST(PrimitivesPropertyTest, SymbolRoundTrip) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100);
    
    for (int i = 0; i < 100; ++i) {
        // Generate random symbol name
        std::string name = "symbol_" + std::to_string(dis(gen));
        
        // Create symbol and serialize/deserialize
        Symbol original(name);
        std::string serialized = original.serialize();
        Symbol deserialized = Symbol::deserialize(serialized);
        
        // Property: round-trip should preserve equality
        EXPECT_EQ(original, deserialized);
    }
}

} // namespace meld::kernel
```

## Contributing Guidelines

### Before You Start

1. **Check existing issues** for similar work
2. **Discuss major changes** with the team
3. **Create an issue** for new features or bugs
4. **Follow the coding standards** outlined above

### Pull Request Process

1. **Create feature branch** from main
2. **Make focused commits** with clear messages
3. **Add tests** for new functionality
4. **Update documentation** as needed
5. **Ensure all tests pass**
6. **Submit pull request** with description

### Commit Message Format

```
type(scope): brief description

Detailed explanation of the change, including:
- What was changed and why
- Any breaking changes
- References to issues

Closes #123
```

**Types:** feat, fix, docs, style, refactor, test, chore

**Examples:**
```
feat(parser): add support for lambda expressions

Implements lambda expression parsing with proper precedence
and associativity rules. Includes comprehensive test coverage
for various lambda forms.

Closes #456

fix(kernel): resolve memory leak in symbol table

The symbol table was not properly releasing memory when
symbols were removed. Added proper cleanup in destructor.

Fixes #789
```

## Getting Help

### Documentation

- **[Migration Guide](MIGRATION_GUIDE.md)**: Migrating from old CMake system
- **[Bazel Commands](BAZEL_COMMANDS.md)**: Complete command reference
- **[Troubleshooting](TROUBLESHOOTING.md)**: Common issues and solutions
- **[Build Guide](BUILD_GUIDE.md)**: Detailed build instructions

### Community

- **GitHub Issues**: Report bugs and request features
- **Team Chat**: Internal Slack/Discord channels
- **Code Reviews**: Learn from feedback on pull requests
- **Pair Programming**: Work with experienced team members

### Learning Resources

- **Bazel Documentation**: https://bazel.build/docs
- **C++23 Reference**: https://en.cppreference.com/
- **Google Test**: https://google.github.io/googletest/
- **Meld Language Spec**: See `packages/meld-core/docs/`

## Next Steps

1. **Complete the setup** following this guide
2. **Explore the codebase** by reading existing code
3. **Run examples** to understand language features
4. **Pick a good first issue** to start contributing
5. **Ask questions** when you need help

Welcome to the Meld development team! 🎉