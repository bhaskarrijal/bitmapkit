#include "bitmapkit/internal.h"

typedef struct qoi_px {
  uint8_t r, g, b, a;
} qoi_px;

static uint8_t qoi_hash(qoi_px p) {
  return (uint8_t)(((uint32_t)p.r * 3u + (uint32_t)p.g * 5u +
                    (uint32_t)p.b * 7u + (uint32_t)p.a * 11u) &
                   63u);
}

static bk_status qoi_read_header(const uint8_t *data, size_t size,
                                 uint32_t *width, uint32_t *height,
                                 uint8_t *channels, uint8_t *colorspace) {
  if (size < 14)
    return BK_ERR_TRUNCATED;
  if (memcmp(data, "qoif", 4) != 0)
    return BK_ERR_BAD_MAGIC;
  *width = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) |
           ((uint32_t)data[6] << 8) | data[7];
  *height = ((uint32_t)data[8] << 24) | ((uint32_t)data[9] << 16) |
            ((uint32_t)data[10] << 8) | data[11];
  *channels = data[12];
  *colorspace = data[13];
  if (!(*channels == 3 || *channels == 4))
    return BK_ERR_UNSUPPORTED;
  if (*colorspace > 1)
    return BK_ERR_UNSUPPORTED;
  return BK_OK;
}

bk_status bk_decode_qoi(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info) {
  uint32_t width, height, px_count, px_pos;
  uint8_t channels, colorspace;
  qoi_px index[64];
  qoi_px px = {0, 0, 0, 255};
  size_t p = 14;
  bk_status st;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  st = qoi_read_header(data, size, &width, &height, &channels, &colorspace);
  if (st != BK_OK)
    return st;
  st = bk_validate_dimensions(width, height, &ctx->options);
  if (st != BK_OK)
    return st;
  if (info) {
    info->format = BK_FORMAT_QOI;
    info->width = width;
    info->height = height;
    info->bits_per_pixel = channels == 4 ? 32 : 24;
    info->frame_count = 1;
    info->has_alpha = channels == 4;
    bk_metadata_add_u32(&info->metadata, "channels", channels);
    bk_metadata_add_u32(&info->metadata, "colorspace", colorspace);
  }
  if (ctx->metadata_only)
    return BK_OK;
  if (!image)
    return BK_ERR_ARGUMENT;
  st = bk_image_alloc(image, width, height);
  if (st != BK_OK)
    return st;
  image->source_format = BK_FORMAT_QOI;
  memset(index, 0, sizeof(index));
  px_count = width * height;
  px_pos = 0;
  while (px_pos < px_count) {
    uint8_t b1;
    if (p >= size) {
      bk_image_free(image);
      return BK_ERR_TRUNCATED;
    }
    b1 = data[p++];
    if (b1 == 0xfe) {
      if (p + 3 > size) {
        bk_image_free(image);
        return BK_ERR_TRUNCATED;
      }
      px.r = data[p++];
      px.g = data[p++];
      px.b = data[p++];
    } else if (b1 == 0xff) {
      if (p + 4 > size) {
        bk_image_free(image);
        return BK_ERR_TRUNCATED;
      }
      px.r = data[p++];
      px.g = data[p++];
      px.b = data[p++];
      px.a = data[p++];
    } else if ((b1 & 0xc0u) == 0x00u) {
      px = index[b1 & 63u];
    } else if ((b1 & 0xc0u) == 0x40u) {
      px.r = (uint8_t)(px.r + ((b1 >> 4) & 3u) - 2u);
      px.g = (uint8_t)(px.g + ((b1 >> 2) & 3u) - 2u);
      px.b = (uint8_t)(px.b + (b1 & 3u) - 2u);
    } else if ((b1 & 0xc0u) == 0x80u) {
      uint8_t b2;
      int dg, dr, db;
      if (p >= size) {
        bk_image_free(image);
        return BK_ERR_TRUNCATED;
      }
      b2 = data[p++];
      dg = (int)(b1 & 0x3fu) - 32;
      dr = ((int)(b2 >> 4) - 8) + dg;
      db = ((int)(b2 & 0x0fu) - 8) + dg;
      px.r = (uint8_t)(px.r + dr);
      px.g = (uint8_t)(px.g + dg);
      px.b = (uint8_t)(px.b + db);
    } else {
      uint32_t run = (b1 & 0x3fu) + 1u;
      while (run-- && px_pos < px_count) {
        bk_set_pixel(image, px_pos % width, px_pos / width, px.r, px.g, px.b,
                     px.a);
        px_pos++;
      }
      continue;
    }
    index[qoi_hash(px)] = px;
    bk_set_pixel(image, px_pos % width, px_pos / width, px.r, px.g, px.b, px.a);
    px_pos++;
  }
  return BK_OK;
}
