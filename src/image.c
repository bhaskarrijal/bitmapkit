#include "bitmapkit/internal.h"

size_t bk_safe_mul3(size_t a, size_t b, size_t c, int *ok) {
  size_t ab;
  if (ok)
    *ok = 0;
  if (a && b > SIZE_MAX / a)
    return 0;
  ab = a * b;
  if (c && ab > SIZE_MAX / c)
    return 0;
  if (ok)
    *ok = 1;
  return ab * c;
}

bk_status bk_validate_dimensions(uint32_t width, uint32_t height,
                                 const bk_decode_options *options) {
  uint32_t max_w = BK_MAX_DIMENSION;
  uint32_t max_h = BK_MAX_DIMENSION;
  uint32_t max_p = BK_MAX_PIXELS;
  uint64_t pixels;
  if (options) {
    if (options->max_width)
      max_w = options->max_width;
    if (options->max_height)
      max_h = options->max_height;
    if (options->max_pixels)
      max_p = options->max_pixels;
  }
  if (width == 0 || height == 0)
    return BK_ERR_DIMENSIONS;
  if (width > max_w || height > max_h)
    return BK_ERR_LIMIT;
  pixels = (uint64_t)width * (uint64_t)height;
  if (pixels == 0 || pixels > (uint64_t)max_p)
    return BK_ERR_LIMIT;
  return BK_OK;
}

bk_status bk_image_alloc(bk_image *image, uint32_t width, uint32_t height) {
  int ok = 0;
  size_t size;
  if (!image)
    return BK_ERR_ARGUMENT;
  memset(image, 0, sizeof(*image));
  if (bk_validate_dimensions(width, height, NULL) != BK_OK)
    return BK_ERR_DIMENSIONS;
  size = bk_safe_mul3(width, height, 4, &ok);
  if (!ok || size > SIZE_MAX - 16)
    return BK_ERR_LIMIT;
  image->pixels = (uint8_t *)calloc(1, size);
  if (!image->pixels)
    return BK_ERR_OOM;
  image->width = width;
  image->height = height;
  image->stride = width * 4u;
  image->format = BK_PIXEL_RGBA8;
  image->pixel_size = size;
  return BK_OK;
}

void bk_image_free(bk_image *image) {
  if (!image)
    return;
  free(image->pixels);
  memset(image, 0, sizeof(*image));
}

void bk_set_pixel(bk_image *image, uint32_t x, uint32_t y, uint8_t r, uint8_t g,
                  uint8_t b, uint8_t a) {
  uint8_t *p;
  if (!image || !image->pixels || x >= image->width || y >= image->height)
    return;
  p = image->pixels + (size_t)y * image->stride + (size_t)x * 4u;
  p[0] = r;
  p[1] = g;
  p[2] = b;
  p[3] = a;
}

void bk_get_pixel(const bk_image *image, uint32_t x, uint32_t y, uint8_t *r,
                  uint8_t *g, uint8_t *b, uint8_t *a) {
  const uint8_t *p;
  uint8_t zero[4] = {0, 0, 0, 0};
  p = zero;
  if (image && image->pixels && x < image->width && y < image->height) {
    p = image->pixels + (size_t)y * image->stride + (size_t)x * 4u;
  }
  if (r)
    *r = p[0];
  if (g)
    *g = p[1];
  if (b)
    *b = p[2];
  if (a)
    *a = p[3];
}

bk_status bk_image_flip_vertical(bk_image *image) {
  uint8_t *tmp;
  uint32_t y;
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  tmp = (uint8_t *)malloc(image->stride);
  if (!tmp)
    return BK_ERR_OOM;
  for (y = 0; y < image->height / 2u; ++y) {
    uint8_t *a = image->pixels + (size_t)y * image->stride;
    uint8_t *b =
        image->pixels + (size_t)(image->height - 1u - y) * image->stride;
    memcpy(tmp, a, image->stride);
    memcpy(a, b, image->stride);
    memcpy(b, tmp, image->stride);
  }
  free(tmp);
  return BK_OK;
}

bk_status bk_image_to_grayscale(bk_image *image) {
  uint32_t x, y;
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  for (y = 0; y < image->height; ++y) {
    uint8_t *row = image->pixels + (size_t)y * image->stride;
    for (x = 0; x < image->width; ++x) {
      uint8_t *p = row + (size_t)x * 4u;
      uint32_t gray =
          (uint32_t)p[0] * 299u + (uint32_t)p[1] * 587u + (uint32_t)p[2] * 114u;
      gray = (gray + 500u) / 1000u;
      p[0] = p[1] = p[2] = (uint8_t)gray;
    }
  }
  return BK_OK;
}

bk_status bk_image_premultiply_alpha(bk_image *image) {
  uint32_t x, y;
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  for (y = 0; y < image->height; ++y) {
    uint8_t *row = image->pixels + (size_t)y * image->stride;
    for (x = 0; x < image->width; ++x) {
      uint8_t *p = row + (size_t)x * 4u;
      uint32_t a = p[3];
      p[0] = (uint8_t)(((uint32_t)p[0] * a + 127u) / 255u);
      p[1] = (uint8_t)(((uint32_t)p[1] * a + 127u) / 255u);
      p[2] = (uint8_t)(((uint32_t)p[2] * a + 127u) / 255u);
    }
  }
  return BK_OK;
}

bk_status bk_image_unpremultiply_alpha(bk_image *image) {
  uint32_t x, y;
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  for (y = 0; y < image->height; ++y) {
    uint8_t *row = image->pixels + (size_t)y * image->stride;
    for (x = 0; x < image->width; ++x) {
      uint8_t *p = row + (size_t)x * 4u;
      uint32_t a = p[3];
      if (a == 0)
        continue;
      p[0] = (uint8_t)(((uint32_t)p[0] * 255u + a / 2u) / a);
      p[1] = (uint8_t)(((uint32_t)p[1] * 255u + a / 2u) / a);
      p[2] = (uint8_t)(((uint32_t)p[2] * 255u + a / 2u) / a);
    }
  }
  return BK_OK;
}

bk_status bk_image_crop(const bk_image *source, uint32_t x, uint32_t y,
                        uint32_t width, uint32_t height, bk_image *out) {
  uint32_t row;
  bk_status st;
  if (!source || !source->pixels || !out)
    return BK_ERR_ARGUMENT;
  if (x > source->width || y > source->height)
    return BK_ERR_DIMENSIONS;
  if (width == 0 || height == 0)
    return BK_ERR_DIMENSIONS;
  if (width > source->width - x || height > source->height - y)
    return BK_ERR_DIMENSIONS;
  st = bk_image_alloc(out, width, height);
  if (st != BK_OK)
    return st;
  for (row = 0; row < height; ++row) {
    const uint8_t *src =
        source->pixels + (size_t)(y + row) * source->stride + (size_t)x * 4u;
    uint8_t *dst = out->pixels + (size_t)row * out->stride;
    memcpy(dst, src, (size_t)width * 4u);
  }
  out->source_format = source->source_format;
  return BK_OK;
}
