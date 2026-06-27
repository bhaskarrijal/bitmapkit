#include "bitmapkit/internal.h"

static uint32_t ras_be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | p[3];
}

bk_status bk_decode_ras(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info) {
  uint32_t width, height, depth, length, type, maptype, maplen, row_stride;
  const uint8_t *pixels;
  bk_status st;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  if (size < 32)
    return BK_ERR_TRUNCATED;
  if (ras_be32(data) != 0x59a66a95u)
    return BK_ERR_BAD_MAGIC;
  width = ras_be32(data + 4);
  height = ras_be32(data + 8);
  depth = ras_be32(data + 12);
  length = ras_be32(data + 16);
  type = ras_be32(data + 20);
  maptype = ras_be32(data + 24);
  maplen = ras_be32(data + 28);
  st = bk_validate_dimensions(width, height, &ctx->options);
  if (st != BK_OK)
    return st;
  if (info) {
    info->format = BK_FORMAT_RAS;
    info->width = width;
    info->height = height;
    info->bits_per_pixel = depth;
    info->frame_count = 1;
    info->indexed = maptype != 0;
    info->has_alpha = 0;
    bk_metadata_add_u32(&info->metadata, "type", type);
    bk_metadata_add_u32(&info->metadata, "map_length", maplen);
  }
  if (ctx->metadata_only)
    return BK_OK;
  if (!image)
    return BK_ERR_ARGUMENT;
  if (!(depth == 8 || depth == 24 || depth == 32) || type > 1)
    return BK_ERR_UNSUPPORTED;
  if (32u + maplen > size)
    return BK_ERR_TRUNCATED;
  pixels = data + 32u + maplen;
  row_stride = ((width * depth + 15u) / 16u) * 2u;
  if ((uint64_t)(pixels - data) + (uint64_t)row_stride * height > size)
    return BK_ERR_TRUNCATED;
  st = bk_image_alloc(image, width, height);
  if (st != BK_OK)
    return st;
  image->source_format = BK_FORMAT_RAS;
  for (uint32_t y = 0; y < height; ++y)
    for (uint32_t x = 0; x < width; ++x) {
      const uint8_t *p =
          pixels + (size_t)y * row_stride + (size_t)x * (depth / 8u);
      if (depth == 8)
        bk_set_pixel(image, x, y, p[0], p[0], p[0], 255);
      else
        bk_set_pixel(image, x, y, p[0], p[1], p[2], depth == 32 ? p[3] : 255);
    }
  return BK_OK;
}
