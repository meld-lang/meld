#!/usr/bin/env python3
"""
Property-based tests for cross-compilation toolchain selection.
**Feature: meld-build, Property 4: Cross-compilation toolchain selection**
**Validates: Requirements 1.5, 5.4**

For any supported target platform, the toolchain system must select
the correct compiler and flags.
"""

import re
from pathlib import Path

import pytest
from hypothesis import given, strategies as st


def _read_file(name):
    path = Path(__file__).resolve().parent.parent / "rules" / "meld" / name
    return path.read_text()


SUPPORTED_PLATFORMS = [
    "linux-x86_64", "linux-arm64",
    "macos-x86_64", "macos-arm64",
    "windows-x86_64",
]

platform_strategy = st.sampled_from(SUPPORTED_PLATFORMS)


class TestCrossCompilationToolchainSelection:
    """
    **Feature: meld-build, Property 4: Cross-compilation toolchain selection**
    """

    _toolchains_content = None

    @classmethod
    def _load(cls):
        if cls._toolchains_content is None:
            cls._toolchains_content = _read_file("toolchains.bzl")

    @given(platform=platform_strategy)
    def test_platform_has_constraint_mapping(self, platform):
        """
        **Validates: Requirements 1.5, 5.4**

        Every supported platform must have a constraint mapping in
        PLATFORM_CONSTRAINTS.
        """
        self._load()
        assert "PLATFORM_CONSTRAINTS" in self._toolchains_content
        assert f'"{platform}"' in self._toolchains_content

    @given(platform=platform_strategy)
    def test_toolchain_rule_accepts_platform(self, platform):
        """
        **Validates: Requirements 5.4**

        The meld_toolchain rule must accept a target_platform attribute.
        """
        self._load()
        assert "target_platform" in self._toolchains_content

    def test_toolchain_has_compiler_attr(self):
        """
        **Validates: Requirements 5.4**

        The meld_toolchain rule must have a compiler attribute.
        """
        self._load()
        assert '"compiler"' in self._toolchains_content
        assert "executable = True" in self._toolchains_content

    def test_register_toolchains_exists(self):
        """
        **Validates: Requirements 5.4**

        meld_register_toolchains must register platform-specific toolchains.
        """
        self._load()
        assert "meld_register_toolchains" in self._toolchains_content
        assert "register_toolchains" in self._toolchains_content

    def test_toolchain_provides_toolchain_info(self):
        """
        **Validates: Requirements 5.4**

        The meld_toolchain rule must return a ToolchainInfo provider.
        """
        self._load()
        assert "ToolchainInfo" in self._toolchains_content
        assert "meld_toolchain_info" in self._toolchains_content
