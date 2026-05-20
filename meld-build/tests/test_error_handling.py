#!/usr/bin/env python3
"""
Unit tests for error handling and validation in Meld build rules.
**Validates: All error handling requirements**

Tests that rules validate inputs and produce clear error messages.
"""

import re
from pathlib import Path

import pytest


def _read_file(name):
    path = Path(__file__).resolve().parent.parent / "rules" / "meld" / name
    return path.read_text()


def _extract_function_body(content, func_name):
    pattern = rf"^def {func_name}\(.*?\):"
    match = re.search(pattern, content, re.MULTILINE)
    if not match:
        return ""
    rest = content[match.start():]
    lines = rest.split("\n")
    body_lines = [lines[0]]
    for line in lines[1:]:
        if line and not line[0].isspace() and line.strip():
            break
        body_lines.append(line)
    return "\n".join(body_lines)


class TestBuildErrorReporting:
    """Tests for build error reporting (Task 11.1)."""

    _rules_content = None

    @classmethod
    def _load(cls):
        if cls._rules_content is None:
            cls._rules_content = _read_file("rules.bzl")

    def test_library_fails_on_empty_srcs(self):
        """meld_library must fail with clear message on empty srcs."""
        self._load()
        body = _extract_function_body(self._rules_content, "_meld_library_impl")
        assert "if not ctx.files.srcs:" in body
        assert "fail(" in body

    def test_binary_fails_on_empty_srcs(self):
        """meld_binary must fail with clear message on empty srcs."""
        self._load()
        body = _extract_function_body(self._rules_content, "_meld_binary_impl")
        assert "if not ctx.files.srcs:" in body
        assert "fail(" in body

    def test_test_fails_on_empty_srcs(self):
        """meld_test must fail with clear message on empty srcs."""
        self._load()
        body = _extract_function_body(self._rules_content, "_meld_test_impl")
        assert "if not ctx.files.srcs:" in body
        assert "fail(" in body

    def test_error_messages_include_label(self):
        """Error messages must include the target label for context."""
        self._load()
        assert "ctx.label" in self._rules_content


class TestConfigurationValidation:
    """Tests for configuration validation (Task 11.2)."""

    _rules_content = None
    _toolchains_content = None

    @classmethod
    def _load(cls):
        if cls._rules_content is None:
            cls._rules_content = _read_file("rules.bzl")
            cls._toolchains_content = _read_file("toolchains.bzl")

    def test_srcs_only_accept_meld_files(self):
        """srcs attributes must only accept .meld files."""
        self._load()
        assert 'allow_files = [".meld"]' in self._rules_content

    def test_toolchain_requires_mandatory_platform(self):
        """meld_toolchain must require target_platform."""
        self._load()
        assert "mandatory = True" in self._toolchains_content

    def test_timeout_has_valid_values(self):
        """meld_test timeout must be constrained to valid values."""
        self._load()
        assert 'values = ["short", "moderate", "long", "eternal"]' in self._rules_content

    def test_deps_require_provider(self):
        """deps must require MeldInfo provider to catch misconfiguration."""
        self._load()
        assert "providers = [MeldInfo]" in self._rules_content
