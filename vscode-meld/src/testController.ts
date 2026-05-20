import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';
import { ChildProcess, spawn } from 'child_process';

/**
 * Meld Test Controller — integrates with VS Code's Test Explorer.
 * Discovers test functions (fnc test-*) in .meld files and runs them
 * via `meld test`.
 *
 * Requirements: 8.1, 8.2
 */
export class MeldTestController {
    private controller: vscode.TestController;
    private runProfiles: vscode.TestRunProfile[] = [];

    constructor(context: vscode.ExtensionContext) {
        this.controller = vscode.tests.createTestController('meldTests', 'Meld Tests');
        context.subscriptions.push(this.controller);

        // Run profile
        this.runProfiles.push(
            this.controller.createRunProfile('Run', vscode.TestRunProfileKind.Run, (request, token) => {
                this.runTests(request, token);
            })
        );

        // Discover tests in workspace
        this.controller.resolveHandler = async (item) => {
            if (!item) {
                await this.discoverAllTests();
            }
        };

        // Watch for file changes
        const watcher = vscode.workspace.createFileSystemWatcher('**/*.meld');
        watcher.onDidChange(uri => this.discoverTestsInFile(uri));
        watcher.onDidCreate(uri => this.discoverTestsInFile(uri));
        watcher.onDidDelete(uri => this.removeTestsForFile(uri));
        context.subscriptions.push(watcher);

        // Initial discovery
        this.discoverAllTests();
    }

    private async discoverAllTests(): Promise<void> {
        const files = await vscode.workspace.findFiles('**/*.meld', '**/bazel-*/**');
        for (const file of files) {
            await this.discoverTestsInFile(file);
        }
    }

    private async discoverTestsInFile(uri: vscode.Uri): Promise<void> {
        try {
            const content = await fs.promises.readFile(uri.fsPath, 'utf-8');
            const lines = content.split('\n');

            // Find test functions: fnc test-* or @test annotations
            const testPattern = /^\s*(?:@test\s+)?fnc\s+(test[-\w]+)/;
            const tests: { name: string; line: number }[] = [];

            for (let i = 0; i < lines.length; i++) {
                const match = lines[i].match(testPattern);
                if (match) {
                    tests.push({ name: match[1], line: i });
                }
            }

            if (tests.length === 0) {
                // Remove any existing test items for this file
                this.removeTestsForFile(uri);
                return;
            }

            // Create or update file-level test item
            const relativePath = vscode.workspace.asRelativePath(uri);
            const fileItem = this.controller.createTestItem(
                uri.toString(),
                relativePath,
                uri
            );

            // Add individual test items
            for (const test of tests) {
                const testItem = this.controller.createTestItem(
                    `${uri.toString()}#${test.name}`,
                    test.name,
                    uri
                );
                testItem.range = new vscode.Range(test.line, 0, test.line, 0);
                fileItem.children.add(testItem);
            }

            this.controller.items.add(fileItem);
        } catch {
            // File read error — skip
        }
    }

    private removeTestsForFile(uri: vscode.Uri): void {
        this.controller.items.delete(uri.toString());
    }

    private async runTests(request: vscode.TestRunRequest, token: vscode.CancellationToken): Promise<void> {
        const run = this.controller.createTestRun(request);
        const items = request.include ?? this.gatherAllTests();

        for (const item of items) {
            if (token.isCancellationRequested) break;

            run.started(item);

            try {
                const result = await this.executeTest(item);
                if (result.passed) {
                    run.passed(item, result.duration);
                } else {
                    run.failed(item, new vscode.TestMessage(result.message), result.duration);
                }
            } catch (err: unknown) {
                const msg = err instanceof Error ? err.message : String(err);
                run.errored(item, new vscode.TestMessage(msg));
            }
        }

        run.end();
    }

    private gatherAllTests(): vscode.TestItem[] {
        const items: vscode.TestItem[] = [];
        this.controller.items.forEach(fileItem => {
            fileItem.children.forEach(testItem => {
                items.push(testItem);
            });
        });
        return items;
    }

    private executeTest(item: vscode.TestItem): Promise<{ passed: boolean; message: string; duration: number }> {
        return new Promise((resolve) => {
            const uri = item.uri;
            if (!uri) {
                resolve({ passed: false, message: 'No file URI', duration: 0 });
                return;
            }

            const wsFolder = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
            const meldBin = wsFolder ? path.join(wsFolder, 'tools', 'meld') : 'meld';
            const startTime = Date.now();

            // Extract test name from item id
            const testName = item.label;

            const proc = spawn(meldBin, ['test', '--filter', testName, '--json', uri.fsPath], {
                cwd: wsFolder,
            });

            let stdout = '';
            let stderr = '';

            proc.stdout?.on('data', (data: Buffer) => { stdout += data.toString(); });
            proc.stderr?.on('data', (data: Buffer) => { stderr += data.toString(); });

            proc.on('close', (code) => {
                const duration = Date.now() - startTime;
                if (code === 0) {
                    resolve({ passed: true, message: '', duration });
                } else {
                    // Try to parse JSON output for failure details
                    let message = stderr || 'Test failed';
                    try {
                        const result = JSON.parse(stdout);
                        if (result.tests?.length > 0 && !result.tests[0].passed) {
                            message = result.tests[0].failure?.message || message;
                        }
                    } catch { /* use stderr */ }
                    resolve({ passed: false, message, duration });
                }
            });

            proc.on('error', (err) => {
                resolve({ passed: false, message: err.message, duration: Date.now() - startTime });
            });
        });
    }
}
