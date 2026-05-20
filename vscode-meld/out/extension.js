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
exports.activate = activate;
exports.deactivate = deactivate;
const vscode = __importStar(require("vscode"));
const languageClient_1 = require("./languageClient");
const commandProvider_1 = require("./commandProvider");
const taskProvider_1 = require("./taskProvider");
const debugAdapterFactory_1 = require("./debugAdapterFactory");
const debugConfigProvider_1 = require("./debugConfigProvider");
const testController_1 = require("./testController");
let languageClient;
let statusBarItem;
async function activate(context) {
    // Status bar
    statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 0);
    statusBarItem.text = '$(symbol-event) Meld';
    statusBarItem.tooltip = 'Meld Language Support';
    statusBarItem.command = 'meld.showDiagnostics';
    statusBarItem.show();
    context.subscriptions.push(statusBarItem);
    // Language client (LSP)
    const config = vscode.workspace.getConfiguration('meld');
    if (config.get('languageServer.enabled', true)) {
        languageClient = new languageClient_1.MeldLanguageClient(context, statusBarItem);
        await languageClient.start();
    }
    // Commands
    (0, commandProvider_1.registerCommands)(context, () => languageClient);
    // Task provider
    context.subscriptions.push(vscode.tasks.registerTaskProvider('meld', new taskProvider_1.MeldTaskProvider()));
    // Debug adapter
    const debugFactory = new debugAdapterFactory_1.MeldDebugAdapterDescriptorFactory();
    context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory('meld', debugFactory), vscode.debug.registerDebugConfigurationProvider('meld', new debugConfigProvider_1.MeldDebugConfigurationProvider()), { dispose: () => debugFactory.dispose() });
    // Test controller
    new testController_1.MeldTestController(context);
    // Welcome message on first activation
    const hasShownWelcome = context.globalState.get('meld.welcomeShown');
    if (!hasShownWelcome) {
        context.globalState.update('meld.welcomeShown', true);
        const action = await vscode.window.showInformationMessage('Welcome to Meld! The extension provides syntax highlighting, IntelliSense, debugging, and build integration.', 'Open Documentation', 'Dismiss');
        if (action === 'Open Documentation') {
            vscode.env.openExternal(vscode.Uri.parse('https://meld-lang.org/docs'));
        }
    }
}
async function deactivate() {
    if (languageClient) {
        await languageClient.stop();
    }
}
//# sourceMappingURL=extension.js.map