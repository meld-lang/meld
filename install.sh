#!/bin/bash
# install.sh — Build and install Meld from source
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
INSTALL_DIR="${MELD_INSTALL_DIR:-$HOME/.local/bin}"

echo "Building Meld..."
cd "$SCRIPT_DIR"
bazel build //meld-cli:meld 2>&1 | grep -v "^INFO:" | grep -v "^Loading:" | grep -v "^Analyzing:" || true

BINARY="$SCRIPT_DIR/bazel-bin/meld-cli/meld"
if [ ! -f "$BINARY" ]; then
    echo "error: Build failed. Run 'bazel build //meld-cli:meld' for details."
    exit 1
fi

mkdir -p "$INSTALL_DIR"
ln -sf "$BINARY" "$INSTALL_DIR/meld"

echo ""
echo "✓ Meld v0.1.0 installed to $INSTALL_DIR/meld"
echo ""

# Check if INSTALL_DIR is in PATH
if ! echo "$PATH" | tr ':' '\n' | grep -q "^$INSTALL_DIR$"; then
    echo "Add to your shell profile:"
    echo "  export PATH=\"$INSTALL_DIR:\$PATH\""
    echo ""
fi

echo "Try it:"
echo "  meld run --text examples/01-hello-world.meld"
echo "  meld run -i"
echo "  meld doctor"
