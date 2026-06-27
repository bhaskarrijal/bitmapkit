#include "bitmapkit/internal.h"
#include "bitmapkit/ops.h"

typedef struct gif_lzw {
  const uint8_t *data;
  size_t size;
  size_t pos;
  uint32_t bitbuf;
  uint32_t bitcount;
  uint16_t prefix[4096];
  uint8_t suffix[4096];
  uint8_t stack[4096];
  uint16_t clear_code;
  uint16_t end_code;
  uint16_t next_code;
  uint16_t code_size;
  uint16_t min_code_size;
  uint16_t old_code;
  uint8_t first_char;
  int old_valid;
} gif_lzw;

static uint16_t rd16(const uint8_t *p) {
  return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static bk_status gif_read_subblocks(const uint8_t *data, size_t size,
                                    size_t *pos, uint8_t **out,
                                    size_t *out_size) {
  bk_buffer b;
  bk_status st;
  st = bk_buffer_init(&b, 256);
  if (st != BK_OK)
    return st;
  while (*pos < size) {
    uint8_t n = data[(*pos)++];
    if (n == 0) {
      *out = b.data;
      *out_size = b.size;
      return BK_OK;
    }
    if (*pos + n > size) {
      bk_buffer_free(&b);
      return BK_ERR_TRUNCATED;
    }
    st = bk_buffer_append(&b, data + *pos, n);
    if (st != BK_OK) {
      bk_buffer_free(&b);
      return st;
    }
    *pos += n;
  }
  bk_buffer_free(&b);
  return BK_ERR_TRUNCATED;
}

static void gif_lzw_init(gif_lzw *lzw, const uint8_t *data, size_t size,
                         uint8_t min_code_size) {
  uint16_t i;
  memset(lzw, 0, sizeof(*lzw));
  lzw->data = data;
  lzw->size = size;
  lzw->min_code_size = min_code_size;
  lzw->clear_code = (uint16_t)(1u << min_code_size);
  lzw->end_code = lzw->clear_code + 1u;
  lzw->next_code = lzw->end_code + 1u;
  lzw->code_size = min_code_size + 1u;
  lzw->old_code = 0;
  lzw->first_char = 0;
  lzw->old_valid = 0;
  for (i = 0; i < lzw->clear_code; ++i) {
    lzw->prefix[i] = 0xffffu;
    lzw->suffix[i] = (uint8_t)i;
  }
}

static bk_status gif_lzw_code(gif_lzw *lzw, uint16_t *code) {
  while (lzw->bitcount < lzw->code_size) {
    if (lzw->pos >= lzw->size)
      return BK_ERR_TRUNCATED;
    lzw->bitbuf |= (uint32_t)lzw->data[lzw->pos++] << lzw->bitcount;
    lzw->bitcount += 8u;
  }
  *code = (uint16_t)(lzw->bitbuf & ((1u << lzw->code_size) - 1u));
  lzw->bitbuf >>= lzw->code_size;
  lzw->bitcount -= lzw->code_size;
  return BK_OK;
}

static bk_status gif_lzw_emit(gif_lzw *lzw, uint16_t code, uint8_t *out,
                              size_t out_cap, size_t *out_pos, uint8_t *first) {
  uint16_t depth = 0;
  uint16_t cur = code;
  while (cur >= lzw->clear_code) {
    if (cur >= 4096u || depth >= 4096u)
      return BK_ERR_CORRUPT;
    lzw->stack[depth++] = lzw->suffix[cur];
    cur = lzw->prefix[cur];
    if (cur == 0xffffu)
      return BK_ERR_CORRUPT;
  }
  if (cur >= lzw->clear_code)
    return BK_ERR_CORRUPT;
  lzw->stack[depth++] = (uint8_t)cur;
  *first = (uint8_t)cur;
  while (depth) {
    if (*out_pos >= out_cap)
      return BK_ERR_LIMIT;
    out[(*out_pos)++] = lzw->stack[--depth];
  }
  return BK_OK;
}

static bk_status gif_lzw_decode(const uint8_t *data, size_t size,
                                uint8_t min_code_size, uint8_t *out,
                                size_t out_cap, size_t *written) {
  gif_lzw lzw;
  bk_status st;
  if (min_code_size < 2 || min_code_size > 8)
    return BK_ERR_CORRUPT;
  gif_lzw_init(&lzw, data, size, min_code_size);
  *written = 0;
  for (;;) {
    uint16_t code;
    uint8_t first = 0;
    st = gif_lzw_code(&lzw, &code);
    if (st != BK_OK)
      return st;
    if (code == lzw.clear_code) {
      lzw.next_code = lzw.end_code + 1u;
      lzw.code_size = lzw.min_code_size + 1u;
      lzw.old_valid = 0;
      continue;
    }
    if (code == lzw.end_code)
      return BK_OK;
    if (code < lzw.next_code) {
      st = gif_lzw_emit(&lzw, code, out, out_cap, written, &first);
      if (st != BK_OK)
        return st;
    } else if (code == lzw.next_code && lzw.old_valid) {
      st = gif_lzw_emit(&lzw, lzw.old_code, out, out_cap, written, &first);
      if (st != BK_OK)
        return st;
      if (*written >= out_cap)
        return BK_ERR_LIMIT;
      out[(*written)++] = lzw.first_char;
      first = lzw.first_char;
    } else {
      return BK_ERR_CORRUPT;
    }
    if (lzw.old_valid && lzw.next_code < 4096u) {
      lzw.prefix[lzw.next_code] = lzw.old_code;
      lzw.suffix[lzw.next_code] = first;
      lzw.next_code++;
      if (lzw.next_code == (1u << lzw.code_size) && lzw.code_size < 12u)
        lzw.code_size++;
    }
    lzw.old_code = code;
    lzw.first_char = first;
    lzw.old_valid = 1;
    if (*written == out_cap)
      return BK_OK;
  }
}

static void gif_palette_entry(const uint8_t *pal, uint32_t idx, uint8_t *r,
                              uint8_t *g, uint8_t *b) {
  const uint8_t *p = pal + idx * 3u;
  *r = p[0];
  *g = p[1];
  *b = p[2];
}

static uint32_t gif_interlace_y(uint32_t row, uint32_t height) {
  static const uint8_t starts[4] = {0, 4, 2, 1};
  static const uint8_t steps[4] = {8, 8, 4, 2};
  uint32_t pass;
  uint32_t acc = 0;
  for (pass = 0; pass < 4; ++pass) {
    uint32_t y;
    for (y = starts[pass]; y < height; y += steps[pass]) {
      if (acc == row)
        return y;
      acc++;
    }
  }
  return row;
}

bk_status bk_decode_gif(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info) {
  uint16_t canvas_w, canvas_h;
  uint8_t packed, bg, aspect;
  uint8_t global_palette[256 * 3];
  uint32_t global_count = 0;
  size_t pos = 13;
  int transparent = -1;
  uint16_t delay = 0;
  bk_status st;
  if (!data || !ctx)
    return BK_ERR_ARGUMENT;
  if (size < 13)
    return BK_ERR_TRUNCATED;
  if (!(memcmp(data, "GIF87a", 6) == 0 || memcmp(data, "GIF89a", 6) == 0))
    return BK_ERR_BAD_MAGIC;
  canvas_w = rd16(data + 6);
  canvas_h = rd16(data + 8);
  packed = data[10];
  bg = data[11];
  aspect = data[12];
  st = bk_validate_dimensions(canvas_w, canvas_h, &ctx->options);
  if (st != BK_OK)
    return st;
  if (packed & 0x80u) {
    global_count = 1u << ((packed & 7u) + 1u);
    if (pos + global_count * 3u > size)
      return BK_ERR_TRUNCATED;
    memcpy(global_palette, data + pos, global_count * 3u);
    pos += global_count * 3u;
  }
  if (info) {
    info->format = BK_FORMAT_GIF;
    info->width = canvas_w;
    info->height = canvas_h;
    info->bits_per_pixel = (packed & 7u) + 1u;
    info->frame_count = 0;
    info->indexed = 1;
    info->has_alpha = 0;
    bk_metadata_add_u32(&info->metadata, "background_index", bg);
    bk_metadata_add_u32(&info->metadata, "pixel_aspect", aspect);
  }
  while (pos < size) {
    uint8_t marker = data[pos++];
    if (marker == 0x3bu)
      break;
    if (marker == 0x21u) {
      uint8_t label;
      if (pos >= size)
        return BK_ERR_TRUNCATED;
      label = data[pos++];
      if (label == 0xf9u) {
        uint8_t block_size, flags;
        if (pos + 6 > size)
          return BK_ERR_TRUNCATED;
        block_size = data[pos++];
        if (block_size != 4)
          return BK_ERR_CORRUPT;
        flags = data[pos++];
        delay = rd16(data + pos);
        pos += 2;
        transparent = (flags & 1u) ? data[pos] : -1;
        pos++;
        if (data[pos++] != 0)
          return BK_ERR_CORRUPT;
        if (info && transparent >= 0)
          info->has_alpha = 1;
      } else {
        uint8_t *ignored = NULL;
        size_t ignored_size = 0;
        st = gif_read_subblocks(data, size, &pos, &ignored, &ignored_size);
        free(ignored);
        if (st != BK_OK)
          return st;
      }
      continue;
    }
    if (marker != 0x2cu)
      return BK_ERR_CORRUPT;
    if (pos + 9 > size)
      return BK_ERR_TRUNCATED;
    uint16_t left = rd16(data + pos);
    pos += 2;
    uint16_t top = rd16(data + pos);
    pos += 2;
    uint16_t w = rd16(data + pos);
    pos += 2;
    uint16_t h = rd16(data + pos);
    pos += 2;
    uint8_t ipacked = data[pos++];
    uint8_t local_palette[256 * 3];
    const uint8_t *palette = global_palette;
    uint32_t palette_count = global_count;
    uint8_t lzw_min;
    uint8_t *compressed = NULL;
    size_t compressed_size = 0;
    uint8_t *indexes = NULL;
    size_t written = 0;
    uint32_t x, y;
    if (w == 0 || h == 0)
      return BK_ERR_DIMENSIONS;
    if ((uint32_t)left + w > canvas_w || (uint32_t)top + h > canvas_h)
      return BK_ERR_CORRUPT;
    if (ipacked & 0x80u) {
      palette_count = 1u << ((ipacked & 7u) + 1u);
      if (pos + palette_count * 3u > size)
        return BK_ERR_TRUNCATED;
      memcpy(local_palette, data + pos, palette_count * 3u);
      pos += palette_count * 3u;
      palette = local_palette;
    }
    if (palette_count == 0)
      return BK_ERR_CORRUPT;
    if (pos >= size)
      return BK_ERR_TRUNCATED;
    lzw_min = data[pos++];
    st = gif_read_subblocks(data, size, &pos, &compressed, &compressed_size);
    if (st != BK_OK)
      return st;
    if (info)
      info->frame_count++;
    if (ctx->metadata_only) {
      free(compressed);
      continue;
    }
    if (!image) {
      free(compressed);
      return BK_ERR_ARGUMENT;
    }
    if (!image->pixels) {
      st = bk_image_alloc(image, canvas_w, canvas_h);
      if (st != BK_OK) {
        free(compressed);
        return st;
      }
      image->source_format = BK_FORMAT_GIF;
      if (bg < palette_count) {
        uint8_t r, g, b;
        gif_palette_entry(palette, bg, &r, &g, &b);
        bk_image_fill_rect(image, 0, 0, canvas_w, canvas_h, r, g, b, 255);
      }
    }
    indexes = (uint8_t *)malloc((size_t)w * h);
    if (!indexes) {
      free(compressed);
      return BK_ERR_OOM;
    }
    st = gif_lzw_decode(compressed, compressed_size, lzw_min, indexes,
                        (size_t)w * h, &written);
    free(compressed);
    if (st != BK_OK) {
      free(indexes);
      return st;
    }
    if (written < (size_t)w * h) {
      free(indexes);
      return BK_ERR_TRUNCATED;
    }
    for (y = 0; y < h; ++y) {
      uint32_t sy = (ipacked & 0x40u) ? gif_interlace_y(y, h) : y;
      for (x = 0; x < w; ++x) {
        uint8_t idx = indexes[(size_t)y * w + x];
        uint8_t r, g, b, a = 255;
        if (idx >= palette_count)
          continue;
        if ((int)idx == transparent)
          a = 0;
        gif_palette_entry(palette, idx, &r, &g, &b);
        bk_set_pixel(image, left + x, top + sy, r, g, b, a);
      }
    }
    free(indexes);
    if (delay)
      bk_metadata_add_u32(&image->metadata, "gif_delay_cs", delay);
    if (image && !ctx->metadata_only)
      return BK_OK;
  }
  if (ctx->metadata_only)
    return BK_OK;
  return image && image->pixels ? BK_OK : BK_ERR_CORRUPT;
}
