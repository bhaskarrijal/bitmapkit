#include "bitmapkit/internal.h"
#include "bitmapkit/ops.h"

static uint8_t nearest_level(uint8_t v, uint32_t levels) {
  if (levels <= 1)
    return v < 128 ? 0 : 255;
  uint32_t q = ((uint32_t)v * (levels - 1u) + 127u) / 255u;
  return (uint8_t)((q * 255u + (levels - 1u) / 2u) / (levels - 1u));
}

static void add_error(int *buf, uint32_t width, uint32_t x, int er, int eg,
                      int eb, int weight) {
  if (x >= width)
    return;
  int *p = buf + x * 3u;
  p[0] += er * weight;
  p[1] += eg * weight;
  p[2] += eb * weight;
}

bk_status bk_image_dither_floyd(bk_image *image, uint32_t levels) {
  if (!image || !image->pixels || levels < 2 || levels > 256)
    return BK_ERR_ARGUMENT;
  int *cur = (int *)calloc(image->width * 3u, sizeof(int));
  int *next = (int *)calloc(image->width * 3u, sizeof(int));
  if (!cur || !next) {
    free(cur);
    free(next);
    return BK_ERR_OOM;
  }
  for (uint32_t y = 0; y < image->height; ++y) {
    memset(next, 0, image->width * 3u * sizeof(int));
    for (uint32_t x = 0; x < image->width; ++x) {
      uint8_t r, g, b, a;
      bk_get_pixel(image, x, y, &r, &g, &b, &a);
      int rr = (int)r + cur[x * 3u] / 16;
      int gg = (int)g + cur[x * 3u + 1u] / 16;
      int bb = (int)b + cur[x * 3u + 2u] / 16;
      if (rr < 0)
        rr = 0;
      if (rr > 255)
        rr = 255;
      if (gg < 0)
        gg = 0;
      if (gg > 255)
        gg = 255;
      if (bb < 0)
        bb = 0;
      if (bb > 255)
        bb = 255;
      uint8_t nr = nearest_level((uint8_t)rr, levels);
      uint8_t ng = nearest_level((uint8_t)gg, levels);
      uint8_t nb = nearest_level((uint8_t)bb, levels);
      int er = rr - nr, eg = gg - ng, eb = bb - nb;
      bk_set_pixel(image, x, y, nr, ng, nb, a);
      add_error(cur, image->width, x + 1u, er, eg, eb, 7);
      if (x > 0)
        add_error(next, image->width, x - 1u, er, eg, eb, 3);
      add_error(next, image->width, x, er, eg, eb, 5);
      add_error(next, image->width, x + 1u, er, eg, eb, 1);
    }
    int *tmp = cur;
    cur = next;
    next = tmp;
  }
  free(cur);
  free(next);
  return BK_OK;
}

bk_status bk_image_dither_ordered4(bk_image *image, uint32_t levels) {
  static const uint8_t bayer[4][4] = {
      {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  if (!image || !image->pixels || levels < 2 || levels > 256)
    return BK_ERR_ARGUMENT;
  for (uint32_t y = 0; y < image->height; ++y) {
    for (uint32_t x = 0; x < image->width; ++x) {
      uint8_t r, g, b, a;
      bk_get_pixel(image, x, y, &r, &g, &b, &a);
      int t = ((int)bayer[y & 3u][x & 3u] - 8) * 8;
      int rr = (int)r + t;
      int gg = (int)g + t;
      int bb = (int)b + t;
      if (rr < 0)
        rr = 0;
      if (rr > 255)
        rr = 255;
      if (gg < 0)
        gg = 0;
      if (gg > 255)
        gg = 255;
      if (bb < 0)
        bb = 0;
      if (bb > 255)
        bb = 255;
      bk_set_pixel(image, x, y, nearest_level((uint8_t)rr, levels),
                   nearest_level((uint8_t)gg, levels),
                   nearest_level((uint8_t)bb, levels), a);
    }
  }
  return BK_OK;
}

bk_status bk_image_quantize_palette_dither(bk_image *image,
                                           const bk_palette *palette) {
  if (!image || !image->pixels || !palette || palette->count == 0)
    return BK_ERR_ARGUMENT;
  bk_image mapped;
  bk_status st = bk_palette_apply(image, palette, &mapped);
  if (st != BK_OK)
    return st;
  bk_image_free(image);
  *image = mapped;
  return BK_OK;
}
