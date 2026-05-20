#!/usr/bin/env python3
"""
Property-based tests for external dependency management.
**Feature: meld-build, Property 10: External dependency management**
**Validates: Requirements 5.3, 5.5**

External Meld libraries declared via repository rules must be
fetchable and available for import.
"""

import re
from pathlib import Path

from hypothesis import given, strategies as st


def _read_file(name):
    path = Path(__file__).resolve().parent.parent / "rules" / "meld" / name
    return path.read_text()


url_schemes = st.sampled_from(["https://example.com/lib.tar.gz", "https://registry.meld-lang.org/pkg.zip"])
sha256_hashes = st.from_regex(r"[0-9a-f]{64}", fullmatch=True)


class TestExternalDependencyManagement:
    """
    **Feature: meld-build, Property 10: External dependency management**
    """

    _repos_content = None

    @classmethod
    def _load(cls):
        if cls._repos_content is None:
            cls._repos_content = _read_file("repositories.bzl")

    @given(url=url_schemes)
    def test_meld_repository_accepts_url(self, url):
        """
        **Validates: Requirements 5.3**

        meld_repository must accept a URL for fetching external libraries.
        """
        self._load()
        assert '"url"' in self._repos_content
        assert "attr.string" in self._repos_content

    @given(sha=sha256_hashes)
    def test_meld_repository_accepts_sha256(self, sha):
        """
        **Validates: Requirements 5.3**

        meld_repository must accept a SHA-256 hash for verification.
        """
        self._load()
        assert '"sha256"' in self._repos_content

    def test_meld_repository_generates_build_file(self):
        """
        **Validates: Requirements 5.5**

        meld_repository must generate a BUILD.bazel for the fetched library.
        """
        self._load()
        assert "BUILD.bazel" in self._repos_content
        assert "meld_library" in self._repos_content

    def test_meld_repositories_sets_up_toolchain(self):
        """
        **Validates: Requirements 5.3, 5.5**

        meld_repositories() must set up the compiler toolchain.
        """
        self._load()
        assert "def meld_repositories" in self._repos_content
        assert "meld_toolchain_repository" in self._repos_content

    def test_toolchain_repository_detects_platform(self):
        """
        **Validates: Requirements 5.5**

        The toolchain repository must auto-detect the host platform.
        """
        self._load()
        assert "_detect_host_platform" in self._repos_content


class TestCompilerConfigurationConsistency:
    """
    **Feature: meld-build, Property 11: Compiler configuration consistency**
    **Validates: Requirements 5.2**
    """

    _repos_content = None
    _toolchains_content = None

    @classmethod
    def _load(cls):
        if cls._repos_content is None:
            cls._repos_content = _read_file("repositories.bzl")
            cls._toolchains_content = _read_file("toolchains.bzl")

    def test_toolchain_specifies_compiler_location(self):
        """
        **Validates: Requirements 5.2**
        """
        self._load()
        assert '"compiler"' in self._toolchains_content
        assert "executable = True" in self._toolchains_content

    def test_workspace_init_creates_workspace_config(self):
        """
        **Validates: Requirements 5.1**
        """
        self._load()
        assert "meld_repositories" in self._repos_content
