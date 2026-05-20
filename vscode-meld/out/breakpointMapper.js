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
exports.MeldBreakpointMapper = void 0;
const vscode = __importStar(require("vscode"));
const path = __importStar(require("path"));
const fs = __importStar(require("fs"));
class MeldBreakpointMapper {
    /**
     * Map VS Code source breakpoints for the given debug mode.
     *
     * For both interpret and compiled modes the breakpoints pass through
     * with normalized paths — the underlying backends understand `.meld`
     * source locations directly.
     */
    mapBreakpoints(mode, breakpoints) {
        const workspaceFolder = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath ?? '';
        return breakpoints
            .filter((bp) => bp.location.uri.fsPath.endsWith('.meld'))
            .map((bp) => {
            const filePath = this.resolveBreakpointPath(bp.location.uri.fsPath, workspaceFolder);
            const mapped = {
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
    resolveBreakpointPath(filePath, workspaceFolder) {
        // Make absolute if relative
        const absolute = path.isAbsolute(filePath)
            ? filePath
            : path.resolve(workspaceFolder, filePath);
        // Resolve symlinks when the file exists on disk
        try {
            return fs.realpathSync(absolute);
        }
        catch {
            // File may not exist yet (e.g. unsaved buffer) — return
            // the normalized absolute path as-is.
            return path.normalize(absolute);
        }
    }
}
exports.MeldBreakpointMapper = MeldBreakpointMapper;
//# sourceMappingURL=breakpointMapper.js.map