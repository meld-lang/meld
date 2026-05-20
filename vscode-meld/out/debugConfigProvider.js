"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.MeldDebugConfigurationProvider = void 0;
const vscode = __importStar(require("vscode"));
const fs = __importStar(require("fs"));
const path = __importStar(require("path"));
/**
 * Debug configuration provider that supplies default launch configurations
 * and auto-detects the appropriate debug mode.
 *
 * Provides two default configurations:
 * - "Meld: Interpret" — runs the AST interpreter with DAP
 * - "Meld: Compiled" — launches a compiled binary under LLDB
 *
 * Auto-detection: if `build/` contains a compiled binary, the compiled
 * configuration is suggested automatically.
 *
 * Requirements: 19.4, 19.5
 */
class MeldDebugConfigurationProvider {
    /**
     * Return the two default debug configurations for the "Add Configuration"
     * dropdown and initial launch.json generation.
     */
    provideDebugConfigurations(_folder, _token) {
        return [
            {
                type: 'meld',
                request: 'launch',
                name: 'Meld: Interpret',
                mode: 'interpret',
                program: '${file}',
            },
            {
                type: 'meld',
                request: 'launch',
                name: 'Meld: Compiled',
                mode: 'compiled',
                program: '${workspaceFolder}/build/${workspaceFolderBasename}',
            },
        ];
    }
    /**
     * Resolve / fill in a debug configuration before the session starts.
     *
     * - If no config is provided (F5 with no launch.json), default to
     *   interpret mode with the current file.
     * - If mode is "compiled", verify the binary exists and warn if missing.
     * - Auto-detect: when `build/` contains files, suggest the compiled config.
     */
    async resolveDebugConfiguration(folder, config, _token) {
        // No config provided — user pressed F5 without a launch.json
        if (!config.type && !config.request && !config.name) {
            const workspacePath = folder?.uri.fsPath;
            // Auto-detect: if build/ contains a compiled binary, offer a choice
            if (workspacePath && this.hasCompiledBinary(workspacePath)) {
                const choice = await vscode.window.showQuickPick([
                    {
                        label: 'Meld: Interpret',
                        description: 'Run with AST interpreter (default)',
                        mode: 'interpret',
                    },
                    {
                        label: 'Meld: Compiled',
                        description: 'Debug compiled binary under LLDB',
                        mode: 'compiled',
                    },
                ], { placeHolder: 'Select debug configuration' });
                if (!choice) {
                    return undefined; // user cancelled
                }
                if (choice.mode === 'compiled') {
                    const folderName = path.basename(workspacePath);
                    return {
                        type: 'meld',
                        request: 'launch',
                        name: 'Meld: Compiled',
                        mode: 'compiled',
                        program: path.join(workspacePath, 'build', folderName),
                    };
                }
            }
            // Default: interpret mode with the current file
            return {
                type: 'meld',
                request: 'launch',
                name: 'Meld: Interpret',
                mode: 'interpret',
                program: vscode.window.activeTextEditor?.document.fileName ?? '${file}',
            };
        }
        // Validate compiled mode — warn if binary doesn't exist
        if (config.mode === 'compiled' && config.program) {
            const resolvedProgram = this.resolveVariables(config.program, folder);
            if (resolvedProgram && !fs.existsSync(resolvedProgram)) {
                const action = await vscode.window.showWarningMessage(`Compiled binary not found: ${resolvedProgram}. Run 'meld build --debug' first.`, 'Continue Anyway', 'Cancel');
                if (action === 'Cancel') {
                    return undefined;
                }
            }
        }
        return config;
    }
    /**
     * Check whether `build/` exists in the workspace and contains at least
     * one file (indicating a compiled binary is available).
     */
    hasCompiledBinary(workspacePath) {
        const buildDir = path.join(workspacePath, 'build');
        try {
            if (!fs.existsSync(buildDir) || !fs.statSync(buildDir).isDirectory()) {
                return false;
            }
            const entries = fs.readdirSync(buildDir);
            return entries.length > 0;
        }
        catch {
            return false;
        }
    }
    /**
     * Best-effort resolution of common VS Code variables in a path string.
     * Returns null if the path still contains unresolved variables.
     */
    resolveVariables(value, folder) {
        let resolved = value;
        if (folder) {
            const workspacePath = folder.uri.fsPath;
            resolved = resolved.replace(/\$\{workspaceFolder\}/g, workspacePath);
            resolved = resolved.replace(/\$\{workspaceFolderBasename\}/g, path.basename(workspacePath));
        }
        // If unresolved variables remain, we can't validate the path
        if (/\$\{.+\}/.test(resolved)) {
            return null;
        }
        return resolved;
    }
}
exports.MeldDebugConfigurationProvider = MeldDebugConfigurationProvider;
//# sourceMappingURL=debugConfigProvider.js.map