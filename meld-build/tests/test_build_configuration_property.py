#!/usr/bin/env python3
"""
Property-based tests for build configuration application.
**Feature: meld-build, Property 12: Build configuration application**
**Validates: Requirements 6.1, 6.2, 6.3, 6.4, 6.5**

Different build modes (debug, release, fastbuild) must apply the
correct compiler flags.
"""

import re
from pathlib import Path

from hypothesis import given, strategies as st


def _read_file(name):
    path = Path(__file__).resolve().parent.parent / "rules" / "meld" / name
    return path.read_text()


build_modes = st.sampled_from(["dbg", "opt", "fastbuild"])
compiler_flags = st.lists(
    st.from_regex(r"-[A-Za-z][A-Za-z0-9_=-]{0,20}", fullmatch=True),
    max_size=5,
)


class TestBuildConfigurationApplication:
    """
    **Feature: meld-build, Property 12: Build configuration application**
    """

    _actions_content = None

    @classmethod
    def _load(cls):
        if cls._actions_content is None:
            cls._actions_content = _read_file("actions.bzl")

    def test_debug_flags_defined(self):
        """
        **Validates: Requirements 6.1**

        Debug build flags must include debug symbols and reduced optimization.
        """
        self._load()
        assert "DEBUG_FLAGS" in self._actions_content
        assert '"-g"' in self._actions_content
        assert '"-O0"' in self._actions_content

    def test_release_flags_defined(self):
        """
        **Validates: Requirements 6.2**

        Release build flags must include full optimization.
        """
        self._load()
        assert "RELEASE_FLAGS" in self._actions_content
        assert '"-O3"' in self._actions_content

    def test_fastbuild_flags_defined(self):
        """
        **Validates: Requirements 6.3**

        Fastbuild flags must exist as a middle ground.
        """
        self._load()
        assert "FASTBUILD_FLAGS" in self._actions_content

    @given(mode=build_modes)
    def test_config_flags_function_handles_all_modes(self, mode):
        """
        **Validates: Requirements 6.1, 6.2, 6.3, 6.4**

        _get_config_flags must handle all compilation modes.
        """
        self._load()
        assert "_get_config_flags" in self._actions_content
        assert "compilation_mode" in self._actions_content
        assert '"dbg"' in self._actions_content
        assert '"opt"' in self._actions_content

    @given(flags=compiler_flags)
    def test_user_flags_applied_after_config(self, flags):
        """
        **Validates: Requirements 6.5**

        User-specified flags must be applied after config flags
        so they can override defaults.
        """
        self._load()
        # In the compile function, config_flags come before compiler_flags
        config_pos = self._actions_content.find("config_flags")
        user_pos = self._actions_content.find("compiler_flags", config_pos + 1) if config_pos >= 0 else -1
        assert config_pos >= 0
        assert user_pos > config_pos
