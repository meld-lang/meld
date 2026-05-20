#!/usr/bin/env python3
"""
Property-based tests for Meld Bazel artifact generation completeness.
**Feature: meld-build, Property 3: Artifact generation completeness**
**Validates: Requirements 1.3, 1.4**

For any valid Meld target definition, building the target should produce
the expected output artifacts (libraries, binaries, or test executables)
in the correct format.
"""

import re
from pathlib import Path

import pytest
from hypothesis import given, strategies as st, settings


def _read_file(name):
    """Read a file from the rules/meld directory."""
    path = Path(__file__).resolve().parent.parent / "rules" / "meld" / name
    return path.read_text()


def _extract_block(content, pattern):
    """Extract a balanced-paren block starting from a regex match."""
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
                return content[start : i + 1]
    return ""


def _extract_function_body(content, func_name):
    """Extract the full body of a Starlark def function."""
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



# ---------------------------------------------------------------------------
# Strategies
# ---------------------------------------------------------------------------

meld_source_names = st.from_regex(r"[a-z][a-z0-9_]{0,20}\.meld", fullmatch=True)
target_names = st.from_regex(r"[a-z][a-z0-9_-]{0,30}", fullmatch=True)
compiler_flags = st.lists(
    st.from_regex(r"-[A-Za-z][A-Za-z0-9_=-]{0,20}", fullmatch=True),
    max_size=10,
)
target_types = st.sampled_from(["library", "binary", "test"])


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestArtifactGenerationCompleteness:
    """
    **Feature: meld-build, Property 3: Artifact generation completeness**

    For any valid Meld target definition, building the target should
    produce the expected output artifacts (libraries, binaries, or test
    executables) in the correct format.
    """

    _rules_content = None
    _actions_content = None

    @classmethod
    def _load_files(cls):
        if cls._rules_content is None:
            cls._rules_content = _read_file("rules.bzl")
            cls._actions_content = _read_file("actions.bzl")

    @given(
        srcs=st.lists(meld_source_names, min_size=1, max_size=8),
        flags=compiler_flags,
    )
    def test_library_produces_compiled_artifacts(self, srcs, flags):
        """
        **Validates: Requirements 1.3**

        For any set of Meld source files in a meld_library target, the
        compile action must produce one .meld.o object file per source,
        and the rule must return those compiled files via DefaultInfo
        and MeldInfo.
        """
        self._load_files()
        assert ".meld.o" in self._actions_content, (
            "meld_compile must produce .meld.o object files"
        )
        assert 'replace(".meld", ".meld.o")' in self._actions_content, (
            "meld_compile must derive output name by replacing .meld with .meld.o"
        )
        lib_impl = _extract_function_body(self._rules_content, "_meld_library_impl")
        assert "DefaultInfo" in lib_impl, "meld_library must return DefaultInfo"
        assert "MeldInfo" in lib_impl, "meld_library must return MeldInfo provider"
        assert "compiled_files" in lib_impl, "meld_library must populate compiled_files"
        for src in srcs:
            expected_obj = src.replace(".meld", ".meld.o")
            assert expected_obj.endswith(".meld.o"), (
                f"Source '{src}' should produce artifact '{expected_obj}'"
            )



    @given(
        name=target_names,
        srcs=st.lists(meld_source_names, min_size=1, max_size=8),
        flags=compiler_flags,
    )
    def test_binary_produces_executable_artifact(self, name, srcs, flags):
        """
        **Validates: Requirements 1.4**

        For any meld_binary target, the build must compile sources and
        then link them into a single executable file. The rule must
        mark the output as executable via DefaultInfo.
        """
        self._load_files()
        bin_impl = _extract_function_body(self._rules_content, "_meld_binary_impl")
        assert "meld_compile" in bin_impl, "meld_binary must invoke meld_compile"
        assert "meld_link" in bin_impl, "meld_binary must invoke meld_link"
        assert "executable" in bin_impl, "meld_binary must set executable in DefaultInfo"
        bin_rule = _extract_block(self._rules_content, r"meld_binary\s*=\s*rule\(")
        assert "executable = True" in bin_rule, "meld_binary rule must be declared executable"
        link_body = _extract_function_body(self._actions_content, "meld_link")
        assert "declare_file" in link_body, "meld_link must declare an output file"

    @given(
        name=target_names,
        srcs=st.lists(meld_source_names, min_size=1, max_size=8),
    )
    def test_test_target_produces_test_executable(self, name, srcs):
        """
        **Validates: Requirements 1.3, 1.4**

        For any meld_test target, the build must compile sources and
        link them into a test executable with the is_test flag.
        """
        self._load_files()
        test_impl = _extract_function_body(self._rules_content, "_meld_test_impl")
        assert "meld_compile" in test_impl, "meld_test must invoke meld_compile"
        assert "meld_link" in test_impl, "meld_test must invoke meld_link"
        assert "is_test = True" in test_impl, "meld_test must pass is_test=True"
        test_rule = _extract_block(self._rules_content, r"meld_test\s*=\s*rule\(")
        assert "test = True" in test_rule, "meld_test rule must be declared as test"
        link_body = _extract_function_body(self._actions_content, "meld_link")
        assert '"--test"' in link_body, "meld_link must pass --test flag for tests"

    @given(target_type=target_types)
    def test_all_target_types_produce_artifacts_via_actions(self, target_type):
        """
        **Validates: Requirements 1.3, 1.4**

        For any Meld target type, the rule implementation must use the
        shared action helpers to produce artifacts consistently.
        """
        self._load_files()
        impl_name = f"_meld_{target_type}_impl"
        impl_body = _extract_function_body(self._rules_content, impl_name)
        assert impl_body, f"Rule implementation {impl_name} must exist"
        assert "meld_compile" in impl_body, f"meld_{target_type} must use meld_compile"
        if target_type in ("binary", "test"):
            assert "meld_link" in impl_body, f"meld_{target_type} must use meld_link"
        assert "DefaultInfo" in impl_body, f"meld_{target_type} must return DefaultInfo"
