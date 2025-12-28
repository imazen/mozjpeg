#!/bin/bash
# Update locked test references (PROTECTED - requires explicit confirmation)
# Only use when making INTENTIONAL improvements, never for refactoring
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
LOCKED_TESTS_DIR="$PROJECT_ROOT/locked-tests"

# PROTECTION: Require explicit confirmation
if [ "${1:-}" != "--confirm" ]; then
    cat <<EOF
================================================================================
PROTECTED OPERATION: Reference Update
================================================================================

This script updates the locked test reference outputs.

ONLY run this if:
  1. You have made an INTENTIONAL algorithm improvement
  2. You have verified the change improves compression (not a regression)
  3. You understand this will change the baseline forever
  4. You have measured file size impact across ALL test images

This should NEVER be used during refactoring work. Refactoring must produce
byte-identical output.

To proceed, run:
  $0 --confirm

To see what would change without updating:
  ./locked-tests/scripts/verify-references.sh

================================================================================
EOF
    exit 1
fi

# PROTECTION: Require clean git state (uncommitted changes could mask issues)
if [ -n "$(git -C "$PROJECT_ROOT" status --porcelain 2>/dev/null | grep -v '^??' | head -1)" ]; then
    echo "ERROR: Working directory has uncommitted changes."
    echo "Commit or stash changes first to ensure a clean baseline."
    echo ""
    git -C "$PROJECT_ROOT" status --short
    exit 1
fi

echo "================================================================================
REFERENCE UPDATE IN PROGRESS
================================================================================"
echo ""

# Save old checksums for comparison
OLD_CHECKSUMS="$LOCKED_TESTS_DIR/references/checksums.sha256"
OLD_SIZES="$LOCKED_TESTS_DIR/references/filesizes.json"

if [ -f "$OLD_CHECKSUMS" ]; then
    cp "$OLD_CHECKSUMS" "$OLD_CHECKSUMS.bak"
fi
if [ -f "$OLD_SIZES" ]; then
    cp "$OLD_SIZES" "$OLD_SIZES.bak"
fi

# Regenerate references
"$SCRIPT_DIR/generate-references.sh"

echo ""
echo "================================================================================"
echo "REFERENCE UPDATE SUMMARY"
echo "================================================================================"
echo ""

# Compare old and new file sizes
if [ -f "$OLD_SIZES.bak" ]; then
    python3 - <<'PYTHON'
import json
import sys

try:
    with open('locked-tests/references/filesizes.json.bak') as f:
        old = json.load(f)
    with open('locked-tests/references/filesizes.json') as f:
        new = json.load(f)
except Exception as e:
    print(f"Could not compare sizes: {e}")
    sys.exit(0)

improved = 0
regressed = 0
unchanged = 0
total_old = 0
total_new = 0

changes = []

for name in sorted(set(old.keys()) | set(new.keys())):
    old_size = old.get(name, 0)
    new_size = new.get(name, 0)
    total_old += old_size
    total_new += new_size

    if old_size == new_size:
        unchanged += 1
    elif new_size < old_size:
        improved += 1
        diff = old_size - new_size
        pct = (diff / old_size) * 100 if old_size > 0 else 0
        changes.append(f"  IMPROVED: {name}: {old_size} -> {new_size} (-{diff} bytes, -{pct:.2f}%)")
    else:
        regressed += 1
        diff = new_size - old_size
        pct = (diff / old_size) * 100 if old_size > 0 else 0
        changes.append(f"  REGRESSED: {name}: {old_size} -> {new_size} (+{diff} bytes, +{pct:.2f}%)")

print(f"Files improved:  {improved}")
print(f"Files regressed: {regressed}")
print(f"Files unchanged: {unchanged}")
print("")
print(f"Total old size: {total_old:,} bytes")
print(f"Total new size: {total_new:,} bytes")

diff = total_new - total_old
pct = (diff / total_old) * 100 if total_old > 0 else 0
if diff < 0:
    print(f"Net improvement: {-diff:,} bytes ({-pct:.2f}%)")
elif diff > 0:
    print(f"Net REGRESSION:  {diff:,} bytes (+{pct:.2f}%)")
else:
    print("No net change")

if regressed > 0:
    print("")
    print("WARNING: Some files got LARGER. Review carefully:")
    for c in changes:
        if "REGRESSED" in c:
            print(c)

if improved > 0 and len(changes) < 50:
    print("")
    print("Improvements:")
    for c in changes:
        if "IMPROVED" in c:
            print(c)
PYTHON

    # Clean up backup
    rm -f "$OLD_CHECKSUMS.bak" "$OLD_SIZES.bak"
fi

echo ""
echo "================================================================================"
echo ""

# Check for regressions
REGRESSED=$(python3 -c "
import json
with open('$LOCKED_TESTS_DIR/references/filesizes.json') as f:
    new = json.load(f)
try:
    with open('$LOCKED_TESTS_DIR/references/filesizes.json.bak') as f:
        old = json.load(f)
    print(sum(1 for k in new if new.get(k, 0) > old.get(k, 0)))
except:
    print(0)
" 2>/dev/null || echo "0")

if [ "$REGRESSED" != "0" ] && [ "$REGRESSED" -gt "0" ]; then
    echo "WARNING: $REGRESSED files got LARGER."
    echo ""
    read -p "Accept regressions and commit? [y/N] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Aborted. References updated but not committed."
        echo "To restore old references: git checkout locked-tests/references/"
        exit 1
    fi
fi

echo ""
read -p "Commit these reference changes? [y/N] " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    cd "$PROJECT_ROOT"
    git add locked-tests/references/

    # Generate commit message with summary
    SUMMARY=$(python3 -c "
import json
try:
    with open('locked-tests/references/filesizes.json') as f:
        new = json.load(f)
    with open('locked-tests/references/filesizes.json.bak') as f:
        old = json.load(f)
    total_old = sum(old.values())
    total_new = sum(new.values())
    diff = total_new - total_old
    pct = (diff / total_old) * 100 if total_old else 0
    if diff < 0:
        print(f'Net: {-diff} bytes smaller ({-pct:.2f}%)')
    elif diff > 0:
        print(f'Net: {diff} bytes larger (+{pct:.2f}%)')
    else:
        print('No size change')
except:
    print('Size comparison unavailable')
" 2>/dev/null || echo "")

    git commit -m "test: Update locked test references

$SUMMARY

Intentional reference update. Verify file size impact above.
"
    echo ""
    echo "References committed. Remember to push and create a PR."
else
    echo ""
    echo "References updated but not committed."
    echo "To commit manually: git add locked-tests/references/ && git commit"
    echo "To restore: git checkout locked-tests/references/"
fi
