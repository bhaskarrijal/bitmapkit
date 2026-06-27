#include "bitmapkit/internal.h"

static void bk_pnm_skip_ws_comments(bk_reader *r) {
  int again = 1;
  while (again && r->pos < r->size) {
    again = 0;
    while (r->pos < r->size &&
           (r->data[r->pos] == ' ' || r->data[r->pos] == '\t' ||
            r->data[r->pos] == '\n' || r->data[r->pos] == '\r' ||
            r->data[r->pos] == '\f'))
      r->pos++;
    if (r->pos < r->size && r->data[r->pos] == '#') {
      while (r->pos < r->size && r->data[r->pos] != '\n')
        r->pos++;
      again = 1;
    }
  }
}

static bk_status bk_pnm_token(bk_reader *r, char *out, size_t out_size) {
  size_t n = 0;
  if (!out || out_size == 0)
    return BK_ERR_ARGUMENT;
  out[0] = 0;
  bk_pnm_skip_ws_comments(r);
  if (r->pos >= r->size)
    return BK_ERR_TRUNCATED;
  while (r->pos < r->size) {
    uint8_t c = r->data[r->pos];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
        c == '#')
      break;
    if (n + 1 < out_size)
      out[n++] = (char)c;
    r->pos++;
  }
  out[n] = 0;
  if (n == 0)
    return BK_ERR_CORRUPT;
  return BK_OK;
}

static int bk_parse_u32_token(const char *s, uint32_t *out) {
  uint64_t v = 0;
  size_t i;
  if (!s || !*s)
    return 0;
  for (i = 0; s[i]; ++i) {
    if (s[i] < '0' || s[i] > '9')
      return 0;
    v = v * 10u + (uint32_t)(s[i] - '0');
    if (v > UINT32_MAX)
      return 0;
  }
  *out = (uint32_t)v;
  return 1;
}

static bk_status bk_pnm_read_number(bk_reader *r, uint32_t *out) {
  char token[64];
  bk_status st = bk_pnm_token(r, token, sizeof(token));
  if (st != BK_OK)
    return st;
  if (!bk_parse_u32_token(token, out))
    return BK_ERR_CORRUPT;
  return BK_OK;
}

static uint8_t bk_pnm_scale(uint32_t v, uint32_t maxv) {
  if (maxv == 0)
    return 0;
  if (maxv == 255)
    return (uint8_t)v;
  if (v > maxv)
    v = maxv;
  return (uint8_t)((v * 255u + maxv / 2u) / maxv);
}

static bk_status bk_pnm_decode_plain(bk_reader *r, int kind, uint32_t width,
                                     uint32_t height, uint32_t maxv,
                                     bk_image *image) {
  uint32_t x, y;
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      uint32_t rch = 0, g = 0, b = 0;
      if (kind == 1) {
        uint32_t bit;
        bk_status st = bk_pnm_read_number(r, &bit);
        if (st != BK_OK)
          return st;
        rch = g = b = bit ? 0 : 255;
      } else if (kind == 2) {
        bk_status st = bk_pnm_read_number(r, &rch);
        if (st != BK_OK)
          return st;
        rch = g = b = bk_pnm_scale(rch, maxv);
      } else {
        bk_status st = bk_pnm_read_number(r, &rch);
        if (st != BK_OK)
          return st;
        st = bk_pnm_read_number(r, &g);
        if (st != BK_OK)
          return st;
        st = bk_pnm_read_number(r, &b);
        if (st != BK_OK)
          return st;
        rch = bk_pnm_scale(rch, maxv);
        g = bk_pnm_scale(g, maxv);
        b = bk_pnm_scale(b, maxv);
      }
      bk_set_pixel(image, x, y, (uint8_t)rch, (uint8_t)g, (uint8_t)b, 255);
    }
  }
  return BK_OK;
}

static bk_status bk_pnm_decode_binary(bk_reader *r, int kind, uint32_t width,
                                      uint32_t height, uint32_t maxv,
                                      bk_image *image) {
  uint32_t x, y;
  if (r->pos < r->size && (r->data[r->pos] == ' ' || r->data[r->pos] == '\t' ||
                           r->data[r->pos] == '\n' || r->data[r->pos] == '\r'))
    r->pos++;
  for (y = 0; y < height; ++y) {
    if (kind == 4) {
      uint32_t byte = 0, bit = 8;
      for (x = 0; x < width; ++x) {
        if (bit == 8) {
          if (bk_reader_u8(r, (uint8_t *)&byte) != BK_OK)
            return BK_ERR_TRUNCATED;
          bit = 0;
        }
        bk_set_pixel(image, x, y, (byte & (0x80u >> bit)) ? 0 : 255,
                     (byte & (0x80u >> bit)) ? 0 : 255,
                     (byte & (0x80u >> bit)) ? 0 : 255, 255);
        bit++;
      }
    } else {
      for (x = 0; x < width; ++x) {
        uint32_t rch = 0, g = 0, b = 0;
        uint8_t c;
        if (kind == 5) {
          if (maxv > 255) {
            uint16_t v;
            if (bk_reader_be16(r, &v) != BK_OK)
              return BK_ERR_TRUNCATED;
            rch = v;
          } else {
            if (bk_reader_u8(r, &c) != BK_OK)
              return BK_ERR_TRUNCATED;
            rch = c;
          }
          rch = g = b = bk_pnm_scale(rch, maxv);
        } else {
          if (maxv > 255) {
            uint16_t rv, gv, bv;
            if (bk_reader_be16(r, &rv) != BK_OK ||
                bk_reader_be16(r, &gv) != BK_OK ||
                bk_reader_be16(r, &bv) != BK_OK)
              return BK_ERR_TRUNCATED;
            rch = rv;
            g = gv;
            b = bv;
          } else {
            uint8_t rv, gv, bv;
            if (bk_reader_u8(r, &rv) != BK_OK ||
                bk_reader_u8(r, &gv) != BK_OK || bk_reader_u8(r, &bv) != BK_OK)
              return BK_ERR_TRUNCATED;
            rch = rv;
            g = gv;
            b = bv;
          }
          rch = bk_pnm_scale(rch, maxv);
          g = bk_pnm_scale(g, maxv);
          b = bk_pnm_scale(b, maxv);
        }
        bk_set_pixel(image, x, y, (uint8_t)rch, (uint8_t)g, (uint8_t)b, 255);
      }
    }
  }
  return BK_OK;
}

static bk_status bk_pam_header(bk_reader *r, uint32_t *width, uint32_t *height,
                               uint32_t *depth, uint32_t *maxv, char *tuple,
                               size_t tuple_size) {
  char key[64];
  char value[128];
  *width = *height = *depth = 0;
  *maxv = 255;
  tuple[0] = 0;
  while (r->pos < r->size) {
    bk_status st = bk_pnm_token(r, key, sizeof(key));
    if (st != BK_OK)
      return st;
    if (strcmp(key, "ENDHDR") == 0)
      return BK_OK;
    st = bk_pnm_token(r, value, sizeof(value));
    if (st != BK_OK)
      return st;
    if (strcmp(key, "WIDTH") == 0) {
      if (!bk_parse_u32_token(value, width))
        return BK_ERR_CORRUPT;
    } else if (strcmp(key, "HEIGHT") == 0) {
      if (!bk_parse_u32_token(value, height))
        return BK_ERR_CORRUPT;
    } else if (strcmp(key, "DEPTH") == 0) {
      if (!bk_parse_u32_token(value, depth))
        return BK_ERR_CORRUPT;
    } else if (strcmp(key, "MAXVAL") == 0) {
      if (!bk_parse_u32_token(value, maxv))
        return BK_ERR_CORRUPT;
    } else if (strcmp(key, "TUPLTYPE") == 0) {
      strncpy(tuple, value, tuple_size - 1);
      tuple[tuple_size - 1] = 0;
    }
  }
  return BK_ERR_TRUNCATED;
}

static bk_status bk_pam_decode(bk_reader *r, const bk_decode_context *ctx,
                               bk_image *image, bk_info *info) {
  uint32_t width, height, depth, maxv;
  char tuple[64];
  uint32_t x, y;
  bk_status st =
      bk_pam_header(r, &width, &height, &depth, &maxv, tuple, sizeof(tuple));
  if (st != BK_OK)
    return st;
  if (!(depth == 1 || depth == 2 || depth == 3 || depth == 4))
    return BK_ERR_UNSUPPORTED;
  if (maxv == 0 || maxv > 65535u)
    return BK_ERR_UNSUPPORTED;
  st = bk_validate_dimensions(width, height, &ctx->options);
  if (st != BK_OK)
    return st;
  if (info) {
    info->format = BK_FORMAT_PNM;
    info->width = width;
    info->height = height;
    info->bits_per_pixel = depth * (maxv > 255 ? 16u : 8u);
    info->frame_count = 1;
    info->has_alpha = depth == 2 || depth == 4;
    info->indexed = 0;
    bk_metadata_add(&info->metadata, "tuple", tuple[0] ? tuple : "unknown");
  }
  if (ctx->metadata_only)
    return BK_OK;
  st = bk_image_alloc(image, width, height);
  if (st != BK_OK)
    return st;
  image->source_format = BK_FORMAT_PNM;
  if (r->pos < r->size && (r->data[r->pos] == '\n' || r->data[r->pos] == '\r'))
    r->pos++;
  for (y = 0; y < height; ++y)
    for (x = 0; x < width; ++x) {
      uint32_t comp[4] = {0, 0, 0, maxv};
      uint32_t i;
      for (i = 0; i < depth; ++i) {
        if (maxv > 255) {
          uint16_t v;
          if (bk_reader_be16(r, &v) != BK_OK) {
            bk_image_free(image);
            return BK_ERR_TRUNCATED;
          }
          comp[i] = v;
        } else {
          uint8_t v;
          if (bk_reader_u8(r, &v) != BK_OK) {
            bk_image_free(image);
            return BK_ERR_TRUNCATED;
          }
          comp[i] = v;
        }
      }
      if (depth == 1)
        bk_set_pixel(image, x, y, bk_pnm_scale(comp[0], maxv),
                     bk_pnm_scale(comp[0], maxv), bk_pnm_scale(comp[0], maxv),
                     255);
      else if (depth == 2)
        bk_set_pixel(image, x, y, bk_pnm_scale(comp[0], maxv),
                     bk_pnm_scale(comp[0], maxv), bk_pnm_scale(comp[0], maxv),
                     bk_pnm_scale(comp[1], maxv));
      else
        bk_set_pixel(image, x, y, bk_pnm_scale(comp[0], maxv),
                     bk_pnm_scale(comp[1], maxv), bk_pnm_scale(comp[2], maxv),
                     depth == 4 ? bk_pnm_scale(comp[3], maxv) : 255);
    }
  return BK_OK;
}

bk_status bk_decode_pnm(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info) {
  bk_reader r;
  char magic[8];
  uint32_t width, height, maxv = 1;
  int kind;
  bk_status st;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  bk_reader_init(&r, data, size,
                 info ? &info->metadata : (image ? &image->metadata : NULL));
  st = bk_pnm_token(&r, magic, sizeof(magic));
  if (st != BK_OK)
    return st;
  if (magic[0] != 'P' || magic[1] < '1' || magic[1] > '7' || magic[2])
    return BK_ERR_BAD_MAGIC;
  kind = magic[1] - '0';
  if (kind == 7)
    return bk_pam_decode(&r, ctx, image, info);
  st = bk_pnm_read_number(&r, &width);
  if (st != BK_OK)
    return st;
  st = bk_pnm_read_number(&r, &height);
  if (st != BK_OK)
    return st;
  if (kind != 1 && kind != 4) {
    st = bk_pnm_read_number(&r, &maxv);
    if (st != BK_OK)
      return st;
    if (maxv == 0 || maxv > 65535u)
      return BK_ERR_UNSUPPORTED;
  }
  st = bk_validate_dimensions(width, height, &ctx->options);
  if (st != BK_OK)
    return st;
  if (info) {
    info->format = BK_FORMAT_PNM;
    info->width = width;
    info->height = height;
    info->bits_per_pixel = (kind == 3 || kind == 6) ? 24 : 8;
    info->frame_count = 1;
    info->indexed = 0;
    info->has_alpha = 0;
    bk_metadata_add_u32(&info->metadata, "pnm_kind", (uint32_t)kind);
    bk_metadata_add_u32(&info->metadata, "maxval", maxv);
  }
  if (ctx->metadata_only)
    return BK_OK;
  st = bk_image_alloc(image, width, height);
  if (st != BK_OK)
    return st;
  image->source_format = BK_FORMAT_PNM;
  st = (kind <= 3) ? bk_pnm_decode_plain(&r, kind, width, height, maxv, image)
                   : bk_pnm_decode_binary(&r, kind, width, height, maxv, image);
  if (st != BK_OK)
    bk_image_free(image);
  return st;
}
