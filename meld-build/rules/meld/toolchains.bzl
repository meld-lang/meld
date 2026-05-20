"""Toolchain definitions for Meld compiler integration.

Defines the meld_toolchain rule and registration helpers for
configuring Meld compiler toolchains across platforms.
"""

load(":providers.bzl", "MeldToolchainInfo")

def _meld_toolchain_impl(ctx):
    """Implementation for the meld_toolchain rule."""
    toolchain_info = MeldToolchainInfo(
        compiler = ctx.file.compiler,
        std_lib = ctx.file.std_lib,
        target_platform = ctx.attr.target_platform,
        compiler_flags = ctx.attr.compiler_flags,
    )
    return [
        platform_common.ToolchainInfo(
            meld_toolchain_info = toolchain_info,
        ),
    ]

meld_toolchain = rule(
    implementation = _meld_toolchain_impl,
    attrs = {
        "compiler": attr.label(
            allow_single_file = True,
            executable = True,
            cfg = "exec",
            doc = "The Meld compiler executable.",
        ),
        "std_lib": attr.label(
            allow_single_file = True,
            doc = "The Meld standard library.",
        ),
        "target_platform": attr.string(
            mandatory = True,
            doc = "Target platform identifier (e.g., 'linux-x86_64', 'macos-arm64').",
        ),
        "compiler_flags": attr.string_list(
            doc = "Default compiler flags for this toolchain.",
        ),
    },
    doc = "Defines a Meld compiler toolchain for a specific platform.",
)

def meld_register_toolchains(compiler_path = None):
    """Register default Meld toolchains for supported platforms.

    Call this in the WORKSPACE file after meld_repositories() to make
    Meld toolchains available for build targets.

    Args:
        compiler_path: Optional explicit path to the Meld compiler.
            If not specified, the toolchain will search $PATH.
    """

    # Register the toolchain type
    native.register_toolchains(
        "//meld-build/rules/meld:linux_x86_64_toolchain",
        "//meld-build/rules/meld:linux_arm64_toolchain",
        "//meld-build/rules/meld:macos_x86_64_toolchain",
        "//meld-build/rules/meld:macos_arm64_toolchain",
        "//meld-build/rules/meld:windows_x86_64_toolchain",
    )

def _detect_platform():
    """Detect the host platform string.

    Returns:
        A platform identifier string (e.g., 'linux-x86_64', 'macos-arm64').
    """
    # In Starlark, platform detection is done via constraint_values
    # This helper is for documentation; actual selection uses select()
    return "unknown"

# Platform constraint mappings for toolchain resolution
PLATFORM_CONSTRAINTS = {
    "linux-x86_64": ["@platforms//os:linux", "@platforms//cpu:x86_64"],
    "linux-arm64": ["@platforms//os:linux", "@platforms//cpu:arm64"],
    "macos-x86_64": ["@platforms//os:macos", "@platforms//cpu:x86_64"],
    "macos-arm64": ["@platforms//os:macos", "@platforms//cpu:arm64"],
    "windows-x86_64": ["@platforms//os:windows", "@platforms//cpu:x86_64"],
}
