#include "bitmapkit/bitmapkit.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  bk_image image;
  bk_info info;
  bk_decode_options opt;
  bk_default_options(&opt);
  opt.max_width = 4096;
  opt.max_height = 4096;
  opt.max_pixels = 16777216;
  (void)bk_probe_memory(data, size, &info);
  if (bk_decode_as(data, size, BK_FORMAT_PNM, &opt, &image) == BK_OK) {
    bk_image_to_grayscale(&image);
    bk_image_premultiply_alpha(&image);
    bk_image_free(&image);
  }
  return 0;
}
