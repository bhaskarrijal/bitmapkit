#include "bitmapkit/internal.h"
#include <ctype.h>

static const char *xbm_find_define(const char *text, const char *name) {
  const char *p = text;
  size_t name_len = strlen(name);
  while ((p = strstr(p, "#define")) != NULL) {
    p += 7;
    while (*p == ' ' || *p == '\t')
      ++p;
    if (strncmp(p, name, name_len) == 0 &&
        (p[name_len] == ' ' || p[name_len] == '\t'))
      return p + name_len;
  }
  return NULL;
}

static int xbm_parse_uint(const char *p, uint32_t *out) {
  uint64_t v = 0;
  p = p ? p : "";
  while (*p == ' ' || *p == '\t')
    ++p;
  if (!isdigit((unsigned char)*p))
    return 0;
  while (isdigit((unsigned char)*p)) {
    v = v * 10u + (uint32_t)(*p - '0');
    if (v > UINT32_MAX)
      return 0;
    ++p;
  }
  *out = (uint32_t)v;
  return 1;
}

static int xbm_next_hex(const char **pp, uint8_t *out) {
  const char *p = *pp;
  while (*p && !(p[0] == '0' && (p[1] == 'x' || p[1] == 'X')))
    ++p;
  if (!*p)
    return 0;
  p += 2;
  unsigned v = 0;
  int n = 0;
  while (n < 2 && isxdigit((unsigned char)*p)) {
    char c = *p++;
    v <<= 4;
    if (c >= '0' && c <= '9')
      v |= (unsigned)(c - '0');
    else if (c >= 'a' && c <= 'f')
      v |= (unsigned)(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F')
      v |= (unsigned)(c - 'A' + 10);
    ++n;
  }
  if (n == 0)
    return 0;
  *out = (uint8_t)v;
  *pp = p;
  return 1;
}

bk_status bk_decode_xbm(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info) {
  char *text;
  uint32_t width, height, x, y;
  const char *array_pos;
  bk_status st;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  text = (char *)malloc(size + 1u);
  if (!text)
    return BK_ERR_OOM;
  memcpy(text, data, size);
  text[size] = 0;
  if (!xbm_parse_uint(xbm_find_define(text, "width"), &width) ||
      !xbm_parse_uint(xbm_find_define(text, "height"), &height)) {
    char *wp = strstr(text, "_width");
    char *hp = strstr(text, "_height");
    if (!wp || !hp || !xbm_parse_uint(wp + 6, &width) ||
        !xbm_parse_uint(hp + 7, &height)) {
      free(text);
      return BK_ERR_BAD_MAGIC;
    }
  }
  st = bk_validate_dimensions(width, height, &ctx->options);
  if (st != BK_OK) {
    free(text);
    return st;
  }
  if (info) {
    info->format = BK_FORMAT_XBM;
    info->width = width;
    info->height = height;
    info->bits_per_pixel = 1;
    info->frame_count = 1;
    info->indexed = 1;
  }
  if (ctx->metadata_only) {
    free(text);
    return BK_OK;
  }
  if (!image) {
    free(text);
    return BK_ERR_ARGUMENT;
  }
  st = bk_image_alloc(image, width, height);
  if (st != BK_OK) {
    free(text);
    return st;
  }
  image->source_format = BK_FORMAT_XBM;
  array_pos = strchr(text, '{');
  if (!array_pos) {
    free(text);
    bk_image_free(image);
    return BK_ERR_CORRUPT;
  }
  ++array_pos;
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; x += 8u) {
      uint8_t byte;
      uint32_t bit;
      if (!xbm_next_hex(&array_pos, &byte)) {
        free(text);
        bk_image_free(image);
        return BK_ERR_TRUNCATED;
      }
      for (bit = 0; bit < 8u && x + bit < width; ++bit) {
        uint8_t on = (byte >> bit) & 1u;
        bk_set_pixel(image, x + bit, y, on ? 0 : 255, on ? 0 : 255,
                     on ? 0 : 255, 255);
      }
    }
  }
  free(text);
  return BK_OK;
}
