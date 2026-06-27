#include "bitmapkit/internal.h"
#include <math.h>

typedef struct bk_rgbf {
  double r, g, b;
} bk_rgbf;
typedef struct bk_hsv {
  double h, s, v;
} bk_hsv;
typedef struct bk_xyz {
  double x, y, z;
} bk_xyz;
typedef struct bk_lab {
  double l, a, b;
} bk_lab;

static double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
static uint8_t to_u8(double v) {
  v = clamp01(v);
  return (uint8_t)(v * 255.0 + 0.5);
}
static double srgb_to_linear(double v) {
  return v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
}
static double linear_to_srgb(double v) {
  return v <= 0.0031308 ? v * 12.92 : 1.055 * pow(v, 1.0 / 2.4) - 0.055;
}

bk_rgbf bk_rgbf_from_u8(uint8_t r, uint8_t g, uint8_t b) {
  bk_rgbf c;
  c.r = r / 255.0;
  c.g = g / 255.0;
  c.b = b / 255.0;
  return c;
}

bk_hsv bk_rgb_to_hsv(bk_rgbf c) {
  double max = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
  double min = c.r < c.g ? (c.r < c.b ? c.r : c.b) : (c.g < c.b ? c.g : c.b);
  double d = max - min;
  bk_hsv h;
  h.v = max;
  h.s = max == 0.0 ? 0.0 : d / max;
  if (d == 0.0)
    h.h = 0.0;
  else if (max == c.r)
    h.h = fmod(((c.g - c.b) / d), 6.0) / 6.0;
  else if (max == c.g)
    h.h = (((c.b - c.r) / d) + 2.0) / 6.0;
  else
    h.h = (((c.r - c.g) / d) + 4.0) / 6.0;
  if (h.h < 0.0)
    h.h += 1.0;
  return h;
}

bk_rgbf bk_hsv_to_rgb(bk_hsv h) {
  double hh = h.h * 6.0;
  int i = (int)floor(hh);
  double f = hh - i;
  double p = h.v * (1.0 - h.s);
  double q = h.v * (1.0 - f * h.s);
  double t = h.v * (1.0 - (1.0 - f) * h.s);
  bk_rgbf c;
  switch (i % 6) {
  case 0:
    c.r = h.v;
    c.g = t;
    c.b = p;
    break;
  case 1:
    c.r = q;
    c.g = h.v;
    c.b = p;
    break;
  case 2:
    c.r = p;
    c.g = h.v;
    c.b = t;
    break;
  case 3:
    c.r = p;
    c.g = q;
    c.b = h.v;
    break;
  case 4:
    c.r = t;
    c.g = p;
    c.b = h.v;
    break;
  default:
    c.r = h.v;
    c.g = p;
    c.b = q;
    break;
  }
  return c;
}

bk_xyz bk_rgb_to_xyz(bk_rgbf c) {
  double r = srgb_to_linear(c.r), g = srgb_to_linear(c.g),
         b = srgb_to_linear(c.b);
  bk_xyz x;
  x.x = r * 0.4124564 + g * 0.3575761 + b * 0.1804375;
  x.y = r * 0.2126729 + g * 0.7151522 + b * 0.0721750;
  x.z = r * 0.0193339 + g * 0.1191920 + b * 0.9503041;
  return x;
}

bk_rgbf bk_xyz_to_rgb(bk_xyz x) {
  double r = x.x * 3.2404542 + x.y * -1.5371385 + x.z * -0.4985314;
  double g = x.x * -0.9692660 + x.y * 1.8760108 + x.z * 0.0415560;
  double b = x.x * 0.0556434 + x.y * -0.2040259 + x.z * 1.0572252;
  bk_rgbf c;
  c.r = clamp01(linear_to_srgb(r));
  c.g = clamp01(linear_to_srgb(g));
  c.b = clamp01(linear_to_srgb(b));
  return c;
}

static double lab_f(double t) {
  return t > 0.008856 ? cbrt(t) : (7.787 * t + 16.0 / 116.0);
}
static double lab_inv(double t) {
  double t3 = t * t * t;
  return t3 > 0.008856 ? t3 : (t - 16.0 / 116.0) / 7.787;
}

bk_lab bk_xyz_to_lab(bk_xyz x) {
  double fx = lab_f(x.x / 0.95047);
  double fy = lab_f(x.y / 1.00000);
  double fz = lab_f(x.z / 1.08883);
  bk_lab l;
  l.l = 116.0 * fy - 16.0;
  l.a = 500.0 * (fx - fy);
  l.b = 200.0 * (fy - fz);
  return l;
}

bk_xyz bk_lab_to_xyz(bk_lab l) {
  double fy = (l.l + 16.0) / 116.0;
  double fx = l.a / 500.0 + fy;
  double fz = fy - l.b / 200.0;
  bk_xyz x;
  x.x = 0.95047 * lab_inv(fx);
  x.y = 1.00000 * lab_inv(fy);
  x.z = 1.08883 * lab_inv(fz);
  return x;
}

bk_status bk_image_adjust_hsv(bk_image *image, double hue_turns,
                              double sat_scale, double val_scale) {
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  for (uint32_t y = 0; y < image->height; ++y)
    for (uint32_t x = 0; x < image->width; ++x) {
      uint8_t r, g, b, a;
      bk_get_pixel(image, x, y, &r, &g, &b, &a);
      bk_hsv h = bk_rgb_to_hsv(bk_rgbf_from_u8(r, g, b));
      h.h = h.h + hue_turns;
      h.h -= floor(h.h);
      h.s = clamp01(h.s * sat_scale);
      h.v = clamp01(h.v * val_scale);
      bk_rgbf c = bk_hsv_to_rgb(h);
      bk_set_pixel(image, x, y, to_u8(c.r), to_u8(c.g), to_u8(c.b), a);
    }
  return BK_OK;
}

bk_status bk_image_apply_gamma(bk_image *image, double gamma_value) {
  if (!image || !image->pixels || gamma_value <= 0.0)
    return BK_ERR_ARGUMENT;
  double inv = 1.0 / gamma_value;
  for (uint32_t y = 0; y < image->height; ++y)
    for (uint32_t x = 0; x < image->width; ++x) {
      uint8_t r, g, b, a;
      bk_get_pixel(image, x, y, &r, &g, &b, &a);
      bk_set_pixel(image, x, y, to_u8(pow(r / 255.0, inv)),
                   to_u8(pow(g / 255.0, inv)), to_u8(pow(b / 255.0, inv)), a);
    }
  return BK_OK;
}

bk_status bk_image_color_matrix(bk_image *image, const double m[12]) {
  if (!image || !image->pixels || !m)
    return BK_ERR_ARGUMENT;
  for (uint32_t y = 0; y < image->height; ++y)
    for (uint32_t x = 0; x < image->width; ++x) {
      uint8_t r8, g8, b8, a;
      double r, g, b;
      bk_get_pixel(image, x, y, &r8, &g8, &b8, &a);
      r = m[0] * r8 + m[1] * g8 + m[2] * b8 + m[3];
      g = m[4] * r8 + m[5] * g8 + m[6] * b8 + m[7];
      b = m[8] * r8 + m[9] * g8 + m[10] * b8 + m[11];
      if (r < 0.0)
        r = 0.0;
      if (r > 255.0)
        r = 255.0;
      if (g < 0.0)
        g = 0.0;
      if (g > 255.0)
        g = 255.0;
      if (b < 0.0)
        b = 0.0;
      if (b > 255.0)
        b = 255.0;
      bk_set_pixel(image, x, y, (uint8_t)(r + 0.5), (uint8_t)(g + 0.5),
                   (uint8_t)(b + 0.5), a);
    }
  return BK_OK;
}

bk_status bk_image_desaturate_lab(bk_image *image, double amount) {
  if (!image || !image->pixels)
    return BK_ERR_ARGUMENT;
  amount = clamp01(amount);
  for (uint32_t y = 0; y < image->height; ++y)
    for (uint32_t x = 0; x < image->width; ++x) {
      uint8_t r, g, b, a;
      bk_get_pixel(image, x, y, &r, &g, &b, &a);
      bk_lab l = bk_xyz_to_lab(bk_rgb_to_xyz(bk_rgbf_from_u8(r, g, b)));
      l.a *= (1.0 - amount);
      l.b *= (1.0 - amount);
      bk_rgbf c = bk_xyz_to_rgb(bk_lab_to_xyz(l));
      bk_set_pixel(image, x, y, to_u8(c.r), to_u8(c.g), to_u8(c.b), a);
    }
  return BK_OK;
}
