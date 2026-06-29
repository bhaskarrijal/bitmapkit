#include "bitmapkit/internal.h"

typedef struct bk_bmp_header {
  uint32_t file_size;
  uint32_t pixel_offset;
  uint32_t dib_size;
  int32_t width;
  int32_t height;
  uint16_t planes;
  uint16_t bpp;
  uint32_t compression;
  uint32_t image_size;
  uint32_t xppm;
  uint32_t yppm;
  uint32_t colors_used;
  uint32_t colors_important;
  uint32_t red_mask;
  uint32_t green_mask;
  uint32_t blue_mask;
  uint32_t alpha_mask;
  int top_down;
} bk_bmp_header;

static uint32_t bk_mask_shift(uint32_t mask) {
  uint32_t s = 0;
  if (!mask)
    return 0;
  while ((mask & 1u) == 0u) {
    mask >>= 1;
    ++s;
  }
  return s;
}

static uint8_t bk_scale_mask(uint32_t value, uint32_t mask) {
  uint32_t shift, maxv, raw;
  if (!mask)
    return 0;
  shift = bk_mask_shift(mask);
  raw = (value & mask) >> shift;
  maxv = mask >> shift;
  if (!maxv)
    return 0;
  return (uint8_t)((raw * 255u + maxv / 2u) / maxv);
}

static bk_status bk_bmp_parse_header(bk_reader *r, bk_bmp_header *h) {
  uint16_t magic, reserved1, reserved2;
  int32_t sw, sh;
  memset(h, 0, sizeof(*h));
BK_UNUSED:;
  if (bk_reader_le16(r, &magic) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (magic != 0x4d42u)
    return BK_ERR_BAD_MAGIC;
  if (bk_reader_le32(r, &h->file_size) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le16(r, &reserved1) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le16(r, &reserved2) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (reserved1 || reserved2)
    bk_metadata_warn(r->metadata, BK_NOTE, "bmp-reserved",
                     "reserved file header fields are not zero", r->pos - 4u);
  if (bk_reader_le32(r, &h->pixel_offset) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le32(r, &h->dib_size) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (h->dib_size < 12)
    return BK_ERR_UNSUPPORTED;
  if (h->dib_size == 12) {
    uint16_t w, ht;
    if (bk_reader_le16(r, &w) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (bk_reader_le16(r, &ht) != BK_OK)
      return BK_ERR_TRUNCATED;
    h->width = w;
    h->height = ht;
    if (bk_reader_le16(r, &h->planes) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (bk_reader_le16(r, &h->bpp) != BK_OK)
      return BK_ERR_TRUNCATED;
    h->compression = 0;
  } else {
    if (bk_reader_sle32(r, &sw) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (bk_reader_sle32(r, &sh) != BK_OK)
      return BK_ERR_TRUNCATED;
    h->width = sw;
    h->height = sh;
    if (h->height < 0) {
      if (h->height == INT32_MIN)
        return BK_ERR_DIMENSIONS;
      h->top_down = 1;
      h->height = -h->height;
    }
    if (bk_reader_le16(r, &h->planes) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (bk_reader_le16(r, &h->bpp) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (bk_reader_le32(r, &h->compression) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (bk_reader_le32(r, &h->image_size) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (bk_reader_le32(r, &h->xppm) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (bk_reader_le32(r, &h->yppm) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (bk_reader_le32(r, &h->colors_used) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (bk_reader_le32(r, &h->colors_important) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (h->dib_size >= 52 && (h->compression == 3 || h->compression == 6)) {
      if (bk_reader_le32(r, &h->red_mask) != BK_OK)
        return BK_ERR_TRUNCATED;
      if (bk_reader_le32(r, &h->green_mask) != BK_OK)
        return BK_ERR_TRUNCATED;
      if (bk_reader_le32(r, &h->blue_mask) != BK_OK)
        return BK_ERR_TRUNCATED;
      if (h->dib_size >= 56) {
        if (bk_reader_le32(r, &h->alpha_mask) != BK_OK)
          return BK_ERR_TRUNCATED;
      }
    }
    if (r->pos < 14u + h->dib_size) {
      if (bk_reader_skip(r, 14u + h->dib_size - r->pos) != BK_OK)
        return BK_ERR_TRUNCATED;
    }
  }
  if (h->planes != 1)
    return BK_ERR_UNSUPPORTED;
  if (h->width <= 0 || h->height <= 0)
    return BK_ERR_DIMENSIONS;
  if (!(h->bpp == 1 || h->bpp == 4 || h->bpp == 8 || h->bpp == 16 ||
        h->bpp == 24 || h->bpp == 32))
    return BK_ERR_UNSUPPORTED;
  if (h->compression == 0) {
    if (h->bpp == 16 && !h->red_mask) {
      h->red_mask = 0x7c00u;
      h->green_mask = 0x03e0u;
      h->blue_mask = 0x001fu;
    }
    if (h->bpp == 32 && !h->red_mask) {
      h->red_mask = 0x00ff0000u;
      h->green_mask = 0x0000ff00u;
      h->blue_mask = 0x000000ffu;
      h->alpha_mask = 0xff000000u;
    }
  } else if (h->compression == 3 || h->compression == 6) {
    if (!h->red_mask || !h->green_mask || !h->blue_mask)
      return BK_ERR_CORRUPT;
  } else if (!((h->compression == 1 && h->bpp == 8) ||
               (h->compression == 2 && h->bpp == 4))) {
    return BK_ERR_UNSUPPORTED;
  }
  return BK_OK;
}

static bk_status bk_bmp_read_palette(bk_reader *r, const bk_bmp_header *h,
                                     bk_palette_entry *pal, uint32_t *count) {
  uint32_t n, i;
  size_t entry_size = h->dib_size == 12 ? 3u : 4u;
  uint32_t by_bpp = h->bpp <= 8 ? (1u << h->bpp) : 0u;
  n = h->colors_used ? h->colors_used : by_bpp;
  if (n > 256u)
    return BK_ERR_LIMIT;
  if (h->pixel_offset > r->pos) {
    size_t avail = h->pixel_offset - r->pos;
    uint32_t possible = (uint32_t)(avail / entry_size);
    if (n > possible)
      n = possible;
  }
  for (i = 0; i < n; ++i) {
    uint8_t b, g, red, a = 255;
    if (bk_reader_u8(r, &b) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (bk_reader_u8(r, &g) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (bk_reader_u8(r, &red) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (entry_size == 4 && bk_reader_u8(r, &a) != BK_OK)
      return BK_ERR_TRUNCATED;
    pal[i].r = red;
    pal[i].g = g;
    pal[i].b = b;
    pal[i].a = 255;
    if (a)
      pal[i].a = a;
  }
  *count = n;
  return BK_OK;
}

static size_t bk_bmp_stride(uint32_t width, uint16_t bpp) {
  uint64_t bits = (uint64_t)width * (uint64_t)bpp;
  return (size_t)(((bits + 31u) / 32u) * 4u);
}

static uint8_t bk_bmp_index_at(const uint8_t *row, uint32_t x, uint16_t bpp) {
  if (bpp == 8)
    return row[x];
  if (bpp == 4) {
    uint8_t v = row[x / 2u];
    return (x & 1u) ? (uint8_t)(v & 15u) : (uint8_t)(v >> 4);
  }
  if (bpp == 1) {
    uint8_t v = row[x / 8u];
    return (uint8_t)((v >> (7u - (x & 7u))) & 1u);
  }
  return 0;
}

static bk_status bk_bmp_decode_uncompressed(const uint8_t *data, size_t size,
                                            const bk_bmp_header *h,
                                            const bk_palette_entry *pal,
                                            uint32_t pal_count,
                                            bk_image *image) {
  uint32_t y, x;
  size_t row_stride = bk_bmp_stride((uint32_t)h->width, h->bpp);
  if (h->pixel_offset > size)
    return BK_ERR_TRUNCATED;
  if (row_stride &&
      (uint64_t)row_stride * (uint64_t)h->height > size - h->pixel_offset)
    return BK_ERR_TRUNCATED;
  for (y = 0; y < (uint32_t)h->height; ++y) {
    uint32_t dst_y = h->top_down ? y : (uint32_t)h->height - 1u - y;
    const uint8_t *row = data + h->pixel_offset + (size_t)y * row_stride;
    for (x = 0; x < (uint32_t)h->width; ++x) {
      if (h->bpp <= 8) {
        uint8_t idx = bk_bmp_index_at(row, x, h->bpp);
        bk_palette_entry c = {0, 0, 0, 255};
        if (idx < pal_count)
          c = pal[idx];
        bk_set_pixel(image, x, dst_y, c.r, c.g, c.b, c.a);
      } else if (h->bpp == 24) {
        const uint8_t *p = row + (size_t)x * 3u;
        bk_set_pixel(image, x, dst_y, p[2], p[1], p[0], 255);
      } else if (h->bpp == 16) {
        uint32_t v = (uint32_t)row[(size_t)x * 2u] |
                     ((uint32_t)row[(size_t)x * 2u + 1u] << 8);
        bk_set_pixel(image, x, dst_y, bk_scale_mask(v, h->red_mask),
                     bk_scale_mask(v, h->green_mask),
                     bk_scale_mask(v, h->blue_mask),
                     h->alpha_mask ? bk_scale_mask(v, h->alpha_mask) : 255);
      } else if (h->bpp == 32) {
        uint32_t v = (uint32_t)row[(size_t)x * 4u] |
                     ((uint32_t)row[(size_t)x * 4u + 1u] << 8) |
                     ((uint32_t)row[(size_t)x * 4u + 2u] << 16) |
                     ((uint32_t)row[(size_t)x * 4u + 3u] << 24);
        if (h->red_mask)
          bk_set_pixel(image, x, dst_y, bk_scale_mask(v, h->red_mask),
                       bk_scale_mask(v, h->green_mask),
                       bk_scale_mask(v, h->blue_mask),
                       h->alpha_mask ? bk_scale_mask(v, h->alpha_mask) : 255);
        else
          bk_set_pixel(image, x, dst_y, row[(size_t)x * 4u + 2u],
                       row[(size_t)x * 4u + 1u], row[(size_t)x * 4u],
                       row[(size_t)x * 4u + 3u]);
      }
    }
  }
  return BK_OK;
}

static void bk_bmp_put_index(bk_image *image, const bk_bmp_header *h,
                             const bk_palette_entry *pal, uint32_t pal_count,
                             uint32_t x, uint32_t y, uint8_t idx) {
  uint32_t dst_y;
  bk_palette_entry c = {0, 0, 0, 255};
  if (x >= (uint32_t)h->width || y >= (uint32_t)h->height)
    return;
  dst_y = h->top_down ? y : (uint32_t)h->height - 1u - y;
  if (idx < pal_count)
    c = pal[idx];
  bk_set_pixel(image, x, dst_y, c.r, c.g, c.b, c.a);
}

static bk_status bk_bmp_decode_rle8(const uint8_t *data, size_t size,
                                    const bk_bmp_header *h,
                                    const bk_palette_entry *pal,
                                    uint32_t pal_count, bk_image *image) {
  size_t p = h->pixel_offset;
  uint32_t x = 0, y = 0;
  while (p + 1 < size && y < (uint32_t)h->height) {
    uint8_t a = data[p++], b = data[p++];
    if (a) {
      uint32_t i;
      for (i = 0; i < a; ++i)
        bk_bmp_put_index(image, h, pal, pal_count, x++, y, b);
    } else if (b == 0) {
      x = 0;
      ++y;
    } else if (b == 1) {
      return BK_OK;
    } else if (b == 2) {
      if (size - p < 2u)
        return BK_ERR_TRUNCATED;
      x += data[p++];
      y += data[p++];
    } else {
      uint32_t i;
      size_t bytes = b;
      if (size - p < bytes + (bytes & 1u))
        return BK_ERR_TRUNCATED;
      for (i = 0; i < b; ++i)
        bk_bmp_put_index(image, h, pal, pal_count, x++, y, data[p++]);
      if (b & 1u)
        ++p;
    }
    if (x > (uint32_t)h->width + 4096u)
      return BK_ERR_CORRUPT;
  }
  return BK_OK;
}

static bk_status bk_bmp_decode_rle4(const uint8_t *data, size_t size,
                                    const bk_bmp_header *h,
                                    const bk_palette_entry *pal,
                                    uint32_t pal_count, bk_image *image) {
  size_t p = h->pixel_offset;
  uint32_t x = 0, y = 0;
  while (p + 1 < size && y < (uint32_t)h->height) {
    uint8_t a = data[p++], b = data[p++];
    if (a) {
      uint32_t i;
      for (i = 0; i < a; ++i) {
        uint8_t idx = (i & 1u) ? (uint8_t)(b & 15u) : (uint8_t)(b >> 4);
        bk_bmp_put_index(image, h, pal, pal_count, x++, y, idx);
      }
    } else if (b == 0) {
      x = 0;
      ++y;
    } else if (b == 1) {
      return BK_OK;
    } else if (b == 2) {
      if (size - p < 2u)
        return BK_ERR_TRUNCATED;
      x += data[p++];
      y += data[p++];
    } else {
      uint32_t i;
      size_t bytes = (b + 1u) / 2u;
      if (size - p < bytes + (bytes & 1u))
        return BK_ERR_TRUNCATED;
      for (i = 0; i < b; ++i) {
        uint8_t v = data[p + i / 2u];
        uint8_t idx = (i & 1u) ? (uint8_t)(v & 15u) : (uint8_t)(v >> 4);
        bk_bmp_put_index(image, h, pal, pal_count, x++, y, idx);
      }
      p += bytes;
      if (bytes & 1u)
        ++p;
    }
    if (x > (uint32_t)h->width + 4096u)
      return BK_ERR_CORRUPT;
  }
  return BK_OK;
}

bk_status bk_decode_bmp(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info) {
  bk_reader r;
  bk_bmp_header h;
  bk_palette_entry pal[256];
  uint32_t pal_count = 0;
  bk_status st;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  bk_reader_init(&r, data, size,
                 info ? &info->metadata : (image ? &image->metadata : NULL));
  st = bk_bmp_parse_header(&r, &h);
  if (st != BK_OK)
    return st;
  st = bk_validate_dimensions((uint32_t)h.width, (uint32_t)h.height,
                              &ctx->options);
  if (st != BK_OK)
    return st;
  if (h.bpp <= 8) {
    st = bk_bmp_read_palette(&r, &h, pal, &pal_count);
    if (st != BK_OK)
      return st;
    if (pal_count == 0)
      return BK_ERR_CORRUPT;
  }
  if (info) {
    info->format = BK_FORMAT_BMP;
    info->width = (uint32_t)h.width;
    info->height = (uint32_t)h.height;
    info->bits_per_pixel = h.bpp;
    info->frame_count = 1;
    info->indexed = h.bpp <= 8;
    info->has_alpha = h.alpha_mask != 0 || h.bpp == 32;
    bk_metadata_add_u32(&info->metadata, "dib_size", h.dib_size);
    bk_metadata_add_u32(&info->metadata, "compression", h.compression);
    bk_metadata_add_u32(&info->metadata, "pixel_offset", h.pixel_offset);
  }
  if (ctx->metadata_only)
    return BK_OK;
  if (!image)
    return BK_ERR_ARGUMENT;
  st = bk_image_alloc(image, (uint32_t)h.width, (uint32_t)h.height);
  if (st != BK_OK)
    return st;
  image->source_format = BK_FORMAT_BMP;
  if (h.compression == 0 || h.compression == 3 || h.compression == 6)
    st = bk_bmp_decode_uncompressed(data, size, &h, pal, pal_count, image);
  else if (h.compression == 1)
    st = bk_bmp_decode_rle8(data, size, &h, pal, pal_count, image);
  else if (h.compression == 2)
    st = bk_bmp_decode_rle4(data, size, &h, pal, pal_count, image);
  else
    st = BK_ERR_UNSUPPORTED;
  if (st != BK_OK)
    bk_image_free(image);
  return st;
}
