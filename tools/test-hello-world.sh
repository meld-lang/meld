#!/bin/bash
# Regression tests for meld run: exit codes, println, and process exit.
# Usage: tools/test-hello-world.sh [path-to-meld-binary]

set -uo pipefail

MELD="${1:-bazel-bin/meld-cli/meld}"
EXAMPLES="meld-examples/examples"
PASS=0
FAIL=0

check() {
    local label="$1" file="$2" expected_exit="$3" expected_output="$4"
    local actual_output actual_exit

    actual_output=$("$MELD" run "$file" 2>&1)
    actual_exit=$?

    local ok=true
    if [ "$actual_exit" -ne "$expected_exit" ]; then
        echo "FAIL  $label: exit $actual_exit (expected $expected_exit)"
        ok=false
    fi
    if [ -n "$expected_output" ] && [ "$actual_output" != "$expected_output" ]; then
        echo "FAIL  $label: output '$actual_output' (expected '$expected_output')"
        ok=false
    fi

    if $ok; then
        echo "  ok  $label"
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
    fi
}

echo "=== meld run regression tests ==="

check "hello-world"       "$EXAMPLES/hello-world.meld"          0 "Hello, World!"
check "exit-code-zero"    "$EXAMPLES/exit-code-zero.meld"       0 ""
check "exit-code-one"     "$EXAMPLES/exit-code-one.meld"        1 ""
check "exit-code-42"      "$EXAMPLES/exit-code-42.meld"        42 ""
check "exit-code-empty"   "$EXAMPLES/exit-code-empty-main.meld" 0 ""
check "void-main-println" "$EXAMPLES/exit-code-void-main.meld"  0 "ok"

echo "=== $PASS passed, $FAIL failed ==="
exit "$FAIL"
