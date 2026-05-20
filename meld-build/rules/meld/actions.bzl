"""Build action helpers for Meld compilation and linking.

These functions generate Bazel actions that invoke the Meld compiler
and linker with the correct arguments and file dependencies.
Includes dependency analysis, incremental build support, and
build configuration handling.
"""

load(":providers.bzl", "MeldInfo", "MeldToolchainInfo")

# Build configuration flags
DEBUG_FLAGS = ["-g", "--debug-symbols", "-O0"]
RELEASE_FLAGS = ["-O3", "--strip-debug"]
FASTBUILD_FLAGS = ["-O1"]

def _get_config_flags(ctx):
    """Derive compiler flags from the current build configuration.

    Args:
        ctx: The rule context.

    Returns:
        A list of compiler flag strings for the active configuration.
    """
    compilation_mode = ctx.var.get("compilation_mode", "fastbuild")
    if compilation_mode == "dbg":
        return DEBUG_FLAGS
    elif compilation_mode == "opt":
        return RELEASE_FLAGS
    else:
        return FASTBUILD_FLAGS

def meld_compile(ctx, srcs, deps, compiler_flags, toolchain_info):
    """Create a compilation action for Meld source files.

    Args:
        ctx: The rule context.
        srcs: List of Meld source files to compile.
        deps: List of MeldInfo providers from dependencies.
        compiler_flags: List of additional compiler flags.
        toolchain_info: MeldToolchainInfo provider for the current toolchain.

    Returns:
        A list of compiled output Files.
    """
    if not srcs:
        return []

    outputs = []
    compiler = toolchain_info.compiler

    # Gather transitive import paths from dependencies for -I flags
    dep_import_paths = depset(transitive = [
        dep.import_paths for dep in deps
    ])

    # Gather transitive compiled files from deps as inputs
    dep_compiled = depset(transitive = [
        dep.transitive_compiled_files for dep in deps
    ])

    # Get build configuration flags
    config_flags = _get_config_flags(ctx)

    for src in srcs:
        # Each source file produces a corresponding .meld.o object file
        out = ctx.actions.declare_file(src.basename.replace(".meld", ".meld.o"))
        outputs.append(out)

        args = ctx.actions.args()
        args.add("compile")
        args.add("-o", out)
        args.add(src)

        # Add import paths from dependencies
        args.add_all(dep_import_paths, before_each = "-I")

        # Add build configuration flags (debug/release/fastbuild)
        args.add_all(config_flags)

        # Add toolchain default flags
        args.add_all(toolchain_info.compiler_flags)

        # Add user-specified compiler flags (highest priority)
        args.add_all(compiler_flags)

        ctx.actions.run(
            executable = compiler,
            arguments = [args],
            inputs = depset(
                direct = [src],
                transitive = [dep_compiled],
            ),
            outputs = [out],
            mnemonic = "MeldCompile",
            progress_message = "Compiling Meld source %{input}",
        )

    return outputs

def meld_link(ctx, compiled_files, deps, toolchain_info, output_name, is_test = False):
    """Create a linking action to produce a Meld binary or test executable.

    Args:
        ctx: The rule context.
        compiled_files: List of compiled object files.
        deps: List of MeldInfo providers from dependencies.
        toolchain_info: MeldToolchainInfo provider for the current toolchain.
        output_name: Name for the output executable.
        is_test: Whether this is a test executable.

    Returns:
        The linked output File.
    """
    output = ctx.actions.declare_file(output_name)
    compiler = toolchain_info.compiler

    # Gather transitive compiled files from dependencies
    dep_compiled = depset(transitive = [
        dep.transitive_compiled_files for dep in deps
    ])

    # All object files to link: direct compiled files + transitive deps
    all_objects = depset(
        direct = compiled_files,
        transitive = [dep_compiled],
    )

    args = ctx.actions.args()
    args.add("link")
    args.add("-o", output)

    if is_test:
        args.add("--test")

    args.add_all(all_objects)

    # Add toolchain default flags
    args.add_all(toolchain_info.compiler_flags)

    ctx.actions.run(
        executable = compiler,
        arguments = [args],
        inputs = all_objects,
        outputs = [output],
        mnemonic = "MeldLink",
        progress_message = "Linking Meld executable %{output}",
    )

    return output


def meld_analyze_imports(ctx, srcs, toolchain_info):
    """Analyze Meld source files to discover import dependencies.

    Parses import statements from source files and returns a depset
    of discovered module names that should be resolved to build targets.

    Args:
        ctx: The rule context.
        srcs: List of Meld source files to analyze.
        toolchain_info: MeldToolchainInfo provider.

    Returns:
        A File containing the dependency analysis output (JSON).
    """
    if not srcs:
        return None

    output = ctx.actions.declare_file(ctx.label.name + ".imports.json")
    compiler = toolchain_info.compiler

    args = ctx.actions.args()
    args.add("analyze-imports")
    args.add("-o", output)
    args.add_all(srcs)

    ctx.actions.run(
        executable = compiler,
        arguments = [args],
        inputs = depset(direct = srcs),
        outputs = [output],
        mnemonic = "MeldAnalyzeImports",
        progress_message = "Analyzing imports for %{label}",
    )

    return output


def meld_format(ctx, srcs, toolchain_info):
    """Run the Meld formatter on source files.

    Args:
        ctx: The rule context.
        srcs: List of Meld source files to format.
        toolchain_info: MeldToolchainInfo provider.

    Returns:
        A list of formatted output Files.
    """
    if not srcs:
        return []

    outputs = []
    compiler = toolchain_info.compiler

    for src in srcs:
        out = ctx.actions.declare_file(src.basename + ".formatted")
        outputs.append(out)

        args = ctx.actions.args()
        args.add("format")
        args.add("-o", out)
        args.add(src)

        ctx.actions.run(
            executable = compiler,
            arguments = [args],
            inputs = depset(direct = [src]),
            outputs = [out],
            mnemonic = "MeldFormat",
            progress_message = "Formatting %{input}",
        )

    return outputs


def meld_lint(ctx, srcs, deps, toolchain_info):
    """Run the Meld linter on source files.

    Args:
        ctx: The rule context.
        srcs: List of Meld source files to lint.
        deps: List of MeldInfo providers from dependencies.
        toolchain_info: MeldToolchainInfo provider.

    Returns:
        A File containing the lint report.
    """
    if not srcs:
        return None

    output = ctx.actions.declare_file(ctx.label.name + ".lint.json")
    compiler = toolchain_info.compiler

    dep_import_paths = depset(transitive = [
        dep.import_paths for dep in deps
    ])

    args = ctx.actions.args()
    args.add("lint")
    args.add("-o", output)
    args.add_all(srcs)
    args.add_all(dep_import_paths, before_each = "-I")

    ctx.actions.run(
        executable = compiler,
        arguments = [args],
        inputs = depset(direct = srcs),
        outputs = [output],
        mnemonic = "MeldLint",
        progress_message = "Linting %{label}",
    )

    return output
