# Requirements Document

## Introduction

This document specifies the requirements for a Visual Studio Code extension that provides comprehensive syntax highlighting, language support, and development tools for the Meld programming language. The extension will enable developers to write, edit, and debug Meld code with full IDE support including syntax highlighting, IntelliSense, error detection, and integration with the Meld Language Server Protocol (LSP) implementation.

### Cross-References

- **meld-lsp-server**: The LSP server that powers IntelliSense, diagnostics, and navigation features in this extension.
- **meld-cli**: The extension invokes CLI commands (`meld run`, `meld test`, `meld fmt`, etc.) for build/run/test workflows.
- **meld-daemon**: The daemon process hosts the LSP server that this extension connects to (meld-daemon Req 1).

## Glossary

- **VSCode_Extension**: The Visual Studio Code extension package that provides Meld language support
- **Meld_Language**: The modern programming language with features like immutable-by-default variables, pattern matching, and multiple dispatch
- **Language_Server**: The Meld LSP server that provides language intelligence features
- **Syntax_Highlighter**: The component responsible for colorizing Meld source code
- **TextMate_Grammar**: The grammar definition file that defines syntax highlighting rules for Meld
- **Extension_Host**: The VS Code process that runs extensions
- **Language_Client**: The LSP client component that communicates with the Meld language server
- **Command_Palette**: VS Code's command interface accessible via Ctrl+Shift+P
- **File_Association**: The mapping between .meld file extensions and the Meld language mode
- **Snippet_Provider**: Component that provides code completion templates
- **Theme_Integration**: Compatibility with VS Code color themes for syntax highlighting

## Requirements

### Requirement 1

**User Story:** As a Meld developer, I want syntax highlighting for Meld source files, so that I can easily read and understand code structure.

#### Acceptance Criteria

1. WHEN a user opens a .meld file, THE VSCode_Extension SHALL apply Meld syntax highlighting automatically
2. WHEN displaying Meld keywords (val, var, fn, struct, class, extends, if, else, while, for), THE Syntax_Highlighter SHALL color them according to the active theme's keyword color
3. WHEN displaying Meld string literals, THE Syntax_Highlighter SHALL highlight them with proper escape sequence recognition
4. WHEN displaying Meld comments (both // and /* */ styles), THE Syntax_Highlighter SHALL apply comment styling consistently
5. WHEN displaying Meld numeric literals, THE Syntax_Highlighter SHALL highlight integers, floats, and scientific notation appropriately

### Requirement 2

**User Story:** As a Meld developer, I want IntelliSense support, so that I can write code efficiently with autocompletion and error detection.

#### Acceptance Criteria

1. WHEN a user types in a .meld file, THE Language_Client SHALL provide autocompletion suggestions for variables, functions, and types in scope
2. WHEN the Meld code contains syntax errors, THE Language_Server SHALL report diagnostics with precise error locations and descriptions
3. WHEN a user hovers over a symbol, THE VSCode_Extension SHALL display type information and documentation
4. WHEN a user requests "Go to Definition" on a symbol, THE Language_Server SHALL navigate to the symbol's declaration
5. WHEN a user requests "Find All References", THE Language_Server SHALL locate all usages of the selected symbol

### Requirement 3

**User Story:** As a Meld developer, I want code formatting and organization features, so that I can maintain consistent code style.

#### Acceptance Criteria

1. WHEN a user triggers document formatting, THE Language_Server SHALL format the entire Meld file according to standard conventions
2. WHEN a user triggers selection formatting, THE Language_Server SHALL format only the selected code region
3. WHEN a user enables format-on-save, THE VSCode_Extension SHALL automatically format Meld files when saved
4. WHEN displaying code structure, THE VSCode_Extension SHALL provide an outline view showing functions, classes, and structs
5. WHEN a user folds code blocks, THE VSCode_Extension SHALL support folding for functions, classes, structs, and control flow blocks

### Requirement 4

**User Story:** As a Meld developer, I want build and execution integration, so that I can compile and run Meld programs from within VS Code.

#### Acceptance Criteria

1. WHEN a user executes the "Build Meld Project" command, THE VSCode_Extension SHALL invoke the Meld compiler and display build results
2. WHEN compilation errors occur, THE VSCode_Extension SHALL display error messages with clickable links to error locations
3. WHEN a user executes the "Run Meld Program" command, THE VSCode_Extension SHALL execute the compiled program and show output
4. WHEN build tasks are configured, THE VSCode_Extension SHALL integrate with VS Code's task system for custom build workflows
5. WHEN debugging is initiated, THE VSCode_Extension SHALL support basic debugging capabilities through the Meld runtime

### Requirement 5

**User Story:** As a Meld developer, I want code snippets and templates, so that I can quickly generate common code patterns.

#### Acceptance Criteria

1. WHEN a user types snippet triggers (like "fn", "struct", "class"), THE Snippet_Provider SHALL offer code completion with template expansion
2. WHEN a user selects a snippet, THE VSCode_Extension SHALL insert the template with placeholder fields for customization
3. WHEN navigating snippet placeholders, THE VSCode_Extension SHALL allow tab navigation between editable fields
4. WHEN creating new Meld files, THE VSCode_Extension SHALL offer file templates for common patterns (main function, class definition, etc.)
5. WHEN snippets are expanded, THE VSCode_Extension SHALL maintain proper indentation and formatting

### Requirement 6

**User Story:** As a Meld developer, I want seamless integration with VS Code features, so that the Meld extension works consistently with my development workflow.

#### Acceptance Criteria

1. WHEN VS Code starts, THE VSCode_Extension SHALL register the .meld file association automatically
2. WHEN using VS Code themes, THE Theme_Integration SHALL ensure Meld syntax highlighting adapts to all standard and custom themes
3. WHEN accessing the Command_Palette, THE VSCode_Extension SHALL provide Meld-specific commands with clear descriptions
4. WHEN working with multi-root workspaces, THE VSCode_Extension SHALL handle Meld projects in any workspace folder
5. WHEN the extension updates, THE VSCode_Extension SHALL maintain backward compatibility with existing Meld projects

### Requirement 7

**User Story:** As a Meld developer, I want language server integration, so that I can benefit from advanced language features powered by the Meld LSP server.

#### Acceptance Criteria

1. WHEN the extension activates, THE Language_Client SHALL automatically connect to the Meld Language Server if available
2. WHEN the language server is unavailable, THE VSCode_Extension SHALL provide graceful degradation with basic syntax highlighting only
3. WHEN language server communication fails, THE Language_Client SHALL attempt reconnection and display appropriate status messages
4. WHEN multiple Meld files are open, THE Language_Server SHALL maintain consistent state across all open documents
5. WHEN the language server provides semantic tokens, THE VSCode_Extension SHALL use them for enhanced syntax highlighting

### Requirement 8

**User Story:** As a developer setting up a Meld development environment, I want easy installation and configuration, so that I can start developing quickly.

#### Acceptance Criteria

1. WHEN a user installs the extension from the VS Code marketplace, THE VSCode_Extension SHALL activate automatically for .meld files
2. WHEN the extension requires the Meld language server, THE VSCode_Extension SHALL provide clear instructions for installation and setup
3. WHEN configuration is needed, THE VSCode_Extension SHALL expose relevant settings through VS Code's settings interface
4. WHEN troubleshooting issues, THE VSCode_Extension SHALL provide diagnostic commands and helpful error messages
5. WHEN the extension is first used, THE VSCode_Extension SHALL display welcome information and setup guidance

### Requirement 9: Seamless Debug Routing

> **Note:** This requirement was moved from meld-cli Requirement 19, as it specifies VS Code extension behavior. It depends on meld-cli Req 18 (`meld debug`) for the underlying debug backends.

**User Story:** As a Meld developer using VS Code, I want to click "Debug" and have Meld automatically route to the correct debugging backend (AST interpreter DAP or LLDB for compiled binaries), so that I get a seamless debugging experience without manual configuration.

#### Acceptance Criteria

1. THE VSCode_Extension SHALL provide a `MeldDebugAdapterDescriptorFactory` that inspects the launch configuration to determine the debug mode
2. WHEN the launch configuration specifies `"mode": "interpret"` (or no mode, as default), THE VSCode_Extension SHALL launch `meld run --debug --debug-wait` and connect the VS Code DAP client to the interpreter's DAP server
3. WHEN the launch configuration specifies `"mode": "compiled"`, THE VSCode_Extension SHALL launch the compiled binary under LLDB (via `meld debug attach` or CodeLLDB) with Meld data formatters auto-loaded
4. THE VSCode_Extension SHALL provide a default `launch.json` template with both `"Meld: Interpret"` and `"Meld: Compiled"` configurations pre-populated
5. THE VSCode_Extension SHALL auto-detect whether the project has a compiled binary in `build/` and offer the appropriate debug configuration
6. WHEN the user sets a breakpoint in a `.meld` file, THE VSCode_Extension SHALL map the breakpoint to the correct backend (AST interpreter breakpoint for interpret mode, DWARF source breakpoint for compiled mode)
7. THE VSCode_Extension SHALL configure LLDB `initCommands` to auto-load `meld_formatters.py` when debugging compiled binaries, so kernel types display correctly in the Variables panel