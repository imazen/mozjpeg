#!/bin/bash
# Generate reference outputs for locked tests
# MUST be run ONCE on pristine upstream mozjpeg before any refactoring
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
LOCKED_TESTS_DIR="$PROJECT_ROOT/locked-tests"

# Configuration
CJPEG="${CJPEG:-$PROJECT_ROOT/build/cjpeg}"
CODEC_CORPUS="${CODEC_CORPUS:-$PROJECT_ROOT/../codec-corpus}"
OUTPUT_DIR="$LOCKED_TESTS_DIR/references/outputs"
QUALITY_LEVELS=(5 15 25 35 45 55 65 75 85 95)
MODES=("baseline" "progressive")

# Verify cjpeg exists
if [ ! -x "$CJPEG" ]; then
    echo "ERROR: cjpeg not found at $CJPEG"
    echo "Build mozjpeg first: mkdir build && cd build && cmake .. && make"
    exit 1
fi

# Verify codec-corpus exists
if [ ! -d "$CODEC_CORPUS" ]; then
    echo "ERROR: codec-corpus not found at $CODEC_CORPUS"
    echo "Clone it: git clone https://github.com/imazen/codec-corpus ../codec-corpus"
    exit 1
fi

# Clean and create output directory
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

echo "Generating reference outputs..."
echo "  cjpeg: $CJPEG"
echo "  codec-corpus: $CODEC_CORPUS"
echo "  output: $OUTPUT_DIR"
echo ""

total=0
failed=0

while IFS= read -r line || [ -n "$line" ]; do
    # Skip comments and empty lines
    [[ -z "$line" || "$line" =~ ^# ]] && continue

    image="$line"

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
        echo "WARNING: Image not found: $image_path"
        failed=$((failed + 1))
        continue
    fi

    for q in "${QUALITY_LEVELS[@]}"; do
        for mode in "${MODES[@]}"; do
            testname="${basename}-q${q}-${mode}"
            outfile="$OUTPUT_DIR/${testname}.jpg"

            if [ "$mode" = "progressive" ]; then
                if ! "$CJPEG" -quality "$q" -progressive -outfile "$outfile" "$image_path" 2>/dev/null; then
                    echo "FAIL: $testname (encoding failed)"
                    failed=$((failed + 1))
                    continue
                fi
            else
                if ! "$CJPEG" -quality "$q" -baseline -outfile "$outfile" "$image_path" 2>/dev/null; then
                    echo "FAIL: $testname (encoding failed)"
                    failed=$((failed + 1))
                    continue
                fi
            fi

            total=$((total + 1))
            echo "OK: $testname ($(stat -c%s "$outfile") bytes)"
        done
    done
done < "$LOCKED_TESTS_DIR/test-images.txt"

echo ""
echo "Generated $total reference files"

if [ $failed -gt 0 ]; then
    echo "WARNING: $failed tests failed to generate"
fi

# Generate checksums
echo "Generating checksums..."
cd "$OUTPUT_DIR"
sha256sum *.jpg | sort > ../checksums.sha256

# Generate file sizes JSON
echo "Generating file sizes..."
python3 - <<'PYTHON'
import os
import json

sizes = {}
for f in sorted(os.listdir('.')):
    if f.endswith('.jpg'):
        sizes[f] = os.path.getsize(f)

with open('../filesizes.json', 'w') as out:
    json.dump(sizes, out, indent=2, sort_keys=True)

print(f"Recorded {len(sizes)} file sizes")

# Summary stats
total_size = sum(sizes.values())
print(f"Total reference size: {total_size:,} bytes ({total_size/1024/1024:.2f} MB)")
PYTHON

# Generate metadata
echo "Generating metadata..."
cat > "$LOCKED_TESTS_DIR/references/REFERENCE_INFO.json" <<EOF
{
    "generated_at": "$(date -Iseconds)",
    "mozjpeg_version": "$(git -C "$PROJECT_ROOT" describe --tags --always 2>/dev/null || echo 'unknown')",
    "git_sha": "$(git -C "$PROJECT_ROOT" rev-parse HEAD 2>/dev/null || echo 'unknown')",
    "git_status": "$(git -C "$PROJECT_ROOT" status --porcelain 2>/dev/null | wc -l) uncommitted changes",
    "platform": "$(uname -s) $(uname -r) $(uname -m)",
    "cjpeg_path": "$CJPEG",
    "quality_levels": [5, 15, 25, 35, 45, 55, 65, 75, 85, 95],
    "modes": ["baseline", "progressive"],
    "test_images": $(cat "$LOCKED_TESTS_DIR/test-images.txt" | grep -v '^#' | grep -v '^$' | wc -l),
    "total_tests": $total,
    "notes": "Reference outputs for locked regression tests. Any byte-level difference fails CI."
}
EOF

echo ""
echo "Reference generation complete!"
echo "  Checksums: $LOCKED_TESTS_DIR/references/checksums.sha256"
echo "  File sizes: $LOCKED_TESTS_DIR/references/filesizes.json"
echo "  Metadata: $LOCKED_TESTS_DIR/references/REFERENCE_INFO.json"
echo "  Outputs: $OUTPUT_DIR/"
