"""Main entry point for Meld Bazel rules.

Load this file to access all Meld build rules:

    load("//meld-build/rules/meld:defs.bzl", "meld_library", "meld_binary", "meld_test")
"""

load(":rules.bzl", _meld_binary = "meld_binary", _meld_library = "meld_library", _meld_test = "meld_test")
load(":providers.bzl", _MeldInfo = "MeldInfo", _MeldToolchainInfo = "MeldToolchainInfo")
load(":toolchains.bzl", _meld_register_toolchains = "meld_register_toolchains", _meld_toolchain = "meld_toolchain", _PLATFORM_CONSTRAINTS = "PLATFORM_CONSTRAINTS")
load(":repositories.bzl", _meld_repositories = "meld_repositories", _meld_repository = "meld_repository", _meld_toolchain_repository = "meld_toolchain_repository")
load(":actions.bzl", _meld_compile = "meld_compile", _meld_link = "meld_link", _meld_analyze_imports = "meld_analyze_imports", _meld_format = "meld_format", _meld_lint = "meld_lint", _DEBUG_FLAGS = "DEBUG_FLAGS", _RELEASE_FLAGS = "RELEASE_FLAGS", _FASTBUILD_FLAGS = "FASTBUILD_FLAGS")

# Core rules
meld_library = _meld_library
meld_binary = _meld_binary
meld_test = _meld_test

# Toolchain
meld_toolchain = _meld_toolchain
meld_register_toolchains = _meld_register_toolchains
PLATFORM_CONSTRAINTS = _PLATFORM_CONSTRAINTS

# Workspace
meld_repositories = _meld_repositories
meld_repository = _meld_repository
meld_toolchain_repository = _meld_toolchain_repository

# Actions
meld_compile = _meld_compile
meld_link = _meld_link
meld_analyze_imports = _meld_analyze_imports
meld_format = _meld_format
meld_lint = _meld_lint
DEBUG_FLAGS = _DEBUG_FLAGS
RELEASE_FLAGS = _RELEASE_FLAGS
FASTBUILD_FLAGS = _FASTBUILD_FLAGS

# Providers
MeldInfo = _MeldInfo
MeldToolchainInfo = _MeldToolchainInfo
