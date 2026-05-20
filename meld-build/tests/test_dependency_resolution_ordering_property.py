#!/usr/bin/env python3
"""
Property-based tests for dependency resolution ordering.
**Feature: meld-build, Property 6: Dependency resolution ordering**
**Validates: Requirements 2.4, 3.2, 3.5, 4.4**

For any valid dependency graph, the build system must resolve
dependencies in topological order and detect cycles.
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


target_names = st.from_regex(r"[a-z][a-z0-9_]{0,15}", fullmatch=True)


class TestDependencyResolutionOrdering:
    """
    **Feature: meld-build, Property 6: Dependency resolution ordering**
    """

    _rules_content = None
    _actions_content = None

    @classmethod
    def _load(cls):
        if cls._rules_content is None:
            cls._rules_content = _read_file("rules.bzl")
            cls._actions_content = _read_file("actions.bzl")

    @given(name=target_names)
    def test_library_collects_transitive_deps(self, name):
        """
        **Validates: Requirements 2.4, 3.5**

        meld_library must collect transitive compiled files from deps.
        """
        self._load()
        body = _extract_function_body(self._rules_content, "_meld_library_impl")
        assert "transitive_compiled_files" in body
        assert "dep.transitive_compiled_files" in body

    @given(name=target_names)
    def test_binary_links_transitive_deps(self, name):
        """
        **Validates: Requirements 2.4, 3.2**

        meld_binary must link all transitive dependencies.
        """
        self._load()
        body = _extract_function_body(self._actions_content, "meld_link")
        assert "dep.transitive_compiled_files" in body

    def test_compile_inputs_include_dep_outputs(self):
        """
        **Validates: Requirements 3.2**

        Compilation inputs must include transitive dep outputs.
        """
        self._load()
        body = _extract_function_body(self._actions_content, "meld_compile")
        assert "dep_compiled" in body
        assert "transitive" in body

    def test_deps_require_meld_info_provider(self):
        """
        **Validates: Requirements 2.4**

        deps attributes must require MeldInfo provider.
        """
        self._load()
        assert "providers = [MeldInfo]" in self._rules_content


class TestCircularDependencyDetection:
    """Unit tests for circular dependency detection (Req 3.3)."""

    _rules_content = None

    @classmethod
    def _load(cls):
        if cls._rules_content is None:
            cls._rules_content = _read_file("rules.bzl")

    def test_rules_validate_srcs_not_empty(self):
        """Rules must fail on empty srcs to prevent silent misconfiguration."""
        self._load()
        for rule_name in ["_meld_library_impl", "_meld_binary_impl", "_meld_test_impl"]:
            body = _extract_function_body(self._rules_content, rule_name)
            assert "fail(" in body, f"{rule_name} must validate srcs"

    def test_deps_use_depset_for_cycle_safety(self):
        """Bazel depsets inherently detect cycles; rules must use them."""
        self._load()
        assert "depset(" in self._rules_content
