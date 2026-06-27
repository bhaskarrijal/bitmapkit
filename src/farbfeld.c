#include "bitmapkit/internal.h"

static uint16_t ff_be16(const uint8_t *p) {
  return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static uint32_t ff_be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | p[3];
}

bk_status bk_decode_farbfeld(const uint8_t *data, size_t size,
                             const bk_decode_context *ctx, bk_image *image,
                             bk_info *info) {
  uint32_t width, height, x, y;
  size_t need;
  bk_status st;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  if (size < 16)
    return BK_ERR_TRUNCATED;
  if (memcmp(data, "farbfeld", 8) != 0)
    return BK_ERR_BAD_MAGIC;
  width = ff_be32(data + 8);
  height = ff_be32(data + 12);
  st = bk_validate_dimensions(width, height, &ctx->options);
  if (st != BK_OK)
    return st;
  if ((uint64_t)width * height > (SIZE_MAX - 16u) / 8u)
    return BK_ERR_LIMIT;
  need = 16u + (size_t)width * height * 8u;
  if (need > size)
    return BK_ERR_TRUNCATED;
  if (info) {
    info->format = BK_FORMAT_FARBFELD;
    info->width = width;
    info->height = height;
    info->bits_per_pixel = 64;
    info->frame_count = 1;
    info->has_alpha = 1;
  }
  if (ctx->metadata_only)
    return BK_OK;
  if (!image)
    return BK_ERR_ARGUMENT;
  st = bk_image_alloc(image, width, height);
  if (st != BK_OK)
    return st;
  image->source_format = BK_FORMAT_FARBFELD;
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      const uint8_t *p = data + 16u + ((size_t)y * width + x) * 8u;
      bk_set_pixel(image, x, y, (uint8_t)(ff_be16(p) >> 8),
                   (uint8_t)(ff_be16(p + 2) >> 8),
                   (uint8_t)(ff_be16(p + 4) >> 8),
                   (uint8_t)(ff_be16(p + 6) >> 8));
    }
  }
  return BK_OK;
}
