#!/bin/bash
# Install git hooks for locked test verification
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
HOOKS_DIR="$PROJECT_ROOT/.git/hooks"

# Create pre-push hook
cat > "$HOOKS_DIR/pre-push" << 'HOOK'
#!/bin/bash
# Pre-push hook: Run quick locked test verification
# To skip: git push --no-verify

echo "Running locked test quick check..."

# Check if build exists
if [ ! -x "./build/cjpeg" ]; then
    echo "WARNING: cjpeg not built, skipping locked tests"
    echo "Build with: mkdir build && cd build && cmake .. && make"
    exit 0
fi

# Check if references exist
if [ ! -d "./locked-tests/references/outputs" ]; then
    echo "WARNING: No locked test references, skipping"
    exit 0
fi

# Run quick test (Kodak q75 baseline only)
QUICK_TEST=1 ./locked-tests/scripts/verify-references.sh

if [ $? -ne 0 ]; then
    echo ""
    echo "PUSH BLOCKED: Locked tests failed."
    echo "Your changes produce different output than the locked references."
    echo ""
    echo "To run full test suite: ./locked-tests/scripts/verify-references.sh"
    echo "To bypass this check (NOT recommended): git push --no-verify"
    exit 1
fi
HOOK

chmod +x "$HOOKS_DIR/pre-push"

echo "Git hooks installed:"
echo "  - pre-push: Quick locked test verification"
echo ""
echo "To skip hooks: git push --no-verify"
