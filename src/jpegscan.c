#include "bitmapkit/internal.h"

static uint16_t jpg_be16(const uint8_t *p) {
  return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static int jpg_standalone(uint8_t m) {
  return m == 0x01 || (m >= 0xd0 && m <= 0xd9);
}
static int jpg_sof(uint8_t m) {
  return (m >= 0xc0 && m <= 0xcf && m != 0xc4 && m != 0xc8 && m != 0xcc);
}

bk_status bk_decode_jpeg(const uint8_t *data, size_t size,
                         const bk_decode_context *ctx, bk_image *image,
                         bk_info *info) {
  size_t pos = 2;
  uint32_t width = 0, height = 0, precision = 0, components = 0;
  uint32_t scans = 0, app_count = 0, marker_count = 0;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  (void)image;
  if (size < 4)
    return BK_ERR_TRUNCATED;
  if (data[0] != 0xff || data[1] != 0xd8)
    return BK_ERR_BAD_MAGIC;
  while (pos < size) {
    uint8_t marker;
    uint16_t len;
    while (pos < size && data[pos] != 0xff)
      ++pos;
    if (pos >= size)
      break;
    while (pos < size && data[pos] == 0xff)
      ++pos;
    if (pos >= size)
      return BK_ERR_TRUNCATED;
    marker = data[pos++];
    marker_count++;
    if (marker == 0xd9)
      break;
    if (jpg_standalone(marker))
      continue;
    if (pos + 2 > size)
      return BK_ERR_TRUNCATED;
    len = jpg_be16(data + pos);
    if (len < 2 || pos + len > size)
      return BK_ERR_TRUNCATED;
    if (marker >= 0xe0 && marker <= 0xef)
      app_count++;
    if (jpg_sof(marker)) {
      if (len < 8)
        return BK_ERR_CORRUPT;
      precision = data[pos + 2];
      height = jpg_be16(data + pos + 3);
      width = jpg_be16(data + pos + 5);
      components = data[pos + 7];
    } else if (marker == 0xda) {
      scans++;
      pos += len;
      while (pos + 1 < size) {
        if (data[pos] == 0xff && data[pos + 1] != 0x00)
          break;
        pos++;
      }
      continue;
    }
    pos += len;
  }
  if (width == 0 || height == 0)
    return BK_ERR_CORRUPT;
  bk_status st = bk_validate_dimensions(width, height, &ctx->options);
  if (st != BK_OK)
    return st;
  if (info) {
    info->format = BK_FORMAT_JPEG;
    info->width = width;
    info->height = height;
    info->bits_per_pixel = precision * components;
    info->frame_count = scans ? scans : 1;
    info->has_alpha = 0;
    bk_metadata_add_u32(&info->metadata, "components", components);
    bk_metadata_add_u32(&info->metadata, "app_segments", app_count);
    bk_metadata_add_u32(&info->metadata, "markers", marker_count);
  }
  return ctx->metadata_only ? BK_OK : BK_ERR_UNSUPPORTED;
}

bk_status bk_jpeg_scan_markers(const uint8_t *data, size_t size,
                               bk_metadata *metadata) {
  size_t pos = 2;
  if (!data || !metadata)
    return BK_ERR_ARGUMENT;
  if (size < 2 || data[0] != 0xff || data[1] != 0xd8)
    return BK_ERR_BAD_MAGIC;
  while (pos < size) {
    uint8_t marker;
    char key[16], val[96];
    while (pos < size && data[pos] != 0xff)
      ++pos;
    if (pos >= size)
      break;
    while (pos < size && data[pos] == 0xff)
      ++pos;
    if (pos >= size)
      return BK_ERR_TRUNCATED;
    marker = data[pos++];
    snprintf(key, sizeof(key), "marker_%zu", metadata->count);
    snprintf(val, sizeof(val), "0x%02x%s", marker,
             jpg_sof(marker)
                 ? ":sof"
                 : (marker >= 0xe0 && marker <= 0xef ? ":app" : ""));
    bk_metadata_add(metadata, key, val);
    if (marker == 0xd9)
      return BK_OK;
    if (!jpg_standalone(marker)) {
      uint16_t len;
      if (pos + 2 > size)
        return BK_ERR_TRUNCATED;
      len = jpg_be16(data + pos);
      if (len < 2 || pos + len > size)
        return BK_ERR_TRUNCATED;
      pos += len;
    }
  }
  return BK_ERR_TRUNCATED;
}
