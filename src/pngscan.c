#include "bitmapkit/internal.h"

static uint32_t png_be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | p[3];
}

static int png_type_eq(const uint8_t *p, const char *s) {
  return memcmp(p, s, 4) == 0;
}
static int png_chunk_critical(const uint8_t *p) {
  return p[0] >= 'A' && p[0] <= 'Z';
}
static int png_chunk_private(const uint8_t *p) {
  return p[1] >= 'a' && p[1] <= 'z';
}
static int png_chunk_reserved_ok(const uint8_t *p) {
  return p[2] >= 'A' && p[2] <= 'Z';
}
static int png_chunk_safe_copy(const uint8_t *p) {
  return p[3] >= 'a' && p[3] <= 'z';
}

bk_status bk_decode_png(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info) {
  static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  size_t pos = 8;
  uint32_t width = 0, height = 0, bit_depth = 0, color_type = 0;
  uint32_t chunk_count = 0, idat_count = 0;
  int seen_ihdr = 0, seen_iend = 0, seen_plte = 0;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  (void)image;
  if (size < 8)
    return BK_ERR_TRUNCATED;
  if (memcmp(data, sig, 8) != 0)
    return BK_ERR_BAD_MAGIC;
  while (pos + 12 <= size) {
    uint32_t len = png_be32(data + pos);
    const uint8_t *type = data + pos + 4;
    size_t payload = pos + 8;
    if (payload + len + 4 > size)
      return BK_ERR_TRUNCATED;
    chunk_count++;
    if (png_type_eq(type, "IHDR")) {
      if (seen_ihdr || len != 13)
        return BK_ERR_CORRUPT;
      width = png_be32(data + payload);
      height = png_be32(data + payload + 4);
      bit_depth = data[payload + 8];
      color_type = data[payload + 9];
      if (data[payload + 10] != 0 || data[payload + 11] != 0 ||
          data[payload + 12] > 1)
        return BK_ERR_UNSUPPORTED;
      seen_ihdr = 1;
    } else if (!seen_ihdr) {
      return BK_ERR_CORRUPT;
    } else if (png_type_eq(type, "PLTE")) {
      if (len == 0 || len % 3u != 0)
        return BK_ERR_CORRUPT;
      seen_plte = 1;
    } else if (png_type_eq(type, "IDAT")) {
      idat_count++;
    } else if (png_type_eq(type, "IEND")) {
      if (len != 0)
        return BK_ERR_CORRUPT;
      seen_iend = 1;
      break;
    } else {
      if (!png_chunk_reserved_ok(type))
        return BK_ERR_CORRUPT;
      (void)png_chunk_critical(type);
      (void)png_chunk_private(type);
      (void)png_chunk_safe_copy(type);
    }
    pos = payload + len + 4;
  }
  if (!seen_ihdr || !seen_iend)
    return BK_ERR_TRUNCATED;
  if (idat_count == 0)
    return BK_ERR_CORRUPT;
  if (color_type == 3 && !seen_plte)
    return BK_ERR_CORRUPT;
  bk_status st = bk_validate_dimensions(width, height, &ctx->options);
  if (st != BK_OK)
    return st;
  if (info) {
    info->format = BK_FORMAT_PNG;
    info->width = width;
    info->height = height;
    info->bits_per_pixel = bit_depth;
    info->frame_count = 1;
    info->indexed = color_type == 3;
    info->has_alpha = color_type == 4 || color_type == 6;
    bk_metadata_add_u32(&info->metadata, "color_type", color_type);
    bk_metadata_add_u32(&info->metadata, "chunk_count", chunk_count);
    bk_metadata_add_u32(&info->metadata, "idat_count", idat_count);
  }
  return ctx->metadata_only ? BK_OK : BK_ERR_UNSUPPORTED;
}

bk_status bk_png_scan_chunks(const uint8_t *data, size_t size,
                             bk_metadata *metadata) {
  static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  size_t pos = 8;
  if (!data || !metadata)
    return BK_ERR_ARGUMENT;
  if (size < 8 || memcmp(data, sig, 8) != 0)
    return BK_ERR_BAD_MAGIC;
  while (pos + 12 <= size) {
    uint32_t len = png_be32(data + pos);
    char key[16];
    char val[96];
    const uint8_t *type = data + pos + 4;
    if (pos + 12u + len > size)
      return BK_ERR_TRUNCATED;
    snprintf(key, sizeof(key), "chunk_%zu", metadata->count);
    snprintf(val, sizeof(val), "%c%c%c%c:%u:%s%s", type[0], type[1], type[2],
             type[3], len, png_chunk_critical(type) ? "critical" : "ancillary",
             png_chunk_private(type) ? ":private" : "");
    bk_metadata_add(metadata, key, val);
    pos += 12u + len;
    if (png_type_eq(type, "IEND"))
      return BK_OK;
  }
  return BK_ERR_TRUNCATED;
}
