"""Provider definitions for Meld build rules.

Providers carry information between Meld build rules, enabling
dependency resolution and artifact propagation through the build graph.
"""

MeldInfo = provider(
    doc = "Information about a compiled Meld target.",
    fields = {
        "compiled_files": "depset of compiled output files",
        "transitive_compiled_files": "depset of all transitive compiled outputs",
        "source_files": "depset of original Meld source files",
        "import_paths": "depset of import search paths for dependents",
    },
)

MeldToolchainInfo = provider(
    doc = "Information about the Meld compiler toolchain.",
    fields = {
        "compiler": "File for the Meld compiler executable",
        "std_lib": "File or directory for the Meld standard library",
        "target_platform": "String identifying the target platform",
        "compiler_flags": "List of default compiler flags",
    },
)
