#include "bitmapkit/internal.h"

static uint32_t exr_le32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static size_t cstrnlen_local(const uint8_t *p, size_t max) {
  size_t i;
  for (i = 0; i < max && p[i]; ++i) {
  }
  return i;
}

bk_status bk_decode_exr(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info) {
  size_t pos = 8;
  uint32_t width = 0, height = 0, compression = 0, channels = 0;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  (void)image;
  if (size < 12)
    return BK_ERR_TRUNCATED;
  if (exr_le32(data) != 20000630u)
    return BK_ERR_BAD_MAGIC;
  while (pos < size && data[pos] != 0) {
    size_t name_len = cstrnlen_local(data + pos, size - pos);
    if (pos + name_len + 1 >= size)
      return BK_ERR_TRUNCATED;
    const char *name = (const char *)(data + pos);
    pos += name_len + 1;
    size_t type_len = cstrnlen_local(data + pos, size - pos);
    if (pos + type_len + 5 > size)
      return BK_ERR_TRUNCATED;
    const char *type = (const char *)(data + pos);
    pos += type_len + 1;
    uint32_t n = exr_le32(data + pos);
    pos += 4;
    if (n > size - pos)
      return BK_ERR_TRUNCATED;
    if (strcmp(name, "compression") == 0 && n >= 1)
      compression = data[pos];
    else if (strcmp(name, "channels") == 0 && strcmp(type, "chlist") == 0) {
      size_t cp = pos;
      size_t end = pos + n;
      while (cp < end && data[cp]) {
        size_t l = cstrnlen_local(data + cp, end - cp);
        if (l == 0 || l > end - cp || end - cp - l < 17u)
          break;
        channels++;
        cp += l + 17;
      }
    } else if (strcmp(name, "dataWindow") == 0 && n >= 16) {
      int32_t xmin = (int32_t)exr_le32(data + pos);
      int32_t ymin = (int32_t)exr_le32(data + pos + 4);
      int32_t xmax = (int32_t)exr_le32(data + pos + 8);
      int32_t ymax = (int32_t)exr_le32(data + pos + 12);
      if (xmax >= xmin && ymax >= ymin) {
        int64_t w = (int64_t)xmax - (int64_t)xmin + 1;
        int64_t h = (int64_t)ymax - (int64_t)ymin + 1;
        if (w <= UINT32_MAX && h <= UINT32_MAX) {
          width = (uint32_t)w;
          height = (uint32_t)h;
        }
      }
    }
    pos += n;
  }
  if (!width || !height)
    return BK_ERR_CORRUPT;
  bk_status st = bk_validate_dimensions(width, height, &ctx->options);
  if (st != BK_OK)
    return st;
  if (info) {
    info->format = BK_FORMAT_EXR;
    info->width = width;
    info->height = height;
    info->bits_per_pixel = channels ? channels * 16u : 0;
    info->frame_count = 1;
    info->has_alpha = channels >= 4;
    bk_metadata_add_u32(&info->metadata, "compression", compression);
    bk_metadata_add_u32(&info->metadata, "channels", channels);
  }
  return ctx->metadata_only ? BK_OK : BK_ERR_UNSUPPORTED;
}
