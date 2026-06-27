#include "bitmapkit/internal.h"
#include "bitmapkit/ops.h"

typedef struct bk_color_accum {
  uint64_t r;
  uint64_t g;
  uint64_t b;
  uint64_t a;
  uint32_t count;
} bk_color_accum;

#define BK_PALETTE_BUCKETS 131072u

static uint32_t bk_color_key(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  uint32_t qr = r >> 3;
  uint32_t qg = g >> 3;
  uint32_t qb = b >> 3;
  uint32_t qa = a >> 6;
  return (qr << 12) | (qg << 7) | (qb << 2) | qa;
}

static uint32_t bk_color_distance2(uint8_t ar, uint8_t ag, uint8_t ab,
                                   uint8_t aa, uint8_t br, uint8_t bg,
                                   uint8_t bb, uint8_t ba) {
  int dr = (int)ar - (int)br;
  int dg = (int)ag - (int)bg;
  int db = (int)ab - (int)bb;
  int da = (int)aa - (int)ba;
  return (uint32_t)(dr * dr * 3 + dg * dg * 4 + db * db * 2 + da * da);
}

static void bk_palette_sort(bk_palette *palette) {
  uint32_t i, j;
  if (!palette)
    return;
  for (i = 1; i < palette->count; ++i) {
    bk_palette_color key = palette->colors[i];
    j = i;
    while (j > 0 && palette->colors[j - 1u].count < key.count) {
      palette->colors[j] = palette->colors[j - 1u];
      --j;
    }
    palette->colors[j] = key;
  }
}

bk_status bk_palette_from_image(const bk_image *image, uint32_t max_colors,
                                bk_palette *palette) {
  bk_color_accum *accum;
  uint32_t x, y, i, out_count = 0;
  if (!image || !image->pixels || !palette)
    return BK_ERR_ARGUMENT;
  if (max_colors == 0 || max_colors > 256)
    return BK_ERR_ARGUMENT;
  memset(palette, 0, sizeof(*palette));
  accum = (bk_color_accum *)calloc(BK_PALETTE_BUCKETS, sizeof(*accum));
  if (!accum)
    return BK_ERR_OOM;
  for (y = 0; y < image->height; ++y) {
    const uint8_t *row = image->pixels + (size_t)y * image->stride;
    for (x = 0; x < image->width; ++x) {
      const uint8_t *p = row + (size_t)x * 4u;
      uint32_t key = bk_color_key(p[0], p[1], p[2], p[3]);
      accum[key].r += p[0];
      accum[key].g += p[1];
      accum[key].b += p[2];
      accum[key].a += p[3];
      accum[key].count++;
    }
  }
  for (i = 0; i < BK_PALETTE_BUCKETS; ++i) {
    if (accum[i].count && out_count < 256u) {
      bk_palette_color *c = &palette->colors[out_count++];
      c->r = (uint8_t)(accum[i].r / accum[i].count);
      c->g = (uint8_t)(accum[i].g / accum[i].count);
      c->b = (uint8_t)(accum[i].b / accum[i].count);
      c->a = (uint8_t)(accum[i].a / accum[i].count);
      c->count = accum[i].count;
    }
  }
  free(accum);
  palette->count = out_count;
  bk_palette_sort(palette);
  if (palette->count > max_colors)
    palette->count = max_colors;
  return BK_OK;
}

static uint32_t bk_palette_nearest_index(const bk_palette *palette, uint8_t r,
                                         uint8_t g, uint8_t b, uint8_t a) {
  uint32_t best = 0;
  uint32_t best_dist = UINT32_MAX;
  uint32_t i;
  if (!palette || palette->count == 0)
    return 0;
  for (i = 0; i < palette->count; ++i) {
    const bk_palette_color *c = &palette->colors[i];
    uint32_t d = bk_color_distance2(r, g, b, a, c->r, c->g, c->b, c->a);
    if (d < best_dist) {
      best_dist = d;
      best = i;
      if (d == 0)
        break;
    }
  }
  return best;
}

bk_status bk_palette_apply(const bk_image *source, const bk_palette *palette,
                           bk_image *out) {
  uint32_t x, y;
  bk_status st;
  if (!source || !source->pixels || !palette || !out)
    return BK_ERR_ARGUMENT;
  if (palette->count == 0)
    return BK_ERR_ARGUMENT;
  st = bk_image_alloc(out, source->width, source->height);
  if (st != BK_OK)
    return st;
  for (y = 0; y < source->height; ++y) {
    for (x = 0; x < source->width; ++x) {
      uint8_t r, g, b, a;
      uint32_t idx;
      const bk_palette_color *c;
      bk_get_pixel(source, x, y, &r, &g, &b, &a);
      idx = bk_palette_nearest_index(palette, r, g, b, a);
      c = &palette->colors[idx];
      bk_set_pixel(out, x, y, c->r, c->g, c->b, c->a);
    }
  }
  return BK_OK;
}
