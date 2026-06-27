#ifndef BITMAPKIT_INTERNAL_H
#define BITMAPKIT_INTERNAL_H

#include "bitmapkit/bitmapkit.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct bk_reader {
  const uint8_t *data;
  size_t size;
  size_t pos;
  bk_metadata *metadata;
} bk_reader;

typedef struct bk_buffer {
  uint8_t *data;
  size_t size;
  size_t capacity;
} bk_buffer;

typedef struct bk_palette_entry {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
} bk_palette_entry;

typedef struct bk_decode_context {
  bk_decode_options options;
  bk_format forced_format;
  int metadata_only;
} bk_decode_context;

void bk_reader_init(bk_reader *r, const uint8_t *data, size_t size,
                    bk_metadata *metadata);
int bk_reader_has(const bk_reader *r, size_t n);
bk_status bk_reader_skip(bk_reader *r, size_t n);
bk_status bk_reader_read(bk_reader *r, void *out, size_t n);
bk_status bk_reader_u8(bk_reader *r, uint8_t *v);
bk_status bk_reader_le16(bk_reader *r, uint16_t *v);
bk_status bk_reader_le32(bk_reader *r, uint32_t *v);
bk_status bk_reader_sle32(bk_reader *r, int32_t *v);
bk_status bk_reader_be16(bk_reader *r, uint16_t *v);
bk_status bk_reader_be32(bk_reader *r, uint32_t *v);
size_t bk_reader_remaining(const bk_reader *r);
const uint8_t *bk_reader_ptr(const bk_reader *r);

bk_status bk_buffer_init(bk_buffer *b, size_t initial);
void bk_buffer_free(bk_buffer *b);
bk_status bk_buffer_reserve(bk_buffer *b, size_t needed);
bk_status bk_buffer_append(bk_buffer *b, const void *data, size_t n);
bk_status bk_buffer_append_u8(bk_buffer *b, uint8_t v);
bk_status bk_buffer_append_le16(bk_buffer *b, uint16_t v);
bk_status bk_buffer_append_le32(bk_buffer *b, uint32_t v);
bk_status bk_buffer_append_be16(bk_buffer *b, uint16_t v);
bk_status bk_buffer_append_be32(bk_buffer *b, uint32_t v);

bk_status bk_image_alloc(bk_image *image, uint32_t width, uint32_t height);
bk_status bk_validate_dimensions(uint32_t width, uint32_t height,
                                 const bk_decode_options *options);
size_t bk_safe_mul3(size_t a, size_t b, size_t c, int *ok);
void bk_set_pixel(bk_image *image, uint32_t x, uint32_t y, uint8_t r, uint8_t g,
                  uint8_t b, uint8_t a);
void bk_get_pixel(const bk_image *image, uint32_t x, uint32_t y, uint8_t *r,
                  uint8_t *g, uint8_t *b, uint8_t *a);

uint32_t bk_crc32(const uint8_t *data, size_t size);
uint32_t bk_adler32(const uint8_t *data, size_t size);

bk_status bk_decode_bmp(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info);
bk_status bk_decode_tga(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info);
bk_status bk_decode_pcx(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info);
bk_status bk_decode_pnm(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info);
bk_status bk_decode_ico(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info);
bk_status bk_decode_qoi(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info);
bk_status bk_decode_farbfeld(const uint8_t *data, size_t size,
                             const bk_decode_context *ctx, bk_image *image,
                             bk_info *info);
bk_status bk_decode_xbm(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info);
bk_status bk_decode_gif(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info);
bk_status bk_decode_psd(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info);
bk_status bk_decode_tiff(const uint8_t *data, size_t size,
                         const bk_decode_context *ctx, bk_image *image,
                         bk_info *info);
bk_status bk_decode_dds(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info);
bk_status bk_decode_ras(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info);
bk_status bk_decode_sgi(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info);
bk_status bk_decode_png(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info);
bk_status bk_decode_jpeg(const uint8_t *data, size_t size,
                         const bk_decode_context *ctx, bk_image *image,
                         bk_info *info);
bk_status bk_decode_iff(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info);
bk_status bk_decode_xwd(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info);
bk_status bk_decode_webp(const uint8_t *data, size_t size,
                         const bk_decode_context *ctx, bk_image *image,
                         bk_info *info);
bk_status bk_decode_exr(const uint8_t *data, size_t size,
                        const bk_decode_context *ctx, bk_image *image,
                        bk_info *info);

bk_status bk_write_file(const char *path, const uint8_t *data, size_t size);
bk_status bk_read_file(const char *path, uint8_t **data, size_t *size);

#endif
