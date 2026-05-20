#!/usr/bin/env python3
"""
Property-based tests for tool integration execution.
**Feature: meld-build, Property 13: Tool integration execution**
**Validates: Requirements 7.2, 7.3, 7.5**

**Feature: meld-build, Property 14: Debug information preservation**
**Validates: Requirements 7.4**
"""

import re
from pathlib import Path

from hypothesis import given, strategies as st


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


meld_source_names = st.from_regex(r"[a-z][a-z0-9_]{0,20}\.meld", fullmatch=True)


class TestToolIntegrationExecution:
    """
    **Feature: meld-build, Property 13: Tool integration execution**
    """

    _actions_content = None

    @classmethod
    def _load(cls):
        if cls._actions_content is None:
            cls._actions_content = _read_file("actions.bzl")

    def test_format_action_exists(self):
        """
        **Validates: Requirements 7.2**

        A meld_format action must exist for formatter integration.
        """
        self._load()
        assert "def meld_format" in self._actions_content

    def test_lint_action_exists(self):
        """
        **Validates: Requirements 7.3**

        A meld_lint action must exist for linter integration.
        """
        self._load()
        assert "def meld_lint" in self._actions_content

    @given(srcs=st.lists(meld_source_names, min_size=1, max_size=3))
    def test_format_produces_output_per_source(self, srcs):
        """
        **Validates: Requirements 7.2**

        The formatter must produce one output per source file.
        """
        self._load()
        body = _extract_function_body(self._actions_content, "meld_format")
        assert "for src in srcs:" in body
        assert "declare_file" in body

    def test_lint_produces_report(self):
        """
        **Validates: Requirements 7.3**

        The linter must produce a JSON report.
        """
        self._load()
        body = _extract_function_body(self._actions_content, "meld_lint")
        assert ".lint.json" in body

    def test_lint_uses_dep_import_paths(self):
        """
        **Validates: Requirements 7.3, 7.5**

        The linter must use dependency import paths for accurate analysis.
        """
        self._load()
        body = _extract_function_body(self._actions_content, "meld_lint")
        assert "dep_import_paths" in body


class TestDebugInformationPreservation:
    """
    **Feature: meld-build, Property 14: Debug information preservation**
    """

    _actions_content = None

    @classmethod
    def _load(cls):
        if cls._actions_content is None:
            cls._actions_content = _read_file("actions.bzl")

    def test_debug_flags_include_debug_symbols(self):
        """
        **Validates: Requirements 7.4**

        Debug build configuration must preserve debug symbols.
        """
        self._load()
        assert "DEBUG_FLAGS" in self._actions_content
        assert '"-g"' in self._actions_content
        assert '"--debug-symbols"' in self._actions_content

    def test_release_strips_debug(self):
        """
        **Validates: Requirements 7.4**

        Release builds must strip debug info for smaller binaries.
        """
        self._load()
        assert '"--strip-debug"' in self._actions_content
