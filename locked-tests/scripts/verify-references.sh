#!/bin/bash
# Verify current build produces byte-identical output to references
# Exit code 0 = all tests pass, 1 = at least one failure
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
LOCKED_TESTS_DIR="$PROJECT_ROOT/locked-tests"

# Configuration
CJPEG="${CJPEG:-$PROJECT_ROOT/build/cjpeg}"
CODEC_CORPUS="${CODEC_CORPUS:-$PROJECT_ROOT/../codec-corpus}"
REFERENCE_DIR="$LOCKED_TESTS_DIR/references/outputs"
QUALITY_LEVELS=(5 15 25 35 45 55 65 75 85 95)
MODES=("baseline" "progressive")

# Quick mode: only test Kodak q75 baseline (for pre-push hook)
QUICK_TEST="${QUICK_TEST:-0}"
if [ "$QUICK_TEST" = "1" ]; then
    QUALITY_LEVELS=(75)
    MODES=("baseline")
    echo "QUICK MODE: Testing Kodak q75 baseline only"
fi

# Create temp directory for test outputs
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

# Verify prerequisites
if [ ! -x "$CJPEG" ]; then
    echo "ERROR: cjpeg not found at $CJPEG"
    echo "Build mozjpeg first: mkdir build && cd build && cmake .. && make"
    exit 1
fi

if [ ! -d "$CODEC_CORPUS" ]; then
    echo "ERROR: codec-corpus not found at $CODEC_CORPUS"
    exit 1
fi

if [ ! -d "$REFERENCE_DIR" ] || [ -z "$(ls -A "$REFERENCE_DIR" 2>/dev/null)" ]; then
    echo "Reference outputs not found at $REFERENCE_DIR"
    echo "Fetching from imazen/mozjpeg-test-references..."
    REFS_REPO="https://github.com/imazen/mozjpeg-test-references.git"
    REFS_TEMP=$(mktemp -d)
    if git clone --depth 1 "$REFS_REPO" "$REFS_TEMP"; then
        mkdir -p "$REFERENCE_DIR"
        cp -r "$REFS_TEMP/outputs/"* "$REFERENCE_DIR/"
        cp "$REFS_TEMP/REFERENCE_INFO.json" "$LOCKED_TESTS_DIR/references/"
        cp "$REFS_TEMP/checksums.sha256" "$LOCKED_TESTS_DIR/references/"
        rm -rf "$REFS_TEMP"
        echo "References fetched successfully."
    else
        rm -rf "$REFS_TEMP"
        echo "ERROR: Failed to fetch references from $REFS_REPO"
        exit 1
    fi
fi

echo "Verifying locked test outputs..."
echo "  cjpeg: $CJPEG"
echo "  codec-corpus: $CODEC_CORPUS"
echo "  references: $REFERENCE_DIR"
echo ""

passed=0
failed=0
skipped=0
failures=()

while IFS= read -r line || [ -n "$line" ]; do
    # Skip comments and empty lines
    [[ -z "$line" || "$line" =~ ^# ]] && continue

    image="$line"

    # In quick mode, only test Kodak images
    if [ "$QUICK_TEST" = "1" ] && [[ ! "$image" =~ ^kodak/ ]]; then
        continue
    fi

    # Handle @testimages/ prefix (images from mozjpeg repo)
    if [[ "$image" == @testimages/* ]]; then
        image_path="$PROJECT_ROOT/${image#@}"
        basename=$(basename "$image_path")
        basename="${basename%.*}"
    else
        image_path="$CODEC_CORPUS/$image"
        basename=$(basename "$image")
        basename="${basename%.*}"
    fi

    if [ ! -f "$image_path" ]; then
        echo "SKIP: $basename (source image not found)"
        skipped=$((skipped + 1))
        continue
    fi

    for q in "${QUALITY_LEVELS[@]}"; do
        for mode in "${MODES[@]}"; do
            testname="${basename}-q${q}-${mode}"
            outfile="$TEMP_DIR/${testname}.jpg"
            reffile="$REFERENCE_DIR/${testname}.jpg"

            if [ ! -f "$reffile" ]; then
                echo "SKIP: $testname (no reference file)"
                skipped=$((skipped + 1))
                continue
            fi

            # Generate output
            if [ "$mode" = "progressive" ]; then
                if ! "$CJPEG" -quality "$q" -progressive -outfile "$outfile" "$image_path" 2>/dev/null; then
                    echo "FAIL: $testname (encoding failed)"
                    failures+=("$testname: encoding failed")
                    failed=$((failed + 1))
                    continue
                fi
            else
                if ! "$CJPEG" -quality "$q" -baseline -outfile "$outfile" "$image_path" 2>/dev/null; then
                    echo "FAIL: $testname (encoding failed)"
                    failures+=("$testname: encoding failed")
                    failed=$((failed + 1))
                    continue
                fi
            fi

            # Byte-for-byte comparison
            if cmp -s "$outfile" "$reffile"; then
                echo "PASS: $testname"
                passed=$((passed + 1))
            else
                ref_size=$(stat -c%s "$reffile")
                out_size=$(stat -c%s "$outfile")
                diff_bytes=$((out_size - ref_size))
                diff_pct=$(echo "scale=4; ($out_size - $ref_size) * 100 / $ref_size" | bc 2>/dev/null || echo "N/A")

                echo "FAIL: $testname"
                echo "      Reference: $ref_size bytes"
                echo "      Actual:    $out_size bytes"
                echo "      Diff:      $diff_bytes bytes ($diff_pct%)"

                failures+=("$testname: size $ref_size -> $out_size ($diff_bytes bytes, $diff_pct%)")
                failed=$((failed + 1))

                # Keep failed outputs for debugging
                cp "$outfile" "$TEMP_DIR/FAILED_${testname}.jpg"
            fi
        done
    done
done < "$LOCKED_TESTS_DIR/test-images.txt"

echo ""
echo "========================================"
echo "LOCKED TEST RESULTS"
echo "========================================"
echo "  Passed:  $passed"
echo "  Failed:  $failed"
echo "  Skipped: $skipped"
echo "========================================"

if [ $failed -gt 0 ]; then
    echo ""
    echo "FAILURES:"
    for f in "${failures[@]}"; do
        echo "  - $f"
    done
    echo ""
    echo "OUTPUT DIFFERS FROM LOCKED REFERENCES."
    echo ""
    echo "If this is an INTENTIONAL improvement:"
    echo "  1. Verify all changes are improvements (no regressions)"
    echo "  2. Run: ./locked-tests/scripts/update-references.sh --confirm"
    echo "  3. Create a PR with the updated references"
    echo ""
    echo "Failed outputs saved to: $TEMP_DIR/FAILED_*.jpg"
    # Don't delete temp dir on failure for debugging
    trap - EXIT
    exit 1
fi

if [ $passed -eq 0 ]; then
    echo ""
    echo "WARNING: No tests passed. Check your setup."
    exit 1
fi

echo ""
echo "All locked tests passed!"
exit 0
