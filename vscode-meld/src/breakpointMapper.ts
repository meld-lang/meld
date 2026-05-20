import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';

/**
 * Maps `.meld` file breakpoints to the correct debugging backend.
 *
 * Both modes use `.meld` file paths directly:
 * - Interpret mode: the AST interpreter DAP server receives `.meld`
 *   file + line breakpoints natively.
 * - Compiled mode: DWARF debug info (emitted by `meld build --debug`)
 *   maps LLVM-generated functions back to `.meld` source locations via
 *   `DISubprogram` metadata, so LLDB resolves breakpoints by matching
 *   the source path in the debug info.
 *
 * The mapper normalizes file paths and returns breakpoints suitable
 * for the active backend.
 *
 * Requirements: 19.6
 */

export interface MappedBreakpoint {
    /** Absolute, resolved path to the `.meld` source file. */
    filePath: string;
    /** 1-based line number. */
    line: number;
    /** Optional condition expression for conditional breakpoints. */
    condition?: string;
    /** Optional hit condition (e.g. ">= 3"). */
    hitCondition?: string;
    /** Optional log message for logpoints. */
    logMessage?: string;
}

export class MeldBreakpointMapper {
    /**
     * Map VS Code source breakpoints for the given debug mode.
     *
     * For both interpret and compiled modes the breakpoints pass through
     * with normalized paths — the underlying backends understand `.meld`
     * source locations directly.
     */
    mapBreakpoints(
        mode: string,
        breakpoints: vscode.SourceBreakpoint[]
    ): MappedBreakpoint[] {
        const workspaceFolder =
            vscode.workspace.workspaceFolders?.[0]?.uri.fsPath ?? '';

        return breakpoints
            .filter((bp) => bp.location.uri.fsPath.endsWith('.meld'))
            .map((bp) => {
                const filePath = this.resolveBreakpointPath(
                    bp.location.uri.fsPath,
                    workspaceFolder
                );

                const mapped: MappedBreakpoint = {
                    filePath,
                    line: bp.location.range.start.line + 1, // VS Code lines are 0-based
                };

                if (bp.condition) {
                    mapped.condition = bp.condition;
                }
                if (bp.hitCondition) {
                    mapped.hitCondition = bp.hitCondition;
                }
                if (bp.logMessage) {
                    mapped.logMessage = bp.logMessage;
                }

                return mapped;
            });
    }

    /**
     * Normalize a file path: resolve symlinks and make it absolute
     * relative to the workspace folder.
     */
    resolveBreakpointPath(filePath: string, workspaceFolder: string): string {
        // Make absolute if relative
        const absolute = path.isAbsolute(filePath)
            ? filePath
            : path.resolve(workspaceFolder, filePath);

        // Resolve symlinks when the file exists on disk
        try {
            return fs.realpathSync(absolute);
        } catch {
            // File may not exist yet (e.g. unsaved buffer) — return
            // the normalized absolute path as-is.
            return path.normalize(absolute);
        }
    }
}
