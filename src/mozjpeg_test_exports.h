/*
 * mozjpeg_test_exports.h
 *
 * Test exports for FFI validation against Rust implementation.
 * These functions expose internal mozjpeg functionality for unit testing.
 */

#ifndef MOZJPEG_TEST_EXPORTS_H
#define MOZJPEG_TEST_EXPORTS_H

#include "jpeglib.h"
#include "jdct.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Forward DCT on a single 8x8 block.
 * Input: 8x8 block of samples (level-shifted by -128)
 * Output: 8x8 block of DCT coefficients (scaled by 8)
 */
EXTERN(void) mozjpeg_test_fdct_islow(DCTELEM *data);

/*
 * Quality to scale factor conversion.
 * Quality 1-100 maps to scale factor for quantization tables.
 * Q50 = 100 (no scaling), Q100 = 0, Q1 = 5000
 */
EXTERN(int) mozjpeg_test_quality_scaling(int quality);

/*
 * RGB to YCbCr conversion for a single pixel.
 * Uses BT.601 coefficients with 16-bit fixed-point arithmetic.
 */
EXTERN(void) mozjpeg_test_rgb_to_ycbcr(
    int r, int g, int b,
    int *y, int *cb, int *cr
);

/*
 * Quantize a single DCT coefficient.
 * Uses rounding to nearest (not truncation).
 */
EXTERN(JCOEF) mozjpeg_test_quantize_coef(DCTELEM coef, UINT16 quantval);

/*
 * Get number of bits needed to represent a value.
 * Returns 0 for input 0, otherwise ceil(log2(abs(value)+1)).
 */
EXTERN(int) mozjpeg_test_nbits(int value);

/*
 * Downsample h2v2 (4:2:0) for testing.
 * Takes 2 input rows, produces 1 output row of half width.
 */
EXTERN(void) mozjpeg_test_downsample_h2v2(
    const JSAMPLE *row0,
    const JSAMPLE *row1,
    JSAMPLE *output,
    JDIMENSION width
);

/*
 * Overshoot deringing preprocessing for testing.
 * Applies deringing to level-shifted (centered) 8x8 block samples.
 * Uses natural (zigzag) order for traversal.
 *
 * Input/Output: data - 64 level-shifted samples (-128 to +127)
 * dc_quant - DC quantization value (used to limit overshoot)
 */
EXTERN(void) mozjpeg_test_preprocess_deringing(
    DCTELEM *data,
    UINT16 dc_quant
);

/*
 * Trellis quantization on a single 8x8 block.
 * This is a simplified test export of the core trellis algorithm.
 *
 * src - Raw DCT coefficients (64 values, scaled by 8)
 * quantized - Output quantized coefficients (64 values)
 * qtbl - Quantization table (64 values)
 * ac_huffsi - AC Huffman code sizes (256 values, from c_derived_tbl.ehufsi)
 * lambda_log_scale1 - Lambda scale parameter 1 (default: 14.75)
 * lambda_log_scale2 - Lambda scale parameter 2 (default: 16.5)
 */
EXTERN(void) mozjpeg_test_trellis_quantize_block(
    const JCOEF *src,
    JCOEF *quantized,
    const UINT16 *qtbl,
    const signed char *ac_huffsi,
    float lambda_log_scale1,
    float lambda_log_scale2
);

#ifdef __cplusplus
}
#endif

#endif /* MOZJPEG_TEST_EXPORTS_H */
