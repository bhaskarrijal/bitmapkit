#include "bitmapkit/internal.h"

bk_status bk_bmp_estimate_missing_size(uint8_t *data, size_t size) {
  if (!data || size < 54 || data[0] != 'B' || data[1] != 'M')
    return BK_ERR_ARGUMENT;
  uint32_t file_size = (uint32_t)size;
  data[2] = (uint8_t)file_size;
  data[3] = (uint8_t)(file_size >> 8);
  data[4] = (uint8_t)(file_size >> 16);
  data[5] = (uint8_t)(file_size >> 24);
  return BK_OK;
}

bk_status bk_bmp_repair_pixel_offset(uint8_t *data, size_t size) {
  if (!data || size < 54 || data[0] != 'B' || data[1] != 'M')
    return BK_ERR_ARGUMENT;
  uint32_t dib = (uint32_t)data[14] | ((uint32_t)data[15] << 8) |
                 ((uint32_t)data[16] << 16) | ((uint32_t)data[17] << 24);
  if (dib < 12 || 14u + dib > size)
    return BK_ERR_CORRUPT;
  uint32_t off = 14u + dib;
  data[10] = (uint8_t)off;
  data[11] = (uint8_t)(off >> 8);
  data[12] = (uint8_t)(off >> 16);
  data[13] = (uint8_t)(off >> 24);
  return BK_OK;
}

bk_status bk_bmp_repair_planes(uint8_t *data, size_t size) {
  if (!data || size < 30 || data[0] != 'B' || data[1] != 'M')
    return BK_ERR_ARGUMENT;
  uint32_t dib = (uint32_t)data[14] | ((uint32_t)data[15] << 8) |
                 ((uint32_t)data[16] << 16) | ((uint32_t)data[17] << 24);
  size_t off = dib == 12 ? 22u : 26u;
  if (off + 2 > size)
    return BK_ERR_TRUNCATED;
  data[off] = 1;
  data[off + 1] = 0;
  return BK_OK;
}

bk_status bk_bmp_repair_header(uint8_t *data, size_t size) {
  bk_status st;
  st = bk_bmp_estimate_missing_size(data, size);
  if (st != BK_OK)
    return st;
  st = bk_bmp_repair_pixel_offset(data, size);
  if (st != BK_OK)
    return st;
  return bk_bmp_repair_planes(data, size);
}

bk_status bk_bmp_find_embedded(const uint8_t *data, size_t size,
                               size_t *offset) {
  if (!data || !offset)
    return BK_ERR_ARGUMENT;
  for (size_t i = 0; i + 2 < size; ++i)
    if (data[i] == 'B' && data[i + 1] == 'M') {
      *offset = i;
      return BK_OK;
    }
  return BK_ERR_BAD_MAGIC;
}
