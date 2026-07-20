#!/bin/bash
# Red Plasma development environment setup
# Run this once after cloning the repository
#
# What it does:
#   - Installs the pre-commit hook (glossary sort + OS call check)
#
# Requirements:
#   - Python 3
#   - Rust (install via rustup: https://rustup.rs)
#
# Usage:
#   chmod +x setup.sh
#   ./setup.sh

set -e

echo "Red Plasma — development environment setup"
echo "==========================================="
echo ""

# Check Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "ERROR: Python 3 is required for development tooling."
    echo "Install it via your package manager:"
    echo "  Fedora/RHEL:  sudo dnf install python3"
    echo "  Ubuntu/Debian: sudo apt install python3"
    echo "  Windows:       https://python.org"
    exit 1
fi

PYTHON_VERSION=$(python3 --version)
echo "Found: $PYTHON_VERSION"

# Check Rust is available
if ! command -v rustc &> /dev/null; then
    echo ""
    echo "WARNING: Rust not found. Install via rustup before building:"
    echo "  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh"
    echo ""
else
    RUST_VERSION=$(rustc --version)
    echo "Found: $RUST_VERSION"
fi

# Install pre-commit hook
echo ""
echo "Installing pre-commit hook..."

if [ ! -d ".git" ]; then
    echo "ERROR: No .git folder found. Run this script from the repo root."
    exit 1
fi

cp tools/pre-commit .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit
echo "Pre-commit hook installed."

echo ""
echo "Setup complete."
echo ""
echo "The pre-commit hook will now run automatically before every commit."
echo "It does two things:"
echo "  1. Sorts all tables in docs/GLOSSARY.md alphabetically"
echo "  2. Checks for raw OS calls outside os/ and blocks the commit if found"
echo ""
echo "To test the hook manually:"
echo "  .git/hooks/pre-commit"
