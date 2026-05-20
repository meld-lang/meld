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
const fc = __importStar(require("fast-check"));
const assert = __importStar(require("assert"));
const path = __importStar(require("path"));
const fs = __importStar(require("fs"));
/**
 * Feature: vscode-meld, Property 34: Semantic token integration
 *
 * Verifies that all 14 daemon token types are declared in the extension's
 * semanticTokenScopes and map to valid VS Code TextMate scopes.
 *
 * Validates: Requirements 7.5
 */
// The exact token type names the daemon's SemanticTokenProvider::token_type_names() returns.
// Order must match meld-daemon/src/semantic_token_provider.cpp.
const DAEMON_TOKEN_TYPES = [
    'keyword', 'function', 'variable', 'type', 'parameter',
    'property', 'string', 'number', 'comment', 'operator',
    'decorator', 'event', 'macro', 'namespace',
];
// Valid TextMate scope prefixes per VS Code conventions
const VALID_SCOPE_PREFIXES = [
    'keyword', 'entity', 'variable', 'string', 'constant',
    'comment', 'storage', 'support', 'meta', 'markup',
    'punctuation', 'invalid',
];
function loadPackageJson() {
    const pkgPath = path.resolve(__dirname, '..', '..', 'package.json');
    return JSON.parse(fs.readFileSync(pkgPath, 'utf-8'));
}
function getSemanticTokenScopes() {
    const pkg = loadPackageJson();
    const contributes = pkg['contributes'];
    const scopes = contributes['semanticTokenScopes'];
    const meldEntry = scopes?.find(s => s.language === 'meld');
    return meldEntry?.scopes ?? {};
}
describe('Property 34: Semantic token integration', () => {
    const scopes = getSemanticTokenScopes();
    it('every daemon token type has a scope mapping in package.json', () => {
        // Property: for all token types T in DAEMON_TOKEN_TYPES, scopes[T] is defined and non-empty
        fc.assert(fc.property(fc.constantFrom(...DAEMON_TOKEN_TYPES), (tokenType) => {
            const mapping = scopes[tokenType];
            assert.ok(mapping, `Missing semanticTokenScopes mapping for daemon token type "${tokenType}"`);
            assert.ok(mapping.length > 0, `Empty scope array for token type "${tokenType}"`);
        }), { numRuns: 100 });
    });
    it('all scope mappings use valid TextMate scope prefixes', () => {
        fc.assert(fc.property(fc.constantFrom(...DAEMON_TOKEN_TYPES), (tokenType) => {
            const mapping = scopes[tokenType];
            if (!mapping)
                return; // covered by previous test
            for (const scope of mapping) {
                const prefix = scope.split('.')[0];
                assert.ok(VALID_SCOPE_PREFIXES.includes(prefix), `Scope "${scope}" for token type "${tokenType}" has invalid prefix "${prefix}"`);
            }
        }), { numRuns: 100 });
    });
    it('all scope mappings end with .meld suffix', () => {
        fc.assert(fc.property(fc.constantFrom(...DAEMON_TOKEN_TYPES), (tokenType) => {
            const mapping = scopes[tokenType];
            if (!mapping)
                return;
            for (const scope of mapping) {
                assert.ok(scope.endsWith('.meld'), `Scope "${scope}" for token type "${tokenType}" should end with .meld`);
            }
        }), { numRuns: 100 });
    });
    it('no extra scopes exist beyond daemon token types', () => {
        const declaredTypes = Object.keys(scopes);
        for (const declared of declaredTypes) {
            assert.ok(DAEMON_TOKEN_TYPES.includes(declared), `package.json declares scope for "${declared}" which is not in the daemon's token type list`);
        }
    });
});
//# sourceMappingURL=semanticTokens.property.test.js.map