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
exports.MeldLanguageClient = void 0;
const vscode = __importStar(require("vscode"));
const path = __importStar(require("path"));
const node_1 = require("vscode-languageclient/node");
/**
 * Manages the Meld LSP client lifecycle: start, stop, restart,
 * graceful degradation, and reconnection with exponential backoff.
 *
 * Validates: Requirements 7.1, 7.2, 7.3, 7.4, 7.5
 */
class MeldLanguageClient {
    constructor(context, statusBar) {
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.disposed = false;
        this.context = context;
        this.statusBar = statusBar;
    }
    async start() {
        const serverPath = this.resolveServerPath();
        this._resolvedServerPath = serverPath ?? undefined;
        if (!serverPath) {
            this.setStatus('$(warning) Meld (no LSP)', 'Meld language server not found. Install it or set meld.languageServer.path.');
            return;
        }
        const serverOptions = {
            command: serverPath,
            args: ['lsp'],
            transport: node_1.TransportKind.stdio,
        };
        const config = vscode.workspace.getConfiguration('meld');
        const trace = config.get('languageServer.trace', 'off');
        const clientOptions = {
            documentSelector: [{ scheme: 'file', language: 'meld' }],
            synchronize: {
                fileEvents: [
                    vscode.workspace.createFileSystemWatcher('**/*.meld'),
                    vscode.workspace.createFileSystemWatcher('**/meld.toml'),
                ],
            },
            outputChannelName: 'Meld Language Server',
            traceOutputChannel: trace !== 'off'
                ? vscode.window.createOutputChannel('Meld LSP Trace')
                : undefined,
            initializationFailedHandler: (error) => {
                this.setStatus('$(error) Meld (LSP error)', `Language server initialization failed: ${error.message}`);
                return false;
            },
        };
        this.client = new node_1.LanguageClient('meld', 'Meld Language Server', serverOptions, clientOptions);
        this.client.onDidChangeState((e) => {
            if (e.newState === 1 /* Running */) {
                this.reconnectAttempts = 0;
                this.setStatus('$(check) Meld', 'Meld language server running');
            }
            else if (e.newState === 3 /* Stopped */) {
                if (!this.disposed) {
                    this.handleDisconnect();
                }
            }
        });
        try {
            await this.client.start();
            this.setStatus('$(check) Meld', 'Meld language server running');
        }
        catch (err) {
            const msg = err instanceof Error ? err.message : String(err);
            this.setStatus('$(warning) Meld (offline)', `Language server failed to start: ${msg}`);
            this.handleDisconnect();
        }
    }
    async stop() {
        this.disposed = true;
        if (this.client) {
            await this.client.stop();
            this.client = undefined;
        }
    }
    async restart() {
        this.disposed = false;
        this.reconnectAttempts = 0;
        await this.stop();
        this.disposed = false;
        await this.start();
    }
    get isRunning() {
        return this.client?.isRunning() ?? false;
    }
    /** The server path resolved during the last start() call. */
    get resolvedServerPath() {
        return this._resolvedServerPath;
    }
    handleDisconnect() {
        if (this.disposed)
            return;
        if (this.reconnectAttempts >= this.maxReconnectAttempts) {
            this.setStatus('$(error) Meld (disconnected)', 'Language server disconnected. Use "Meld: Restart Language Server" to reconnect.');
            vscode.window.showWarningMessage('Meld language server disconnected after multiple attempts. Syntax highlighting remains active.', 'Restart Server').then((action) => {
                if (action === 'Restart Server') {
                    this.restart();
                }
            });
            return;
        }
        this.reconnectAttempts++;
        const delay = Math.min(1000 * Math.pow(2, this.reconnectAttempts - 1), 30000);
        this.setStatus('$(sync~spin) Meld (reconnecting...)', `Reconnection attempt ${this.reconnectAttempts}/${this.maxReconnectAttempts}`);
        setTimeout(() => {
            if (!this.disposed) {
                this.start();
            }
        }, delay);
    }
    resolveServerPath() {
        const config = vscode.workspace.getConfiguration('meld');
        const configPath = config.get('languageServer.path', '');
        if (configPath)
            return configPath;
        // Try workspace-local tools/meld first (dev workflow)
        const wsFolder = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
        if (wsFolder) {
            const localMeld = path.join(wsFolder, 'tools', 'meld');
            try {
                if (require('fs').existsSync(localMeld))
                    return localMeld;
            }
            catch { /* fall through */ }
            const bazelMeld = path.join(wsFolder, 'bazel-bin', 'meld-examples', 'meld');
            try {
                if (require('fs').existsSync(bazelMeld))
                    return bazelMeld;
            }
            catch { /* fall through */ }
        }
        // Fall back to 'meld' from PATH
        return 'meld';
    }
    setStatus(text, tooltip) {
        this.statusBar.text = text;
        this.statusBar.tooltip = tooltip;
    }
}
exports.MeldLanguageClient = MeldLanguageClient;
//# sourceMappingURL=languageClient.js.map