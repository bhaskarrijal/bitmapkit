#include "bitmapkit/internal.h"

static uint32_t le24(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}
static uint32_t le32w(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

bk_status bk_decode_webp(const uint8_t *data, size_t size,
                         const bk_decode_context *ctx, bk_image *image,
                         bk_info *info) {
  size_t pos = 12;
  uint32_t width = 0, height = 0, flags = 0, frames = 1;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  (void)image;
  if (size < 20)
    return BK_ERR_TRUNCATED;
  if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WEBP", 4) != 0)
    return BK_ERR_BAD_MAGIC;
  while (pos + 8 <= size) {
    const uint8_t *id = data + pos;
    uint32_t n = le32w(data + pos + 4);
    const uint8_t *payload = data + pos + 8;
    if (pos + 8u + n > size)
      return BK_ERR_TRUNCATED;
    if (memcmp(id, "VP8X", 4) == 0 && n >= 10) {
      flags = payload[0];
      width = le24(payload + 4) + 1u;
      height = le24(payload + 7) + 1u;
    } else if (memcmp(id, "VP8 ", 4) == 0 && n >= 10) {
      width = (uint32_t)(payload[6] | ((uint32_t)payload[7] << 8)) & 0x3fffu;
      height = (uint32_t)(payload[8] | ((uint32_t)payload[9] << 8)) & 0x3fffu;
    } else if (memcmp(id, "ANMF", 4) == 0)
      frames++;
    pos += 8u + n + (n & 1u);
  }
  if (!width || !height)
    return BK_ERR_CORRUPT;
  bk_status st = bk_validate_dimensions(width, height, &ctx->options);
  if (st != BK_OK)
    return st;
  if (info) {
    info->format = BK_FORMAT_WEBP;
    info->width = width;
    info->height = height;
    info->bits_per_pixel = 32;
    info->frame_count = frames;
    info->has_alpha = (flags & 0x10u) != 0;
    bk_metadata_add_u32(&info->metadata, "vp8x_flags", flags);
  }
  return ctx->metadata_only ? BK_OK : BK_ERR_UNSUPPORTED;
}
