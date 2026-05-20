# Meld Build Tools

Build tools and utilities for Meld programming language projects. This package provides comprehensive build system integration and project management capabilities.

## Overview

This package provides build tools and project management utilities for Meld, including:

- Project creation and scaffolding
- Build system integration
- Package management
- Development tools (formatting, linting, documentation generation)
- Dependency resolution

## Status

This is currently a placeholder package with minimal stub implementation. Full build tool functionality will be implemented in future iterations.

## Dependencies

- `//packages/meld-lang:compiler` - For compiling Meld source code

## Build

```bash
bazel build //packages/meld-build:build-tool
```

## Usage

```bash
# Create a new Meld project
bazel run //packages/meld-build:build-tool -- new my-project

# Build a Meld project
bazel run //packages/meld-build:build-tool -- build

# Run tests
bazel run //packages/meld-build:build-tool -- test
```

## Architecture

The build tool will be structured as follows:

- **Project Management**: Project creation and configuration
- **Build System Integration**: Integration with various build systems
- **Package Management**: Dependency resolution and package publishing
- **Development Tools**: Formatting, linting, and documentation generation