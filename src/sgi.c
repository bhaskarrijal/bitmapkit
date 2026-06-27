#include "bitmapkit/internal.h"

static uint16_t sgi_be16(const uint8_t *p) {
  return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static uint32_t sgi_be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | p[3];
}

static bk_status sgi_decode_rle_channel(const uint8_t *data, size_t size,
                                        size_t *pos, uint8_t *dst,
                                        size_t pixels) {
  size_t out = 0;
  while (out < pixels) {
    uint8_t c;
    if (*pos >= size)
      return BK_ERR_TRUNCATED;
    c = data[(*pos)++];
    if ((c & 0x7fu) == 0)
      return BK_OK;
    if (c & 0x80u) {
      uint32_t n = c & 0x7fu;
      if (*pos + n > size || out + n > pixels)
        return BK_ERR_TRUNCATED;
      memcpy(dst + out, data + *pos, n);
      *pos += n;
      out += n;
    } else {
      uint32_t n = c & 0x7fu;
      uint8_t v;
      if (*pos >= size || out + n > pixels)
        return BK_ERR_TRUNCATED;
      v = data[(*pos)++];
      memset(dst + out, v, n);
      out += n;
    }
  }
  return BK_OK;
}

bk_status bk_decode_sgi(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info) {
  uint8_t storage, bpc;
  uint16_t dimension, width, height, channels;
  bk_status st;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  if (size < 512)
    return BK_ERR_TRUNCATED;
  if (sgi_be16(data) != 474u)
    return BK_ERR_BAD_MAGIC;
  storage = data[2];
  bpc = data[3];
  dimension = sgi_be16(data + 4);
  width = sgi_be16(data + 6);
  height = sgi_be16(data + 8);
  channels = sgi_be16(data + 10);
  if (bpc != 1 || channels == 0 || channels > 4 || dimension == 0)
    return BK_ERR_UNSUPPORTED;
  st = bk_validate_dimensions(width, height, &ctx->options);
  if (st != BK_OK)
    return st;
  if (info) {
    info->format = BK_FORMAT_SGI;
    info->width = width;
    info->height = height;
    info->bits_per_pixel = channels * 8u;
    info->frame_count = 1;
    info->has_alpha = channels == 4;
    bk_metadata_add_u32(&info->metadata, "storage", storage);
    bk_metadata_add_u32(&info->metadata, "channels", channels);
  }
  if (ctx->metadata_only)
    return BK_OK;
  if (!image)
    return BK_ERR_ARGUMENT;
  st = bk_image_alloc(image, width, height);
  if (st != BK_OK)
    return st;
  image->source_format = BK_FORMAT_SGI;
  size_t plane = (size_t)width * height;
  uint8_t *planes = (uint8_t *)calloc(channels, plane);
  if (!planes) {
    bk_image_free(image);
    return BK_ERR_OOM;
  }
  if (storage == 0) {
    if (512u + plane * channels > size) {
      free(planes);
      bk_image_free(image);
      return BK_ERR_TRUNCATED;
    }
    memcpy(planes, data + 512u, plane * channels);
  } else if (storage == 1) {
    uint32_t rows = (uint32_t)height * channels;
    if (512u + (size_t)rows * 8u > size) {
      free(planes);
      bk_image_free(image);
      return BK_ERR_TRUNCATED;
    }
    for (uint32_t c = 0; c < channels; ++c)
      for (uint32_t y = 0; y < height; ++y) {
        uint32_t row = c * height + y;
        size_t off = sgi_be32(data + 512u + (size_t)row * 4u);
        st = sgi_decode_rle_channel(
            data, size, &off, planes + (size_t)c * plane + (size_t)y * width,
            width);
        if (st != BK_OK) {
          free(planes);
          bk_image_free(image);
          return st;
        }
      }
  } else {
    free(planes);
    bk_image_free(image);
    return BK_ERR_UNSUPPORTED;
  }
  for (uint32_t y = 0; y < height; ++y)
    for (uint32_t x = 0; x < width; ++x) {
      uint8_t r = planes[(size_t)y * width + x];
      uint8_t g = channels > 1 ? planes[plane + (size_t)y * width + x] : r;
      uint8_t b = channels > 2 ? planes[plane * 2u + (size_t)y * width + x] : r;
      uint8_t a =
          channels > 3 ? planes[plane * 3u + (size_t)y * width + x] : 255;
      bk_set_pixel(image, x, y, r, g, b, a);
    }
  free(planes);
  return BK_OK;
}
