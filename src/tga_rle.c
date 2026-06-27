#include "bitmapkit/internal.h"

static int same_bgr(const uint8_t *a, const uint8_t *b, int alpha) {
  return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] &&
         (!alpha || a[3] == b[3]);
}

bk_status bk_encode_tga_rle(const bk_image *image, uint8_t **out,
                            size_t *out_size) {
  if (!image || !image->pixels || !out || !out_size)
    return BK_ERR_ARGUMENT;
  int alpha = 0;
  for (uint32_t y = 0; y < image->height && !alpha; ++y)
    for (uint32_t x = 0; x < image->width; ++x) {
      uint8_t a;
      bk_get_pixel(image, x, y, NULL, NULL, NULL, &a);
      if (a != 255) {
        alpha = 1;
        break;
      }
    }
  bk_buffer b;
  bk_status st = bk_buffer_init(&b, 128);
  if (st != BK_OK)
    return st;
  bk_buffer_append_u8(&b, 0);
  bk_buffer_append_u8(&b, 0);
  bk_buffer_append_u8(&b, 10);
  bk_buffer_append_le16(&b, 0);
  bk_buffer_append_le16(&b, 0);
  bk_buffer_append_u8(&b, 0);
  bk_buffer_append_le16(&b, 0);
  bk_buffer_append_le16(&b, 0);
  bk_buffer_append_le16(&b, (uint16_t)image->width);
  bk_buffer_append_le16(&b, (uint16_t)image->height);
  bk_buffer_append_u8(&b, alpha ? 32 : 24);
  bk_buffer_append_u8(&b, (uint8_t)(0x20u | (alpha ? 8u : 0u)));
  size_t count = (size_t)image->width * image->height;
  uint8_t *px = (uint8_t *)malloc(count * (alpha ? 4u : 3u));
  if (!px) {
    bk_buffer_free(&b);
    return BK_ERR_OOM;
  }
  for (uint32_t y = 0; y < image->height; ++y)
    for (uint32_t x = 0; x < image->width; ++x) {
      uint8_t r, g, bb, a;
      bk_get_pixel(image, x, y, &r, &g, &bb, &a);
      uint8_t *p = px + ((size_t)y * image->width + x) * (alpha ? 4u : 3u);
      p[0] = bb;
      p[1] = g;
      p[2] = r;
      if (alpha)
        p[3] = a;
    }
  size_t i = 0, step = alpha ? 4u : 3u;
  while (i < count) {
    size_t run = 1;
    while (i + run < count && run < 128 &&
           same_bgr(px + i * step, px + (i + run) * step, alpha))
      run++;
    if (run >= 2) {
      bk_buffer_append_u8(&b, (uint8_t)(0x80u | (run - 1u)));
      bk_buffer_append(&b, px + i * step, step);
      i += run;
    } else {
      size_t start = i;
      i++;
      while (i < count && i - start < 128) {
        if (i + 1 < count &&
            same_bgr(px + i * step, px + (i + 1) * step, alpha))
          break;
        i++;
      }
      bk_buffer_append_u8(&b, (uint8_t)(i - start - 1u));
      bk_buffer_append(&b, px + start * step, (i - start) * step);
    }
  }
  free(px);
  *out = b.data;
  *out_size = b.size;
  return BK_OK;
}
