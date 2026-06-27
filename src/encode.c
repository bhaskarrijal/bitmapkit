#include "bitmapkit/internal.h"

bk_status bk_encode_bmp(const bk_image *image, uint8_t **out,
                        size_t *out_size) {
  bk_buffer b;
  uint32_t row_stride, pixel_offset, file_size;
  uint32_t x, y;
  if (!image || !image->pixels || !out || !out_size)
    return BK_ERR_ARGUMENT;
  row_stride = ((image->width * 3u + 3u) / 4u) * 4u;
  if ((uint64_t)row_stride * image->height > UINT32_MAX - 54u)
    return BK_ERR_LIMIT;
  pixel_offset = 54u;
  file_size = pixel_offset + row_stride * image->height;
  if (bk_buffer_init(&b, file_size) != BK_OK)
    return BK_ERR_OOM;
  bk_buffer_append_u8(&b, 'B');
  bk_buffer_append_u8(&b, 'M');
  bk_buffer_append_le32(&b, file_size);
  bk_buffer_append_le16(&b, 0);
  bk_buffer_append_le16(&b, 0);
  bk_buffer_append_le32(&b, pixel_offset);
  bk_buffer_append_le32(&b, 40);
  bk_buffer_append_le32(&b, image->width);
  bk_buffer_append_le32(&b, image->height);
  bk_buffer_append_le16(&b, 1);
  bk_buffer_append_le16(&b, 24);
  bk_buffer_append_le32(&b, 0);
  bk_buffer_append_le32(&b, row_stride * image->height);
  bk_buffer_append_le32(&b, 2835);
  bk_buffer_append_le32(&b, 2835);
  bk_buffer_append_le32(&b, 0);
  bk_buffer_append_le32(&b, 0);
  for (y = 0; y < image->height; ++y) {
    uint32_t sy = image->height - 1u - y;
    size_t row_start = b.size;
    for (x = 0; x < image->width; ++x) {
      uint8_t r, g, blue, a;
      (void)a;
      bk_get_pixel(image, x, sy, &r, &g, &blue, &a);
      bk_buffer_append_u8(&b, blue);
      bk_buffer_append_u8(&b, g);
      bk_buffer_append_u8(&b, r);
    }
    while (b.size - row_start < row_stride)
      bk_buffer_append_u8(&b, 0);
  }
  *out = b.data;
  *out_size = b.size;
  return BK_OK;
}

bk_status bk_encode_tga(const bk_image *image, uint8_t **out,
                        size_t *out_size) {
  bk_buffer b;
  uint32_t x, y;
  int has_alpha = 0;
  if (!image || !image->pixels || !out || !out_size)
    return BK_ERR_ARGUMENT;
  for (y = 0; y < image->height && !has_alpha; ++y)
    for (x = 0; x < image->width; ++x) {
      uint8_t a;
      bk_get_pixel(image, x, y, NULL, NULL, NULL, &a);
      if (a != 255) {
        has_alpha = 1;
        break;
      }
    }
  if ((uint64_t)image->width * image->height * (has_alpha ? 4u : 3u) >
      SIZE_MAX - 18u)
    return BK_ERR_LIMIT;
  if (bk_buffer_init(&b, 18u + (size_t)image->width * image->height *
                                   (has_alpha ? 4u : 3u)) != BK_OK)
    return BK_ERR_OOM;
  bk_buffer_append_u8(&b, 0);
  bk_buffer_append_u8(&b, 0);
  bk_buffer_append_u8(&b, 2);
  bk_buffer_append_le16(&b, 0);
  bk_buffer_append_le16(&b, 0);
  bk_buffer_append_u8(&b, 0);
  bk_buffer_append_le16(&b, 0);
  bk_buffer_append_le16(&b, 0);
  bk_buffer_append_le16(&b, (uint16_t)image->width);
  bk_buffer_append_le16(&b, (uint16_t)image->height);
  bk_buffer_append_u8(&b, has_alpha ? 32 : 24);
  bk_buffer_append_u8(&b, (uint8_t)(0x20u | (has_alpha ? 8u : 0u)));
  for (y = 0; y < image->height; ++y)
    for (x = 0; x < image->width; ++x) {
      uint8_t r, g, blue, a;
      bk_get_pixel(image, x, y, &r, &g, &blue, &a);
      bk_buffer_append_u8(&b, blue);
      bk_buffer_append_u8(&b, g);
      bk_buffer_append_u8(&b, r);
      if (has_alpha)
        bk_buffer_append_u8(&b, a);
    }
  *out = b.data;
  *out_size = b.size;
  return BK_OK;
}

bk_status bk_encode_pnm(const bk_image *image, uint8_t **out,
                        size_t *out_size) {
  bk_buffer b;
  char header[128];
  int n;
  uint32_t x, y;
  if (!image || !image->pixels || !out || !out_size)
    return BK_ERR_ARGUMENT;
  n = snprintf(header, sizeof(header), "P6\n%u %u\n255\n", image->width,
               image->height);
  if (n <= 0 || (size_t)n >= sizeof(header))
    return BK_ERR_INTERNAL;
  if ((uint64_t)image->width * image->height * 3u > SIZE_MAX - (size_t)n)
    return BK_ERR_LIMIT;
  if (bk_buffer_init(&b, (size_t)n + (size_t)image->width * image->height *
                                         3u) != BK_OK)
    return BK_ERR_OOM;
  bk_buffer_append(&b, header, (size_t)n);
  for (y = 0; y < image->height; ++y)
    for (x = 0; x < image->width; ++x) {
      uint8_t r, g, blue, a;
      (void)a;
      bk_get_pixel(image, x, y, &r, &g, &blue, &a);
      bk_buffer_append_u8(&b, r);
      bk_buffer_append_u8(&b, g);
      bk_buffer_append_u8(&b, blue);
    }
  *out = b.data;
  *out_size = b.size;
  return BK_OK;
}
