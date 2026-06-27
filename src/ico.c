#include "bitmapkit/internal.h"

typedef struct bk_ico_entry {
  uint32_t width;
  uint32_t height;
  uint8_t color_count;
  uint8_t reserved;
  uint16_t planes;
  uint16_t bit_count;
  uint32_t bytes_in_res;
  uint32_t image_offset;
} bk_ico_entry;

static bk_status bk_ico_read_entry(bk_reader *r, int cursor, bk_ico_entry *e) {
  uint8_t w, h;
  if (bk_reader_u8(r, &w) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_u8(r, &h) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_u8(r, &e->color_count) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_u8(r, &e->reserved) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le16(r, &e->planes) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le16(r, &e->bit_count) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le32(r, &e->bytes_in_res) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (bk_reader_le32(r, &e->image_offset) != BK_OK)
    return BK_ERR_TRUNCATED;
  e->width = w ? w : 256u;
  e->height = h ? h : 256u;
  if (cursor) {
    uint16_t tmp = e->planes;
    e->planes = e->bit_count;
    e->bit_count = tmp;
  }
  if (e->reserved != 0)
    return BK_ERR_CORRUPT;
  if (e->width == 0 || e->height == 0)
    return BK_ERR_DIMENSIONS;
  return BK_OK;
}

static void bk_ico_entry_score(const bk_ico_entry *e, uint32_t *score) {
  uint32_t s = e->width * e->height;
  if (e->bit_count >= 32)
    s += 1000000u;
  else if (e->bit_count >= 24)
    s += 500000u;
  else if (e->bit_count >= 8)
    s += 100000u;
  *score = s;
}

static bk_status bk_ico_wrap_bmp(const uint8_t *payload, size_t payload_size,
                                 uint8_t **out, size_t *out_size) {
  bk_buffer b;
  uint32_t file_size;
  if (!payload || !out || !out_size)
    return BK_ERR_ARGUMENT;
  if (payload_size > UINT32_MAX - 14u)
    return BK_ERR_LIMIT;
  if (bk_buffer_init(&b, payload_size + 14u) != BK_OK)
    return BK_ERR_OOM;
  file_size = (uint32_t)payload_size + 14u;
  bk_buffer_append_u8(&b, 'B');
  bk_buffer_append_u8(&b, 'M');
  bk_buffer_append_le32(&b, file_size);
  bk_buffer_append_le16(&b, 0);
  bk_buffer_append_le16(&b, 0);
  bk_buffer_append_le32(&b, 14u + 40u);
  bk_buffer_append(&b, payload, payload_size);
  *out = b.data;
  *out_size = b.size;
  return BK_OK;
}

static bk_status bk_ico_decode_embedded_bmp(const uint8_t *payload,
                                            size_t payload_size,
                                            const bk_decode_context *ctx,
                                            bk_image *image, bk_info *info) {
  uint8_t *wrapped = NULL;
  size_t wrapped_size = 0;
  bk_status st;
  st = bk_ico_wrap_bmp(payload, payload_size, &wrapped, &wrapped_size);
  if (st != BK_OK)
    return st;
  st = bk_decode_bmp(wrapped, wrapped_size, ctx, image, info);
  free(wrapped);
  if (st == BK_OK && image && image->height >= 2) {
    image->height /= 2u;
    image->pixel_size = (size_t)image->stride * image->height;
  }
  return st;
}

static int bk_ico_payload_is_png(const uint8_t *p, size_t n) {
  static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  return n >= 8 && memcmp(p, sig, 8) == 0;
}

bk_status bk_decode_ico(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info) {
  bk_reader r;
  uint16_t reserved, type, count;
  bk_ico_entry entries[256];
  uint32_t best_score = 0;
  uint16_t best = 0;
  uint16_t i;
  bk_status st;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  bk_reader_init(&r, data, size,
                 info ? &info->metadata : (image ? &image->metadata : NULL));
  if (bk_reader_le16(&r, &reserved) != BK_OK ||
      bk_reader_le16(&r, &type) != BK_OK || bk_reader_le16(&r, &count) != BK_OK)
    return BK_ERR_TRUNCATED;
  if (reserved != 0 || !(type == 1 || type == 2))
    return BK_ERR_BAD_MAGIC;
  if (count == 0 || count > 256)
    return BK_ERR_LIMIT;
  for (i = 0; i < count; ++i) {
    uint32_t score;
    st = bk_ico_read_entry(&r, type == 2, &entries[i]);
    if (st != BK_OK)
      return st;
    if ((uint64_t)entries[i].image_offset + entries[i].bytes_in_res > size)
      return BK_ERR_TRUNCATED;
    bk_ico_entry_score(&entries[i], &score);
    if (score > best_score) {
      best_score = score;
      best = i;
    }
  }
  if (info) {
    const bk_ico_entry *e = &entries[best];
    info->format = type == 1 ? BK_FORMAT_ICO : BK_FORMAT_CUR;
    info->width = e->width;
    info->height = e->height;
    info->bits_per_pixel = e->bit_count;
    info->frame_count = count;
    info->indexed = e->bit_count <= 8;
    info->has_alpha = e->bit_count >= 32;
    bk_metadata_add_u32(&info->metadata, "selected_entry", best);
    bk_metadata_add_u32(&info->metadata, "entry_bytes", e->bytes_in_res);
  }
  if (ctx->metadata_only)
    return BK_OK;
  if (!image)
    return BK_ERR_ARGUMENT;
  if (bk_ico_payload_is_png(data + entries[best].image_offset,
                            entries[best].bytes_in_res)) {
    return BK_ERR_UNSUPPORTED;
  }
  st = bk_ico_decode_embedded_bmp(data + entries[best].image_offset,
                                  entries[best].bytes_in_res, ctx, image, NULL);
  if (st == BK_OK)
    image->source_format = type == 1 ? BK_FORMAT_ICO : BK_FORMAT_CUR;
  return st;
}
