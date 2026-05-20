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
exports.MeldDebugAdapterDescriptorFactory = void 0;
const vscode = __importStar(require("vscode"));
const path = __importStar(require("path"));
const fs = __importStar(require("fs"));
const net = __importStar(require("net"));
const child_process_1 = require("child_process");
/**
 * Debug adapter factory that routes to the correct Meld debugging backend
 * based on the launch.json `mode` field.
 *
 * - "interpret" (default): spawns `meld run --debug --debug-wait --port <port>`
 *   and connects VS Code's DAP client to the AST interpreter's DAP server.
 * - "compiled": delegates to CodeLLDB or `meld debug attach --dap` with
 *   `initCommands` loading `meld_formatters.py`.
 *
 * Requirements: 19.1, 19.2, 19.3
 */
class MeldDebugAdapterDescriptorFactory {
    constructor() {
        this.childProcesses = [];
    }
    async createDebugAdapterDescriptor(session, _executable) {
        const config = session.configuration;
        const mode = config.mode || 'interpret';
        if (mode === 'compiled') {
            return this.resolveCompiled(session);
        }
        return this.resolveInterpret(session);
    }
    /**
     * Interpret mode: spawn `meld run --debug --debug-wait --port <port> <program>`,
     * then return a DebugAdapterServer pointing at that port.
     */
    async resolveInterpret(session) {
        const config = session.configuration;
        const program = config.program || '${file}';
        const port = await this.findFreePort();
        const args = [
            'run',
            '--debug',
            '--debug-wait',
            '--port',
            String(port),
            program,
        ];
        const meldBin = this.resolveMeldBin(config.cwd || vscode.workspace.workspaceFolders?.[0]?.uri.fsPath);
        const child = (0, child_process_1.spawn)(meldBin, args, {
            cwd: config.cwd || vscode.workspace.workspaceFolders?.[0]?.uri.fsPath,
            stdio: 'pipe',
        });
        this.childProcesses.push(child);
        child.on('error', (err) => {
            vscode.window.showErrorMessage(`Failed to start Meld interpreter debug session: ${err.message}`);
        });
        // Wait briefly for the DAP server to start listening
        await this.waitForPort(port, 5000);
        return new vscode.DebugAdapterServer(port);
    }
    /**
     * Compiled mode: delegate to CodeLLDB or `meld debug attach --dap`.
     *
     * Builds initCommands that auto-load meld_formatters.py so kernel types
     * display correctly in the Variables panel.
     */
    async resolveCompiled(session) {
        const config = session.configuration;
        const program = config.program;
        if (program && !fs.existsSync(program)) {
            vscode.window.showErrorMessage(`Compiled binary not found: ${program}. Run 'meld build --debug' first.`);
        }
        const extensionPath = this.getExtensionPath();
        const formatterPath = path.join(extensionPath, 'formatters', 'meld_formatters.py');
        const initCommands = [];
        if (fs.existsSync(formatterPath)) {
            initCommands.push(`command script import ${formatterPath}`);
        }
        // Try meld debug attach --dap if a PID is provided
        const pid = config.pid;
        if (pid !== undefined) {
            const port = await this.findFreePort();
            const args = [
                'debug',
                'attach',
                String(pid),
                '--dap',
                '--port',
                String(port),
            ];
            const child = (0, child_process_1.spawn)(this.resolveMeldBin(config.cwd || vscode.workspace.workspaceFolders?.[0]?.uri.fsPath), args, {
                cwd: config.cwd ||
                    vscode.workspace.workspaceFolders?.[0]?.uri.fsPath,
                stdio: 'pipe',
            });
            this.childProcesses.push(child);
            child.on('error', (err) => {
                vscode.window.showErrorMessage(`Failed to start Meld compiled debug session: ${err.message}`);
            });
            await this.waitForPort(port, 5000);
            return new vscode.DebugAdapterServer(port);
        }
        // Delegate to CodeLLDB by returning undefined — VS Code will use the
        // debug configuration's initCommands to load formatters automatically.
        // The caller should set initCommands on the launch config before this
        // point, but we inject them here as a fallback.
        if (initCommands.length > 0) {
            config.initCommands = [
                ...(config.initCommands || []),
                ...initCommands,
            ];
        }
        return undefined;
    }
    /**
     * Find a free TCP port starting from 4711, incrementing on conflict.
     */
    findFreePort(startPort = 4711) {
        return new Promise((resolve, reject) => {
            const tryPort = (port) => {
                if (port > startPort + 100) {
                    reject(new Error('Could not find a free port'));
                    return;
                }
                const server = net.createServer();
                server.once('error', () => tryPort(port + 1));
                server.once('listening', () => {
                    server.close(() => resolve(port));
                });
                server.listen(port, '127.0.0.1');
            };
            tryPort(startPort);
        });
    }
    /**
     * Wait for a TCP port to accept connections, with a timeout.
     */
    waitForPort(port, timeoutMs) {
        return new Promise((resolve) => {
            const deadline = Date.now() + timeoutMs;
            const attempt = () => {
                if (Date.now() > deadline) {
                    resolve(); // proceed anyway; the DAP client will retry
                    return;
                }
                const socket = net.createConnection({ port, host: '127.0.0.1' });
                socket.once('connect', () => {
                    socket.destroy();
                    resolve();
                });
                socket.once('error', () => {
                    setTimeout(attempt, 100);
                });
            };
            attempt();
        });
    }
    /**
     * Resolve the meld CLI binary, preferring workspace-local tools/meld.
     */
    resolveMeldBin(workspaceRoot) {
        if (workspaceRoot) {
            const local = path.join(workspaceRoot, 'tools', 'meld');
            if (fs.existsSync(local))
                return local;
            const bazelBin = path.join(workspaceRoot, 'bazel-bin', 'meld-examples', 'meld');
            if (fs.existsSync(bazelBin))
                return bazelBin;
        }
        return 'meld';
    }
    /**
     * Resolve the VS Code extension's installation path.
     */
    getExtensionPath() {
        const ext = vscode.extensions.getExtension('meld-lang.meld');
        if (ext) {
            return ext.extensionPath;
        }
        // Fallback: use __dirname relative path
        return path.resolve(__dirname, '..');
    }
    /**
     * Kill all spawned child processes on dispose.
     */
    dispose() {
        for (const child of this.childProcesses) {
            if (!child.killed) {
                child.kill();
            }
        }
        this.childProcesses = [];
    }
}
exports.MeldDebugAdapterDescriptorFactory = MeldDebugAdapterDescriptorFactory;
//# sourceMappingURL=debugAdapterFactory.js.map