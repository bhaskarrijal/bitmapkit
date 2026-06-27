# bitmapkit

bitmapkit is a small C library and command line tool for inspecting and decoding older raster image files. It is aimed at build tools, asset pipelines, and recovery scripts that need a dependency-light way to look at BMP/DIB, TGA, PCX, PNM/PAM, and ICO/CUR files.

The library decodes images into RGBA8, keeps basic metadata and warnings, and has encoders for BMP, TGA, and PNM so decoded assets can be normalized for other tools. The command line program can print file info, decode to another format, validate input, and scan damaged byte streams for embedded image starts.

## Build

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

## CLI

```sh
bitmapkit info image.bmp
bitmapkit validate image.tga
bitmapkit convert input.pcx output.ppm
bitmapkit recover damaged.bin
```

`convert` chooses the output writer from the file extension: `.bmp`, `.tga`, `.ppm`, or `.pnm`.

## Library shape

Public headers live in `include/bitmapkit`. The implementation is split by responsibility rather than by the fuzz targets:

- byte reader and growable output buffer
- image allocation and pixel operations
- metadata and diagnostics
- format detection and dispatch
- BMP/DIB, TGA, PCX, PNM/PAM, and ICO/CUR decoders
- BMP, TGA, and PNM writers
- recovery scanner

All decoded images use straight RGBA8 pixels. The decoders do not try to preserve every historical extension; unsupported compression modes return an explicit error.

## Fuzzing

The `fuzz/` directory contains libFuzzer entry points for individual formats, full decode dispatch, metadata probing, conversion, and recovery scanning. `.clusterfuzzlite/build.sh` builds every target into `$OUT` without fetching dependencies.
