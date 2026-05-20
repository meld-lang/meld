# Implementation Plan

- [x] 1. Set up VS Code extension project structure
  - Create TypeScript-based VS Code extension project with proper package.json configuration
  - Set up build system with webpack and TypeScript compilation
  - Configure extension manifest with Meld language contribution points
  - Set up testing framework with VS Code extension test runner
  - _Requirements: 6.1, 8.1_

- [x] 1.1 Write property test for extension activation
  - **Property 26: File association registration**
  - **Validates: Requirements 6.1**

- [x] 2. Implement TextMate grammar for Meld syntax highlighting
  - [x] 2.1 Create Meld TextMate grammar definition file
    - Write grammar rules for Meld keywords, operators, and language constructs
    - Define token patterns for strings, comments, numbers, and identifiers
    - Implement proper scope naming following TextMate conventions
    - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5_

  - [ ] 2.2 Write property test for keyword highlighting
    - **Property 2: Keyword highlighting consistency**
    - **Validates: Requirements 1.2**

  - [ ] 2.3 Write property test for string literal highlighting
    - **Property 3: String literal highlighting accuracy**
    - **Validates: Requirements 1.3**

  - [ ] 2.4 Write property test for comment highlighting
    - **Property 4: Comment highlighting uniformity**
    - **Validates: Requirements 1.4**

  - [ ] 2.5 Write property test for numeric literal recognition
    - **Property 5: Numeric literal recognition**
    - **Validates: Requirements 1.5**

  - [x] 2.6 Implement syntax provider component
    - Create SyntaxProvider class to manage TextMate grammar loading
    - Implement theme integration for syntax highlighting adaptation
    - Handle grammar registration and activation lifecycle
    - _Requirements: 1.1, 6.2_

  - [ ] 2.7 Write property test for theme adaptation
    - **Property 27: Theme adaptation**
    - **Validates: Requirements 6.2**

- [x] 3. Implement Language Server Protocol client
  - [x] 3.1 Create LSP client infrastructure
    - Implement LanguageClient class using vscode-languageclient library
    - Set up server connection management and lifecycle handling
    - Implement graceful degradation when server is unavailable
    - _Requirements: 7.1, 7.2, 7.3_

  - [ ] 3.2 Write property test for language server connection
    - **Property 30: Language server connection**
    - **Validates: Requirements 7.1**

  - [ ] 3.3 Write property test for graceful degradation
    - **Property 31: Graceful degradation**
    - **Validates: Requirements 7.2**

  - [ ] 3.4 Write property test for connection recovery
    - **Property 32: Connection recovery**
    - **Validates: Requirements 7.3**

  - [x] 3.5 Implement LSP feature providers
    - Create completion provider for autocompletion functionality
    - Implement hover provider for symbol information display
    - Set up definition and reference providers for navigation
    - Implement diagnostic provider for error reporting
    - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5_

  - [ ] 3.6 Write property test for autocompletion provision
    - **Property 6: Autocompletion provision**
    - **Validates: Requirements 2.1**

  - [ ] 3.7 Write property test for diagnostic accuracy
    - **Property 7: Diagnostic accuracy**
    - **Validates: Requirements 2.2**

  - [ ] 3.8 Write property test for hover information
    - **Property 8: Hover information availability**
    - **Validates: Requirements 2.3**

  - [ ] 3.9 Write property test for definition navigation
    - **Property 9: Definition navigation correctness**
    - **Validates: Requirements 2.4**

  - [ ] 3.10 Write property test for reference finding
    - **Property 10: Reference finding completeness**
    - **Validates: Requirements 2.5**

- [x] 4. Implement document formatting and code organization
  - [x] 4.1 Create formatting provider
    - Implement document and range formatting providers
    - Set up format-on-save functionality with configuration
    - Handle formatting errors and user feedback
    - _Requirements: 3.1, 3.2, 3.3_

  - [ ] 4.2 Write property test for document formatting
    - **Property 11: Document formatting consistency**
    - **Validates: Requirements 3.1**

  - [ ] 4.3 Write property test for selection formatting
    - **Property 12: Selection formatting precision**
    - **Validates: Requirements 3.2**

  - [ ] 4.4 Write property test for format-on-save
    - **Property 13: Format-on-save automation**
    - **Validates: Requirements 3.3**

  - [x] 4.5 Implement code structure providers
    - Create document symbol provider for outline view
    - Implement folding range provider for code folding
    - Set up semantic token provider integration
    - _Requirements: 3.4, 3.5, 7.5_

  - [ ] 4.6 Write property test for outline completeness
    - **Property 14: Outline completeness**
    - **Validates: Requirements 3.4**

  - [ ] 4.7 Write property test for code folding
    - **Property 15: Code folding support**
    - **Validates: Requirements 3.5**

  - [ ] 4.8 Write property test for semantic token integration
    - **Property 34: Semantic token integration**
    - **Validates: Requirements 7.5**

- [x] 5. Checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [x] 6. Implement build and execution integration
  - [x] 6.1 Create command provider for build operations
    - Implement "Build Meld Project" command with compiler invocation
    - Create "Run Meld Program" command for program execution
    - Set up build output parsing and error message linking
    - _Requirements: 4.1, 4.2, 4.3_

  - [ ] 6.2 Write property test for build command execution
    - **Property 16: Build command execution**
    - **Validates: Requirements 4.1**

  - [ ] 6.3 Write property test for compilation error linking
    - **Property 17: Compilation error linking**
    - **Validates: Requirements 4.2**

  - [ ] 6.4 Write property test for program execution
    - **Property 18: Program execution output**
    - **Validates: Requirements 4.3**

  - [x] 6.5 Implement task provider for VS Code integration
    - Create TaskProvider for custom build workflows
    - Implement task detection and configuration
    - Set up debugging support integration
    - _Requirements: 4.4, 4.5_

  - [ ] 6.6 Write property test for task system integration
    - **Property 19: Task system integration**
    - **Validates: Requirements 4.4**

  - [ ] 6.7 Write property test for debugging capability
    - **Property 20: Debugging capability**
    - **Validates: Requirements 4.5**

- [x] 7. Implement code snippets and templates
  - [x] 7.1 Create snippet definitions and provider
    - Define Meld code snippets for common patterns (functions, classes, structs)
    - Implement SnippetProvider with context-aware snippet suggestions
    - Set up snippet expansion with placeholder navigation
    - _Requirements: 5.1, 5.2, 5.3_

  - [ ] 7.2 Write property test for snippet trigger recognition
    - **Property 21: Snippet trigger recognition**
    - **Validates: Requirements 5.1**

  - [ ] 7.3 Write property test for snippet template insertion
    - **Property 22: Snippet template insertion**
    - **Validates: Requirements 5.2**

  - [ ] 7.4 Write property test for placeholder navigation
    - **Property 23: Placeholder navigation**
    - **Validates: Requirements 5.3**

  - [x] 7.5 Implement file templates and formatting
    - Create file template system for new Meld files
    - Implement proper indentation and formatting for snippets
    - Set up template customization and user preferences
    - _Requirements: 5.4, 5.5_

  - [ ] 7.6 Write property test for file template availability
    - **Property 24: File template availability**
    - **Validates: Requirements 5.4**

  - [ ] 7.7 Write property test for snippet formatting
    - **Property 25: Snippet formatting preservation**
    - **Validates: Requirements 5.5**

- [x] 8. Implement VS Code integration features
  - [x] 8.1 Create command palette integration
    - Register all Meld-specific commands with clear descriptions
    - Implement command categorization and organization
    - Set up command enablement conditions and contexts
    - _Requirements: 6.3_

  - [ ] 8.2 Write property test for command palette integration
    - **Property 28: Command palette integration**
    - **Validates: Requirements 6.3**

  - [x] 8.3 Implement multi-root workspace support
    - Handle Meld projects across multiple workspace folders
    - Implement workspace-specific configuration and state management
    - Set up cross-workspace symbol resolution
    - _Requirements: 6.4, 7.4_

  - [ ] 8.4 Write property test for multi-root workspace support
    - **Property 29: Multi-root workspace support**
    - **Validates: Requirements 6.4**

  - [ ] 8.5 Write property test for multi-document state consistency
    - **Property 33: Multi-document state consistency**
    - **Validates: Requirements 7.4**

- [x] 9. Implement configuration and setup features
  - [x] 9.1 Create configuration management
    - Define extension configuration schema with proper defaults
    - Implement settings validation and change handling
    - Set up configuration UI integration with VS Code settings
    - _Requirements: 8.3_

  - [ ] 9.2 Write property test for settings exposure
    - **Property 37: Settings exposure**
    - **Validates: Requirements 8.3**

  - [x] 9.3 Implement setup and diagnostic features
    - Create language server installation guidance system
    - Implement diagnostic commands for troubleshooting
    - Set up welcome experience for first-time users
    - _Requirements: 8.2, 8.4, 8.5_

  - [ ] 9.4 Write property test for setup guidance
    - **Property 36: Setup guidance provision**
    - **Validates: Requirements 8.2**

  - [ ] 9.5 Write property test for diagnostic commands
    - **Property 38: Diagnostic command availability**
    - **Validates: Requirements 8.4**

  - [ ] 9.6 Write property test for welcome experience
    - **Property 39: Welcome experience**
    - **Validates: Requirements 8.5**

- [x] 10. Implement extension lifecycle and error handling
  - [x] 10.1 Create extension main module
    - Implement extension activation and deactivation lifecycle
    - Set up proper resource cleanup and error handling
    - Create extension context management and state persistence
    - _Requirements: 8.1_

  - [ ] 10.2 Write property test for installation activation
    - **Property 35: Installation activation**
    - **Validates: Requirements 8.1**

  - [x] 10.3 Implement comprehensive error handling
    - Create error handling for LSP communication failures
    - Implement file system error handling and recovery
    - Set up build system error handling and user feedback
    - Handle extension lifecycle errors and cleanup
    - _Requirements: 7.3, 4.2_

- [x] 11. Integrate semantic token support from meldd LSP server
  - The daemon's `SemanticTokenProvider` exposes 14 token types (Keyword, Function, Variable, Type, Parameter, Property, String, Number, Comment, Operator, Decorator, Effect, Macro, Namespace). The extension currently has no semantic token wiring — VS Code falls back to TextMate-only highlighting and misses Meld-specific tokens like Effect, Macro, and ownership markers.

  - [x] 11.1 Declare semantic token legend in package.json
    - Add `semanticTokenScopes` contribution to `package.json` mapping the daemon's 14 token types to VS Code theme scopes
    - This lets VS Code map server-provided semantic tokens to theme colors for Effect, Macro, Namespace, Decorator, etc.
    - _Requirements: 7.5, 1.1_

  - [x] 11.2 Configure LSP client to opt into semantic tokens
    - Update `LanguageClientOptions` in `languageClient.ts` to include `semanticTokensProvider` capability negotiation
    - Ensure the client requests `textDocument/semanticTokens/full` from the daemon
    - _Requirements: 7.5_

  - [x] 11.3 Write property test for semantic token legend completeness
    - **Property 34: Semantic token integration** (existing property, needs test)
    - Verify that all 14 daemon token types are declared in the extension's legend and map to valid VS Code scopes
    - **Validates: Requirements 7.5**

- [x] 12. Wire native LSP formatting instead of manual format-on-save hack
  - The extension currently uses `onWillSaveTextDocument` to manually trigger `editor.action.formatDocument`. If the daemon advertises `textDocument/formatting`, the `LanguageClient` handles format-on-save natively via VS Code's built-in mechanism — the manual hack is redundant and can conflict.

  - [x] 12.1 Remove manual format-on-save listener from extension.ts
    - Remove the `onWillSaveTextDocument` handler that manually calls `editor.action.formatDocument`
    - The `vscode-languageclient` library automatically registers the formatting provider when the server advertises `documentFormattingProvider`
    - Users control format-on-save via VS Code's native `editor.formatOnSave` setting
    - _Requirements: 3.1, 3.3_

  - [x] 12.2 Add `meld.toml` to file watcher patterns
    - Update `synchronize.fileEvents` in `languageClient.ts` to also watch `**/meld.toml` since the daemon uses it for project detection and configuration
    - _Requirements: 7.1_

- [x] 13. Improve daemon discovery and status reporting
  - The `resolveServerPath` fallback chain is `config path → tools/meld → 'meld'`. The diagnostics command doesn't report the resolved server path or daemon socket status, making troubleshooting harder.

  - [x] 13.1 Fix server path fallback to use `meldd` instead of `meld`
    - Change the final fallback in `resolveServerPath()` from `'meld'` to `'meldd'` so it finds the daemon binary on PATH
    - _Requirements: 7.1, 8.2_

  - [x] 13.2 Enrich the diagnostics command with LSP connection details
    - Add resolved server path, daemon PID (from `.meld/meldd.pid` if present), and socket status to the `meld.showDiagnostics` output
    - _Requirements: 8.4_

- [x] 14. Final checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.