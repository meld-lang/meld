import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';
import { MeldLanguageClient } from './languageClient';

/**
 * Registers all Meld-specific commands in the command palette.
 *
 * Validates: Requirements 4.1, 4.2, 4.3, 6.3, 8.2, 8.4
 */
export function registerCommands(
    context: vscode.ExtensionContext,
    getClient: () => MeldLanguageClient | undefined
): void {
    const outputChannel = vscode.window.createOutputChannel('Meld');

    context.subscriptions.push(
        vscode.commands.registerCommand('meld.build', () => runMeldCli('build', outputChannel)),
        vscode.commands.registerCommand('meld.run', () => runMeldCli('run', outputChannel)),
        vscode.commands.registerCommand('meld.test', () => runMeldCli('test', outputChannel)),
        vscode.commands.registerCommand('meld.fmt', () => runMeldCli('fmt', outputChannel)),

        vscode.commands.registerCommand('meld.restartServer', async () => {
            const client = getClient();
            if (client) {
                await client.restart();
                vscode.window.showInformationMessage('Meld language server restarted.');
            } else {
                vscode.window.showWarningMessage('Meld language server is not enabled. Check meld.languageServer.enabled setting.');
            }
        }),

        vscode.commands.registerCommand('meld.showDiagnostics', () => {
            const client = getClient();
            const wsRoot = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
            const items: string[] = [
                `Language server: ${client?.isRunning ? 'running' : 'not running'}`,
                `Server path: ${client?.resolvedServerPath ?? '(not resolved)'}`,
                `Extension path: ${context.extensionPath}`,
                `Workspace: ${wsRoot ?? 'none'}`,
            ];

            // Daemon status from .meld/ directory
            if (wsRoot) {
                const meldDir = path.join(wsRoot, '.meld');
                const pidFile = path.join(meldDir, 'meldd.pid');
                const lspSock = path.join(meldDir, 'lsp.sock');
                const mcpSock = path.join(meldDir, 'mcp.sock');

                try {
                    const pid = fs.readFileSync(pidFile, 'utf-8').trim();
                    items.push(`Daemon PID: ${pid}`);
                } catch {
                    items.push('Daemon PID: (no .meld/meldd.pid)');
                }

                items.push(`LSP socket: ${fs.existsSync(lspSock) ? 'exists' : 'missing'} (${lspSock})`);
                items.push(`MCP socket: ${fs.existsSync(mcpSock) ? 'exists' : 'missing'} (${mcpSock})`);
            }

            outputChannel.clear();
            outputChannel.appendLine('=== Meld Diagnostics ===');
            items.forEach(i => outputChannel.appendLine(i));
            outputChannel.show();
        }),

        vscode.commands.registerCommand('meld.newFile', async () => {
            const templates = [
                { label: 'Main Program', detail: 'Entry point with main function', template: mainTemplate },
                { label: 'Struct', detail: 'Struct with trait conformance', template: structTemplate },
                { label: 'Trait', detail: 'Trait definition', template: traitTemplate },
                { label: 'Module', detail: 'Module with imports', template: moduleTemplate },
            ];

            const choice = await vscode.window.showQuickPick(templates, {
                placeHolder: 'Select a Meld file template',
            });
            if (!choice) return;

            const doc = await vscode.workspace.openTextDocument({ language: 'meld', content: choice.template });
            await vscode.window.showTextDocument(doc);
        })
    );
}

function runMeldCli(command: string, outputChannel: vscode.OutputChannel): void {
    const config = vscode.workspace.getConfiguration('meld');
    const showOutput = config.get<boolean>('build.showOutput', true);

    const cwd = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
    if (!cwd) {
        vscode.window.showErrorMessage('No workspace folder open.');
        return;
    }

    const meldBin = resolveMeldBin(cwd);
    const file = vscode.window.activeTextEditor?.document.fileName;
    const args = [command];
    if (file && (command === 'run' || command === 'fmt')) {
        args.push(file);
    }

    const task = new vscode.Task(
        { type: 'meld', command },
        vscode.TaskScope.Workspace,
        `meld ${command}`,
        'meld',
        new vscode.ShellExecution(meldBin, args, { cwd }),
        '$meld'
    );

    if (showOutput) {
        task.presentationOptions = { reveal: vscode.TaskRevealKind.Always, panel: vscode.TaskPanelKind.Shared };
    }

    vscode.tasks.executeTask(task);
}

function resolveMeldBin(workspaceRoot: string): string {
    const local = path.join(workspaceRoot, 'tools', 'meld');
    try {
        if (require('fs').existsSync(local)) return local;
    } catch { /* fall through */ }
    return 'meld';
}

// ── File templates ──────────────────────────────────────────────────

const mainTemplate = `// Main Program

fnc main() -> int {
    println("Hello, Meld!")
    rtn 0
}
`;

const structTemplate = `// Struct Definition

trait Displayable {
    fnc display() -> string
}

struct MyStruct : Displayable {
    val name: string
    val value: int
}

trait Displayable {
    fnc display(s: MyStruct) -> string {
        rtn \`MyStruct(name=\${s.name}, value=\${s.value})\`
    }
}
`;

const traitTemplate = `// Trait Definition

trait MyTrait {
    fnc process(input: int) -> int
    fnc validate() -> bool
}
`;

const moduleTemplate = `// Module

imp std.io
imp { sqrt, abs } = std.math

fnc main() -> int {
    val result = sqrt(abs(-16))
    println(\`Result: \${result}\`)
    rtn 0
}
`;
