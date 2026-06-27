#include "bitmapkit/internal.h"

typedef struct bk_tga_header {
  uint8_t id_length;
  uint8_t color_map_type;
  uint8_t image_type;
  uint16_t cmap_first;
  uint16_t cmap_length;
  uint8_t cmap_depth;
  uint16_t x_origin;
  uint16_t y_origin;
  uint16_t width;
  uint16_t height;
  uint8_t depth;
  uint8_t descriptor;
} bk_tga_header;

static bk_status bk_tga_parse_header(bk_reader *r, bk_tga_header *h) {
  if (bk_reader_u8(r, &h->id_length) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_u8(r, &h->color_map_type) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_u8(r, &h->image_type) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le16(r, &h->cmap_first) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le16(r, &h->cmap_length) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_u8(r, &h->cmap_depth) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le16(r, &h->x_origin) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le16(r, &h->y_origin) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le16(r, &h->width) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le16(r, &h->height) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_u8(r, &h->depth) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_u8(r, &h->descriptor) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (h->width == 0 || h->height == 0)
    return BK_ERR_DIMENSIONS;
  if (!(h->image_type == 1 || h->image_type == 2 || h->image_type == 3 ||
        h->image_type == 9 || h->image_type == 10 || h->image_type == 11))
    return BK_ERR_UNSUPPORTED;
  if (!(h->depth == 8 || h->depth == 15 || h->depth == 16 || h->depth == 24 ||
        h->depth == 32))
    return BK_ERR_UNSUPPORTED;
  if (h->color_map_type > 1)
    return BK_ERR_UNSUPPORTED;
  return BK_OK;
}

static bk_status bk_tga_read_color(bk_reader *r, uint8_t depth,
                                   bk_palette_entry *out) {
  uint8_t b, g, red, a = 255;
  if (depth == 15 || depth == 16) {
    uint16_t v;
    if (bk_reader_le16(r, &v) != BK_OK)
      return BK_ERR_TRUNCATED;
    out->r = (uint8_t)((((v >> 10) & 31u) * 255u + 15u) / 31u);
    out->g = (uint8_t)((((v >> 5) & 31u) * 255u + 15u) / 31u);
    out->b = (uint8_t)(((v & 31u) * 255u + 15u) / 31u);
    out->a = (depth == 16 && (v & 0x8000u)) ? 255u : 255u;
    return BK_OK;
  }
  if (depth == 24 || depth == 32) {
    if (bk_reader_u8(r, &b) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (bk_reader_u8(r, &g) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (bk_reader_u8(r, &red) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (depth == 32 && bk_reader_u8(r, &a) != BK_OK)
      return BK_ERR_TRUNCATED;
    out->r = red;
    out->g = g;
    out->b = b;
    out->a = a;
    return BK_OK;
  }
  if (depth == 8) {
    uint8_t y;
    if (bk_reader_u8(r, &y) != BK_OK)
      return BK_ERR_TRUNCATED;
    out->r = out->g = out->b = y;
    out->a = 255;
    return BK_OK;
  }
  return BK_ERR_UNSUPPORTED;
}

static bk_status bk_tga_read_palette(bk_reader *r, const bk_tga_header *h,
                                     bk_palette_entry *pal, uint32_t *count) {
  uint32_t i;
  *count = 0;
  if (!h->color_map_type)
    return BK_OK;
  if (h->cmap_length > 256u || h->cmap_first > 255u)
    return BK_ERR_LIMIT;
  for (i = 0; i < h->cmap_first + h->cmap_length && i < 256u; ++i)
    pal[i].a = 0;
  for (i = 0; i < h->cmap_length; ++i) {
    uint32_t idx = h->cmap_first + i;
    bk_palette_entry c;
    bk_status st = bk_tga_read_color(r, h->cmap_depth, &c);
    if (st != BK_OK)
      return st;
    if (idx < 256u)
      pal[idx] = c;
  }
  *count = h->cmap_first + h->cmap_length;
  return BK_OK;
}

static void bk_tga_put(bk_image *image, const bk_tga_header *h, uint32_t index,
                       bk_palette_entry c) {
  uint32_t x = index % h->width;
  uint32_t y = index / h->width;
  int top = (h->descriptor & 0x20u) != 0;
  int right = (h->descriptor & 0x10u) != 0;
  uint32_t dst_x = right ? (uint32_t)h->width - 1u - x : x;
  uint32_t dst_y = top ? y : (uint32_t)h->height - 1u - y;
  bk_set_pixel(image, dst_x, dst_y, c.r, c.g, c.b, c.a);
}

static bk_status bk_tga_read_pixel(bk_reader *r, const bk_tga_header *h,
                                   const bk_palette_entry *pal,
                                   uint32_t pal_count, bk_palette_entry *c) {
  if (h->image_type == 1 || h->image_type == 9) {
    uint8_t idx;
    if (h->depth != 8)
      return BK_ERR_UNSUPPORTED;
    if (bk_reader_u8(r, &idx) != BK_OK)
      return BK_ERR_TRUNCATED;
    if (idx >= pal_count) {
      c->r = c->g = c->b = 0;
      c->a = 255;
      return BK_OK;
    }
    *c = pal[idx];
    return BK_OK;
  }
  if (h->image_type == 3 || h->image_type == 11) {
    uint8_t y;
    if (h->depth != 8)
      return BK_ERR_UNSUPPORTED;
    if (bk_reader_u8(r, &y) != BK_OK)
      return BK_ERR_TRUNCATED;
    c->r = c->g = c->b = y;
    c->a = 255;
    return BK_OK;
  }
  return bk_tga_read_color(r, h->depth, c);
}

static bk_status bk_tga_decode_raw(bk_reader *r, const bk_tga_header *h,
                                   const bk_palette_entry *pal,
                                   uint32_t pal_count, bk_image *image) {
  uint32_t total = (uint32_t)h->width * (uint32_t)h->height;
  uint32_t i;
  for (i = 0; i < total; ++i) {
    bk_palette_entry c;
    bk_status st = bk_tga_read_pixel(r, h, pal, pal_count, &c);
    if (st != BK_OK)
      return st;
    bk_tga_put(image, h, i, c);
  }
  return BK_OK;
}

static bk_status bk_tga_decode_rle(bk_reader *r, const bk_tga_header *h,
                                   const bk_palette_entry *pal,
                                   uint32_t pal_count, bk_image *image) {
  uint32_t total = (uint32_t)h->width * (uint32_t)h->height;
  uint32_t written = 0;
  while (written < total) {
    uint8_t packet;
    uint32_t count, i;
    bk_palette_entry c;
    if (bk_reader_u8(r, &packet) != BK_OK)
      return BK_ERR_TRUNCATED;
    count = (packet & 0x7fu) + 1u;
    if (count > total - written)
      return BK_ERR_CORRUPT;
    if (packet & 0x80u) {
      bk_status st = bk_tga_read_pixel(r, h, pal, pal_count, &c);
      if (st != BK_OK)
        return st;
      for (i = 0; i < count; ++i)
        bk_tga_put(image, h, written++, c);
    } else {
      for (i = 0; i < count; ++i) {
        bk_status st = bk_tga_read_pixel(r, h, pal, pal_count, &c);
        if (st != BK_OK)
          return st;
        bk_tga_put(image, h, written++, c);
      }
    }
  }
  return BK_OK;
}

bk_status bk_decode_tga(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info) {
  bk_reader r;
  bk_tga_header h;
  bk_palette_entry pal[256];
  uint32_t pal_count = 0;
  bk_status st;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  bk_reader_init(&r, data, size,
                 info ? &info->metadata : (image ? &image->metadata : NULL));
  st = bk_tga_parse_header(&r, &h);
  if (st != BK_OK)
    return st;
  st = bk_validate_dimensions(h.width, h.height, &ctx->options);
  if (st != BK_OK)
    return st;
  if (h.id_length && bk_reader_skip(&r, h.id_length) != BK_OK)
    return BK_ERR_TRUNCATED;
  st = bk_tga_read_palette(&r, &h, pal, &pal_count);
  if (st != BK_OK)
    return st;
  if (info) {
    info->format = BK_FORMAT_TGA;
    info->width = h.width;
    info->height = h.height;
    info->bits_per_pixel = h.depth;
    info->frame_count = 1;
    info->indexed = h.color_map_type != 0;
    info->has_alpha = h.depth == 32 || (h.descriptor & 15u) != 0;
    bk_metadata_add_u32(&info->metadata, "image_type", h.image_type);
    bk_metadata_add_u32(&info->metadata, "descriptor", h.descriptor);
    bk_metadata_add_u32(&info->metadata, "color_map_entries", h.cmap_length);
  }
  if (ctx->metadata_only)
    return BK_OK;
  if (!image)
    return BK_ERR_ARGUMENT;
  st = bk_image_alloc(image, h.width, h.height);
  if (st != BK_OK)
    return st;
  image->source_format = BK_FORMAT_TGA;
  if (h.image_type == 9 || h.image_type == 10 || h.image_type == 11)
    st = bk_tga_decode_rle(&r, &h, pal, pal_count, image);
  else
    st = bk_tga_decode_raw(&r, &h, pal, pal_count, image);
  if (st != BK_OK)
    bk_image_free(image);
  return st;
}
