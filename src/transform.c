#include "bitmapkit/internal.h"
#include "bitmapkit/ops.h"

bk_status bk_image_clone(const bk_image *source, bk_image *out) {
  bk_status st;
  if (!source || !source->pixels || !out)
    return BK_ERR_ARGUMENT;
  st = bk_image_alloc(out, source->width, source->height);
  if (st != BK_OK)
    return st;
  memcpy(out->pixels, source->pixels, source->pixel_size);
  out->source_format = source->source_format;
  out->metadata = source->metadata;
  return BK_OK;
}

bk_status bk_image_rotate90(const bk_image *source, bk_image *out) {
  uint32_t x, y;
  bk_status st;
  if (!source || !source->pixels || !out)
    return BK_ERR_ARGUMENT;
  st = bk_image_alloc(out, source->height, source->width);
  if (st != BK_OK)
    return st;
  for (y = 0; y < source->height; ++y) {
    for (x = 0; x < source->width; ++x) {
      uint8_t r, g, b, a;
      bk_get_pixel(source, x, y, &r, &g, &b, &a);
      bk_set_pixel(out, source->height - 1u - y, x, r, g, b, a);
    }
  }
  out->source_format = source->source_format;
  return BK_OK;
}

bk_status bk_image_rotate180(const bk_image *source, bk_image *out) {
  uint32_t x, y;
  bk_status st;
  if (!source || !source->pixels || !out)
    return BK_ERR_ARGUMENT;
  st = bk_image_alloc(out, source->width, source->height);
  if (st != BK_OK)
    return st;
  for (y = 0; y < source->height; ++y) {
    for (x = 0; x < source->width; ++x) {
      uint8_t r, g, b, a;
      bk_get_pixel(source, x, y, &r, &g, &b, &a);
      bk_set_pixel(out, source->width - 1u - x, source->height - 1u - y, r, g,
                   b, a);
    }
  }
  out->source_format = source->source_format;
  return BK_OK;
}

bk_status bk_image_rotate270(const bk_image *source, bk_image *out) {
  uint32_t x, y;
  bk_status st;
  if (!source || !source->pixels || !out)
    return BK_ERR_ARGUMENT;
  st = bk_image_alloc(out, source->height, source->width);
  if (st != BK_OK)
    return st;
  for (y = 0; y < source->height; ++y) {
    for (x = 0; x < source->width; ++x) {
      uint8_t r, g, b, a;
      bk_get_pixel(source, x, y, &r, &g, &b, &a);
      bk_set_pixel(out, y, source->width - 1u - x, r, g, b, a);
    }
  }
  out->source_format = source->source_format;
  return BK_OK;
}

bk_status bk_image_mirror_horizontal(bk_image *image) {
  uint32_t x, y;
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  for (y = 0; y < image->height; ++y) {
    for (x = 0; x < image->width / 2u; ++x) {
      uint8_t *left =
          image->pixels + (size_t)y * image->stride + (size_t)x * 4u;
      uint8_t *right = image->pixels + (size_t)y * image->stride +
                       (size_t)(image->width - 1u - x) * 4u;
      uint8_t tmp[4];
      memcpy(tmp, left, 4);
      memcpy(left, right, 4);
      memcpy(right, tmp, 4);
    }
  }
  return BK_OK;
}

bk_status bk_image_fill_rect(bk_image *image, uint32_t x, uint32_t y,
                             uint32_t w, uint32_t h, uint8_t r, uint8_t g,
                             uint8_t b, uint8_t a) {
  uint32_t xx, yy;
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  if (x >= image->width || y >= image->height)
    return BK_ERR_DIMENSIONS;
  if (w > image->width - x)
    w = image->width - x;
  if (h > image->height - y)
    h = image->height - y;
  for (yy = y; yy < y + h; ++yy) {
    for (xx = x; xx < x + w; ++xx) {
      bk_set_pixel(image, xx, yy, r, g, b, a);
    }
  }
  return BK_OK;
}

static uint8_t bk_lerp_u8(uint8_t a, uint8_t b, uint32_t t, uint32_t scale) {
  uint32_t av = (uint32_t)a * (scale - t);
  uint32_t bv = (uint32_t)b * t;
  return (uint8_t)((av + bv + scale / 2u) / scale);
}

bk_status bk_image_resize_nearest(const bk_image *source, uint32_t width,
                                  uint32_t height, bk_image *out) {
  uint32_t x, y;
  bk_status st;
  if (!source || !source->pixels || !out)
    return BK_ERR_ARGUMENT;
  st = bk_image_alloc(out, width, height);
  if (st != BK_OK)
    return st;
  for (y = 0; y < height; ++y) {
    uint32_t sy = (uint32_t)(((uint64_t)y * source->height) / height);
    if (sy >= source->height)
      sy = source->height - 1u;
    for (x = 0; x < width; ++x) {
      uint32_t sx = (uint32_t)(((uint64_t)x * source->width) / width);
      uint8_t r, g, b, a;
      if (sx >= source->width)
        sx = source->width - 1u;
      bk_get_pixel(source, sx, sy, &r, &g, &b, &a);
      bk_set_pixel(out, x, y, r, g, b, a);
    }
  }
  out->source_format = source->source_format;
  return BK_OK;
}

bk_status bk_image_resize_bilinear(const bk_image *source, uint32_t width,
                                   uint32_t height, bk_image *out) {
  uint32_t x, y;
  bk_status st;
  if (!source || !source->pixels || !out)
    return BK_ERR_ARGUMENT;
  st = bk_image_alloc(out, width, height);
  if (st != BK_OK)
    return st;
  for (y = 0; y < height; ++y) {
    uint64_t gy = height > 1 ? ((uint64_t)y * (source->height - 1u) * 65536u) /
                                   (height - 1u)
                             : 0;
    uint32_t y0 = (uint32_t)(gy >> 16);
    uint32_t y1 = y0 + 1u < source->height ? y0 + 1u : y0;
    uint32_t ty = (uint32_t)(gy & 65535u);
    for (x = 0; x < width; ++x) {
      uint64_t gx = width > 1 ? ((uint64_t)x * (source->width - 1u) * 65536u) /
                                    (width - 1u)
                              : 0;
      uint32_t x0 = (uint32_t)(gx >> 16);
      uint32_t x1 = x0 + 1u < source->width ? x0 + 1u : x0;
      uint32_t tx = (uint32_t)(gx & 65535u);
      uint8_t c00[4], c10[4], c01[4], c11[4];
      uint8_t top[4], bot[4], outc[4];
      int i;
      bk_get_pixel(source, x0, y0, &c00[0], &c00[1], &c00[2], &c00[3]);
      bk_get_pixel(source, x1, y0, &c10[0], &c10[1], &c10[2], &c10[3]);
      bk_get_pixel(source, x0, y1, &c01[0], &c01[1], &c01[2], &c01[3]);
      bk_get_pixel(source, x1, y1, &c11[0], &c11[1], &c11[2], &c11[3]);
      for (i = 0; i < 4; ++i) {
        top[i] = bk_lerp_u8(c00[i], c10[i], tx, 65536u);
        bot[i] = bk_lerp_u8(c01[i], c11[i], tx, 65536u);
        outc[i] = bk_lerp_u8(top[i], bot[i], ty, 65536u);
      }
      bk_set_pixel(out, x, y, outc[0], outc[1], outc[2], outc[3]);
    }
  }
  out->source_format = source->source_format;
  return BK_OK;
}

bk_status bk_image_filter_invert(bk_image *image) {
  uint32_t x;
  uint32_t y;
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  for (y = 0; y < image->height; ++y) {
    uint8_t *row = image->pixels + (size_t)y * image->stride;
    for (x = 0; x < image->width; ++x) {
      uint8_t *p = row + (size_t)x * 4u;
      p[0] = 255u - p[0];
      p[1] = 255u - p[1];
      p[2] = 255u - p[2];
    }
  }
  return BK_OK;
}

bk_status bk_image_filter_clear_alpha(bk_image *image) {
  uint32_t x;
  uint32_t y;
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  for (y = 0; y < image->height; ++y) {
    uint8_t *row = image->pixels + (size_t)y * image->stride;
    for (x = 0; x < image->width; ++x) {
      uint8_t *p = row + (size_t)x * 4u;
      p[3] = 255u;
    }
  }
  return BK_OK;
}

bk_status bk_image_filter_zero_alpha(bk_image *image) {
  uint32_t x;
  uint32_t y;
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  for (y = 0; y < image->height; ++y) {
    uint8_t *row = image->pixels + (size_t)y * image->stride;
    for (x = 0; x < image->width; ++x) {
      uint8_t *p = row + (size_t)x * 4u;
      p[3] = 0u;
    }
  }
  return BK_OK;
}

bk_status bk_image_filter_swap_rb(bk_image *image) {
  uint32_t x;
  uint32_t y;
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  for (y = 0; y < image->height; ++y) {
    uint8_t *row = image->pixels + (size_t)y * image->stride;
    for (x = 0; x < image->width; ++x) {
      uint8_t *p = row + (size_t)x * 4u;
      uint8_t t = p[0];
      p[0] = p[2];
      p[2] = t;
    }
  }
  return BK_OK;
}

bk_status bk_image_filter_threshold_32(bk_image *image) {
  uint32_t x;
  uint32_t y;
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  for (y = 0; y < image->height; ++y) {
    uint8_t *row = image->pixels + (size_t)y * image->stride;
    for (x = 0; x < image->width; ++x) {
      uint8_t *p = row + (size_t)x * 4u;
      uint32_t yv = ((uint32_t)p[0] * 299u + (uint32_t)p[1] * 587u +
                     (uint32_t)p[2] * 114u) /
                    1000u;
      p[0] = p[1] = p[2] = (uint8_t)(yv >= 32u ? 255u : 0u);
    }
  }
  return BK_OK;
}

bk_status bk_image_filter_threshold_64(bk_image *image) {
  uint32_t x;
  uint32_t y;
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  for (y = 0; y < image->height; ++y) {
    uint8_t *row = image->pixels + (size_t)y * image->stride;
    for (x = 0; x < image->width; ++x) {
      uint8_t *p = row + (size_t)x * 4u;
      uint32_t yv = ((uint32_t)p[0] * 299u + (uint32_t)p[1] * 587u +
                     (uint32_t)p[2] * 114u) /
                    1000u;
      p[0] = p[1] = p[2] = (uint8_t)(yv >= 64u ? 255u : 0u);
    }
  }
  return BK_OK;
}

bk_status bk_image_filter_threshold_128(bk_image *image) {
  uint32_t x;
  uint32_t y;
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  for (y = 0; y < image->height; ++y) {
    uint8_t *row = image->pixels + (size_t)y * image->stride;
    for (x = 0; x < image->width; ++x) {
      uint8_t *p = row + (size_t)x * 4u;
      uint32_t yv = ((uint32_t)p[0] * 299u + (uint32_t)p[1] * 587u +
                     (uint32_t)p[2] * 114u) /
                    1000u;
      p[0] = p[1] = p[2] = (uint8_t)(yv >= 128u ? 255u : 0u);
    }
  }
  return BK_OK;
}

bk_status bk_image_filter_threshold_192(bk_image *image) {
  uint32_t x;
  uint32_t y;
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  for (y = 0; y < image->height; ++y) {
    uint8_t *row = image->pixels + (size_t)y * image->stride;
    for (x = 0; x < image->width; ++x) {
      uint8_t *p = row + (size_t)x * 4u;
      uint32_t yv = ((uint32_t)p[0] * 299u + (uint32_t)p[1] * 587u +
                     (uint32_t)p[2] * 114u) /
                    1000u;
      p[0] = p[1] = p[2] = (uint8_t)(yv >= 192u ? 255u : 0u);
    }
  }
  return BK_OK;
}

bk_status bk_image_filter_halve_rgb(bk_image *image) {
  uint32_t x;
  uint32_t y;
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  for (y = 0; y < image->height; ++y) {
    uint8_t *row = image->pixels + (size_t)y * image->stride;
    for (x = 0; x < image->width; ++x) {
      uint8_t *p = row + (size_t)x * 4u;
      p[0] = (uint8_t)(p[0] / 2u);
      p[1] = (uint8_t)(p[1] / 2u);
      p[2] = (uint8_t)(p[2] / 2u);
    }
  }
  return BK_OK;
}

bk_status bk_image_filter_boost_rgb(bk_image *image) {
  uint32_t x;
  uint32_t y;
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  for (y = 0; y < image->height; ++y) {
    uint8_t *row = image->pixels + (size_t)y * image->stride;
    for (x = 0; x < image->width; ++x) {
      uint8_t *p = row + (size_t)x * 4u;
      p[0] = (uint8_t)(p[0] > 205u ? 255u : p[0] + 50u);
      p[1] = (uint8_t)(p[1] > 205u ? 255u : p[1] + 50u);
      p[2] = (uint8_t)(p[2] > 205u ? 255u : p[2] + 50u);
    }
  }
  return BK_OK;
}
