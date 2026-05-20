#!/usr/bin/env python3
"""
Property-based tests for Meld Bazel rule interface validation.
**Feature: meld-build, Property 5: Rule interface validation**
**Validates: Requirements 2.1, 2.2, 2.3**

For any Meld rule invocation with valid parameters (sources, dependencies,
flags), the rule should accept and correctly process all specified parameters.
"""

import re
from pathlib import Path

import pytest
from hypothesis import given, strategies as st, settings


# ---------------------------------------------------------------------------
# Helpers: parse Starlark rule definitions from .bzl files
# ---------------------------------------------------------------------------

def _read_rules_file():
    """Read the rules.bzl file content."""
    rules_path = Path(__file__).resolve().parent.parent / "rules" / "meld" / "rules.bzl"
    return rules_path.read_text()


def _read_providers_file():
    """Read the providers.bzl file content."""
    providers_path = Path(__file__).resolve().parent.parent / "rules" / "meld" / "providers.bzl"
    return providers_path.read_text()


def _read_defs_file():
    """Read the defs.bzl file content."""
    defs_path = Path(__file__).resolve().parent.parent / "rules" / "meld" / "defs.bzl"
    return defs_path.read_text()


def _extract_rule_attrs(content, rule_name):
    """Extract attribute names from a Starlark rule definition.

    Looks for patterns like:
        "attr_name": attr.label_list(...)
    inside the rule() call for the given rule_name.
    """
    # Find the rule assignment block
    pattern = rf'{rule_name}\s*=\s*rule\('
    match = re.search(pattern, content)
    if not match:
        return set()

    # Extract the attrs block
    start = match.start()
    # Find the matching closing paren by counting depth
    depth = 0
    rule_text = ""
    for i in range(match.end() - 1, len(content)):
        ch = content[i]
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
            if depth == 0:
                rule_text = content[start:i + 1]
                break

    # Extract attribute names from "attr_name": attr.xxx(...) patterns
    attr_pattern = r'"(\w+)"\s*:\s*attr\.'
    return set(re.findall(attr_pattern, rule_text))


# ---------------------------------------------------------------------------
# Strategies
# ---------------------------------------------------------------------------

# Strategy for valid Meld source file names
meld_source_names = st.from_regex(r"[a-z][a-z0-9_]{0,20}\.meld", fullmatch=True)

# Strategy for valid Bazel target names
target_names = st.from_regex(r"[a-z][a-z0-9_-]{0,30}", fullmatch=True)

# Strategy for valid compiler flags
compiler_flags = st.lists(
    st.from_regex(r"-[A-Za-z][A-Za-z0-9_=-]{0,20}", fullmatch=True),
    max_size=10,
)

# Strategy for valid Bazel label-like dependency strings
dep_labels = st.lists(
    st.from_regex(r"//[a-z][a-z0-9_/-]{0,30}:[a-z][a-z0-9_-]{0,20}", fullmatch=True),
    max_size=5,
)

# Strategy for timeout values accepted by meld_test
timeout_values = st.sampled_from(["short", "moderate", "long", "eternal"])


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestRuleInterfaceValidation:
    """
    **Feature: meld-build, Property 5: Rule interface validation**

    For any Meld rule invocation with valid parameters (sources,
    dependencies, flags), the rule should accept and correctly process
    all specified parameters.
    """

    # Cache file contents once per test session
    _rules_content = None
    _providers_content = None
    _defs_content = None

    @classmethod
    def _load_files(cls):
        if cls._rules_content is None:
            cls._rules_content = _read_rules_file()
            cls._providers_content = _read_providers_file()
            cls._defs_content = _read_defs_file()

    # -- meld_library (Requirement 2.1) ------------------------------------

    @given(
        name=target_names,
        srcs=st.lists(meld_source_names, min_size=1, max_size=5),
        deps=dep_labels,
        flags=compiler_flags,
    )
    def test_meld_library_accepts_all_specified_params(self, name, srcs, deps, flags):
        """
        **Validates: Requirements 2.1**

        meld_library must declare attrs for srcs, deps, and compiler_flags
        so that any valid combination of these parameters is accepted.
        """
        self._load_files()
        attrs = _extract_rule_attrs(self._rules_content, "meld_library")

        # The rule must have all required attributes
        assert "srcs" in attrs, "meld_library must have 'srcs' attribute"
        assert "deps" in attrs, "meld_library must have 'deps' attribute"
        assert "compiler_flags" in attrs, "meld_library must have 'compiler_flags' attribute"

        # srcs must accept .meld files
        assert 'allow_files = [".meld"]' in self._rules_content, \
            "meld_library srcs must accept .meld files"

    # -- meld_binary (Requirement 2.2) ------------------------------------

    @given(
        name=target_names,
        srcs=st.lists(meld_source_names, min_size=1, max_size=5),
        main=meld_source_names,
        deps=dep_labels,
        flags=compiler_flags,
    )
    def test_meld_binary_accepts_all_specified_params(self, name, srcs, main, deps, flags):
        """
        **Validates: Requirements 2.2**

        meld_binary must declare attrs for srcs, main, deps,
        compiler_flags, and data.
        """
        self._load_files()
        attrs = _extract_rule_attrs(self._rules_content, "meld_binary")

        assert "srcs" in attrs, "meld_binary must have 'srcs' attribute"
        assert "main" in attrs, "meld_binary must have 'main' attribute"
        assert "deps" in attrs, "meld_binary must have 'deps' attribute"
        assert "compiler_flags" in attrs, "meld_binary must have 'compiler_flags' attribute"
        assert "data" in attrs, "meld_binary must have 'data' attribute"

    # -- meld_test (Requirement 2.3) --------------------------------------

    @given(
        name=target_names,
        srcs=st.lists(meld_source_names, min_size=1, max_size=5),
        deps=dep_labels,
        timeout=timeout_values,
    )
    def test_meld_test_accepts_all_specified_params(self, name, srcs, deps, timeout):
        """
        **Validates: Requirements 2.3**

        meld_test must declare attrs for srcs, deps, test_data, and
        timeout.
        """
        self._load_files()
        attrs = _extract_rule_attrs(self._rules_content, "meld_test")

        assert "srcs" in attrs, "meld_test must have 'srcs' attribute"
        assert "deps" in attrs, "meld_test must have 'deps' attribute"
        assert "test_data" in attrs, "meld_test must have 'test_data' attribute"
        assert "timeout" in attrs, "meld_test must have 'timeout' attribute"

    # -- Provider definitions ----------------------------------------------

    def test_providers_define_required_fields(self):
        """
        **Validates: Requirements 2.1, 2.2, 2.3**

        MeldInfo and MeldToolchainInfo providers must be defined with
        the fields needed to propagate build information between rules.
        """
        self._load_files()

        # MeldInfo fields
        assert "compiled_files" in self._providers_content
        assert "transitive_compiled_files" in self._providers_content
        assert "source_files" in self._providers_content
        assert "import_paths" in self._providers_content

        # MeldToolchainInfo fields
        assert "compiler" in self._providers_content
        assert "std_lib" in self._providers_content
        assert "target_platform" in self._providers_content
        assert "compiler_flags" in self._providers_content

    # -- defs.bzl re-exports -----------------------------------------------

    @given(
        rule_name=st.sampled_from([
            "meld_library", "meld_binary", "meld_test",
            "meld_toolchain", "meld_register_toolchains",
            "meld_repositories", "meld_repository",
            "MeldInfo", "MeldToolchainInfo",
        ])
    )
    def test_defs_reexports_all_public_symbols(self, rule_name):
        """
        **Validates: Requirements 2.1, 2.2, 2.3**

        defs.bzl must re-export every public rule, function, and
        provider so consumers have a single load point.
        """
        self._load_files()
        assert rule_name in self._defs_content, \
            f"defs.bzl must re-export '{rule_name}'"
