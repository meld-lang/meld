import * as fc from 'fast-check';
import * as assert from 'assert';
import * as path from 'path';
import * as fs from 'fs';

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
] as const;

// Valid TextMate scope prefixes per VS Code conventions
const VALID_SCOPE_PREFIXES = [
    'keyword', 'entity', 'variable', 'string', 'constant',
    'comment', 'storage', 'support', 'meta', 'markup',
    'punctuation', 'invalid',
];

function loadPackageJson(): Record<string, unknown> {
    const pkgPath = path.resolve(__dirname, '..', '..', 'package.json');
    return JSON.parse(fs.readFileSync(pkgPath, 'utf-8'));
}

function getSemanticTokenScopes(): Record<string, string[]> {
    const pkg = loadPackageJson();
    const contributes = pkg['contributes'] as Record<string, unknown>;
    const scopes = contributes['semanticTokenScopes'] as Array<{
        language?: string;
        scopes: Record<string, string[]>;
    }>;
    const meldEntry = scopes?.find(s => s.language === 'meld');
    return meldEntry?.scopes ?? {};
}

describe('Property 34: Semantic token integration', () => {
    const scopes = getSemanticTokenScopes();

    it('every daemon token type has a scope mapping in package.json', () => {
        // Property: for all token types T in DAEMON_TOKEN_TYPES, scopes[T] is defined and non-empty
        fc.assert(
            fc.property(
                fc.constantFrom(...DAEMON_TOKEN_TYPES),
                (tokenType) => {
                    const mapping = scopes[tokenType];
                    assert.ok(mapping, `Missing semanticTokenScopes mapping for daemon token type "${tokenType}"`);
                    assert.ok(mapping.length > 0, `Empty scope array for token type "${tokenType}"`);
                }
            ),
            { numRuns: 100 }
        );
    });

    it('all scope mappings use valid TextMate scope prefixes', () => {
        fc.assert(
            fc.property(
                fc.constantFrom(...DAEMON_TOKEN_TYPES),
                (tokenType) => {
                    const mapping = scopes[tokenType];
                    if (!mapping) return; // covered by previous test
                    for (const scope of mapping) {
                        const prefix = scope.split('.')[0];
                        assert.ok(
                            VALID_SCOPE_PREFIXES.includes(prefix),
                            `Scope "${scope}" for token type "${tokenType}" has invalid prefix "${prefix}"`
                        );
                    }
                }
            ),
            { numRuns: 100 }
        );
    });

    it('all scope mappings end with .meld suffix', () => {
        fc.assert(
            fc.property(
                fc.constantFrom(...DAEMON_TOKEN_TYPES),
                (tokenType) => {
                    const mapping = scopes[tokenType];
                    if (!mapping) return;
                    for (const scope of mapping) {
                        assert.ok(
                            scope.endsWith('.meld'),
                            `Scope "${scope}" for token type "${tokenType}" should end with .meld`
                        );
                    }
                }
            ),
            { numRuns: 100 }
        );
    });

    it('no extra scopes exist beyond daemon token types', () => {
        const declaredTypes = Object.keys(scopes);
        for (const declared of declaredTypes) {
            assert.ok(
                (DAEMON_TOKEN_TYPES as readonly string[]).includes(declared),
                `package.json declares scope for "${declared}" which is not in the daemon's token type list`
            );
        }
    });
});
