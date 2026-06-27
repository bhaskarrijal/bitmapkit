cmake_minimum_required(VERSION 3.16)
project(bitmapkit C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(BITMAPKIT_BUILD_TESTS "Build bitmapkit tests" ON)

add_library(bitmapkit STATIC
  src/alpha.c
  src/atlas.c
  src/audit.c
  src/bmff.c
  src/bmp.c
  src/bmp_repair.c
  src/buffer.c
  src/catalog.c
  src/checksum.c
  src/color.c
  src/dds.c
  src/detect.c
  src/diff.c
  src/dither.c
  src/encode.c
  src/exrscan.c
  src/farbfeld.c
  src/fileio.c
  src/gif.c
  src/histogram.c
  src/ico.c
  src/iff.c
  src/image.c
  src/jpegscan.c
  src/manifest.c
  src/metadata.c
  src/pack.c
  src/palette.c
  src/pathutil.c
  src/pcx.c
  src/pixelformat.c
  src/pngscan.c
  src/pnm.c
  src/psd.c
  src/qoi.c
  src/ras.c
  src/reader.c
  src/recover.c
  src/registry.c
  src/report.c
  src/resample.c
  src/scanline.c
  src/script.c
  src/sgi.c
  src/sheet.c
  src/status.c
  src/tag_registry.c
  src/tga.c
  src/tga_rle.c
  src/tiff.c
  src/transform.c
  src/webp.c
  src/xbm.c
  src/xwd.c
)

target_include_directories(bitmapkit PUBLIC include)

target_compile_options(bitmapkit PRIVATE
  $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic -Wconversion -Wshadow>
  $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic -Wconversion -Wshadow>
)

if(UNIX)
  target_link_libraries(bitmapkit PUBLIC m)
endif()

add_executable(bitmapkit_cli tools/bitmapkit.c)
target_link_libraries(bitmapkit_cli PRIVATE bitmapkit)
set_target_properties(bitmapkit_cli PROPERTIES OUTPUT_NAME bitmapkit)

if(BITMAPKIT_BUILD_TESTS)
  enable_testing()
  add_executable(test_bitmapkit tests/test_bitmapkit.c)
  target_link_libraries(test_bitmapkit PRIVATE bitmapkit)
  add_test(NAME bitmapkit_core COMMAND test_bitmapkit)
endif()
