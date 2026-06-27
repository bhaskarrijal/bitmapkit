#include "bitmapkit/internal.h"

typedef struct bk_pcx_header {
  uint8_t manufacturer;
  uint8_t version;
  uint8_t encoding;
  uint8_t bpp;
  uint16_t xmin, ymin, xmax, ymax;
  uint16_t hdpi, vdpi;
  uint8_t ega_palette[48];
  uint8_t reserved;
  uint8_t planes;
  uint16_t bytes_per_line;
  uint16_t palette_info;
  uint16_t hscreen, vscreen;
} bk_pcx_header;

static bk_status bk_pcx_parse_header(bk_reader *r, bk_pcx_header *h) {
  if (bk_reader_u8(r, &h->manufacturer) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_u8(r, &h->version) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_u8(r, &h->encoding) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_u8(r, &h->bpp) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le16(r, &h->xmin) != BK_OK ||
      bk_reader_le16(r, &h->ymin) != BK_OK ||
      bk_reader_le16(r, &h->xmax) != BK_OK ||
      bk_reader_le16(r, &h->ymax) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le16(r, &h->hdpi) != BK_OK ||
      bk_reader_le16(r, &h->vdpi) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_read(r, h->ega_palette, 48) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_u8(r, &h->reserved) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_u8(r, &h->planes) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le16(r, &h->bytes_per_line) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le16(r, &h->palette_info) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le16(r, &h->hscreen) != BK_OK ||
      bk_reader_le16(r, &h->vscreen) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_skip(r, 54) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (h->manufacturer != 0x0a || h->encoding > 1)
    return BK_ERR_BAD_MAGIC;
  if (!(h->bpp == 1 || h->bpp == 2 || h->bpp == 4 || h->bpp == 8))
    return BK_ERR_UNSUPPORTED;
  if (h->xmax < h->xmin || h->ymax < h->ymin)
    return BK_ERR_DIMENSIONS;
  if (h->planes == 0 || h->planes > 4)
    return BK_ERR_UNSUPPORTED;
  if (h->bytes_per_line == 0)
    return BK_ERR_CORRUPT;
  return BK_OK;
}

static bk_status bk_pcx_read_rle(bk_reader *r, uint8_t *out, size_t n) {
  size_t pos = 0;
  while (pos < n) {
    uint8_t c;
    if (bk_reader_u8(r, &c) != BK_OK)
      return BK_ERR_TRUNCATED;
    if ((c & 0xc0u) == 0xc0u) {
      uint8_t run = (uint8_t)(c & 0x3fu);
      uint8_t value;
      size_t i;
      if (run == 0)
        return BK_ERR_CORRUPT;
      if (bk_reader_u8(r, &value) != BK_OK)
        return BK_ERR_TRUNCATED;
      for (i = 0; i < run && pos < n; ++i)
        out[pos++] = value;
    } else {
      out[pos++] = c;
    }
  }
  return BK_OK;
}

static bk_palette_entry bk_pcx_ega_color(const bk_pcx_header *h, uint8_t idx) {
  bk_palette_entry c;
  size_t p = (size_t)(idx & 15u) * 3u;
  c.r = h->ega_palette[p];
  c.g = h->ega_palette[p + 1u];
  c.b = h->ega_palette[p + 2u];
  c.a = 255;
  return c;
}

static bk_status bk_pcx_vga_palette(const uint8_t *data, size_t size,
                                    bk_palette_entry *pal, uint32_t *count) {
  size_t i, start;
  *count = 0;
  if (size < 769 || data[size - 769] != 12)
    return BK_ERR_BAD_MAGIC;
  start = size - 768;
  for (i = 0; i < 256; ++i) {
    pal[i].r = data[start + i * 3u];
    pal[i].g = data[start + i * 3u + 1u];
    pal[i].b = data[start + i * 3u + 2u];
    pal[i].a = 255;
  }
  *count = 256;
  return BK_OK;
}

static uint8_t bk_pcx_planar_index(const uint8_t *scan, const bk_pcx_header *h,
                                   uint32_t x) {
  if (h->bpp == 8 && h->planes == 1)
    return scan[x];
  if (h->bpp == 1) {
    uint8_t idx = 0;
    uint8_t bit = (uint8_t)(0x80u >> (x & 7u));
    uint32_t byte = x / 8u;
    uint8_t plane;
    for (plane = 0; plane < h->planes; ++plane) {
      const uint8_t *p = scan + (size_t)plane * h->bytes_per_line;
      if (p[byte] & bit)
        idx |= (uint8_t)(1u << plane);
    }
    return idx;
  }
  if (h->bpp == 4 && h->planes == 1) {
    uint8_t v = scan[x / 2u];
    return (x & 1u) ? (uint8_t)(v & 15u) : (uint8_t)(v >> 4);
  }
  return 0;
}

bk_status bk_decode_pcx(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info) {
  bk_reader r;
  bk_pcx_header h;
  bk_palette_entry pal[256];
  uint32_t pal_count = 0;
  uint32_t width, height, y, x;
  uint8_t *scan = NULL;
  bk_status st;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  bk_reader_init(&r, data, size,
                 info ? &info->metadata : (image ? &image->metadata : NULL));
  st = bk_pcx_parse_header(&r, &h);
  if (st != BK_OK)
    return st;
  width = (uint32_t)h.xmax - h.xmin + 1u;
  height = (uint32_t)h.ymax - h.ymin + 1u;
  st = bk_validate_dimensions(width, height, &ctx->options);
  if (st != BK_OK)
    return st;
  if (h.bpp == 8 && h.planes == 1)
    bk_pcx_vga_palette(data, size, pal, &pal_count);
  else {
    uint32_t i;
    for (i = 0; i < 16; ++i)
      pal[i] = bk_pcx_ega_color(&h, (uint8_t)i);
    pal_count = 16;
  }
  if (info) {
    info->format = BK_FORMAT_PCX;
    info->width = width;
    info->height = height;
    info->bits_per_pixel = h.bpp * h.planes;
    info->frame_count = 1;
    info->indexed = h.planes <= 1 || h.bpp == 1;
    info->has_alpha = 0;
    bk_metadata_add_u32(&info->metadata, "version", h.version);
    bk_metadata_add_u32(&info->metadata, "planes", h.planes);
    bk_metadata_add_u32(&info->metadata, "bytes_per_line", h.bytes_per_line);
  }
  if (ctx->metadata_only)
    return BK_OK;
  st = bk_image_alloc(image, width, height);
  if (st != BK_OK)
    return st;
  image->source_format = BK_FORMAT_PCX;
  scan = (uint8_t *)malloc((size_t)h.bytes_per_line * h.planes);
  if (!scan) {
    bk_image_free(image);
    return BK_ERR_OOM;
  }
  for (y = 0; y < height; ++y) {
    st = h.encoding
             ? bk_pcx_read_rle(&r, scan, (size_t)h.bytes_per_line * h.planes)
             : bk_reader_read(&r, scan, (size_t)h.bytes_per_line * h.planes);
    if (st != BK_OK) {
      free(scan);
      bk_image_free(image);
      return st;
    }
    for (x = 0; x < width; ++x) {
      if (h.bpp == 8 && h.planes >= 3) {
        uint8_t red = scan[x];
        uint8_t g = scan[(size_t)h.bytes_per_line + x];
        uint8_t b = scan[(size_t)h.bytes_per_line * 2u + x];
        uint8_t a =
            h.planes == 4 ? scan[(size_t)h.bytes_per_line * 3u + x] : 255;
        bk_set_pixel(image, x, y, red, g, b, a);
      } else {
        uint8_t idx = bk_pcx_planar_index(scan, &h, x);
        bk_palette_entry c =
            idx < pal_count ? pal[idx] : (bk_palette_entry){0, 0, 0, 255};
        bk_set_pixel(image, x, y, c.r, c.g, c.b, c.a);
      }
    }
  }
  free(scan);
  return BK_OK;
}
