#include "bitmapkit/bitmapkit.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  bk_image image;
  bk_decode_options opt;
  uint8_t *out = NULL;
  size_t out_size = 0;
  bk_default_options(&opt);
  opt.max_width = 512;
  opt.max_height = 512;
  opt.max_pixels = 262144;
  if (bk_decode_memory(data, size, &opt, &image) == BK_OK) {
    if (size & 1)
      (void)bk_encode_bmp(&image, &out, &out_size);
    else if (size & 2)
      (void)bk_encode_tga(&image, &out, &out_size);
    else
      (void)bk_encode_pnm(&image, &out, &out_size);
    free(out);
    bk_image_free(&image);
  }
  return 0;
}
