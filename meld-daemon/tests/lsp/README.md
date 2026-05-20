# LSP Test Specs

The files in `spec/` are test specifications from the original `meld-lsp-server` package.
They describe desired LSP behavior but reference an API (`AnalysisEngine`, `LanguageService`,
`WorkspaceManager`) that has been superseded by the daemon's provider architecture.

These tests need to be rewritten to test the daemon's actual providers:
- `completion_provider.cpp`
- `diagnostics_provider.cpp`
- `formatting_provider.cpp`
- `navigation_provider.cpp`
- `semantic_token_provider.cpp`
- `workspace_provider.cpp`

The spec files are kept as reference for the expected behavior.
