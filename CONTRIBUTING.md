# Contributing to Meld

Thank you for your interest in contributing to Meld!

## Getting Started

### Prerequisites

- Bazel 9.0+
- C++23 compiler (GCC 13+, Clang 16+, or MSVC 17.5+)

### Building

```bash
bazel build //meld-cli:meld
bazel test //meld-interpreter/...
```

### Running Examples

```bash
meld run meld-examples/examples/01-hello-world.meld --text
```

### Running Conformance Tests

```bash
./meld-conformance/run.sh
```

## Project Structure

- `meld-core/` — Parser (Boost.Spirit X3), kernel, effects, type checker, standard library
- `meld-core/std/` — Standard library (pure Meld)
- `meld-interpreter/` — AST interpreter
- `meld-daemon/` — Language server daemon (LSP, MCP, DAP channels)
- `meld-cli/` — Unified CLI (`meld run`, `meld explain`, `meld fix`, etc.)
- `meld-examples/` — 60+ executable examples
- `meld-conformance/` — Fixture-based conformance tests
- `vscode-meld/` — VS Code extension

## Code Style

- Meld source files use kebab-case: `my-function`, `user-name`
- C++ follows the existing codebase conventions
- All Meld examples must run: `meld run --text file.meld`

## License

By contributing, you agree that your contributions will be licensed under the same terms as the project (MIT/Apache-2.0).
