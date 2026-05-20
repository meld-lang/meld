#!/usr/bin/env python3
"""
Property-based tests for test execution and reporting.
**Feature: meld-build, Property 8: Test execution and reporting**
**Validates: Requirements 4.1, 4.2**

**Feature: meld-build, Property 9: Test data availability**
**Validates: Requirements 4.5**
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


def _extract_block(content, pattern):
    match = re.search(pattern, content)
    if not match:
        return ""
    depth = 0
    start = match.start()
    for i in range(match.end() - 1, len(content)):
        ch = content[i]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return content[start:i + 1]
    return ""


meld_source_names = st.from_regex(r"[a-z][a-z0-9_]{0,20}\.meld", fullmatch=True)
timeout_values = st.sampled_from(["short", "moderate", "long", "eternal"])


class TestTestExecutionAndReporting:
    """
    **Feature: meld-build, Property 8: Test execution and reporting**
    """

    _rules_content = None
    _actions_content = None

    @classmethod
    def _load(cls):
        if cls._rules_content is None:
            cls._rules_content = _read_file("rules.bzl")
            cls._actions_content = _read_file("actions.bzl")

    @given(srcs=st.lists(meld_source_names, min_size=1, max_size=5))
    def test_meld_test_compiles_and_links(self, srcs):
        """
        **Validates: Requirements 4.1**

        meld_test must compile sources and link into a test executable.
        """
        self._load()
        body = _extract_function_body(self._rules_content, "_meld_test_impl")
        assert "meld_compile" in body
        assert "meld_link" in body

    def test_meld_test_is_declared_as_test(self):
        """
        **Validates: Requirements 4.1, 4.2**

        meld_test rule must be declared with test=True.
        """
        self._load()
        block = _extract_block(self._rules_content, r"meld_test\s*=\s*rule\(")
        assert "test = True" in block

    def test_meld_test_passes_test_flag_to_linker(self):
        """
        **Validates: Requirements 4.1**

        meld_test must pass is_test=True to the linker.
        """
        self._load()
        body = _extract_function_body(self._rules_content, "_meld_test_impl")
        assert "is_test = True" in body

    @given(timeout=timeout_values)
    def test_meld_test_accepts_timeout(self, timeout):
        """
        **Validates: Requirements 4.2**

        meld_test must accept a timeout attribute.
        """
        self._load()
        block = _extract_block(self._rules_content, r"meld_test\s*=\s*rule\(")
        assert '"timeout"' in block
        assert f'"{timeout}"' in block or "values" in block


class TestTestDataAvailability:
    """
    **Feature: meld-build, Property 9: Test data availability**
    """

    _rules_content = None

    @classmethod
    def _load(cls):
        if cls._rules_content is None:
            cls._rules_content = _read_file("rules.bzl")

    def test_meld_test_has_test_data_attr(self):
        """
        **Validates: Requirements 4.5**

        meld_test must have a test_data attribute.
        """
        self._load()
        block = _extract_block(self._rules_content, r"meld_test\s*=\s*rule\(")
        assert '"test_data"' in block

    def test_meld_test_creates_runfiles_from_test_data(self):
        """
        **Validates: Requirements 4.5**

        meld_test must make test_data available via runfiles.
        """
        self._load()
        body = _extract_function_body(self._rules_content, "_meld_test_impl")
        assert "runfiles" in body
        assert "test_data" in body

    def test_meld_binary_has_data_attr(self):
        """
        **Validates: Requirements 4.5**

        meld_binary must also support data files via runfiles.
        """
        self._load()
        body = _extract_function_body(self._rules_content, "_meld_binary_impl")
        assert "runfiles" in body


class TestTestFailureHandling:
    """Unit tests for test failure handling (Req 4.3)."""

    _actions_content = None

    @classmethod
    def _load(cls):
        if cls._actions_content is None:
            cls._actions_content = _read_file("actions.bzl")

    def test_link_passes_test_flag(self):
        """Test executables must receive --test flag for proper exit codes."""
        self._load()
        body = _extract_function_body(self._actions_content, "meld_link")
        assert '"--test"' in body
        assert "is_test" in body
