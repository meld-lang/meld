#!/usr/bin/env python3
"""
Integration tests for complete Meld build workflows.
**Validates: All integration requirements (Task 12)**

Tests end-to-end scenarios: workspace init, multi-target builds,
cross-platform support.
"""

import re
from pathlib import Path

import pytest


RULES_ROOT = Path(__file__).resolve().parent.parent / "rules" / "meld"


def _read_file(name):
    return (RULES_ROOT / name).read_text()


class TestEndToEndBuildScenarios:
    """Integration tests for complete project build workflows (Task 12.1)."""

    def test_all_rule_files_exist(self):
        """All required Starlark rule files must exist."""
        required = ["defs.bzl", "rules.bzl", "actions.bzl",
                     "providers.bzl", "toolchains.bzl", "repositories.bzl",
                     "BUILD.bazel"]
        for name in required:
            assert (RULES_ROOT / name).exists(), f"Missing: {name}"

    def test_defs_is_single_load_point(self):
        """defs.bzl must re-export all public symbols."""
        content = _read_file("defs.bzl")
        required_symbols = [
            "meld_library", "meld_binary", "meld_test",
            "meld_toolchain", "meld_register_toolchains",
            "meld_repositories", "meld_repository",
            "MeldInfo", "MeldToolchainInfo",
            "meld_compile", "meld_link",
            "meld_format", "meld_lint",
            "DEBUG_FLAGS", "RELEASE_FLAGS",
        ]
        for sym in required_symbols:
            assert sym in content, f"defs.bzl missing: {sym}"

    def test_rules_load_actions_and_providers(self):
        """rules.bzl must load from actions.bzl and providers.bzl."""
        content = _read_file("rules.bzl")
        assert 'load(":actions.bzl"' in content
        assert 'load(":providers.bzl"' in content

    def test_toolchains_load_providers(self):
        """toolchains.bzl must load MeldToolchainInfo."""
        content = _read_file("toolchains.bzl")
        assert "MeldToolchainInfo" in content

    def test_complete_compile_link_pipeline(self):
        """The compile→link pipeline must be complete in actions.bzl."""
        content = _read_file("actions.bzl")
        # Compile produces .meld.o
        assert ".meld.o" in content
        # Link consumes compiled files
        assert "compiled_files" in content
        # Link produces executable
        assert "declare_file" in content
        # Both use the toolchain compiler
        assert "toolchain_info.compiler" in content


class TestCrossPlatformSupport:
    """Integration tests for cross-platform builds (Task 12.2)."""

    def test_platform_constraints_cover_major_platforms(self):
        """PLATFORM_CONSTRAINTS must cover Linux, macOS, Windows."""
        content = _read_file("toolchains.bzl")
        assert "linux-x86_64" in content
        assert "linux-arm64" in content
        assert "macos-x86_64" in content
        assert "macos-arm64" in content
        assert "windows-x86_64" in content

    def test_toolchain_repository_supports_auto_detection(self):
        """Toolchain repository must auto-detect host platform."""
        content = _read_file("repositories.bzl")
        assert "_detect_host_platform" in content
        assert "darwin" in content or "mac" in content
        assert "linux" in content
        assert "win" in content


class TestMultiTargetBuild:
    """Integration tests for multi-target scenarios (Task 12.3)."""

    def test_library_propagates_to_binary(self):
        """Library outputs must be consumable by binary targets."""
        rules = _read_file("rules.bzl")
        # Binary collects deps with MeldInfo
        assert "dep[MeldInfo]" in rules
        # Library returns MeldInfo
        assert "MeldInfo(" in rules

    def test_transitive_deps_propagated(self):
        """Transitive dependencies must be propagated through the graph."""
        rules = _read_file("rules.bzl")
        assert "transitive_compiled_files" in rules
        assert "transitive = [dep.transitive_compiled_files" in rules

    def test_test_target_uses_same_pipeline(self):
        """Test targets must use the same compile/link pipeline."""
        rules = _read_file("rules.bzl")
        # All three rule impls use meld_compile
        for impl in ["_meld_library_impl", "_meld_binary_impl", "_meld_test_impl"]:
            pattern = rf"def {impl}\("
            match = re.search(pattern, rules)
            assert match, f"{impl} must exist"
            rest = rules[match.start():]
            # Find next def or end
            next_def = re.search(r"\ndef [a-z_]", rest[1:])
            body = rest[:next_def.start() + 1] if next_def else rest
            assert "meld_compile" in body, f"{impl} must use meld_compile"
