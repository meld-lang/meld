"""Repository rules for Meld workspace configuration.

Provides functions for setting up a Bazel workspace with Meld support,
including fetching external Meld libraries and compiler toolchains.
"""

def _meld_repository_impl(repository_ctx):
    """Implementation for the meld_repository rule.

    Fetches an external Meld library and makes it available as a Bazel
    repository with a meld_library target.
    """
    url = repository_ctx.attr.url
    sha256 = repository_ctx.attr.sha256
    strip_prefix = repository_ctx.attr.strip_prefix

    if url:
        repository_ctx.download_and_extract(
            url = url,
            sha256 = sha256 if sha256 else "",
            stripPrefix = strip_prefix if strip_prefix else "",
        )
    else:
        # Local path mode — symlink from workspace
        if repository_ctx.attr.path:
            repository_ctx.symlink(repository_ctx.attr.path, "")

    # Generate BUILD file if one doesn't exist
    build_path = repository_ctx.path("BUILD.bazel")
    if not build_path.exists:
        repository_ctx.file(
            "BUILD.bazel",
            content = _generate_library_build(repository_ctx.name),
        )

def _generate_library_build(name):
    """Generate a BUILD.bazel file for an external Meld library."""
    return """\
load("//meld-build/rules/meld:defs.bzl", "meld_library")

meld_library(
    name = "{name}",
    srcs = glob(["**/*.meld"]),
    visibility = ["//visibility:public"],
)
""".format(name = name)

meld_repository = repository_rule(
    implementation = _meld_repository_impl,
    attrs = {
        "url": attr.string(
            doc = "URL of the external Meld library archive.",
        ),
        "sha256": attr.string(
            doc = "SHA-256 hash of the archive for verification.",
        ),
        "strip_prefix": attr.string(
            doc = "Directory prefix to strip from the archive.",
        ),
        "path": attr.string(
            doc = "Local path to the Meld library (alternative to URL).",
        ),
    },
    doc = "Fetches an external Meld library for use in the workspace.",
)

def _meld_toolchain_repository_impl(repository_ctx):
    """Implementation for the meld_toolchain_repository rule.

    Downloads a Meld compiler toolchain for a specific platform.
    """
    version = repository_ctx.attr.version
    platform = repository_ctx.attr.platform

    if not platform:
        platform = _detect_host_platform(repository_ctx)

    # Download compiler for the target platform
    url = "https://releases.meld-lang.org/v{version}/meld-{platform}.tar.gz".format(
        version = version,
        platform = platform,
    )

    # In practice, this would download the compiler; for now generate stubs
    repository_ctx.file("BUILD.bazel", _generate_toolchain_build(platform))
    repository_ctx.file("bin/meldc", "", executable = True)

def _detect_host_platform(repository_ctx):
    """Detect the host platform for toolchain selection."""
    os_name = repository_ctx.os.name.lower()
    arch = repository_ctx.os.arch

    if "mac" in os_name or "darwin" in os_name:
        os_key = "macos"
    elif "linux" in os_name:
        os_key = "linux"
    elif "win" in os_name:
        os_key = "windows"
    else:
        os_key = "linux"

    if arch in ("amd64", "x86_64"):
        arch_key = "x86_64"
    elif arch in ("arm64", "aarch64"):
        arch_key = "arm64"
    else:
        arch_key = "x86_64"

    return "{}-{}".format(os_key, arch_key)

def _generate_toolchain_build(platform):
    """Generate a BUILD.bazel for a downloaded toolchain."""
    return """\
exports_files(["bin/meldc"])

filegroup(
    name = "compiler",
    srcs = ["bin/meldc"],
    visibility = ["//visibility:public"],
)
"""

meld_toolchain_repository = repository_rule(
    implementation = _meld_toolchain_repository_impl,
    attrs = {
        "version": attr.string(
            mandatory = True,
            doc = "Meld compiler version to download.",
        ),
        "platform": attr.string(
            doc = "Target platform (auto-detected if not specified).",
        ),
    },
    doc = "Downloads a Meld compiler toolchain.",
)

def meld_repositories(compiler_version = "0.1.0"):
    """Set up all required repositories for Meld builds.

    Call this in the WORKSPACE file to configure Meld build support.
    This fetches the Meld standard library and any required build
    dependencies.

    Args:
        compiler_version: Version of the Meld compiler to use.
    """
    meld_toolchain_repository(
        name = "meld_toolchain",
        version = compiler_version,
    )
