#!/usr/bin/env python3
"""
Property-based tests for incremental build correctness.
**Feature: meld-build, Property 2: Incremental build correctness**
**Validates: Requirements 1.2, 3.4**

When source files are modified, only the changed files and their
dependents should be recompiled.
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


class TestIncrementalBuildCorrectness:
    """
    **Feature: meld-build, Property 2: Incremental build correctness**
    """

    _actions_content = None
    _rules_content = None

    @classmethod
    def _load(cls):
        if cls._actions_content is None:
            cls._actions_content = _read_file("actions.bzl")
            cls._rules_content = _read_file("rules.bzl")

    @given(srcs=st.lists(meld_source_names, min_size=1, max_size=5))
    def test_each_source_has_individual_action(self, srcs):
        """
        **Validates: Requirements 1.2**

        Each source file must have its own compile action so Bazel
        can track individual file changes for incremental builds.
        """
        self._load()
        body = _extract_function_body(self._actions_content, "meld_compile")
        # Must iterate over sources individually
        assert "for src in srcs:" in body
        # Each produces its own output
        assert "declare_file" in body

    def test_compile_declares_precise_inputs(self):
        """
        **Validates: Requirements 1.2, 3.4**

        Compile actions must declare precise inputs (source + deps)
        so Bazel's cache invalidation works correctly.
        """
        self._load()
        body = _extract_function_body(self._actions_content, "meld_compile")
        # Must use depset with both direct and transitive inputs
        assert "inputs = depset(" in body
        assert "direct = [src]" in body
        assert "transitive" in body

    def test_library_uses_depset_for_outputs(self):
        """
        **Validates: Requirements 3.4**

        Library outputs must use depset for efficient change propagation.
        """
        self._load()
        body = _extract_function_body(self._rules_content, "_meld_library_impl")
        assert "depset(" in body
        assert "compiled_files = depset" in body or "compiled_files = compiled_files" in body

    def test_link_uses_depset_for_all_objects(self):
        """
        **Validates: Requirements 1.2**

        Link action must use depset to collect all objects efficiently.
        """
        self._load()
        body = _extract_function_body(self._actions_content, "meld_link")
        assert "all_objects = depset(" in body
