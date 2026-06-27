#include "bitmapkit/internal.h"

static uint32_t xwd_be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | p[3];
}
static uint32_t xwd_le32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

bk_status bk_decode_xwd(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info) {
  uint32_t header_size_be, header_size, file_version, pixmap_format, depth,
      width, height, byte_order, bitmap_unit, bitmap_bit_order, bitmap_pad,
      bits_per_pixel, bytes_per_line, visual_class, ncolors;
  int le = 0;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  if (size < 100)
    return BK_ERR_TRUNCATED;
  header_size_be = xwd_be32(data);
  if (header_size_be >= 100 && header_size_be <= size)
    le = 0;
  else {
    uint32_t hs = xwd_le32(data);
    if (hs >= 100 && hs <= size)
      le = 1;
    else
      return BK_ERR_BAD_MAGIC;
  }
#define XWD32(off) (le ? xwd_le32(data + (off)) : xwd_be32(data + (off)))
  header_size = XWD32(0);
  file_version = XWD32(4);
  pixmap_format = XWD32(8);
  depth = XWD32(12);
  width = XWD32(16);
  height = XWD32(20);
  byte_order = XWD32(28);
  bitmap_unit = XWD32(32);
  bitmap_bit_order = XWD32(36);
  bitmap_pad = XWD32(40);
  bits_per_pixel = XWD32(44);
  bytes_per_line = XWD32(48);
  visual_class = XWD32(52);
  ncolors = XWD32(76);
#undef XWD32
  if (file_version != 7 || header_size > size)
    return BK_ERR_BAD_MAGIC;
  bk_status st = bk_validate_dimensions(width, height, &ctx->options);
  if (st != BK_OK)
    return st;
  if (info) {
    info->format = BK_FORMAT_XWD;
    info->width = width;
    info->height = height;
    info->bits_per_pixel = bits_per_pixel;
    info->frame_count = 1;
    info->indexed = ncolors != 0;
    info->has_alpha = bits_per_pixel == 32;
    bk_metadata_add_u32(&info->metadata, "depth", depth);
    bk_metadata_add_u32(&info->metadata, "pixmap_format", pixmap_format);
    bk_metadata_add_u32(&info->metadata, "byte_order", byte_order);
    bk_metadata_add_u32(&info->metadata, "bitmap_unit", bitmap_unit);
    bk_metadata_add_u32(&info->metadata, "bit_order", bitmap_bit_order);
    bk_metadata_add_u32(&info->metadata, "bitmap_pad", bitmap_pad);
    bk_metadata_add_u32(&info->metadata, "visual_class", visual_class);
  }
  if (ctx->metadata_only)
    return BK_OK;
  if (!image)
    return BK_ERR_ARGUMENT;
  if (!(bits_per_pixel == 24 || bits_per_pixel == 32) ||
      header_size + (uint64_t)ncolors * 12u +
              (uint64_t)bytes_per_line * height >
          size)
    return BK_ERR_UNSUPPORTED;
  const uint8_t *pixels = data + header_size + (size_t)ncolors * 12u;
  st = bk_image_alloc(image, width, height);
  if (st != BK_OK)
    return st;
  image->source_format = BK_FORMAT_XWD;
  for (uint32_t y = 0; y < height; ++y)
    for (uint32_t x = 0; x < width; ++x) {
      const uint8_t *p = pixels + (size_t)y * bytes_per_line +
                         (size_t)x * (bits_per_pixel / 8u);
      if (byte_order == 0)
        bk_set_pixel(image, x, y, p[2], p[1], p[0],
                     bits_per_pixel == 32 ? p[3] : 255);
      else
        bk_set_pixel(image, x, y, p[1], p[2], p[3],
                     bits_per_pixel == 32 ? p[0] : 255);
    }
  return BK_OK;
}
