import * as vscode from 'vscode';
import * as path from 'path';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind,
} from 'vscode-languageclient/node';

/**
 * Manages the Meld LSP client lifecycle: start, stop, restart,
 * graceful degradation, and reconnection with exponential backoff.
 *
 * Validates: Requirements 7.1, 7.2, 7.3, 7.4, 7.5
 */
export class MeldLanguageClient {
    private client: LanguageClient | undefined;
    private context: vscode.ExtensionContext;
    private statusBar: vscode.StatusBarItem;
    private reconnectAttempts = 0;
    private maxReconnectAttempts = 5;
    private disposed = false;
    private _resolvedServerPath: string | undefined;

    constructor(context: vscode.ExtensionContext, statusBar: vscode.StatusBarItem) {
        this.context = context;
        this.statusBar = statusBar;
    }

    async start(): Promise<void> {
        const serverPath = this.resolveServerPath();
        this._resolvedServerPath = serverPath ?? undefined;
        if (!serverPath) {
            this.setStatus('$(warning) Meld (no LSP)', 'Meld language server not found. Install it or set meld.languageServer.path.');
            return;
        }

        const serverOptions: ServerOptions = {
            command: serverPath,
            args: ['lsp'],
            transport: TransportKind.stdio,
        };

        const config = vscode.workspace.getConfiguration('meld');
        const trace = config.get<string>('languageServer.trace', 'off');

        const clientOptions: LanguageClientOptions = {
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

        this.client = new LanguageClient('meld', 'Meld Language Server', serverOptions, clientOptions);

        this.client.onDidChangeState((e) => {
            if (e.newState === 1 /* Running */) {
                this.reconnectAttempts = 0;
                this.setStatus('$(check) Meld', 'Meld language server running');
            } else if (e.newState === 3 /* Stopped */) {
                if (!this.disposed) {
                    this.handleDisconnect();
                }
            }
        });

        try {
            await this.client.start();
            this.setStatus('$(check) Meld', 'Meld language server running');
        } catch (err: unknown) {
            const msg = err instanceof Error ? err.message : String(err);
            this.setStatus('$(warning) Meld (offline)', `Language server failed to start: ${msg}`);
            this.handleDisconnect();
        }
    }

    async stop(): Promise<void> {
        this.disposed = true;
        if (this.client) {
            await this.client.stop();
            this.client = undefined;
        }
    }

    async restart(): Promise<void> {
        this.disposed = false;
        this.reconnectAttempts = 0;
        await this.stop();
        this.disposed = false;
        await this.start();
    }

    get isRunning(): boolean {
        return this.client?.isRunning() ?? false;
    }

    /** The server path resolved during the last start() call. */
    get resolvedServerPath(): string | undefined {
        return this._resolvedServerPath;
    }

    private handleDisconnect(): void {
        if (this.disposed) return;
        if (this.reconnectAttempts >= this.maxReconnectAttempts) {
            this.setStatus('$(error) Meld (disconnected)', 'Language server disconnected. Use "Meld: Restart Language Server" to reconnect.');
            vscode.window.showWarningMessage(
                'Meld language server disconnected after multiple attempts. Syntax highlighting remains active.',
                'Restart Server'
            ).then((action) => {
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

    private resolveServerPath(): string | undefined {
        const config = vscode.workspace.getConfiguration('meld');
        const configPath = config.get<string>('languageServer.path', '');
        if (configPath) return configPath;

        // Try workspace-local tools/meld first (dev workflow)
        const wsFolder = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;
        if (wsFolder) {
            const localMeld = path.join(wsFolder, 'tools', 'meld');
            try {
                if (require('fs').existsSync(localMeld)) return localMeld;
            } catch { /* fall through */ }
            const bazelMeld = path.join(wsFolder, 'bazel-bin', 'meld-examples', 'meld');
            try {
                if (require('fs').existsSync(bazelMeld)) return bazelMeld;
            } catch { /* fall through */ }
        }

        // Fall back to 'meld' from PATH
        return 'meld';
    }

    private setStatus(text: string, tooltip: string): void {
        this.statusBar.text = text;
        this.statusBar.tooltip = tooltip;
    }
}
