/*
 * mozjpeg_test_exports.c
 *
 * Implementation of test exports for FFI validation.
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include "jdct.h"
#include "mozjpeg_test_exports.h"
#include <math.h>

/* Include the nbits implementation */
#include "jpeg_nbits.h"

/*
 * Forward DCT - calls the internal islow implementation directly.
 * This is defined in jfdctint.c.
 */
extern void jpeg_fdct_islow(DCTELEM *data);

GLOBAL(void)
mozjpeg_test_fdct_islow(DCTELEM *data)
{
    jpeg_fdct_islow(data);
}

/*
 * Quality scaling - reimplemented here to expose the algorithm.
 * Original is in jcparam.c:jpeg_quality_scaling()
 */
GLOBAL(int)
mozjpeg_test_quality_scaling(int quality)
{
    /* Safety limit on quality factor */
    if (quality <= 0) quality = 1;
    if (quality > 100) quality = 100;

    /* The basic table is used as-is (scale 100) for quality 50.
     * Qualities 1-50 are scaled up, 51-100 are scaled down.
     */
    if (quality < 50)
        return 5000 / quality;
    else
        return 200 - quality * 2;
}

/*
 * RGB to YCbCr conversion.
 * Uses the same fixed-point constants as jccolor.c.
 *
 * BT.601 coefficients:
 *   Y  =  0.299*R + 0.587*G + 0.114*B
 *   Cb = -0.169*R - 0.331*G + 0.500*B + 128
 *   Cr =  0.500*R - 0.419*G - 0.081*B + 128
 *
 * Scaled by 2^16 for fixed-point:
 *   Y  = (19595*R + 38470*G + 7471*B + 32768) >> 16
 *   Cb = (-11059*R - 21709*G + 32768*B + 32768) >> 16 + 128
 *   Cr = (32768*R - 27439*G - 5329*B + 32768) >> 16 + 128
 */
GLOBAL(void)
mozjpeg_test_rgb_to_ycbcr(int r, int g, int b, int *y, int *cb, int *cr)
{
    /* Fixed-point constants (scaled by 2^16) */
    const int FIX_0_299 = 19595;
    const int FIX_0_587 = 38470;
    const int FIX_0_114 = 7471;
    const int FIX_0_169 = 11059;
    const int FIX_0_331 = 21709;
    const int FIX_0_500 = 32768;
    const int FIX_0_419 = 27439;
    const int FIX_0_081 = 5329;
    const int ONE_HALF = 32768;  /* 0.5 in fixed-point */

    *y = (FIX_0_299 * r + FIX_0_587 * g + FIX_0_114 * b + ONE_HALF) >> 16;
    *cb = ((-FIX_0_169 * r - FIX_0_331 * g + FIX_0_500 * b + ONE_HALF) >> 16) + 128;
    *cr = ((FIX_0_500 * r - FIX_0_419 * g - FIX_0_081 * b + ONE_HALF) >> 16) + 128;

    /* Clamp to valid range */
    if (*y < 0) *y = 0;
    if (*y > 255) *y = 255;
    if (*cb < 0) *cb = 0;
    if (*cb > 255) *cb = 255;
    if (*cr < 0) *cr = 0;
    if (*cr > 255) *cr = 255;
}

/*
 * Quantize a single coefficient.
 * Uses rounding to nearest: (coef + quantval/2) / quantval
 * Handles negative values correctly.
 */
GLOBAL(JCOEF)
mozjpeg_test_quantize_coef(DCTELEM coef, UINT16 quantval)
{
    int temp, qval;

    /* Handle sign */
    if (coef < 0) {
        temp = -coef;
        temp += quantval >> 1;  /* Add half for rounding */
        qval = temp / quantval;
        return (JCOEF)(-qval);
    } else {
        temp = coef;
        temp += quantval >> 1;
        qval = temp / quantval;
        return (JCOEF)qval;
    }
}

/*
 * Get number of bits needed to represent a value.
 */
GLOBAL(int)
mozjpeg_test_nbits(int value)
{
    if (value == 0)
        return 0;
    if (value < 0)
        value = -value;
    return JPEG_NBITS(value);
}

/*
 * Downsample h2v2 (4:2:0).
 * Averages 2x2 blocks with alternating bias for rounding.
 */
GLOBAL(void)
mozjpeg_test_downsample_h2v2(
    const JSAMPLE *row0,
    const JSAMPLE *row1,
    JSAMPLE *output,
    JDIMENSION width
)
{
    JDIMENSION outcol;
    int bias = 1;  /* 1, 2, 1, 2, ... */

    for (outcol = 0; outcol < width / 2; outcol++) {
        int sum = row0[0] + row0[1] + row1[0] + row1[1];
        *output++ = (JSAMPLE)((sum + bias) >> 2);
        bias ^= 3;  /* Toggle between 1 and 2 */
        row0 += 2;
        row1 += 2;
    }
}

/*
 * Catmull-Rom spline interpolation for deringing.
 * See jcdctmgr.c:catmull_rom() for original.
 */
static float
catmull_rom_test(const DCTELEM value1, const DCTELEM value2,
                 const DCTELEM value3, const DCTELEM value4,
                 const float t, int size)
{
    const int tan1 = (value3 - value1) * size;
    const int tan2 = (value4 - value2) * size;

    const float t2 = t * t;
    const float t3 = t2 * t;

    const float f1 = 2.f * t3 - 3.f * t2 + 1.f;
    const float f2 = -2.f * t3 + 3.f * t2;
    const float f3 = t3 - 2.f * t2 + t;
    const float f4 = t3 - t2;

    return value2 * f1 + tan1 * f3 +
           value3 * f2 + tan2 * f4;
}

/*
 * Overshoot deringing preprocessing.
 * Reimplemented here to expose for testing.
 * See jcdctmgr.c:preprocess_deringing() for original.
 */
GLOBAL(void)
mozjpeg_test_preprocess_deringing(DCTELEM *data, UINT16 dc_quant)
{
    const DCTELEM maxsample = 255 - CENTERJSAMPLE;  /* 127 */
    const int size = DCTSIZE * DCTSIZE;  /* 64 */

    int sum = 0;
    int maxsample_count = 0;
    int i;
    DCTELEM maxovershoot;
    int n;

    for (i = 0; i < size; i++) {
        sum += data[i];
        if (data[i] >= maxsample) {
            maxsample_count++;
        }
    }

    /* If nothing reaches max value or block is completely flat, return */
    if (!maxsample_count || maxsample_count == size) {
        return;
    }

    /* Calculate maximum safe overshoot */
    {
        int dc_limit = 2 * dc_quant;
        int headroom = (maxsample * size - sum) / maxsample_count;
        int overshoot_limit = 31;
        if (dc_limit < overshoot_limit) overshoot_limit = dc_limit;
        if (headroom < overshoot_limit) overshoot_limit = headroom;
        maxovershoot = maxsample + overshoot_limit;
    }

    n = 0;
    do {
        int start, end, length;
        DCTELEM f1, f2, l1, l2, fslope, lslope;
        float step, position;

        /* Pixels are traversed in zig-zag order to process them as a line */
        if (data[jpeg_natural_order[n]] < maxsample) {
            n++;
            continue;
        }

        /* Find a run of maxsample pixels */
        start = n;
        while (++n < size && data[jpeg_natural_order[n]] >= maxsample) {}
        end = n;

        /* Get values around edges for slope calculation */
        f1 = data[jpeg_natural_order[start >= 1 ? start - 1 : 0]];
        f2 = data[jpeg_natural_order[start >= 2 ? start - 2 : 0]];

        l1 = data[jpeg_natural_order[end < size - 1 ? end : size - 1]];
        l2 = data[jpeg_natural_order[end < size - 2 ? end + 1 : size - 1]];

        fslope = f1 - f2;
        if (maxsample - f1 > fslope) fslope = maxsample - f1;
        lslope = l1 - l2;
        if (maxsample - l1 > lslope) lslope = maxsample - l1;

        /* If at start/end of block, make curve symmetric */
        if (start == 0) {
            fslope = lslope;
        }
        if (end == size) {
            lslope = fslope;
        }

        /* Apply Catmull-Rom interpolation across the run */
        length = end - start;
        step = 1.f / (float)(length + 1);
        position = step;

        for (i = start; i < end; i++, position += step) {
            DCTELEM tmp = (DCTELEM)ceilf(catmull_rom_test(
                maxsample - fslope, maxsample, maxsample, maxsample - lslope,
                position, length));
            if (tmp > maxovershoot) tmp = maxovershoot;
            data[jpeg_natural_order[i]] = tmp;
        }
        n++;
    } while (n < size);
}

/*
 * Trellis quantization on a single 8x8 block.
 * This is a reimplementation of the core algorithm from jcdctmgr.c
 * for testing purposes, without the cinfo dependency.
 *
 * The algorithm uses dynamic programming to find the optimal quantization
 * decisions that minimize: Cost = Rate + Lambda * Distortion
 */
GLOBAL(void)
mozjpeg_test_trellis_quantize_block(
    const JCOEF *src,
    JCOEF *quantized,
    const UINT16 *qtbl,
    const signed char *ac_huffsi,
    float lambda_log_scale1,
    float lambda_log_scale2
)
{
    int i, j, k;
    float accumulated_zero_dist[DCTSIZE2];
    float accumulated_cost[DCTSIZE2];
    int run_start[DCTSIZE2];
    float best_cost;
    int last_coeff_idx;
    const int max_coef_bits = 10; /* 8-bit data precision + 2 */
    float norm = 0.0;
    float lambda;
    float lambda_tbl[DCTSIZE2];
    int Ss = 1, Se = 63;

    /* Mode 1: use flat lambda weights = 1/q^2 */
    for (i = 0; i < DCTSIZE2; i++) {
        lambda_tbl[i] = 1.0f / (qtbl[i] * qtbl[i]);
    }

    /* Calculate block norm from AC coefficients */
    for (i = 1; i < DCTSIZE2; i++) {
        norm += (float)src[i] * (float)src[i];
    }
    norm /= 63.0f;

    /* Calculate lambda */
    if (lambda_log_scale2 > 0.0f) {
        lambda = powf(2.0f, lambda_log_scale1) /
                 (powf(2.0f, lambda_log_scale2) + norm);
    } else {
        lambda = powf(2.0f, lambda_log_scale1 - 12.0f);
    }

    /* Quantize DC coefficient with simple rounding */
    {
        int sign = src[0] >> 15;
        int x = abs(src[0]);
        int q = 8 * qtbl[0];
        int qval = (x + q/2) / q;
        if (qval >= (1 << max_coef_bits))
            qval = (1 << max_coef_bits) - 1;
        quantized[0] = qval * (1 + 2 * sign);
    }

    /* Initialize AC coefficients to zero */
    for (i = 1; i < DCTSIZE2; i++) {
        quantized[i] = 0;
    }

    accumulated_zero_dist[Ss - 1] = 0.0f;
    accumulated_cost[Ss - 1] = 0.0f;
    run_start[Ss - 1] = 0;

    /* Process AC coefficients in zigzag order */
    for (i = Ss; i <= Se; i++) {
        int z = jpeg_natural_order[i];
        int sign = src[z] >> 15;
        int x = abs(src[z]);
        int q = 8 * qtbl[z];
        int qval;
        float zero_dist;
        int num_candidates;
        int candidate_vals[16];
        int candidate_bits[16];
        float candidate_dists[16];

        /* Distortion from zeroing this coefficient */
        zero_dist = (float)x * (float)x * lambda * lambda_tbl[z];
        accumulated_zero_dist[i] = zero_dist + accumulated_zero_dist[i - 1];

        /* Quantized value with rounding */
        qval = (x + q/2) / q;

        if (qval == 0) {
            /* Coefficient rounds to zero - no choice needed */
            quantized[z] = 0;
            accumulated_cost[i] = 1e38f; /* Very large */
            run_start[i] = i - 1;
            continue;
        }

        /* Clamp to valid range */
        if (qval >= (1 << max_coef_bits))
            qval = (1 << max_coef_bits) - 1;

        /* Generate candidate quantized values: 1, 3, 7, 15, ..., and qval */
        num_candidates = JPEG_NBITS(qval);
        for (k = 0; k < num_candidates; k++) {
            int candidate_val;
            int delta;
            float dist;

            if (k < num_candidates - 1) {
                candidate_val = (2 << k) - 1; /* 1, 3, 7, 15, ... */
            } else {
                candidate_val = qval;
            }

            delta = candidate_val * q - x;
            dist = (float)delta * (float)delta * lambda * lambda_tbl[z];

            candidate_vals[k] = candidate_val;
            candidate_bits[k] = k + 1;
            candidate_dists[k] = dist;
        }

        /* Find optimal choice using dynamic programming */
        accumulated_cost[i] = 1e38f;
        run_start[i] = i - 1;

        /* Try starting a run from each valid previous position */
        for (j = Ss - 1; j < i; j++) {
            int zz;
            int zero_run;
            int zrl_cost = 0;
            int run_mod_16;

            if (j != Ss - 1) {
                zz = jpeg_natural_order[j];
                if (quantized[zz] == 0)
                    continue;
            }

            zero_run = i - 1 - j;

            /* Cost of ZRL codes for runs >= 16 */
            if (zero_run >= 16) {
                int zrl_size = ac_huffsi[0xF0]; /* ZRL symbol */
                if (zrl_size == 0)
                    continue;
                zrl_cost = (zero_run / 16) * zrl_size;
            }

            run_mod_16 = zero_run & 15;

            /* Try each candidate value */
            for (k = 0; k < num_candidates; k++) {
                int symbol;
                int code_size;
                int rate;
                float zero_run_dist;
                float prev_cost;
                float cost;

                /* Huffman symbol: (run << 4) | size */
                symbol = (run_mod_16 << 4) | candidate_bits[k];
                code_size = ac_huffsi[symbol];
                if (code_size == 0)
                    continue;

                /* Rate = Huffman code + value bits + ZRL codes */
                rate = code_size + candidate_bits[k] + zrl_cost;

                /* Cost = rate + distortion of this coef + distortion of zeros in run */
                zero_run_dist = accumulated_zero_dist[i - 1] - accumulated_zero_dist[j];
                prev_cost = (j == Ss - 1) ? 0.0f : accumulated_cost[j];
                cost = (float)rate + candidate_dists[k] + zero_run_dist + prev_cost;

                if (cost < accumulated_cost[i]) {
                    quantized[z] = candidate_vals[k] * (1 + 2 * sign);
                    accumulated_cost[i] = cost;
                    run_start[i] = j;
                }
            }
        }
    }

    /* Find optimal ending point (last non-zero coefficient) */
    {
        float eob_cost = (float)ac_huffsi[0x00]; /* EOB symbol size */

        best_cost = accumulated_zero_dist[Se] + eob_cost;
        last_coeff_idx = Ss - 1;

        for (i = Ss; i <= Se; i++) {
            int z = jpeg_natural_order[i];
            if (quantized[z] != 0) {
                float tail_zero_dist = accumulated_zero_dist[Se] - accumulated_zero_dist[i];
                float cost = accumulated_cost[i] + tail_zero_dist;
                if (i < Se)
                    cost += eob_cost;

                if (cost < best_cost) {
                    best_cost = cost;
                    last_coeff_idx = i;
                }
            }
        }
    }

    /* Zero out coefficients after optimal ending and those in runs */
    i = Se;
    while (i >= Ss) {
        while (i > last_coeff_idx) {
            int z = jpeg_natural_order[i];
            quantized[z] = 0;
            i--;
        }
        if (i >= Ss) {
            last_coeff_idx = run_start[i];
            i--;
        }
    }
}
