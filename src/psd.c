#include "bitmapkit/internal.h"

static uint16_t psd_be16(const uint8_t *p) {
  return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static uint32_t psd_be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | p[3];
}

static bk_status psd_skip_section(size_t *pos, const uint8_t *data,
                                  size_t size) {
  uint32_t n;
  if (*pos + 4 > size)
    return BK_ERR_TRUNCATED;
  n = psd_be32(data + *pos);
  *pos += 4;
  if (*pos + n > size)
    return BK_ERR_TRUNCATED;
  *pos += n;
  return BK_OK;
}

static bk_status psd_decode_packbits_row(const uint8_t *src, size_t src_size,
                                         size_t *src_pos, uint8_t *dst,
                                         size_t dst_size) {
  size_t out = 0;
  while (out < dst_size) {
    int8_t n;
    if (*src_pos >= src_size)
      return BK_ERR_TRUNCATED;
    n = (int8_t)src[(*src_pos)++];
    if (n >= 0) {
      size_t count = (size_t)n + 1u;
      if (*src_pos + count > src_size || out + count > dst_size)
        return BK_ERR_TRUNCATED;
      memcpy(dst + out, src + *src_pos, count);
      *src_pos += count;
      out += count;
    } else if (n >= -127) {
      size_t count = (size_t)(1 - n);
      uint8_t v;
      if (*src_pos >= src_size || out + count > dst_size)
        return BK_ERR_TRUNCATED;
      v = src[(*src_pos)++];
      memset(dst + out, v, count);
      out += count;
    }
  }
  return BK_OK;
}

static uint8_t psd_sample8(const uint8_t *planes, uint32_t plane,
                           uint32_t width, uint32_t height, uint32_t x,
                           uint32_t y) {
  return planes[(size_t)plane * width * height + (size_t)y * width + x];
}

bk_status bk_decode_psd(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info) {
  uint16_t version, channels, depth, color_mode, compression;
  uint32_t height, width;
  size_t pos = 26;
  size_t plane_size;
  uint8_t *planes = NULL;
  bk_status st;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  if (size < 28)
    return BK_ERR_TRUNCATED;
  if (memcmp(data, "8BPS", 4) != 0)
    return BK_ERR_BAD_MAGIC;
  version = psd_be16(data + 4);
  channels = psd_be16(data + 12);
  height = psd_be32(data + 14);
  width = psd_be32(data + 18);
  depth = psd_be16(data + 22);
  color_mode = psd_be16(data + 24);
  if (version != 1 || channels == 0 || channels > 8)
    return BK_ERR_UNSUPPORTED;
  if (!(depth == 8 || depth == 16))
    return BK_ERR_UNSUPPORTED;
  if (!(color_mode == 1 || color_mode == 3 || color_mode == 4))
    return BK_ERR_UNSUPPORTED;
  st = bk_validate_dimensions(width, height, &ctx->options);
  if (st != BK_OK)
    return st;
  st = psd_skip_section(&pos, data, size);
  if (st != BK_OK)
    return st;
  st = psd_skip_section(&pos, data, size);
  if (st != BK_OK)
    return st;
  st = psd_skip_section(&pos, data, size);
  if (st != BK_OK)
    return st;
  if (pos + 2 > size)
    return BK_ERR_TRUNCATED;
  compression = psd_be16(data + pos);
  pos += 2;
  if (info) {
    info->format = BK_FORMAT_PSD;
    info->width = width;
    info->height = height;
    info->bits_per_pixel = channels * depth;
    info->frame_count = 1;
    info->has_alpha = channels >= 4;
    info->indexed = color_mode == 1;
    bk_metadata_add_u32(&info->metadata, "channels", channels);
    bk_metadata_add_u32(&info->metadata, "depth", depth);
    bk_metadata_add_u32(&info->metadata, "color_mode", color_mode);
    bk_metadata_add_u32(&info->metadata, "compression", compression);
  }
  if (ctx->metadata_only)
    return BK_OK;
  if (!image)
    return BK_ERR_ARGUMENT;
  if ((uint64_t)width * height > SIZE_MAX / channels)
    return BK_ERR_LIMIT;
  plane_size = (size_t)width * height;
  planes = (uint8_t *)calloc(channels, plane_size);
  if (!planes)
    return BK_ERR_OOM;
  if (compression == 0) {
    uint32_t c, y;
    size_t bytes_per_sample = depth == 16 ? 2u : 1u;
    if (pos + (size_t)channels * plane_size * bytes_per_sample > size) {
      free(planes);
      return BK_ERR_TRUNCATED;
    }
    for (c = 0; c < channels; ++c) {
      for (y = 0; y < height; ++y) {
        uint32_t x;
        for (x = 0; x < width; ++x) {
          planes[(size_t)c * plane_size + (size_t)y * width + x] = data[pos];
          pos += bytes_per_sample;
        }
      }
    }
  } else if (compression == 1) {
    uint32_t rows = (uint32_t)channels * height;
    uint32_t row;
    size_t table = pos;
    if (pos + (size_t)rows * 2u > size) {
      free(planes);
      return BK_ERR_TRUNCATED;
    }
    pos += (size_t)rows * 2u;
    for (row = 0; row < rows; ++row) {
      uint16_t row_len = psd_be16(data + table + (size_t)row * 2u);
      uint32_t c = row / height;
      uint32_t y = row % height;
      size_t row_pos = pos;
      if (pos + row_len > size) {
        free(planes);
        return BK_ERR_TRUNCATED;
      }
      st = psd_decode_packbits_row(
          data, pos + row_len, &row_pos,
          planes + (size_t)c * plane_size + (size_t)y * width, width);
      if (st != BK_OK) {
        free(planes);
        return st;
      }
      pos += row_len;
    }
  } else {
    free(planes);
    return BK_ERR_UNSUPPORTED;
  }
  st = bk_image_alloc(image, width, height);
  if (st != BK_OK) {
    free(planes);
    return st;
  }
  image->source_format = BK_FORMAT_PSD;
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      uint8_t r = 0, g = 0, b = 0, a = 255;
      if (color_mode == 1) {
        r = g = b = psd_sample8(planes, 0, width, height, x, y);
      } else if (color_mode == 3) {
        r = psd_sample8(planes, 0, width, height, x, y);
        g = channels > 1 ? psd_sample8(planes, 1, width, height, x, y) : r;
        b = channels > 2 ? psd_sample8(planes, 2, width, height, x, y) : r;
        if (channels > 3)
          a = psd_sample8(planes, 3, width, height, x, y);
      } else {
        uint8_t c = psd_sample8(planes, 0, width, height, x, y);
        uint8_t m =
            channels > 1 ? psd_sample8(planes, 1, width, height, x, y) : 0;
        uint8_t yy =
            channels > 2 ? psd_sample8(planes, 2, width, height, x, y) : 0;
        uint8_t k =
            channels > 3 ? psd_sample8(planes, 3, width, height, x, y) : 0;
        r = (uint8_t)(255u - ((uint32_t)c + k > 255u ? 255u : (uint32_t)c + k));
        g = (uint8_t)(255u - ((uint32_t)m + k > 255u ? 255u : (uint32_t)m + k));
        b = (uint8_t)(255u -
                      ((uint32_t)yy + k > 255u ? 255u : (uint32_t)yy + k));
      }
      bk_set_pixel(image, x, y, r, g, b, a);
    }
  }
  free(planes);
  return BK_OK;
}
