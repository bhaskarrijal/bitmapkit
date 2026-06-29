#include "bitmapkit/internal.h"

typedef struct tiff_reader {
  const uint8_t *data;
  size_t size;
  int le;
} tiff_reader;
typedef struct tiff_tag {
  uint16_t tag, type;
  uint32_t count, value;
} tiff_tag;

static uint16_t tr16(const tiff_reader *r, size_t off) {
  const uint8_t *p = r->data + off;
  return r->le ? (uint16_t)(p[0] | ((uint16_t)p[1] << 8))
               : (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static uint32_t tr32(const tiff_reader *r, size_t off) {
  const uint8_t *p = r->data + off;
  return r->le ? ((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                  ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24))
               : (((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                  ((uint32_t)p[2] << 8) | p[3]);
}
static uint32_t tiff_type_size(uint16_t type) {
  switch (type) {
  case 1:
  case 2:
  case 6:
  case 7:
    return 1;
  case 3:
  case 8:
    return 2;
  case 4:
  case 9:
  case 11:
    return 4;
  case 5:
  case 10:
  case 12:
    return 8;
  default:
    return 0;
  }
}

static bk_status tiff_read_tag(const tiff_reader *r, size_t off,
                               tiff_tag *tag) {
  if (off + 12 > r->size)
    return BK_ERR_TRUNCATED;
  tag->tag = tr16(r, off);
  tag->type = tr16(r, off + 2);
  tag->count = tr32(r, off + 4);
  tag->value = tr32(r, off + 8);
  return BK_OK;
}

static bk_status tiff_value_u32(const tiff_reader *r, const tiff_tag *tag,
                                uint32_t index, uint32_t *out) {
  uint32_t ts = tiff_type_size(tag->type);
  uint64_t total = (uint64_t)ts * tag->count;
  size_t off;
  if (index >= tag->count || ts == 0)
    return BK_ERR_CORRUPT;
  off = total <= 4 ? 0 : tag->value;
  if (total <= 4) {
    uint8_t tmp[4];
    if (r->le) {
      tmp[0] = (uint8_t)(tag->value & 255u);
      tmp[1] = (uint8_t)((tag->value >> 8) & 255u);
      tmp[2] = (uint8_t)((tag->value >> 16) & 255u);
      tmp[3] = (uint8_t)((tag->value >> 24) & 255u);
    } else {
      tmp[0] = (uint8_t)((tag->value >> 24) & 255u);
      tmp[1] = (uint8_t)((tag->value >> 16) & 255u);
      tmp[2] = (uint8_t)((tag->value >> 8) & 255u);
      tmp[3] = (uint8_t)(tag->value & 255u);
    }
    if (tag->type == 3)
      *out = r->le ? (uint32_t)(tmp[index * 2u] |
                                ((uint32_t)tmp[index * 2u + 1u] << 8))
                   : (uint32_t)(((uint32_t)tmp[index * 2u] << 8) |
                                tmp[index * 2u + 1u]);
    else
      *out = tmp[index];
    return BK_OK;
  }
  off += (size_t)index * ts;
  if (off + ts > r->size)
    return BK_ERR_TRUNCATED;
  if (tag->type == 3)
    *out = tr16(r, off);
  else if (tag->type == 4)
    *out = tr32(r, off);
  else
    *out = r->data[off];
  return BK_OK;
}

static int tiff_find(const tiff_tag *tags, uint16_t n, uint16_t id,
                     tiff_tag *out) {
  for (uint16_t i = 0; i < n; ++i)
    if (tags[i].tag == id) {
      *out = tags[i];
      return 1;
    }
  return 0;
}

static bk_status tiff_read_ifd(const tiff_reader *r, uint32_t ifd,
                               tiff_tag *tags, uint16_t *count) {
  size_t base = (size_t)ifd;
  size_t entry_bytes;
  uint16_t n;
  if (base > r->size || r->size - base < 2u)
    return BK_ERR_TRUNCATED;
  n = tr16(r, base);
  if (n > 128)
    return BK_ERR_LIMIT;
  entry_bytes = (size_t)n * 12u;
  if (r->size - base - 2u < entry_bytes)
    return BK_ERR_TRUNCATED;
  for (uint16_t i = 0; i < n; ++i) {
    bk_status st = tiff_read_tag(r, base + 2u + (size_t)i * 12u, &tags[i]);
    if (st != BK_OK)
      return st;
  }
  *count = n;
  return BK_OK;
}

bk_status bk_decode_tiff(const uint8_t *data, size_t size,
                         const bk_decode_context *ctx, bk_image *image,
                         bk_info *info) {
  tiff_reader r;
  tiff_tag tags[128], tag;
  uint16_t tag_count = 0;
  uint32_t ifd, width = 0, height = 0, bps = 1, compression = 1,
                photometric = 1, spp = 1, rows_per_strip = 0;
  bk_status st;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  if (size < 8)
    return BK_ERR_TRUNCATED;
  if (data[0] == 'I' && data[1] == 'I')
    r.le = 1;
  else if (data[0] == 'M' && data[1] == 'M')
    r.le = 0;
  else
    return BK_ERR_BAD_MAGIC;
  r.data = data;
  r.size = size;
  if (tr16(&r, 2) != 42)
    return BK_ERR_BAD_MAGIC;
  ifd = tr32(&r, 4);
  st = tiff_read_ifd(&r, ifd, tags, &tag_count);
  if (st != BK_OK)
    return st;
  if (tiff_find(tags, tag_count, 256, &tag))
    tiff_value_u32(&r, &tag, 0, &width);
  if (tiff_find(tags, tag_count, 257, &tag))
    tiff_value_u32(&r, &tag, 0, &height);
  if (tiff_find(tags, tag_count, 258, &tag))
    tiff_value_u32(&r, &tag, 0, &bps);
  if (tiff_find(tags, tag_count, 259, &tag))
    tiff_value_u32(&r, &tag, 0, &compression);
  if (tiff_find(tags, tag_count, 262, &tag))
    tiff_value_u32(&r, &tag, 0, &photometric);
  if (tiff_find(tags, tag_count, 277, &tag))
    tiff_value_u32(&r, &tag, 0, &spp);
  if (tiff_find(tags, tag_count, 278, &tag))
    tiff_value_u32(&r, &tag, 0, &rows_per_strip);
  if (!rows_per_strip)
    rows_per_strip = height;
  st = bk_validate_dimensions(width, height, &ctx->options);
  if (st != BK_OK)
    return st;
  if (info) {
    info->format = BK_FORMAT_TIFF;
    info->width = width;
    info->height = height;
    info->bits_per_pixel = bps * spp;
    info->frame_count = 1;
    info->indexed = photometric == 3;
    info->has_alpha = spp >= 4;
    bk_metadata_add_u32(&info->metadata, "compression", compression);
    bk_metadata_add_u32(&info->metadata, "photometric", photometric);
    bk_metadata_add_u32(&info->metadata, "samples_per_pixel", spp);
    bk_metadata_add_u32(&info->metadata, "tag_count", tag_count);
  }
  if (ctx->metadata_only)
    return BK_OK;
  if (!image)
    return BK_ERR_ARGUMENT;
  if (!(compression == 1 && bps == 8 && (spp == 1 || spp == 3 || spp == 4)))
    return BK_ERR_UNSUPPORTED;
  if (!tiff_find(tags, tag_count, 273, &tag))
    return BK_ERR_CORRUPT;
  tiff_tag strip_offsets = tag;
  if (!tiff_find(tags, tag_count, 279, &tag))
    return BK_ERR_CORRUPT;
  tiff_tag strip_counts = tag;
  st = bk_image_alloc(image, width, height);
  if (st != BK_OK)
    return st;
  image->source_format = BK_FORMAT_TIFF;
  for (uint32_t s = 0; s < strip_offsets.count; ++s) {
    uint32_t off = 0, bytes = 0;
    uint32_t start_y = s * rows_per_strip;
    uint32_t rows = rows_per_strip;
    tiff_value_u32(&r, &strip_offsets, s, &off);
    tiff_value_u32(&r, &strip_counts, s, &bytes);
    if (start_y >= height)
      break;
    if (rows > height - start_y)
      rows = height - start_y;
    if ((uint64_t)off + bytes > size) {
      bk_image_free(image);
      return BK_ERR_TRUNCATED;
    }
    size_t need = (size_t)rows * width * spp;
    if (bytes < need) {
      bk_image_free(image);
      return BK_ERR_TRUNCATED;
    }
    for (uint32_t y = 0; y < rows; ++y)
      for (uint32_t x = 0; x < width; ++x) {
        const uint8_t *p = data + off + ((size_t)y * width + x) * spp;
        if (spp == 1)
          bk_set_pixel(image, x, start_y + y, p[0], p[0], p[0], 255);
        else
          bk_set_pixel(image, x, start_y + y, p[0], p[1], p[2],
                       spp > 3 ? p[3] : 255);
      }
  }
  return BK_OK;
}
