#include "bitmapkit/internal.h"

typedef enum bk_raw_format {
  BK_RAW_RGB565,
  BK_RAW_RGB555,
  BK_RAW_RGBA4444,
  BK_RAW_BGRA8888,
  BK_RAW_ARGB8888,
  BK_RAW_ABGR8888,
  BK_RAW_RGB888,
  BK_RAW_BGR888
} bk_raw_format;

static uint8_t expand_bits(uint32_t v, uint32_t bits) {
  uint32_t maxv = (1u << bits) - 1u;
  return (uint8_t)((v * 255u + maxv / 2u) / maxv);
}

static void decode_one(bk_raw_format fmt, const uint8_t *p, uint8_t *r,
                       uint8_t *g, uint8_t *b, uint8_t *a) {
  uint16_t v;
  switch (fmt) {
  case BK_RAW_RGB565:
    v = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
    *r = expand_bits((v >> 11) & 31u, 5);
    *g = expand_bits((v >> 5) & 63u, 6);
    *b = expand_bits(v & 31u, 5);
    *a = 255;
    break;
  case BK_RAW_RGB555:
    v = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
    *r = expand_bits((v >> 10) & 31u, 5);
    *g = expand_bits((v >> 5) & 31u, 5);
    *b = expand_bits(v & 31u, 5);
    *a = 255;
    break;
  case BK_RAW_RGBA4444:
    v = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
    *r = expand_bits((v >> 12) & 15u, 4);
    *g = expand_bits((v >> 8) & 15u, 4);
    *b = expand_bits((v >> 4) & 15u, 4);
    *a = expand_bits(v & 15u, 4);
    break;
  case BK_RAW_BGRA8888:
    *b = p[0];
    *g = p[1];
    *r = p[2];
    *a = p[3];
    break;
  case BK_RAW_ARGB8888:
    *a = p[0];
    *r = p[1];
    *g = p[2];
    *b = p[3];
    break;
  case BK_RAW_ABGR8888:
    *a = p[0];
    *b = p[1];
    *g = p[2];
    *r = p[3];
    break;
  case BK_RAW_RGB888:
    *r = p[0];
    *g = p[1];
    *b = p[2];
    *a = 255;
    break;
  case BK_RAW_BGR888:
    *b = p[0];
    *g = p[1];
    *r = p[2];
    *a = 255;
    break;
  }
}

static void encode_one(bk_raw_format fmt, uint8_t *p, uint8_t r, uint8_t g,
                       uint8_t b, uint8_t a) {
  uint16_t v;
  switch (fmt) {
  case BK_RAW_RGB565:
    v = (uint16_t)(((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) |
                   (b >> 3));
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    break;
  case BK_RAW_RGB555:
    v = (uint16_t)(((uint16_t)(r >> 3) << 10) | ((uint16_t)(g >> 3) << 5) |
                   (b >> 3));
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    break;
  case BK_RAW_RGBA4444:
    v = (uint16_t)(((uint16_t)(r >> 4) << 12) | ((uint16_t)(g >> 4) << 8) |
                   ((uint16_t)(b >> 4) << 4) | (a >> 4));
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    break;
  case BK_RAW_BGRA8888:
    p[0] = b;
    p[1] = g;
    p[2] = r;
    p[3] = a;
    break;
  case BK_RAW_ARGB8888:
    p[0] = a;
    p[1] = r;
    p[2] = g;
    p[3] = b;
    break;
  case BK_RAW_ABGR8888:
    p[0] = a;
    p[1] = b;
    p[2] = g;
    p[3] = r;
    break;
  case BK_RAW_RGB888:
    p[0] = r;
    p[1] = g;
    p[2] = b;
    break;
  case BK_RAW_BGR888:
    p[0] = b;
    p[1] = g;
    p[2] = r;
    break;
  }
}

static uint32_t bytes_per_pixel(bk_raw_format fmt) {
  return (fmt == BK_RAW_RGB888 || fmt == BK_RAW_BGR888)
             ? 3u
             : (fmt <= BK_RAW_RGBA4444 ? 2u : 4u);
}

bk_status bk_decode_raw_pixels(const uint8_t *data, size_t size, uint32_t width,
                               uint32_t height, uint32_t stride,
                               bk_raw_format fmt, bk_image *out) {
  bk_status st;
  uint32_t bpp = bytes_per_pixel(fmt);
  if (!data || !out || width == 0 || height == 0)
    return BK_ERR_ARGUMENT;
  if (stride == 0)
    stride = width * bpp;
  if ((uint64_t)stride * height > size)
    return BK_ERR_TRUNCATED;
  st = bk_image_alloc(out, width, height);
  if (st != BK_OK)
    return st;
  for (uint32_t y = 0; y < height; ++y)
    for (uint32_t x = 0; x < width; ++x) {
      uint8_t r, g, b, a;
      decode_one(fmt, data + (size_t)y * stride + (size_t)x * bpp, &r, &g, &b,
                 &a);
      bk_set_pixel(out, x, y, r, g, b, a);
    }
  return BK_OK;
}

bk_status bk_encode_raw_pixels(const bk_image *image, bk_raw_format fmt,
                               uint8_t **out, size_t *out_size) {
  if (!image || !image->pixels || !out || !out_size)
    return BK_ERR_ARGUMENT;
  uint32_t bpp = bytes_per_pixel(fmt);
  size_t size = (size_t)image->width * image->height * bpp;
  uint8_t *buf = (uint8_t *)malloc(size ? size : 1u);
  if (!buf)
    return BK_ERR_OOM;
  for (uint32_t y = 0; y < image->height; ++y)
    for (uint32_t x = 0; x < image->width; ++x) {
      uint8_t r, g, b, a;
      bk_get_pixel(image, x, y, &r, &g, &b, &a);
      encode_one(fmt, buf + ((size_t)y * image->width + x) * bpp, r, g, b, a);
    }
  *out = buf;
  *out_size = size;
  return BK_OK;
}

bk_status bk_swizzle_rgba_to_bgra(uint8_t *data, size_t pixels) {
  if (!data)
    return BK_ERR_ARGUMENT;
  for (size_t i = 0; i < pixels; ++i) {
    uint8_t t = data[i * 4];
    data[i * 4] = data[i * 4 + 2];
    data[i * 4 + 2] = t;
  }
  return BK_OK;
}
bk_status bk_swizzle_argb_to_rgba(uint8_t *data, size_t pixels) {
  if (!data)
    return BK_ERR_ARGUMENT;
  for (size_t i = 0; i < pixels; ++i) {
    uint8_t a = data[i * 4], r = data[i * 4 + 1], g = data[i * 4 + 2],
            b = data[i * 4 + 3];
    data[i * 4] = r;
    data[i * 4 + 1] = g;
    data[i * 4 + 2] = b;
    data[i * 4 + 3] = a;
  }
  return BK_OK;
}
