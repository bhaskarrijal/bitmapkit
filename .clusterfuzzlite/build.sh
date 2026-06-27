#!/bin/bash -eu
ROOT="$SRC"
if [ ! -f "$ROOT/CMakeLists.txt" ]; then
  ROOT="$SRC/bitmapkit"
fi
cd "$ROOT"
mkdir -p build-fuzz
OBJDIR="$PWD/build-fuzz/obj"
mkdir -p "$OBJDIR"
COMMON_FLAGS="${CFLAGS:-} -std=c99 -I$PWD/include -g -O1 -fno-omit-frame-pointer"
SRC_FILES="src/alpha.c src/atlas.c src/audit.c src/bmff.c src/bmp.c src/bmp_repair.c src/buffer.c src/catalog.c src/checksum.c src/color.c src/dds.c src/detect.c src/diff.c src/dither.c src/encode.c src/exrscan.c src/farbfeld.c src/fileio.c src/gif.c src/histogram.c src/ico.c src/iff.c src/image.c src/jpegscan.c src/manifest.c src/metadata.c src/pack.c src/palette.c src/pathutil.c src/pcx.c src/pixelformat.c src/pngscan.c src/pnm.c src/psd.c src/qoi.c src/ras.c src/reader.c src/recover.c src/registry.c src/report.c src/resample.c src/scanline.c src/script.c src/sgi.c src/sheet.c src/status.c src/tag_registry.c src/tga.c src/tga_rle.c src/tiff.c src/transform.c src/webp.c src/xbm.c src/xwd.c"
OBJS=""
for f in $SRC_FILES; do
  o="$OBJDIR/$(basename "$f" .c).o"
  ${CC:-clang} $COMMON_FLAGS -c "$f" -o "$o"
  OBJS="$OBJS $o"
done
for f in fuzz/*_fuzzer.c; do
  name="$(basename "$f" .c)"
  fo="$OBJDIR/$name.o"
  ${CC:-clang} $COMMON_FLAGS -c "$f" -o "$fo"
  ${CXX:-clang++} ${CXXFLAGS:-} $fo $OBJS $LIB_FUZZING_ENGINE -o "$OUT/$name"
done
