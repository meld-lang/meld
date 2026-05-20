"""Core Meld Bazel rule definitions.

Defines meld_library, meld_binary, and meld_test rules for building
Meld source code within the Bazel build system.
"""

load(":actions.bzl", "meld_compile", "meld_link")
load(":providers.bzl", "MeldInfo")

def _meld_library_impl(ctx):
    """Implementation for the meld_library rule."""

    # Validate that srcs is not empty
    if not ctx.files.srcs:
        fail("meld_library rule '{}' requires at least one source file in 'srcs'".format(ctx.label))

    # Collect source files
    source_files = depset(ctx.files.srcs)

    # Gather MeldInfo providers from dependencies
    dep_infos = [dep[MeldInfo] for dep in ctx.attr.deps]

    # Get the toolchain
    toolchain_info = ctx.toolchains["//meld-build/rules/meld:toolchain_type"].meld_toolchain_info

    # Compile source files using the action helper
    compiled = meld_compile(
        ctx,
        srcs = ctx.files.srcs,
        deps = dep_infos,
        compiler_flags = ctx.attr.compiler_flags,
        toolchain_info = toolchain_info,
    )

    compiled_files = depset(compiled)

    # Merge transitive compiled files from all dependencies
    transitive_compiled_files = depset(
        compiled,
        transitive = [dep.transitive_compiled_files for dep in dep_infos],
    )

    # Compute import paths: include the package directory so dependents
    # can locate this library's compiled outputs
    import_path = ctx.label.package
    import_paths = depset(
        [import_path],
        transitive = [dep.import_paths for dep in dep_infos],
    )

    return [
        DefaultInfo(files = compiled_files),
        MeldInfo(
            compiled_files = compiled_files,
            transitive_compiled_files = transitive_compiled_files,
            source_files = source_files,
            import_paths = import_paths,
        ),
    ]

meld_library = rule(
    implementation = _meld_library_impl,
    attrs = {
        "srcs": attr.label_list(
            allow_files = [".meld"],
            doc = "Meld source files for this library.",
        ),
        "deps": attr.label_list(
            providers = [MeldInfo],
            doc = "Dependencies for this library.",
        ),
        "compiler_flags": attr.string_list(
            doc = "Additional flags passed to the Meld compiler.",
        ),
    },
    toolchains = ["//meld-build/rules/meld:toolchain_type"],
    doc = "Compiles Meld source files into a reusable library artifact.",
)

def _meld_binary_impl(ctx):
    """Implementation for the meld_binary rule."""

    # Validate that srcs is not empty
    if not ctx.files.srcs:
        fail("meld_binary rule '{}' requires at least one source file in 'srcs'".format(ctx.label))

    # Gather MeldInfo providers from dependencies
    dep_infos = [dep[MeldInfo] for dep in ctx.attr.deps]

    # Get the toolchain
    toolchain_info = ctx.toolchains["//meld-build/rules/meld:toolchain_type"].meld_toolchain_info

    # Compile source files using the action helper
    compiled = meld_compile(
        ctx,
        srcs = ctx.files.srcs,
        deps = dep_infos,
        compiler_flags = ctx.attr.compiler_flags,
        toolchain_info = toolchain_info,
    )

    # Link compiled files into an executable binary
    executable = meld_link(
        ctx,
        compiled_files = compiled,
        deps = dep_infos,
        toolchain_info = toolchain_info,
        output_name = ctx.label.name,
    )

    # Collect data files into runfiles
    runfiles = ctx.runfiles(files = ctx.files.data)

    return [
        DefaultInfo(
            files = depset([executable]),
            executable = executable,
            runfiles = runfiles,
        ),
    ]

meld_binary = rule(
    implementation = _meld_binary_impl,
    attrs = {
        "srcs": attr.label_list(
            allow_files = [".meld"],
            doc = "Meld source files for this binary.",
        ),
        "main": attr.label(
            allow_single_file = [".meld"],
            doc = "Main entry point source file.",
        ),
        "deps": attr.label_list(
            providers = [MeldInfo],
            doc = "Dependencies for this binary.",
        ),
        "compiler_flags": attr.string_list(
            doc = "Additional flags passed to the Meld compiler.",
        ),
        "data": attr.label_list(
            allow_files = True,
            doc = "Data files available at runtime.",
        ),
    },
    executable = True,
    toolchains = ["//meld-build/rules/meld:toolchain_type"],
    doc = "Compiles Meld source files into an executable binary.",
)

def _meld_test_impl(ctx):
    """Implementation for the meld_test rule."""

    # Validate that srcs is not empty
    if not ctx.files.srcs:
        fail("meld_test rule '{}' requires at least one source file in 'srcs'".format(ctx.label))

    # Gather MeldInfo providers from dependencies
    dep_infos = [dep[MeldInfo] for dep in ctx.attr.deps]

    # Get the toolchain
    toolchain_info = ctx.toolchains["//meld-build/rules/meld:toolchain_type"].meld_toolchain_info

    # Compile test source files using the action helper
    compiled = meld_compile(
        ctx,
        srcs = ctx.files.srcs,
        deps = dep_infos,
        compiler_flags = [],
        toolchain_info = toolchain_info,
    )

    # Link compiled files into a test executable
    executable = meld_link(
        ctx,
        compiled_files = compiled,
        deps = dep_infos,
        toolchain_info = toolchain_info,
        output_name = ctx.label.name,
        is_test = True,
    )

    # Collect test_data files into runfiles so they're available at test runtime
    runfiles = ctx.runfiles(files = ctx.files.test_data)

    return [
        DefaultInfo(
            files = depset([executable]),
            executable = executable,
            runfiles = runfiles,
        ),
    ]

meld_test = rule(
    implementation = _meld_test_impl,
    attrs = {
        "srcs": attr.label_list(
            allow_files = [".meld"],
            doc = "Meld test source files.",
        ),
        "deps": attr.label_list(
            providers = [MeldInfo],
            doc = "Dependencies for this test.",
        ),
        "test_data": attr.label_list(
            allow_files = True,
            doc = "Data files available during test execution.",
        ),
        "timeout": attr.string(
            default = "short",
            values = ["short", "moderate", "long", "eternal"],
            doc = "Test timeout category.",
        ),
    },
    test = True,
    toolchains = ["//meld-build/rules/meld:toolchain_type"],
    doc = "Compiles and runs Meld test files.",
)
