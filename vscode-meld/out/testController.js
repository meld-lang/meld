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
exports.MeldTestController = void 0;
const vscode = __importStar(require("vscode"));
const path = __importStar(require("path"));
const fs = __importStar(require("fs"));
const child_process_1 = require("child_process");
/**
 * Meld Test Controller — integrates with VS Code's Test Explorer.
 * Discovers test functions (fnc test-*) in .meld files and runs them
 * via `meld test`.
 *
 * Requirements: 8.1, 8.2
 */
class MeldTestController {
    constructor(context) {
        this.runProfiles = [];
        this.controller = vscode.tests.createTestController('meldTests', 'Meld Tests');
        context.subscriptions.push(this.controller);
        // Run profile
        this.runProfiles.push(this.controller.createRunProfile('Run', vscode.TestRunProfileKind.Run, (request, token) => {
            this.runTests(request, token);
        }));
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
    async discoverAllTests() {
        const files = await vscode.workspace.findFiles('**/*.meld', '**/bazel-*/**');
        for (const file of files) {
            await this.discoverTestsInFile(file);
        }
    }
    async discoverTestsInFile(uri) {
        try {
            const content = await fs.promises.readFile(uri.fsPath, 'utf-8');
            const lines = content.split('\n');
            // Find test functions: fnc test-* or @test annotations
            const testPattern = /^\s*(?:@test\s+)?fnc\s+(test[-\w]+)/;
            const tests = [];
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
            const fileItem = this.controller.createTestItem(uri.toString(), relativePath, uri);
            // Add individual test items
            for (const test of tests) {
                const testItem = this.controller.createTestItem(`${uri.toString()}#${test.name}`, test.name, uri);
                testItem.range = new vscode.Range(test.line, 0, test.line, 0);
                fileItem.children.add(testItem);
            }
            this.controller.items.add(fileItem);
        }
        catch {
            // File read error — skip
        }
    }
    removeTestsForFile(uri) {
        this.controller.items.delete(uri.toString());
    }
    async runTests(request, token) {
        const run = this.controller.createTestRun(request);
        const items = request.include ?? this.gatherAllTests();
        for (const item of items) {
            if (token.isCancellationRequested)
                break;
            run.started(item);
            try {
                const result = await this.executeTest(item);
                if (result.passed) {
                    run.passed(item, result.duration);
                }
                else {
                    run.failed(item, new vscode.TestMessage(result.message), result.duration);
                }
            }
            catch (err) {
                const msg = err instanceof Error ? err.message : String(err);
                run.errored(item, new vscode.TestMessage(msg));
            }
        }
        run.end();
    }
    gatherAllTests() {
        const items = [];
        this.controller.items.forEach(fileItem => {
            fileItem.children.forEach(testItem => {
                items.push(testItem);
            });
        });
        return items;
    }
    executeTest(item) {
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
            const proc = (0, child_process_1.spawn)(meldBin, ['test', '--filter', testName, '--json', uri.fsPath], {
                cwd: wsFolder,
            });
            let stdout = '';
            let stderr = '';
            proc.stdout?.on('data', (data) => { stdout += data.toString(); });
            proc.stderr?.on('data', (data) => { stderr += data.toString(); });
            proc.on('close', (code) => {
                const duration = Date.now() - startTime;
                if (code === 0) {
                    resolve({ passed: true, message: '', duration });
                }
                else {
                    // Try to parse JSON output for failure details
                    let message = stderr || 'Test failed';
                    try {
                        const result = JSON.parse(stdout);
                        if (result.tests?.length > 0 && !result.tests[0].passed) {
                            message = result.tests[0].failure?.message || message;
                        }
                    }
                    catch { /* use stderr */ }
                    resolve({ passed: false, message, duration });
                }
            });
            proc.on('error', (err) => {
                resolve({ passed: false, message: err.message, duration: Date.now() - startTime });
            });
        });
    }
}
exports.MeldTestController = MeldTestController;
//# sourceMappingURL=testController.js.map