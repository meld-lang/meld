import * as vscode from 'vscode';
import { MeldLanguageClient } from './languageClient';
import { registerCommands } from './commandProvider';
import { MeldTaskProvider } from './taskProvider';
import { MeldDebugAdapterDescriptorFactory } from './debugAdapterFactory';
import { MeldDebugConfigurationProvider } from './debugConfigProvider';
import { MeldTestController } from './testController';

let languageClient: MeldLanguageClient | undefined;
let statusBarItem: vscode.StatusBarItem;

export async function activate(context: vscode.ExtensionContext): Promise<void> {
    // Status bar
    statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 0);
    statusBarItem.text = '$(symbol-event) Meld';
    statusBarItem.tooltip = 'Meld Language Support';
    statusBarItem.command = 'meld.showDiagnostics';
    statusBarItem.show();
    context.subscriptions.push(statusBarItem);

    // Language client (LSP)
    const config = vscode.workspace.getConfiguration('meld');
    if (config.get<boolean>('languageServer.enabled', true)) {
        languageClient = new MeldLanguageClient(context, statusBarItem);
        await languageClient.start();
    }

    // Commands
    registerCommands(context, () => languageClient);

    // Task provider
    context.subscriptions.push(
        vscode.tasks.registerTaskProvider('meld', new MeldTaskProvider())
    );

    // Debug adapter
    const debugFactory = new MeldDebugAdapterDescriptorFactory();
    context.subscriptions.push(
        vscode.debug.registerDebugAdapterDescriptorFactory('meld', debugFactory),
        vscode.debug.registerDebugConfigurationProvider('meld', new MeldDebugConfigurationProvider()),
        { dispose: () => debugFactory.dispose() }
    );

    // Test controller
    new MeldTestController(context);

    // Welcome message on first activation
    const hasShownWelcome = context.globalState.get<boolean>('meld.welcomeShown');
    if (!hasShownWelcome) {
        context.globalState.update('meld.welcomeShown', true);
        const action = await vscode.window.showInformationMessage(
            'Welcome to Meld! The extension provides syntax highlighting, IntelliSense, debugging, and build integration.',
            'Open Documentation',
            'Dismiss'
        );
        if (action === 'Open Documentation') {
            vscode.env.openExternal(vscode.Uri.parse('https://meld-lang.org/docs'));
        }
    }
}

export async function deactivate(): Promise<void> {
    if (languageClient) {
        await languageClient.stop();
    }
}
