#include "bitmapkit/internal.h"
#include "bitmapkit/ops.h"

static int clamp_i(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}
static double cubic_weight(double x) {
  double a = -0.5;
  if (x < 0)
    x = -x;
  if (x <= 1.0)
    return (a + 2.0) * x * x * x - (a + 3.0) * x * x + 1.0;
  if (x < 2.0)
    return a * x * x * x - 5.0 * a * x * x + 8.0 * a * x - 4.0 * a;
  return 0.0;
}
static uint8_t clamp_u8d(double v) {
  if (v < 0.0)
    return 0;
  if (v > 255.0)
    return 255;
  return (uint8_t)(v + 0.5);
}

bk_status bk_image_resize_bicubic(const bk_image *src, uint32_t width,
                                  uint32_t height, bk_image *out) {
  bk_status st;
  if (!src || !src->pixels || !out)
    return BK_ERR_ARGUMENT;
  st = bk_image_alloc(out, width, height);
  if (st != BK_OK)
    return st;
  for (uint32_t y = 0; y < height; ++y) {
    double gy = ((double)y + 0.5) * src->height / height - 0.5;
    int ybase = (int)gy;
    for (uint32_t x = 0; x < width; ++x) {
      double gx = ((double)x + 0.5) * src->width / width - 0.5;
      int xbase = (int)gx;
      double acc[4] = {0, 0, 0, 0};
      double wsum = 0.0;
      for (int yy = -1; yy <= 2; ++yy)
        for (int xx = -1; xx <= 2; ++xx) {
          int sx = clamp_i(xbase + xx, 0, (int)src->width - 1);
          int sy = clamp_i(ybase + yy, 0, (int)src->height - 1);
          double w =
              cubic_weight(gx - (xbase + xx)) * cubic_weight(gy - (ybase + yy));
          uint8_t r, g, b, a;
          bk_get_pixel(src, (uint32_t)sx, (uint32_t)sy, &r, &g, &b, &a);
          acc[0] += r * w;
          acc[1] += g * w;
          acc[2] += b * w;
          acc[3] += a * w;
          wsum += w;
        }
      if (wsum == 0.0)
        wsum = 1.0;
      bk_set_pixel(out, x, y, clamp_u8d(acc[0] / wsum),
                   clamp_u8d(acc[1] / wsum), clamp_u8d(acc[2] / wsum),
                   clamp_u8d(acc[3] / wsum));
    }
  }
  return BK_OK;
}

bk_status bk_image_convolve3(const bk_image *src, const int kernel[9],
                             int divisor, int bias, bk_image *out) {
  bk_status st;
  if (!src || !src->pixels || !kernel || !out)
    return BK_ERR_ARGUMENT;
  if (divisor == 0)
    divisor = 1;
  st = bk_image_alloc(out, src->width, src->height);
  if (st != BK_OK)
    return st;
  for (uint32_t y = 0; y < src->height; ++y)
    for (uint32_t x = 0; x < src->width; ++x) {
      int acc[4] = {0, 0, 0, 0};
      for (int ky = -1; ky <= 1; ++ky)
        for (int kx = -1; kx <= 1; ++kx) {
          int sx = clamp_i((int)x + kx, 0, (int)src->width - 1);
          int sy = clamp_i((int)y + ky, 0, (int)src->height - 1);
          int kw = kernel[(ky + 1) * 3 + (kx + 1)];
          uint8_t r, g, b, a;
          bk_get_pixel(src, (uint32_t)sx, (uint32_t)sy, &r, &g, &b, &a);
          acc[0] += r * kw;
          acc[1] += g * kw;
          acc[2] += b * kw;
          acc[3] += a * kw;
        }
      bk_set_pixel(out, x, y, clamp_u8d(acc[0] / (double)divisor + bias),
                   clamp_u8d(acc[1] / (double)divisor + bias),
                   clamp_u8d(acc[2] / (double)divisor + bias),
                   clamp_u8d(acc[3] / (double)divisor));
    }
  return BK_OK;
}

bk_status bk_image_blur3(const bk_image *src, bk_image *out) {
  static const int k[9] = {1, 2, 1, 2, 4, 2, 1, 2, 1};
  return bk_image_convolve3(src, k, 16, 0, out);
}
bk_status bk_image_sharpen3(const bk_image *src, bk_image *out) {
  static const int k[9] = {0, -1, 0, -1, 5, -1, 0, -1, 0};
  return bk_image_convolve3(src, k, 1, 0, out);
}
bk_status bk_image_edge3(const bk_image *src, bk_image *out) {
  static const int k[9] = {-1, -1, -1, -1, 8, -1, -1, -1, -1};
  return bk_image_convolve3(src, k, 1, 128, out);
}

bk_status bk_image_build_mipmap(const bk_image *src, bk_image *out) {
  uint32_t width, height;
  if (!src || !src->pixels || !out)
    return BK_ERR_ARGUMENT;
  width = src->width > 1 ? src->width / 2u : 1u;
  height = src->height > 1 ? src->height / 2u : 1u;
  return bk_image_resize_bilinear(src, width, height, out);
}

bk_status bk_image_alpha_bounds(const bk_image *image, uint8_t threshold,
                                uint32_t *x0, uint32_t *y0, uint32_t *x1,
                                uint32_t *y1) {
  uint32_t minx, miny, maxx, maxy;
  int found = 0;
  if (!image || !image->pixels || !x0 || !y0 || !x1 || !y1)
    return BK_ERR_ARGUMENT;
  minx = image->width;
  miny = image->height;
  maxx = 0;
  maxy = 0;
  for (uint32_t y = 0; y < image->height; ++y)
    for (uint32_t x = 0; x < image->width; ++x) {
      uint8_t a;
      bk_get_pixel(image, x, y, NULL, NULL, NULL, &a);
      if (a >= threshold) {
        if (x < minx)
          minx = x;
        if (y < miny)
          miny = y;
        if (x > maxx)
          maxx = x;
        if (y > maxy)
          maxy = y;
        found = 1;
      }
    }
  if (!found) {
    *x0 = *y0 = *x1 = *y1 = 0;
    return BK_OK;
  }
  *x0 = minx;
  *y0 = miny;
  *x1 = maxx + 1u;
  *y1 = maxy + 1u;
  return BK_OK;
}
