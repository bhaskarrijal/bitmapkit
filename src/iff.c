#include "bitmapkit/internal.h"

static uint16_t iff_be16(const uint8_t *p) {
  return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static uint32_t iff_be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | p[3];
}

static bk_status byterun_decode(const uint8_t *src, size_t src_size,
                                uint8_t *dst, size_t dst_size) {
  size_t sp = 0, dp = 0;
  while (sp < src_size && dp < dst_size) {
    int8_t n = (int8_t)src[sp++];
    if (n >= 0) {
      size_t count = (size_t)n + 1u;
      if (sp + count > src_size || dp + count > dst_size)
        return BK_ERR_TRUNCATED;
      memcpy(dst + dp, src + sp, count);
      sp += count;
      dp += count;
    } else if (n >= -127) {
      size_t count = (size_t)(1 - n);
      uint8_t v;
      if (sp >= src_size || dp + count > dst_size)
        return BK_ERR_TRUNCATED;
      v = src[sp++];
      memset(dst + dp, v, count);
      dp += count;
    }
  }
  return dp == dst_size ? BK_OK : BK_ERR_TRUNCATED;
}

static uint8_t bitplane_index(const uint8_t *planes, uint32_t row_bytes,
                              uint32_t planes_count, uint32_t x) {
  uint8_t idx = 0;
  uint32_t byte = x / 8u;
  uint8_t mask = (uint8_t)(0x80u >> (x & 7u));
  for (uint32_t p = 0; p < planes_count && p < 8u; ++p) {
    if (planes[(size_t)p * row_bytes + byte] & mask)
      idx |= (uint8_t)(1u << p);
  }
  return idx;
}

bk_status bk_decode_iff(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info) {
  size_t pos = 12;
  uint32_t width = 0, height = 0, planes_count = 0, compression = 0;
  const uint8_t *cmap = NULL;
  uint32_t cmap_count = 0;
  const uint8_t *body = NULL;
  size_t body_size = 0;
  bk_status st;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  if (size < 12)
    return BK_ERR_TRUNCATED;
  if (memcmp(data, "FORM", 4) != 0 ||
      !(memcmp(data + 8, "ILBM", 4) == 0 || memcmp(data + 8, "PBM ", 4) == 0))
    return BK_ERR_BAD_MAGIC;
  while (pos + 8 <= size) {
    const uint8_t *id = data + pos;
    uint32_t n = iff_be32(data + pos + 4);
    const uint8_t *payload = data + pos + 8;
    if (pos + 8u + n > size)
      return BK_ERR_TRUNCATED;
    if (memcmp(id, "BMHD", 4) == 0 && n >= 20) {
      width = iff_be16(payload);
      height = iff_be16(payload + 2);
      planes_count = payload[8];
      compression = payload[10];
    } else if (memcmp(id, "CMAP", 4) == 0) {
      cmap = payload;
      cmap_count = n / 3u;
    } else if (memcmp(id, "BODY", 4) == 0) {
      body = payload;
      body_size = n;
    }
    pos += 8u + n + (n & 1u);
  }
  if (!width || !height || !body)
    return BK_ERR_CORRUPT;
  st = bk_validate_dimensions(width, height, &ctx->options);
  if (st != BK_OK)
    return st;
  if (info) {
    info->format = BK_FORMAT_IFF;
    info->width = width;
    info->height = height;
    info->bits_per_pixel = planes_count;
    info->frame_count = 1;
    info->indexed = cmap != NULL;
    bk_metadata_add_u32(&info->metadata, "planes", planes_count);
    bk_metadata_add_u32(&info->metadata, "compression", compression);
    bk_metadata_add_u32(&info->metadata, "palette_entries", cmap_count);
  }
  if (ctx->metadata_only)
    return BK_OK;
  if (!image)
    return BK_ERR_ARGUMENT;
  if (planes_count == 0 || planes_count > 8 || !cmap || cmap_count == 0)
    return BK_ERR_UNSUPPORTED;
  uint32_t row_bytes = ((width + 15u) / 16u) * 2u;
  size_t decoded_size = (size_t)row_bytes * planes_count * height;
  uint8_t *decoded = (uint8_t *)malloc(decoded_size);
  if (!decoded)
    return BK_ERR_OOM;
  if (compression == 0) {
    if (body_size < decoded_size) {
      free(decoded);
      return BK_ERR_TRUNCATED;
    }
    memcpy(decoded, body, decoded_size);
  } else if (compression == 1) {
    st = byterun_decode(body, body_size, decoded, decoded_size);
    if (st != BK_OK) {
      free(decoded);
      return st;
    }
  } else {
    free(decoded);
    return BK_ERR_UNSUPPORTED;
  }
  st = bk_image_alloc(image, width, height);
  if (st != BK_OK) {
    free(decoded);
    return st;
  }
  image->source_format = BK_FORMAT_IFF;
  for (uint32_t y = 0; y < height; ++y) {
    const uint8_t *row = decoded + (size_t)y * row_bytes * planes_count;
    for (uint32_t x = 0; x < width; ++x) {
      uint8_t idx = bitplane_index(row, row_bytes, planes_count, x);
      uint32_t pi = idx < cmap_count ? idx : 0;
      bk_set_pixel(image, x, y, cmap[pi * 3u], cmap[pi * 3u + 1u],
                   cmap[pi * 3u + 2u], 255);
    }
  }
  free(decoded);
  return BK_OK;
}
