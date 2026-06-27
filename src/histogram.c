#include "bitmapkit/internal.h"
#include "bitmapkit/ops.h"
#include <math.h>

bk_status bk_histogram_build(const bk_image *image, bk_histogram *histogram) {
  uint32_t x, y;
  if (!image || !image->pixels || !histogram)
    return BK_ERR_ARGUMENT;
  memset(histogram, 0, sizeof(*histogram));
  for (y = 0; y < image->height; ++y) {
    const uint8_t *row = image->pixels + (size_t)y * image->stride;
    for (x = 0; x < image->width; ++x) {
      const uint8_t *p = row + (size_t)x * 4u;
      histogram->red[p[0]]++;
      histogram->green[p[1]]++;
      histogram->blue[p[2]]++;
      histogram->alpha[p[3]]++;
      histogram->total++;
    }
  }
  return BK_OK;
}

bk_status bk_histogram_channel_stats(const uint32_t bins[256], uint64_t total,
                                     bk_channel_stats *stats) {
  uint32_t i;
  double mean = 0.0;
  double var = 0.0;
  double entropy = 0.0;
  if (!bins || !stats || total == 0)
    return BK_ERR_ARGUMENT;
  stats->min_value = 255;
  stats->max_value = 0;
  for (i = 0; i < 256; ++i) {
    if (bins[i]) {
      double p = (double)bins[i] / (double)total;
      if (i < stats->min_value)
        stats->min_value = (uint8_t)i;
      if (i > stats->max_value)
        stats->max_value = (uint8_t)i;
      mean += (double)i * p;
      entropy -= p * (log(p) / log(2.0));
    }
  }
  for (i = 0; i < 256; ++i) {
    if (bins[i]) {
      double d = (double)i - mean;
      var += d * d * ((double)bins[i] / (double)total);
    }
  }
  stats->mean = mean;
  stats->variance = var;
  stats->entropy = entropy;
  return BK_OK;
}

uint32_t bk_histogram_opaque_pixels(const bk_histogram *histogram) {
  if (!histogram)
    return 0;
  return histogram->alpha[255];
}

uint32_t bk_histogram_transparent_pixels(const bk_histogram *histogram) {
  if (!histogram)
    return 0;
  return histogram->alpha[0];
}

uint32_t bk_histogram_used_red_values(const bk_histogram *histogram) {
  uint32_t i, n = 0;
  if (!histogram)
    return 0;
  for (i = 0; i < 256; ++i)
    if (histogram->red[i])
      ++n;
  return n;
}

uint32_t bk_histogram_used_green_values(const bk_histogram *histogram) {
  uint32_t i, n = 0;
  if (!histogram)
    return 0;
  for (i = 0; i < 256; ++i)
    if (histogram->green[i])
      ++n;
  return n;
}

uint32_t bk_histogram_used_blue_values(const bk_histogram *histogram) {
  uint32_t i, n = 0;
  if (!histogram)
    return 0;
  for (i = 0; i < 256; ++i)
    if (histogram->blue[i])
      ++n;
  return n;
}

uint32_t bk_histogram_used_alpha_values(const bk_histogram *histogram) {
  uint32_t i, n = 0;
  if (!histogram)
    return 0;
  for (i = 0; i < 256; ++i)
    if (histogram->alpha[i])
      ++n;
  return n;
}

static uint32_t bk_histogram_percentile_bin(const uint32_t bins[256],
                                            uint64_t total,
                                            uint32_t percentile) {
  uint64_t target;
  uint64_t acc = 0;
  uint32_t i;
  if (!bins || total == 0)
    return 0;
  if (percentile > 100)
    percentile = 100;
  target = (total * percentile + 99u) / 100u;
  for (i = 0; i < 256; ++i) {
    acc += bins[i];
    if (acc >= target)
      return i;
  }
  return 255;
}

uint32_t bk_histogram_red_p01(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->red, h->total, 1) : 0;
}
uint32_t bk_histogram_red_p05(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->red, h->total, 5) : 0;
}
uint32_t bk_histogram_red_p25(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->red, h->total, 25) : 0;
}
uint32_t bk_histogram_red_p50(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->red, h->total, 50) : 0;
}
uint32_t bk_histogram_red_p75(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->red, h->total, 75) : 0;
}
uint32_t bk_histogram_red_p95(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->red, h->total, 95) : 0;
}
uint32_t bk_histogram_red_p99(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->red, h->total, 99) : 0;
}

uint32_t bk_histogram_green_p01(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->green, h->total, 1) : 0;
}
uint32_t bk_histogram_green_p05(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->green, h->total, 5) : 0;
}
uint32_t bk_histogram_green_p25(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->green, h->total, 25) : 0;
}
uint32_t bk_histogram_green_p50(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->green, h->total, 50) : 0;
}
uint32_t bk_histogram_green_p75(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->green, h->total, 75) : 0;
}
uint32_t bk_histogram_green_p95(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->green, h->total, 95) : 0;
}
uint32_t bk_histogram_green_p99(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->green, h->total, 99) : 0;
}

uint32_t bk_histogram_blue_p01(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->blue, h->total, 1) : 0;
}
uint32_t bk_histogram_blue_p05(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->blue, h->total, 5) : 0;
}
uint32_t bk_histogram_blue_p25(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->blue, h->total, 25) : 0;
}
uint32_t bk_histogram_blue_p50(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->blue, h->total, 50) : 0;
}
uint32_t bk_histogram_blue_p75(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->blue, h->total, 75) : 0;
}
uint32_t bk_histogram_blue_p95(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->blue, h->total, 95) : 0;
}
uint32_t bk_histogram_blue_p99(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->blue, h->total, 99) : 0;
}

uint32_t bk_histogram_alpha_p01(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->alpha, h->total, 1) : 0;
}
uint32_t bk_histogram_alpha_p05(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->alpha, h->total, 5) : 0;
}
uint32_t bk_histogram_alpha_p25(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->alpha, h->total, 25) : 0;
}
uint32_t bk_histogram_alpha_p50(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->alpha, h->total, 50) : 0;
}
uint32_t bk_histogram_alpha_p75(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->alpha, h->total, 75) : 0;
}
uint32_t bk_histogram_alpha_p95(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->alpha, h->total, 95) : 0;
}
uint32_t bk_histogram_alpha_p99(const bk_histogram *h) {
  return h ? bk_histogram_percentile_bin(h->alpha, h->total, 99) : 0;
}
