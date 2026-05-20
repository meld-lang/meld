import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';

/**
 * Task provider that auto-detects Meld projects and provides
 * build/run/test/fmt tasks for VS Code's task system.
 *
 * Validates: Requirements 4.4
 */
export class MeldTaskProvider implements vscode.TaskProvider {
    provideTasks(): vscode.ProviderResult<vscode.Task[]> {
        const folders = vscode.workspace.workspaceFolders;
        if (!folders) return [];

        const tasks: vscode.Task[] = [];

        for (const folder of folders) {
            if (!this.isMeldProject(folder.uri.fsPath)) continue;

            tasks.push(
                this.createTask('build', 'Build', folder),
                this.createTask('run', 'Run', folder),
                this.createTask('test', 'Test', folder),
                this.createTask('fmt', 'Format', folder),
            );
        }

        return tasks;
    }

    resolveTask(task: vscode.Task): vscode.ProviderResult<vscode.Task> {
        const def = task.definition as { type: string; command: string; file?: string };
        if (def.type !== 'meld' || !def.command) return undefined;

        const args = [def.command];
        if (def.file) args.push(def.file);

        const folder = task.scope as vscode.WorkspaceFolder | undefined;
        const cwd = folder?.uri.fsPath;

        return new vscode.Task(
            def,
            folder ?? vscode.TaskScope.Workspace,
            `meld ${def.command}`,
            'meld',
            new vscode.ShellExecution(resolveMeldBin(cwd), args, { cwd }),
            '$meld'
        );
    }

    private createTask(command: string, label: string, folder: vscode.WorkspaceFolder): vscode.Task {
        const task = new vscode.Task(
            { type: 'meld', command },
            folder,
            `Meld: ${label}`,
            'meld',
            new vscode.ShellExecution(resolveMeldBin(folder.uri.fsPath), [command], { cwd: folder.uri.fsPath }),
            '$meld'
        );

        if (command === 'build') {
            task.group = vscode.TaskGroup.Build;
        } else if (command === 'test') {
            task.group = vscode.TaskGroup.Test;
        }

        return task;
    }

    private isMeldProject(dir: string): boolean {
        // Check for meld.toml or any .meld files
        try {
            if (fs.existsSync(path.join(dir, 'meld.toml'))) return true;
            const entries = fs.readdirSync(dir);
            return entries.some(e => e.endsWith('.meld'));
        } catch {
            return false;
        }
    }
}

function resolveMeldBin(workspaceRoot: string | undefined): string {
    if (workspaceRoot) {
        const local = path.join(workspaceRoot, 'tools', 'meld');
        if (fs.existsSync(local)) return local;
    }
    return 'meld';
}
