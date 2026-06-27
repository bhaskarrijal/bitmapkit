#include "bitmapkit/internal.h"

bk_status bk_buffer_init(bk_buffer *b, size_t initial) {
  if (!b)
    return BK_ERR_ARGUMENT;
  b->data = NULL;
  b->size = 0;
  b->capacity = 0;
  if (initial)
    return bk_buffer_reserve(b, initial);
  return BK_OK;
}

void bk_buffer_free(bk_buffer *b) {
  if (!b)
    return;
  free(b->data);
  b->data = NULL;
  b->size = 0;
  b->capacity = 0;
}

bk_status bk_buffer_reserve(bk_buffer *b, size_t needed) {
  uint8_t *next;
  size_t cap;
  if (!b)
    return BK_ERR_ARGUMENT;
  if (needed <= b->capacity)
    return BK_OK;
  cap = b->capacity ? b->capacity : 256;
  while (cap < needed) {
    if (cap > SIZE_MAX / 2)
      return BK_ERR_LIMIT;
    cap *= 2;
  }
  next = (uint8_t *)realloc(b->data, cap);
  if (!next)
    return BK_ERR_OOM;
  b->data = next;
  b->capacity = cap;
  return BK_OK;
}

bk_status bk_buffer_append(bk_buffer *b, const void *data, size_t n) {
  if (!b || (!data && n))
    return BK_ERR_ARGUMENT;
  if (n > SIZE_MAX - b->size)
    return BK_ERR_LIMIT;
  if (bk_buffer_reserve(b, b->size + n) != BK_OK)
    return BK_ERR_OOM;
  if (n)
    memcpy(b->data + b->size, data, n);
  b->size += n;
  return BK_OK;
}

bk_status bk_buffer_append_u8(bk_buffer *b, uint8_t v) {
  return bk_buffer_append(b, &v, 1);
}

bk_status bk_buffer_append_le16(bk_buffer *b, uint16_t v) {
  uint8_t tmp[2];
  tmp[0] = (uint8_t)(v & 255u);
  tmp[1] = (uint8_t)((v >> 8) & 255u);
  return bk_buffer_append(b, tmp, sizeof(tmp));
}

bk_status bk_buffer_append_le32(bk_buffer *b, uint32_t v) {
  uint8_t tmp[4];
  tmp[0] = (uint8_t)(v & 255u);
  tmp[1] = (uint8_t)((v >> 8) & 255u);
  tmp[2] = (uint8_t)((v >> 16) & 255u);
  tmp[3] = (uint8_t)((v >> 24) & 255u);
  return bk_buffer_append(b, tmp, sizeof(tmp));
}

bk_status bk_buffer_append_be16(bk_buffer *b, uint16_t v) {
  uint8_t tmp[2];
  tmp[0] = (uint8_t)((v >> 8) & 255u);
  tmp[1] = (uint8_t)(v & 255u);
  return bk_buffer_append(b, tmp, sizeof(tmp));
}

bk_status bk_buffer_append_be32(bk_buffer *b, uint32_t v) {
  uint8_t tmp[4];
  tmp[0] = (uint8_t)((v >> 24) & 255u);
  tmp[1] = (uint8_t)((v >> 16) & 255u);
  tmp[2] = (uint8_t)((v >> 8) & 255u);
  tmp[3] = (uint8_t)(v & 255u);
  return bk_buffer_append(b, tmp, sizeof(tmp));
}
