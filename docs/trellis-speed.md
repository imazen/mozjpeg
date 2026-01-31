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
| 1-6 | Conservative (only the densest blocks are limited) |
| 7 | **Default** (balanced speed/quality) |
| 8-10 | Aggressive (lower threshold, targets more blocks) |

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

## Empirical Analysis on CID22 Corpus

Measured on the CID22 validation set (41 diverse images, 512x512) at default
level 7 (max_lookback=12, max_ac_candidates=5 when triggered).

### Trigger Rates and Limit Binding

The key question is not just how often the threshold is exceeded, but whether
the limits actually constrain the search when they're applied.

| Quality | Blocks limited | Lookback binds | Candidate binds |
|---------|---------------|----------------|-----------------|
| Q75 | 32 / 251,904 (0.01%) | 100% of limited | 12.5% of limited |
| Q85 | 796 / 335,872 (0.24%) | 100% of limited | 61.9% of limited |
| Q90 | 3,854 / 503,808 (0.77%) | 100% of limited | 87.7% of limited |
| Q95 | 19,121 / 503,808 (3.8%) | 100% of limited | 91.5% of limited |
| Q97 | 40,742 / 503,808 (8.1%) | 100% of limited | 95.4% of limited |
| Q99 | 104,477 / 503,808 (20.7%) | 100% of limited | 77.1% of limited |
| Q100 | 189,887 / 503,808 (37.7%) | 100% of limited | 58.5% of limited |

**Key findings:**

1. **Dormant below Q90:** Under 1% of blocks are affected at Q75-Q90.
   The optimization primarily targets high-quality encoding (Q95+).

2. **Lookback limit always binds:** When a block enters limited mode,
   the lookback limit constrains the search 100% of the time. This is
   the primary source of speedup.

3. **Candidate limit binds at Q90-Q97:** In the quality range where the
   speed optimization matters most (Q90-Q97), the candidate limit binds
   in 88-95% of limited blocks. At Q100 it drops to 58% because smaller
   quantization steps produce smaller quantized values with fewer
   Huffman categories.

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

### Baseline Q100 DSSIM (vs Original)

For context, here is the DSSIM of Q100 compression itself (comparing compressed
output to uncompressed original), using default speed level 7:

| Image | Q100 vs Original | Speed Delta (L10-L0) | Delta as % of Baseline |
|-------|------------------|----------------------|------------------------|
| kodak/1 | 0.00005183 | 0.00000915 | 18% |
| kodak/5 | 0.00003983 | 0.00000684 | 17% |
| kodak/8 | 0.00003917 | 0.00000604 | 15% |
| kodak/13 | 0.00003733 | 0.00000870 | 23% |
| kodak/19 | 0.00007556 | 0.00001202 | 16% |

**Full Kodak corpus at Q100:** mean DSSIM 0.00007, range 0.00004-0.00009

The speed optimization's worst-case impact (~0.00001) adds roughly 15-20%
additional DSSIM on top of the baseline compression loss. Both values remain
well below 0.0001 (generally considered imperceptible).

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
