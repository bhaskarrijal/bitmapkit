#include "bitmapkit/bitmapkit.h"
#include "bitmapkit/ops.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  bk_image image;
  bk_decode_options opt;
  bk_default_options(&opt);
  opt.max_width = 2048;
  opt.max_height = 2048;
  opt.max_pixels = 4194304;
  if (bk_decode_memory(data, size, &opt, &image) == BK_OK) {
    bk_histogram histogram;
    bk_palette palette;
    bk_image_flip_vertical(&image);
    if (bk_histogram_build(&image, &histogram) == BK_OK) {
      bk_channel_stats stats;
      (void)bk_histogram_channel_stats(histogram.red, histogram.total, &stats);
    }
    if (bk_palette_from_image(&image, 32, &palette) == BK_OK) {
      bk_image mapped;
      if (bk_palette_apply(&image, &palette, &mapped) == BK_OK)
        bk_image_free(&mapped);
    }
    if (image.width > 0 && image.height > 0) {
      bk_image crop;
      uint32_t cw = image.width > 8 ? 8 : image.width;
      uint32_t ch = image.height > 8 ? 8 : image.height;
      if (bk_image_crop(&image, 0, 0, cw, ch, &crop) == BK_OK)
        bk_image_free(&crop);
    }
    bk_image_free(&image);
  }
  return 0;
}
