#include "bitmapkit/internal.h"
#include "bitmapkit/ops.h"

typedef struct bk_alpha_stats {
  uint64_t transparent, translucent, opaque;
  uint8_t min_alpha, max_alpha;
} bk_alpha_stats;

bk_status bk_alpha_analyze(const bk_image *image, bk_alpha_stats *stats) {
  if (!image || !image->pixels || !stats)
    return BK_ERR_ARGUMENT;
  memset(stats, 0, sizeof(*stats));
  stats->min_alpha = 255;
  for (uint32_t y = 0; y < image->height; ++y)
    for (uint32_t x = 0; x < image->width; ++x) {
      uint8_t a;
      bk_get_pixel(image, x, y, NULL, NULL, NULL, &a);
      if (a == 0)
        stats->transparent++;
      else if (a == 255)
        stats->opaque++;
      else
        stats->translucent++;
      if (a < stats->min_alpha)
        stats->min_alpha = a;
      if (a > stats->max_alpha)
        stats->max_alpha = a;
    }
  return BK_OK;
}

bk_status bk_alpha_threshold(bk_image *image, uint8_t threshold) {
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  for (uint32_t y = 0; y < image->height; ++y)
    for (uint32_t x = 0; x < image->width; ++x) {
      uint8_t r, g, b, a;
      bk_get_pixel(image, x, y, &r, &g, &b, &a);
      bk_set_pixel(image, x, y, r, g, b, a >= threshold ? 255 : 0);
    }
  return BK_OK;
}

bk_status bk_alpha_bleed_rgb(bk_image *image, uint32_t passes) {
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  if (passes > 32)
    passes = 32;
  for (uint32_t pass = 0; pass < passes; ++pass) {
    bk_image tmp;
    bk_status st = bk_image_clone(image, &tmp);
    if (st != BK_OK)
      return st;
    for (uint32_t y = 0; y < image->height; ++y)
      for (uint32_t x = 0; x < image->width; ++x) {
        uint8_t r, g, b, a;
        bk_get_pixel(image, x, y, &r, &g, &b, &a);
        if (a != 0)
          continue;
        uint32_t sr = 0, sg = 0, sb = 0, n = 0;
        for (int yy = -1; yy <= 1; ++yy)
          for (int xx = -1; xx <= 1; ++xx) {
            int sx = (int)x + xx, sy = (int)y + yy;
            if (sx < 0 || sy < 0 || sx >= (int)image->width ||
                sy >= (int)image->height)
              continue;
            uint8_t nr, ng, nb, na;
            bk_get_pixel(image, (uint32_t)sx, (uint32_t)sy, &nr, &ng, &nb, &na);
            if (na) {
              sr += nr;
              sg += ng;
              sb += nb;
              n++;
            }
          }
        if (n)
          bk_set_pixel(&tmp, x, y, (uint8_t)(sr / n), (uint8_t)(sg / n),
                       (uint8_t)(sb / n), 0);
      }
    free(image->pixels);
    *image = tmp;
  }
  return BK_OK;
}

bk_status bk_alpha_outline(const bk_image *image, uint8_t threshold,
                           bk_image *out) {
  bk_status st;
  if (!image || !image->pixels || !out)
    return BK_ERR_ARGUMENT;
  st = bk_image_alloc(out, image->width, image->height);
  if (st != BK_OK)
    return st;
  for (uint32_t y = 0; y < image->height; ++y)
    for (uint32_t x = 0; x < image->width; ++x) {
      uint8_t a;
      int edge = 0;
      bk_get_pixel(image, x, y, NULL, NULL, NULL, &a);
      for (int yy = -1; yy <= 1 && !edge; ++yy)
        for (int xx = -1; xx <= 1; ++xx) {
          int sx = (int)x + xx, sy = (int)y + yy;
          uint8_t na = 0;
          if (sx >= 0 && sy >= 0 && sx < (int)image->width &&
              sy < (int)image->height)
            bk_get_pixel(image, (uint32_t)sx, (uint32_t)sy, NULL, NULL, NULL,
                         &na);
          if ((a >= threshold) != (na >= threshold)) {
            edge = 1;
            break;
          }
        }
      bk_set_pixel(out, x, y, edge ? 255 : 0, edge ? 255 : 0, edge ? 255 : 0,
                   255);
    }
  return BK_OK;
}
