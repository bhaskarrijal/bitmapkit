#ifndef BITMAPKIT_OPS_H
#define BITMAPKIT_OPS_H

#include "bitmapkit/bitmapkit.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bk_channel_stats {
  uint8_t min_value;
  uint8_t max_value;
  double mean;
  double variance;
  double entropy;
} bk_channel_stats;

typedef struct bk_histogram {
  uint32_t red[256];
  uint32_t green[256];
  uint32_t blue[256];
  uint32_t alpha[256];
  uint64_t total;
} bk_histogram;

typedef struct bk_palette_color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
  uint32_t count;
} bk_palette_color;

typedef struct bk_palette {
  bk_palette_color colors[256];
  uint32_t count;
} bk_palette;

typedef struct bk_audit_entry {
  char rule[48];
  char message[160];
  bk_severity severity;
  size_t offset;
} bk_audit_entry;

typedef struct bk_audit_report {
  bk_audit_entry entries[256];
  size_t count;
  bk_format detected_format;
} bk_audit_report;

bk_status bk_image_clone(const bk_image *source, bk_image *out);
bk_status bk_image_rotate90(const bk_image *source, bk_image *out);
bk_status bk_image_rotate180(const bk_image *source, bk_image *out);
bk_status bk_image_rotate270(const bk_image *source, bk_image *out);
bk_status bk_image_mirror_horizontal(bk_image *image);
bk_status bk_image_fill_rect(bk_image *image, uint32_t x, uint32_t y,
                             uint32_t w, uint32_t h, uint8_t r, uint8_t g,
                             uint8_t b, uint8_t a);
bk_status bk_image_resize_nearest(const bk_image *source, uint32_t width,
                                  uint32_t height, bk_image *out);
bk_status bk_image_resize_bilinear(const bk_image *source, uint32_t width,
                                   uint32_t height, bk_image *out);
bk_status bk_histogram_build(const bk_image *image, bk_histogram *histogram);
bk_status bk_histogram_channel_stats(const uint32_t bins[256], uint64_t total,
                                     bk_channel_stats *stats);
bk_status bk_palette_from_image(const bk_image *image, uint32_t max_colors,
                                bk_palette *palette);
bk_status bk_palette_apply(const bk_image *source, const bk_palette *palette,
                           bk_image *out);
bk_status bk_audit_memory(const uint8_t *data, size_t size,
                          bk_audit_report *report);

#ifdef __cplusplus
}
#endif

#endif
