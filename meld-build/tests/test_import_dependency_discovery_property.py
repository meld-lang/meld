#!/usr/bin/env python3
"""
Property-based tests for import-based dependency discovery.
**Feature: meld-build, Property 7: Import-based dependency discovery**
**Validates: Requirements 3.1**

For any Meld source file with import statements, the build system
must discover and include imported modules in the dependency graph.
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


module_names = st.from_regex(r"[a-z][a-z0-9_]{0,20}", fullmatch=True)


class TestImportDependencyDiscovery:
    """
    **Feature: meld-build, Property 7: Import-based dependency discovery**
    """

    _actions_content = None
    _rules_content = None

    @classmethod
    def _load(cls):
        if cls._actions_content is None:
            cls._actions_content = _read_file("actions.bzl")
            cls._rules_content = _read_file("rules.bzl")

    def test_analyze_imports_action_exists(self):
        """
        **Validates: Requirements 3.1**

        An analyze-imports action must exist for dependency discovery.
        """
        self._load()
        assert "meld_analyze_imports" in self._actions_content

    def test_analyze_imports_produces_json(self):
        """
        **Validates: Requirements 3.1**

        The import analysis must produce a JSON output file.
        """
        self._load()
        body = _extract_function_body(self._actions_content, "meld_analyze_imports")
        assert ".imports.json" in body

    def test_compile_uses_dep_import_paths(self):
        """
        **Validates: Requirements 3.1**

        Compilation must use import paths from dependencies.
        """
        self._load()
        body = _extract_function_body(self._actions_content, "meld_compile")
        assert "dep_import_paths" in body
        assert "import_paths" in body

    @given(module=module_names)
    def test_library_propagates_import_paths(self, module):
        """
        **Validates: Requirements 3.1**

        meld_library must propagate import_paths to dependents.
        """
        self._load()
        body = _extract_function_body(self._rules_content, "_meld_library_impl")
        assert "import_paths" in body
        assert "import_path" in body
