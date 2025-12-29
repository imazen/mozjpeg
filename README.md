Mozilla JPEG Encoder Project (Imazen Fork)
===========================================

This is [Imazen's fork](https://github.com/imazen/mozjpeg) of MozJPEG, focused on code clarity, documentation, and performance tuning for integration with image processing pipelines.

MozJPEG improves JPEG compression efficiency achieving higher visual quality and smaller file sizes at the same time. It is compatible with the JPEG standard, and the vast majority of the world's deployed JPEG decoders.

MozJPEG is compatible with the libjpeg API and ABI. It is intended to be a drop-in replacement for libjpeg. MozJPEG is a strict superset of libjpeg-turbo's functionality. All MozJPEG's improvements can be disabled at run time, and in that case it behaves exactly like libjpeg-turbo.

MozJPEG is meant to be used as a library in graphics programs and image processing tools. We include a demo `cjpeg` command-line tool, but it's not intended for serious use. We encourage authors of graphics programs to use libjpeg's [C API](libjpeg.txt) and link with MozJPEG library instead.

## Branch Structure

| Branch | Description |
|--------|-------------|
| `main` | Active development with Imazen enhancements |
| `upstream` | Tracks [mozilla/mozjpeg](https://github.com/mozilla/mozjpeg) for merging upstream changes |

Note: Upstream MozJPEG is itself a fork of [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo). The libjpeg-turbo 3.0.x merge listed below came via upstream MozJPEG.

## Changelog (vs upstream MozJPEG 4.1.5)

### New Features
- **Trellis speed optimization** (`JINT_TRELLIS_SPEED_LEVEL`): Configurable 0-10 speed/quality tradeoff for trellis quantization. Level 10 is up to 50% faster than level 0 with imperceptible quality difference. See [docs/trellis-speed.md](docs/trellis-speed.md).
- **Locked regression tests**: Test harness ensuring refactoring safety with bit-exact output verification.

### Code Quality
- **Trellis quantization refactoring**: Extracted helpers, added named constants, comprehensive documentation of the Viterbi algorithm, rate-distortion optimization, and cross-block EOB optimization.
- **Documentation for Rust port**: Detailed interface documentation to support clean-room reimplementation.

### Build Fixes
- Fixed PNG reader linking in shared library builds
- Fixed libmath linking on Unix

### From Upstream
- libjpeg-turbo 3.0.x (lossless JPEG, 12/16-bit support, TurboJPEG 3 API)

## Features

* Progressive encoding with "jpegrescan" optimization. It can be applied to any JPEG file (with `jpegtran`) to losslessly reduce file size.
* Trellis quantization. When converting other formats to JPEG it maximizes quality/filesize ratio. A configurable speed level (0-10) allows trading thorough optimization for faster encoding—level 10 is up to 50% faster than level 0 with imperceptible quality difference (adds ~15% to baseline Q100 DSSIM of ~0.00007; both well below the 0.0001 perceptibility threshold).
* Comes with new quantization table presets, e.g. tuned for high-resolution displays.
* Fully compatible with all web browsers.
* Can be seamlessly integrated into any program that uses the industry-standard libjpeg API. There's no need to write any MozJPEG-specific integration code. Advanced tuning is available via backwards-compatible [extension parameters](README-mozilla.txt) using `jpeg_c_set_int_param()` and related functions.

## Releases

* [Upstream MozJPEG releases](https://github.com/mozilla/mozjpeg/releases)

## Compiling

See [BUILDING](BUILDING.md). MozJPEG is built exactly the same way as libjpeg-turbo, so if you need additional help please consult [libjpeg-turbo documentation](https://libjpeg-turbo.org/).
