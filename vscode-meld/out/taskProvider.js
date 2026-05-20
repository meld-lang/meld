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
exports.MeldTaskProvider = void 0;
const vscode = __importStar(require("vscode"));
const path = __importStar(require("path"));
const fs = __importStar(require("fs"));
/**
 * Task provider that auto-detects Meld projects and provides
 * build/run/test/fmt tasks for VS Code's task system.
 *
 * Validates: Requirements 4.4
 */
class MeldTaskProvider {
    provideTasks() {
        const folders = vscode.workspace.workspaceFolders;
        if (!folders)
            return [];
        const tasks = [];
        for (const folder of folders) {
            if (!this.isMeldProject(folder.uri.fsPath))
                continue;
            tasks.push(this.createTask('build', 'Build', folder), this.createTask('run', 'Run', folder), this.createTask('test', 'Test', folder), this.createTask('fmt', 'Format', folder));
        }
        return tasks;
    }
    resolveTask(task) {
        const def = task.definition;
        if (def.type !== 'meld' || !def.command)
            return undefined;
        const args = [def.command];
        if (def.file)
            args.push(def.file);
        const folder = task.scope;
        const cwd = folder?.uri.fsPath;
        return new vscode.Task(def, folder ?? vscode.TaskScope.Workspace, `meld ${def.command}`, 'meld', new vscode.ShellExecution(resolveMeldBin(cwd), args, { cwd }), '$meld');
    }
    createTask(command, label, folder) {
        const task = new vscode.Task({ type: 'meld', command }, folder, `Meld: ${label}`, 'meld', new vscode.ShellExecution(resolveMeldBin(folder.uri.fsPath), [command], { cwd: folder.uri.fsPath }), '$meld');
        if (command === 'build') {
            task.group = vscode.TaskGroup.Build;
        }
        else if (command === 'test') {
            task.group = vscode.TaskGroup.Test;
        }
        return task;
    }
    isMeldProject(dir) {
        // Check for meld.toml or any .meld files
        try {
            if (fs.existsSync(path.join(dir, 'meld.toml')))
                return true;
            const entries = fs.readdirSync(dir);
            return entries.some(e => e.endsWith('.meld'));
        }
        catch {
            return false;
        }
    }
}
exports.MeldTaskProvider = MeldTaskProvider;
function resolveMeldBin(workspaceRoot) {
    if (workspaceRoot) {
        const local = path.join(workspaceRoot, 'tools', 'meld');
        if (fs.existsSync(local))
            return local;
    }
    return 'meld';
}
//# sourceMappingURL=taskProvider.js.map