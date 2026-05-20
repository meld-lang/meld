# VS Code Meld Plugin Design Document

## Overview

The VS Code Meld Plugin is a comprehensive language extension that provides full development support for the Meld programming language. The extension follows VS Code's extension architecture patterns and integrates with the existing Meld Language Server Protocol (LSP) implementation to deliver features like syntax highlighting, IntelliSense, error detection, code formatting, and build integration.

The extension is designed as a TypeScript-based VS Code extension that acts as an LSP client, communicating with the Meld LSP server for advanced language features while providing immediate syntax highlighting and basic functionality even when the language server is unavailable.

## Architecture

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    VS Code Extension Host                    │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐ │
│  │   Syntax        │  │   Language      │  │   Command   │ │
│  │   Highlighter   │  │   Client        │  │   Provider  │ │
│  └─────────────────┘  └─────────────────┘  └─────────────┘ │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐ │
│  │   Snippet       │  │   Task          │  │   Config    │ │
│  │   Provider      │  │   Provider      │  │   Manager   │ │
│  └─────────────────┘  └─────────────────┘  └─────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                    LSP Communication Layer                   │
└─────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────┐
│                    Meld LSP Server                          │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐ │
│  │   Parser        │  │   Type Checker  │  │   Formatter │ │
│  └─────────────────┘  └─────────────────┘  └─────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### Component Interaction Flow

1. **File Opening**: User opens .meld file → Extension activates → Syntax highlighting applied immediately
2. **Language Server**: Extension starts LSP client → Connects to Meld LSP server → Enhanced features available
3. **User Interaction**: User types/edits → LSP client sends changes → Server provides diagnostics/completions
4. **Build Integration**: User triggers build → Extension invokes Meld CLI → Results displayed in VS Code

## Components and Interfaces

### Core Extension Components

#### 1. Extension Main (`extension.ts`)
- **Purpose**: Entry point for the extension, handles activation and deactivation
- **Responsibilities**:
  - Register language configuration and providers
  - Initialize LSP client
  - Set up command handlers
  - Manage extension lifecycle

#### 2. Language Client (`languageClient.ts`)
- **Purpose**: LSP client implementation for communicating with Meld LSP server
- **Responsibilities**:
  - Establish connection to Meld LSP server
  - Handle server lifecycle (start, stop, restart)
  - Provide graceful degradation when server unavailable
  - Manage document synchronization

#### 3. Syntax Provider (`syntaxProvider.ts`)
- **Purpose**: Manages TextMate grammar and syntax highlighting
- **Responsibilities**:
  - Load and register Meld TextMate grammar
  - Provide immediate syntax highlighting
  - Handle theme integration
  - Support semantic token highlighting from LSP

#### 4. Command Provider (`commandProvider.ts`)
- **Purpose**: Implements Meld-specific VS Code commands
- **Responsibilities**:
  - Build and run commands
  - Language server management commands
  - Diagnostic and troubleshooting commands
  - File template creation commands

#### 5. Snippet Provider (`snippetProvider.ts`)
- **Purpose**: Provides code snippets and templates
- **Responsibilities**:
  - Load snippet definitions
  - Handle snippet expansion
  - Manage placeholder navigation
  - Provide context-aware snippets

#### 6. Task Provider (`taskProvider.ts`)
- **Purpose**: Integrates with VS Code task system for build automation
- **Responsibilities**:
  - Detect Meld projects
  - Generate build tasks
  - Handle task execution
  - Parse build output

### External Interfaces

#### LSP Server Interface
```typescript
interface MeldLSPCapabilities {
  textDocumentSync: TextDocumentSyncKind;
  completionProvider: CompletionOptions;
  hoverProvider: boolean;
  definitionProvider: boolean;
  referencesProvider: boolean;
  documentFormattingProvider: boolean;
  documentRangeFormattingProvider: boolean;
  semanticTokensProvider: SemanticTokensOptions;
  diagnosticProvider: DiagnosticOptions;
}
```

#### Configuration Interface
```typescript
interface MeldExtensionConfig {
  languageServer: {
    enabled: boolean;
    path?: string;
    args?: string[];
    trace: 'off' | 'messages' | 'verbose';
  };
  formatting: {
    enable: boolean;
    formatOnSave: boolean;
    indentSize: number;
  };
  build: {
    defaultTarget: string;
    showBuildOutput: boolean;
    clearOutputOnBuild: boolean;
  };
}
```

## Data Models

### Document Model
```typescript
interface MeldDocument {
  uri: string;
  languageId: 'meld';
  version: number;
  content: string;
  diagnostics: Diagnostic[];
  symbols: DocumentSymbol[];
}
```

### Symbol Information
```typescript
interface MeldSymbol {
  name: string;
  kind: SymbolKind;
  location: Location;
  containerName?: string;
  detail?: string;
  documentation?: string;
}
```

### Build Configuration
```typescript
interface MeldBuildConfig {
  projectRoot: string;
  buildFile?: string;
  targets: BuildTarget[];
  defaultTarget: string;
}

interface BuildTarget {
  name: string;
  command: string;
  args: string[];
  group: 'build' | 'test' | 'run';
}
```

### Snippet Definition
```typescript
interface MeldSnippet {
  prefix: string;
  body: string[];
  description: string;
  scope?: string;
  context?: SnippetContext;
}

interface SnippetContext {
  inFunction?: boolean;
  inClass?: boolean;
  inStruct?: boolean;
  topLevel?: boolean;
}
```

## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system-essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

Based on the prework analysis, I'll now define the correctness properties that must hold for the VS Code Meld plugin:

**Property 1: File association activation**
*For any* .meld file opened in VS Code, the extension should automatically activate and apply syntax highlighting
**Validates: Requirements 1.1**

**Property 2: Keyword highlighting consistency**
*For any* Meld code containing keywords (val, var, fn, struct, class, extends, if, else, while, for), all keywords should be highlighted according to the active theme's keyword color
**Validates: Requirements 1.2**

**Property 3: String literal highlighting accuracy**
*For any* Meld string literal with escape sequences, the syntax highlighter should correctly recognize and highlight both the string boundaries and escape sequences
**Validates: Requirements 1.3**

**Property 4: Comment highlighting uniformity**
*For any* Meld code containing comments (// or /* */), all comments should receive consistent styling regardless of content or location
**Validates: Requirements 1.4**

**Property 5: Numeric literal recognition**
*For any* valid Meld numeric literal (integer, float, scientific notation), the syntax highlighter should apply appropriate numeric styling
**Validates: Requirements 1.5**

**Property 6: Autocompletion provision**
*For any* typing context in a .meld file, the language client should provide relevant autocompletion suggestions for symbols in scope
**Validates: Requirements 2.1**

**Property 7: Diagnostic accuracy**
*For any* Meld code containing syntax errors, the language server should report diagnostics with precise locations and descriptive error messages
**Validates: Requirements 2.2**

**Property 8: Hover information availability**
*For any* symbol in Meld code, hovering should display type information and documentation when available
**Validates: Requirements 2.3**

**Property 9: Definition navigation correctness**
*For any* symbol with a definition, "Go to Definition" should navigate to the correct declaration location
**Validates: Requirements 2.4**

**Property 10: Reference finding completeness**
*For any* symbol in Meld code, "Find All References" should locate all actual usages of that symbol
**Validates: Requirements 2.5**

**Property 11: Document formatting consistency**
*For any* Meld file, document formatting should produce output that conforms to standard Meld formatting conventions
**Validates: Requirements 3.1**

**Property 12: Selection formatting precision**
*For any* selected code region in a Meld file, selection formatting should modify only the selected content while preserving surrounding code
**Validates: Requirements 3.2**

**Property 13: Format-on-save automation**
*For any* Meld file when format-on-save is enabled, saving the file should automatically apply formatting
**Validates: Requirements 3.3**

**Property 14: Outline completeness**
*For any* Meld file containing functions, classes, and structs, the outline view should display all top-level symbols with correct hierarchy
**Validates: Requirements 3.4**

**Property 15: Code folding support**
*For any* foldable code construct (functions, classes, structs, control blocks) in Meld, the extension should support folding and unfolding
**Validates: Requirements 3.5**

**Property 16: Build command execution**
*For any* Meld project, executing the "Build Meld Project" command should invoke the compiler and display build results
**Validates: Requirements 4.1**

**Property 17: Compilation error linking**
*For any* compilation error, the error message should include clickable links that navigate to the exact error location
**Validates: Requirements 4.2**

**Property 18: Program execution output**
*For any* executable Meld program, the "Run Meld Program" command should execute the program and display its output
**Validates: Requirements 4.3**

**Property 19: Task system integration**
*For any* configured build task, the extension should properly integrate with VS Code's task system for execution
**Validates: Requirements 4.4**

**Property 20: Debugging capability**
*For any* Meld program, initiating debugging should provide basic debugging functionality through the Meld runtime
**Validates: Requirements 4.5**

**Property 21: Snippet trigger recognition**
*For any* snippet trigger (fn, struct, class), typing the trigger should offer the corresponding code completion with template expansion
**Validates: Requirements 5.1**

**Property 22: Snippet template insertion**
*For any* selected snippet, the extension should insert the complete template with properly positioned placeholder fields
**Validates: Requirements 5.2**

**Property 23: Placeholder navigation**
*For any* expanded snippet with placeholders, tab navigation should move between editable fields in the correct order
**Validates: Requirements 5.3**

**Property 24: File template availability**
*For any* new Meld file creation, the extension should offer appropriate file templates for common patterns
**Validates: Requirements 5.4**

**Property 25: Snippet formatting preservation**
*For any* expanded snippet, the inserted code should maintain proper indentation and formatting relative to the insertion context
**Validates: Requirements 5.5**

**Property 26: File association registration**
*For any* VS Code startup with the extension installed, the .meld file association should be automatically registered
**Validates: Requirements 6.1**

**Property 27: Theme adaptation**
*For any* VS Code theme (standard or custom), Meld syntax highlighting should adapt appropriately to the theme's color scheme
**Validates: Requirements 6.2**

**Property 28: Command palette integration**
*For any* access to the command palette, Meld-specific commands should be available with clear, descriptive names
**Validates: Requirements 6.3**

**Property 29: Multi-root workspace support**
*For any* multi-root workspace containing Meld projects, the extension should handle projects in all workspace folders correctly
**Validates: Requirements 6.4**

**Property 30: Language server connection**
*For any* extension activation when the Meld language server is available, the language client should automatically establish connection
**Validates: Requirements 7.1**

**Property 31: Graceful degradation**
*For any* scenario where the language server is unavailable, the extension should provide basic syntax highlighting functionality
**Validates: Requirements 7.2**

**Property 32: Connection recovery**
*For any* language server communication failure, the client should attempt reconnection and display appropriate status messages
**Validates: Requirements 7.3**

**Property 33: Multi-document state consistency**
*For any* set of open Meld files, the language server should maintain consistent state and cross-file symbol resolution
**Validates: Requirements 7.4**

**Property 34: Semantic token integration**
*For any* semantic tokens provided by the language server, the extension should use them to enhance syntax highlighting
**Validates: Requirements 7.5**

**Property 35: Installation activation**
*For any* extension installation from the VS Code marketplace, the extension should automatically activate for .meld files
**Validates: Requirements 8.1**

**Property 36: Setup guidance provision**
*For any* scenario requiring the Meld language server, the extension should provide clear installation and setup instructions
**Validates: Requirements 8.2**

**Property 37: Settings exposure**
*For any* configurable extension setting, it should be properly exposed through VS Code's settings interface
**Validates: Requirements 8.3**

**Property 38: Diagnostic command availability**
*For any* troubleshooting scenario, the extension should provide diagnostic commands and helpful error messages
**Validates: Requirements 8.4**

**Property 39: Welcome experience**
*For any* first-time use of the extension, welcome information and setup guidance should be displayed
**Validates: Requirements 8.5**

## Error Handling

### Language Server Connection Errors
- **Connection Timeout**: Implement exponential backoff for reconnection attempts
- **Server Crash Recovery**: Automatically restart the language server with user notification
- **Protocol Errors**: Log detailed error information and provide user-friendly messages
- **Version Mismatch**: Detect and warn about incompatible language server versions

### File System Errors
- **File Access Permissions**: Handle read/write permission errors gracefully
- **Missing Files**: Provide clear error messages for missing Meld files or dependencies
- **Large File Handling**: Implement size limits and streaming for large Meld files
- **Encoding Issues**: Support UTF-8 encoding with fallback handling

### Build System Errors
- **Compiler Not Found**: Detect missing Meld compiler and provide installation guidance
- **Build Configuration**: Validate build configuration and provide helpful error messages
- **Output Parsing**: Handle malformed compiler output gracefully
- **Process Management**: Properly handle build process lifecycle and cleanup

### Extension Lifecycle Errors
- **Activation Failures**: Log activation errors and provide recovery options
- **Configuration Errors**: Validate extension settings and provide defaults
- **Resource Cleanup**: Ensure proper cleanup of resources on deactivation
- **Update Compatibility**: Handle extension updates and migration scenarios

## Testing Strategy

### Dual Testing Approach

The VS Code Meld Plugin will employ both unit testing and property-based testing to ensure comprehensive correctness validation:

#### Unit Testing
Unit tests will verify specific examples, edge cases, and integration points:
- **Syntax Highlighting**: Test specific code samples with known highlighting expectations
- **LSP Integration**: Test specific LSP message exchanges and responses
- **Command Execution**: Test specific build and run scenarios with known outcomes
- **Configuration**: Test specific configuration scenarios and edge cases
- **Error Handling**: Test specific error conditions and recovery scenarios

#### Property-Based Testing
Property-based tests will verify universal properties across all inputs using **fast-check** as the property-based testing library. Each property-based test will run a minimum of 100 iterations to ensure thorough coverage.

**Property-Based Testing Requirements**:
- Each correctness property will be implemented as a single property-based test
- Tests will be tagged with comments referencing the design document property: **Feature: vscode-meld, Property {number}: {property_text}**
- Generators will create valid Meld code samples, file structures, and extension states
- Properties will verify behavior across all generated inputs

**Key Property Test Categories**:
1. **Syntax Highlighting Properties**: Generate various Meld code patterns and verify consistent highlighting
2. **LSP Communication Properties**: Generate various document states and verify correct LSP behavior
3. **File Association Properties**: Generate various file scenarios and verify correct extension activation
4. **Command Execution Properties**: Generate various project states and verify correct command behavior
5. **Configuration Properties**: Generate various settings combinations and verify correct behavior

#### Integration Testing
- **End-to-End Workflows**: Test complete development workflows from file creation to execution
- **Multi-File Projects**: Test behavior across complex project structures
- **Theme Compatibility**: Test syntax highlighting across various VS Code themes
- **Performance**: Test extension performance with large files and projects

#### Test Environment Setup
- **VS Code Extension Test Framework**: Use VS Code's extension testing framework for integration tests
- **Mock LSP Server**: Create mock language server for testing LSP integration without dependencies
- **Test Workspaces**: Maintain test Meld projects for consistent testing scenarios
- **Automated CI/CD**: Run all tests automatically on code changes and releases

The testing strategy ensures that both specific scenarios work correctly (unit tests) and that universal properties hold across all possible inputs (property-based tests), providing comprehensive validation of the extension's correctness.