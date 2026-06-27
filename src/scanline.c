#include "bitmapkit/internal.h"

bk_status bk_unpack_1bit_scanline(const uint8_t *src, size_t src_size,
                                  uint8_t *dst, uint32_t width, int msb_first) {
  if (!src || !dst)
    return BK_ERR_ARGUMENT;
  if ((width + 7u) / 8u > src_size)
    return BK_ERR_TRUNCATED;
  for (uint32_t x = 0; x < width; ++x) {
    uint8_t byte = src[x / 8u];
    uint8_t bit = msb_first ? (uint8_t)(7u - (x & 7u)) : (uint8_t)(x & 7u);
    dst[x] = (byte >> bit) & 1u;
  }
  return BK_OK;
}

bk_status bk_unpack_2bit_scanline(const uint8_t *src, size_t src_size,
                                  uint8_t *dst, uint32_t width) {
  if (!src || !dst)
    return BK_ERR_ARGUMENT;
  if ((width + 3u) / 4u > src_size)
    return BK_ERR_TRUNCATED;
  for (uint32_t x = 0; x < width; ++x) {
    uint8_t byte = src[x / 4u];
    uint8_t shift = (uint8_t)(6u - (x & 3u) * 2u);
    dst[x] = (byte >> shift) & 3u;
  }
  return BK_OK;
}

bk_status bk_unpack_4bit_scanline(const uint8_t *src, size_t src_size,
                                  uint8_t *dst, uint32_t width) {
  if (!src || !dst)
    return BK_ERR_ARGUMENT;
  if ((width + 1u) / 2u > src_size)
    return BK_ERR_TRUNCATED;
  for (uint32_t x = 0; x < width; ++x) {
    uint8_t byte = src[x / 2u];
    dst[x] = (x & 1u) ? (byte & 15u) : (byte >> 4);
  }
  return BK_OK;
}

bk_status bk_pack_1bit_scanline(const uint8_t *src, uint8_t *dst,
                                uint32_t width, int msb_first) {
  if (!src || !dst)
    return BK_ERR_ARGUMENT;
  memset(dst, 0, (width + 7u) / 8u);
  for (uint32_t x = 0; x < width; ++x)
    if (src[x]) {
      uint8_t bit = msb_first ? (uint8_t)(7u - (x & 7u)) : (uint8_t)(x & 7u);
      dst[x / 8u] |= (uint8_t)(1u << bit);
    }
  return BK_OK;
}

bk_status bk_pack_4bit_scanline(const uint8_t *src, uint8_t *dst,
                                uint32_t width) {
  if (!src || !dst)
    return BK_ERR_ARGUMENT;
  memset(dst, 0, (width + 1u) / 2u);
  for (uint32_t x = 0; x < width; ++x) {
    if (x & 1u)
      dst[x / 2u] |= src[x] & 15u;
    else
      dst[x / 2u] |= (uint8_t)((src[x] & 15u) << 4);
  }
  return BK_OK;
}

bk_status bk_predictor_none(uint8_t *row, uint32_t row_size, uint32_t bpp) {
  (void)row;
  (void)row_size;
  (void)bpp;
  return BK_OK;
}

bk_status bk_predictor_sub(uint8_t *row, uint32_t row_size, uint32_t bpp) {
  if (!row || bpp == 0)
    return BK_ERR_ARGUMENT;
  for (uint32_t i = bpp; i < row_size; ++i)
    row[i] = (uint8_t)(row[i] + row[i - bpp]);
  return BK_OK;
}

bk_status bk_predictor_up(uint8_t *row, const uint8_t *prev,
                          uint32_t row_size) {
  if (!row)
    return BK_ERR_ARGUMENT;
  if (!prev)
    return BK_OK;
  for (uint32_t i = 0; i < row_size; ++i)
    row[i] = (uint8_t)(row[i] + prev[i]);
  return BK_OK;
}

static uint8_t paeth(uint8_t a, uint8_t b, uint8_t c) {
  int p = (int)a + (int)b - (int)c;
  int pa = abs(p - (int)a);
  int pb = abs(p - (int)b);
  int pc = abs(p - (int)c);
  return pa <= pb && pa <= pc ? a : (pb <= pc ? b : c);
}

bk_status bk_predictor_paeth(uint8_t *row, const uint8_t *prev,
                             uint32_t row_size, uint32_t bpp) {
  if (!row || bpp == 0)
    return BK_ERR_ARGUMENT;
  for (uint32_t i = 0; i < row_size; ++i) {
    uint8_t a = i >= bpp ? row[i - bpp] : 0;
    uint8_t b = prev ? prev[i] : 0;
    uint8_t c = (prev && i >= bpp) ? prev[i - bpp] : 0;
    row[i] = (uint8_t)(row[i] + paeth(a, b, c));
  }
  return BK_OK;
}
