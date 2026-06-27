#include "bitmapkit/internal.h"

static uint32_t dds_le32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}
static uint32_t mask_shift(uint32_t m) {
  uint32_t s = 0;
  if (!m)
    return 0;
  while ((m & 1u) == 0) {
    m >>= 1;
    ++s;
  }
  return s;
}
static uint8_t mask_scale(uint32_t v, uint32_t m) {
  uint32_t s, maxv, raw;
  if (!m)
    return 0;
  s = mask_shift(m);
  raw = (v & m) >> s;
  maxv = m >> s;
  return maxv ? (uint8_t)((raw * 255u + maxv / 2u) / maxv) : 0;
}

bk_status bk_decode_dds(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info) {
  uint32_t height, width, pitch, pf_size, flags, fourcc, rgb_bits, rmask, gmask,
      bmask, amask;
  bk_status st;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  if (size < 128)
    return BK_ERR_TRUNCATED;
  if (memcmp(data, "DDS ", 4) != 0 || dds_le32(data + 4) != 124)
    return BK_ERR_BAD_MAGIC;
  flags = dds_le32(data + 8);
  height = dds_le32(data + 12);
  width = dds_le32(data + 16);
  pitch = dds_le32(data + 20);
  pf_size = dds_le32(data + 76);
  fourcc = dds_le32(data + 84);
  rgb_bits = dds_le32(data + 88);
  rmask = dds_le32(data + 92);
  gmask = dds_le32(data + 96);
  bmask = dds_le32(data + 100);
  amask = dds_le32(data + 104);
  if (pf_size != 32)
    return BK_ERR_CORRUPT;
  st = bk_validate_dimensions(width, height, &ctx->options);
  if (st != BK_OK)
    return st;
  if (info) {
    info->format = BK_FORMAT_DDS;
    info->width = width;
    info->height = height;
    info->bits_per_pixel = rgb_bits;
    info->frame_count = 1;
    info->has_alpha = amask != 0;
    bk_metadata_add_u32(&info->metadata, "flags", flags);
    bk_metadata_add_u32(&info->metadata, "fourcc", fourcc);
  }
  if (ctx->metadata_only)
    return BK_OK;
  if (!image)
    return BK_ERR_ARGUMENT;
  if (fourcc != 0 || !(rgb_bits == 32 || rgb_bits == 24 || rgb_bits == 16))
    return BK_ERR_UNSUPPORTED;
  if (!pitch)
    pitch = ((width * rgb_bits + 31u) / 32u) * 4u;
  if (128u + (uint64_t)pitch * height > size)
    return BK_ERR_TRUNCATED;
  st = bk_image_alloc(image, width, height);
  if (st != BK_OK)
    return st;
  image->source_format = BK_FORMAT_DDS;
  for (uint32_t y = 0; y < height; ++y)
    for (uint32_t x = 0; x < width; ++x) {
      const uint8_t *p =
          data + 128u + (size_t)y * pitch + (size_t)x * (rgb_bits / 8u);
      uint32_t v = p[0] | ((uint32_t)p[1] << 8) |
                   (rgb_bits >= 24 ? ((uint32_t)p[2] << 16) : 0) |
                   (rgb_bits == 32 ? ((uint32_t)p[3] << 24) : 0);
      bk_set_pixel(image, x, y, mask_scale(v, rmask), mask_scale(v, gmask),
                   mask_scale(v, bmask), amask ? mask_scale(v, amask) : 255);
    }
  return BK_OK;
}
