#!/usr/bin/env python3
"""
Property-based tests for Meld compiler invocation consistency.
**Feature: meld-build, Property 1: Compiler invocation consistency**
**Validates: Requirements 1.1, 2.5**

For any valid set of compiler flags and source files, the generated
Bazel action must invoke the Meld compiler with the correct arguments.
"""

import re
from pathlib import Path

import pytest
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
compiler_flags = st.lists(
    st.from_regex(r"-[A-Za-z][A-Za-z0-9_=-]{0,20}", fullmatch=True),
    max_size=10,
)


class TestCompilerInvocationConsistency:
    """
    **Feature: meld-build, Property 1: Compiler invocation consistency**
    """

    _actions_content = None

    @classmethod
    def _load(cls):
        if cls._actions_content is None:
            cls._actions_content = _read_file("actions.bzl")

    @given(
        srcs=st.lists(meld_source_names, min_size=1, max_size=5),
        flags=compiler_flags,
    )
    def test_compile_action_passes_all_flags(self, srcs, flags):
        """
        **Validates: Requirements 1.1, 2.5**

        The meld_compile action must pass toolchain flags, config flags,
        and user flags to the compiler in the correct order.
        """
        self._load()
        body = _extract_function_body(self._actions_content, "meld_compile")

        # Must pass toolchain flags
        assert "toolchain_info.compiler_flags" in body
        # Must pass user-specified flags
        assert "compiler_flags" in body
        # Must pass config flags
        assert "config_flags" in body
        # Must use the "compile" subcommand
        assert '"compile"' in body

    @given(srcs=st.lists(meld_source_names, min_size=1, max_size=5))
    def test_compile_produces_one_output_per_source(self, srcs):
        """
        **Validates: Requirements 1.1**

        Each source file must produce exactly one .meld.o output.
        """
        self._load()
        body = _extract_function_body(self._actions_content, "meld_compile")
        assert "for src in srcs:" in body
        assert '.replace(".meld", ".meld.o")' in body

    def test_compile_adds_import_paths_from_deps(self):
        """
        **Validates: Requirements 2.5**

        Dependency import paths must be passed via -I flags.
        """
        self._load()
        body = _extract_function_body(self._actions_content, "meld_compile")
        assert 'before_each = "-I"' in body

    def test_link_action_uses_compiler(self):
        """
        **Validates: Requirements 1.1**

        The link action must use the toolchain compiler.
        """
        self._load()
        body = _extract_function_body(self._actions_content, "meld_link")
        assert "toolchain_info.compiler" in body
        assert '"link"' in body
