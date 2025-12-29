# Trellis Speed Optimization

## Overview

Trellis quantization in mozjpeg has O(n²) complexity per 8x8 block due to the
Viterbi predecessor search. For high-entropy blocks (many non-zero coefficients
at high quality settings like Q95-Q100), this becomes very slow.

The trellis speed optimization detects high-entropy blocks and limits the search
space, dramatically improving encoding speed with negligible quality impact.

## API

### Parameter: `JINT_TRELLIS_SPEED_LEVEL`

Controls the speed/quality tradeoff for trellis quantization.

```c
// Set speed level (0-10)
jpeg_c_set_int_param(cinfo, JINT_TRELLIS_SPEED_LEVEL, 7);

// Get current level
int level = jpeg_c_get_int_param(cinfo, JINT_TRELLIS_SPEED_LEVEL);
```

**Level values (0-10):**
| Level | Description |
|-------|-------------|
| 0 | Thorough (full search, slowest but theoretically optimal) |
| 1-6 | Conservative (only high-entropy blocks are limited) |
| 7 | **Default** (balanced speed/quality) |
| 8-10 | Fast (more blocks use reduced search) |

### CLI Option

```bash
cjpeg -trellis-speed 10 -quality 100 input.png -outfile output.jpg  # fast
cjpeg -trellis-speed 0 -quality 100 input.png -outfile output.jpg   # thorough
```

## Algorithm

The algorithm counts non-zero DCT coefficients in each 8x8 block before trellis
quantization. When the count exceeds a threshold, reduced lookback and candidate
limits are applied.

### Formula-Based Parameter Mapping

```
threshold = 61 - level * 3
max_lookback = 26 - level * 2  (minimum: 4)
max_ac_candidates = 9 - (level + 1) / 2  (minimum: 2)
```

### Level-to-Parameter Table

| Level | Threshold | Max Lookback | Max Candidates |
|-------|-----------|--------------|----------------|
| 0 | disabled | 63 (full) | 16 (all) |
| 1 | 58 | 24 | 8 |
| 2 | 55 | 22 | 8 |
| 3 | 52 | 20 | 7 |
| 4 | 49 | 18 | 7 |
| 5 | 46 | 16 | 6 |
| 6 | 43 | 14 | 6 |
| 7 | 40 | 12 | 5 |
| 8 | 37 | 10 | 5 |
| 9 | 34 | 8 | 4 |
| 10 | 31 | 6 | 4 |

## Performance Results

### Speed by Level (kodak/13, Q100)

| Level | Time | Speedup vs L0 | DSSIM vs L0 |
|-------|------|---------------|-------------|
| 0 (thorough) | 258ms | baseline | - |
| 7 (default) | 174ms | 33% | 0.00000358 |
| 10 (fast) | 136ms | 47% | 0.00000870 |

### Quality Impact Across Kodak Corpus (Level 10 vs Level 0, Q100)

| Image | DSSIM |
|-------|-------|
| kodak/1 | 0.00000915 |
| kodak/5 | 0.00000684 |
| kodak/8 | 0.00000604 |
| kodak/13 | 0.00000870 |
| kodak/19 | 0.00001202 |

All values are well below 0.0001 (generally considered imperceptible).

## Why Level 10 is the Maximum

We tested levels 11-15 to determine the optimal ceiling:

| Level | Threshold | Lookback | Candidates | Time (kodak/13) | DSSIM |
|-------|-----------|----------|------------|-----------------|-------|
| 10 | 31 | 6 | 4 | 136ms | 0.00000870 |
| 11 | 28 | 4 | 3 | ~110ms | - |
| 12 | 25 | 4 | 3 | 110ms | 0.00001640 |
| 13 | 22 | 4 | 2 | ~110ms | - |
| 14 | 19 | 4 | 2 | ~111ms | - |
| 15 | 16 | 4 | 2 | 111ms | 0.00002006 |

**Observations:**

1. **Speed plateaus at level 12-13**: Levels 12-15 hit the parameter minimums
   (lookback=4, candidates=2), so they have nearly identical encoding times.

2. **DSSIM keeps growing**: While still under 0.0001, quality degradation
   continues to accumulate at higher levels.

3. **Diminishing returns**: Going from level 10 to 15 gains only ~10% more
   speed but doubles the DSSIM difference.

4. **Worst-case at level 15**: kodak/19 showed DSSIM of 0.00003389 - still
   imperceptible but 3x worse than level 10.

**Conclusion**: Level 10 represents the optimal ceiling where:
- Meaningful speed improvements are still possible (47% vs baseline)
- Quality impact remains negligible (~0.00001 DSSIM)
- Higher levels provide minimal additional speed benefit
- The formula hasn't yet hit parameter minimums, preserving some adaptivity

## DSSIM Reference Scale

For context on interpreting DSSIM values:

| DSSIM Range | Interpretation |
|-------------|----------------|
| < 0.0001 | Imperceptible difference |
| 0.0001 - 0.001 | Very minor, expert eye might detect |
| 0.001 - 0.01 | Minor but potentially noticeable |
| > 0.01 | Clearly visible difference |

All tested levels (0-15) remained well under the imperceptible threshold.

## Naming Rationale

The parameter is named `JINT_TRELLIS_SPEED_LEVEL` rather than "adaptive" or
"aggressive" because:

- **Higher = faster**: The name makes the tradeoff clear
- **Avoids compression terminology confusion**: In compression, "aggressive"
  typically means more compression/quality loss. Here higher levels mean
  faster encoding with slightly less optimal compression.
- **Matches user intent**: Users typically think in terms of speed vs quality,
  not algorithm internals.

## Implementation Files

- `jpeglib.h` - `JINT_TRELLIS_SPEED_LEVEL` enum constant
- `jpegint.h` - `trellis_speed_level` field in `jpeg_comp_master`
- `jcparam.c` - Default value (7)
- `jcext.c` - Accessor functions
- `jcdctmgr.c` - Core speed limiting logic in `quantize_trellis()`
- `cjpeg.c` - CLI option `-trellis-speed`
