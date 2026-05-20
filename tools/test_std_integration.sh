#!/bin/bash
# test_std_integration.sh — Run std/ library examples through the interpreter
# Verifies that the standard library files parse and compile without errors.
#
# Usage: bazel test //meld-examples:std_integration_test
#        or: ./tools/test_std_integration.sh

set -euo pipefail

MELD_RUNNER="${1:-./bazel-bin/meld-examples/meld-runner}"
STD_DIR="meld-core/std"
EXAMPLES_DIR="meld-examples/examples"
PASS=0
FAIL=0
ERRORS=""

run_test() {
    local file="$1"
    local name
    name=$(basename "$file")
    if "$MELD_RUNNER" "$file" >/dev/null 2>&1; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        ERRORS="${ERRORS}\n  FAIL: ${file}"
    fi
}

echo "=== Meld Standard Library Integration Tests ==="
echo ""

# Test 1: All std/ .meld files parse without errors
echo "── Phase 1: Parse std/ library files ──"
for f in $(find "$STD_DIR" -name '*.meld' | sort); do
    run_test "$f"
done
echo "  Parsed: $PASS files"

# Test 2: All examples compile and run
echo ""
echo "── Phase 2: Run example programs ──"
EXAMPLE_PASS=0
EXAMPLE_FAIL=0
for f in $(find "$EXAMPLES_DIR" -name '*.meld' | sort); do
    name=$(basename "$f")
    if "$MELD_RUNNER" "$f" >/dev/null 2>&1; then
        EXAMPLE_PASS=$((EXAMPLE_PASS + 1))
    else
        EXAMPLE_FAIL=$((EXAMPLE_FAIL + 1))
        ERRORS="${ERRORS}\n  FAIL: ${f}"
    fi
done
echo "  Passed: $EXAMPLE_PASS, Failed: $EXAMPLE_FAIL"

# Summary
echo ""
echo "── Summary ──"
TOTAL_PASS=$((PASS + EXAMPLE_PASS))
TOTAL_FAIL=$((FAIL + EXAMPLE_FAIL))
echo "  Total: $((TOTAL_PASS + TOTAL_FAIL)) tests, $TOTAL_PASS passed, $TOTAL_FAIL failed"

if [ $TOTAL_FAIL -gt 0 ]; then
    echo ""
    echo "Failures:"
    echo -e "$ERRORS"
    exit 1
fi

echo ""
echo "All tests passed ✓"
exit 0
