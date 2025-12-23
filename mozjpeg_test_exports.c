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
