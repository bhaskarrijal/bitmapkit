#include "bitmapkit/internal.h"
#include <math.h>

typedef struct bk_diff_stats {
  uint64_t compared_pixels;
  uint64_t changed_pixels;
  uint64_t sum_abs_red;
  uint64_t sum_abs_green;
  uint64_t sum_abs_blue;
  uint64_t sum_abs_alpha;
  uint32_t max_distance;
  double mean_distance;
  double rms_distance;
} bk_diff_stats;

static uint32_t abs_u8(uint8_t a, uint8_t b) {
  return a > b ? (uint32_t)(a - b) : (uint32_t)(b - a);
}

bk_status bk_image_diff_stats(const bk_image *a, const bk_image *b,
                              bk_diff_stats *stats) {
  uint32_t x, y;
  double sum = 0.0;
  double sum2 = 0.0;
  if (!a || !b || !a->pixels || !b->pixels || !stats)
    return BK_ERR_ARGUMENT;
  if (a->width != b->width || a->height != b->height)
    return BK_ERR_DIMENSIONS;
  memset(stats, 0, sizeof(*stats));
  for (y = 0; y < a->height; ++y) {
    for (x = 0; x < a->width; ++x) {
      uint8_t ar, ag, ab, aa, br, bg, bb, ba;
      uint32_t dr, dg, db, da, dist;
      bk_get_pixel(a, x, y, &ar, &ag, &ab, &aa);
      bk_get_pixel(b, x, y, &br, &bg, &bb, &ba);
      dr = abs_u8(ar, br);
      dg = abs_u8(ag, bg);
      db = abs_u8(ab, bb);
      da = abs_u8(aa, ba);
      dist = dr + dg + db + da;
      stats->sum_abs_red += dr;
      stats->sum_abs_green += dg;
      stats->sum_abs_blue += db;
      stats->sum_abs_alpha += da;
      stats->changed_pixels += dist != 0;
      stats->compared_pixels++;
      if (dist > stats->max_distance)
        stats->max_distance = dist;
      sum += (double)dist;
      sum2 += (double)dist * (double)dist;
    }
  }
  if (stats->compared_pixels) {
    stats->mean_distance = sum / (double)stats->compared_pixels;
    stats->rms_distance = sqrt(sum2 / (double)stats->compared_pixels);
  }
  return BK_OK;
}

bk_status bk_image_make_difference(const bk_image *a, const bk_image *b,
                                   bk_image *out) {
  uint32_t x, y;
  bk_status st;
  if (!a || !b || !a->pixels || !b->pixels || !out)
    return BK_ERR_ARGUMENT;
  if (a->width != b->width || a->height != b->height)
    return BK_ERR_DIMENSIONS;
  st = bk_image_alloc(out, a->width, a->height);
  if (st != BK_OK)
    return st;
  for (y = 0; y < a->height; ++y) {
    for (x = 0; x < a->width; ++x) {
      uint8_t ar, ag, ab, aa, br, bg, bb, ba;
      bk_get_pixel(a, x, y, &ar, &ag, &ab, &aa);
      bk_get_pixel(b, x, y, &br, &bg, &bb, &ba);
      bk_set_pixel(out, x, y, (uint8_t)abs_u8(ar, br), (uint8_t)abs_u8(ag, bg),
                   (uint8_t)abs_u8(ab, bb),
                   (uint8_t)(abs_u8(aa, ba) ? 255 : 255));
    }
  }
  return BK_OK;
}

bk_status bk_image_compare_threshold(const bk_image *a, const bk_image *b,
                                     uint32_t threshold, uint32_t *changed) {
  uint32_t x, y, n = 0;
  if (!a || !b || !a->pixels || !b->pixels || !changed)
    return BK_ERR_ARGUMENT;
  if (a->width != b->width || a->height != b->height)
    return BK_ERR_DIMENSIONS;
  for (y = 0; y < a->height; ++y) {
    for (x = 0; x < a->width; ++x) {
      uint8_t ar, ag, ab, aa, br, bg, bb, ba;
      uint32_t dist;
      bk_get_pixel(a, x, y, &ar, &ag, &ab, &aa);
      bk_get_pixel(b, x, y, &br, &bg, &bb, &ba);
      dist = abs_u8(ar, br) + abs_u8(ag, bg) + abs_u8(ab, bb) + abs_u8(aa, ba);
      if (dist > threshold)
        ++n;
    }
  }
  *changed = n;
  return BK_OK;
}
