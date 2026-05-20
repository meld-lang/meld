#!/bin/bash
# meld-conformance/run.sh — Run all conformance fixtures
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
MELD="${MELD:-$ROOT_DIR/bazel-bin/meld-cli/meld}"
PASS=0
FAIL=0
SKIP=0
ERRORS=""

for fixture in "$SCRIPT_DIR"/fixtures/*.meld; do
    name=$(basename "$fixture" .meld)
    expected="$SCRIPT_DIR/fixtures/${name}.expected"
    
    if [ ! -f "$expected" ]; then
        echo "SKIP: $name (no .expected file)"
        SKIP=$((SKIP + 1))
        continue
    fi

    # Run and extract program output
    raw=$("$MELD" run "$fixture" --warnings-ok 2>&1)
    # If output is JSON, extract the "output" field
    if echo "$raw" | grep -q '"output"'; then
        actual=$(echo "$raw" | python3 -c 'import sys,json; d=json.load(sys.stdin); print(d.get("output",""),end="")' 2>/dev/null | grep -v "^$" || true)
    else
        # Legacy format: filter metadata lines
        actual=$(echo "$raw" | grep -v "^=== Meld Runner ===" | grep -v "^File:" | grep -v "^Compilation:" | grep -v "^  Functions" | grep -v "^  Provenance" | grep -v "^  Borrow" | grep -v "^  Flows" | grep -v "^  Safety" | grep -v "^Diagnostics:" | grep -v "^  \[INFO\]" | grep -v "^  \[WARN\]" | grep -v "^=== Done ===" | grep -v "^$" || true)
    fi
    expected_content=$(cat "$expected" | grep -v "^$" || true)

    if [ "$actual" = "$expected_content" ]; then
        echo "PASS: $name"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $name"
        ERRORS="${ERRORS}\n--- $name ---\nExpected:\n$expected_content\nActual:\n$actual\n"
        FAIL=$((FAIL + 1))
    fi
done

echo ""
echo "Results: $PASS passed, $FAIL failed, $SKIP skipped"

if [ $FAIL -gt 0 ]; then
    echo -e "$ERRORS"
    exit 1
fi
