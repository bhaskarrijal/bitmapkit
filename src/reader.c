#include "bitmapkit/internal.h"

void bk_reader_init(bk_reader *r, const uint8_t *data, size_t size,
                    bk_metadata *metadata) {
  r->data = data;
  r->size = size;
  r->pos = 0;
  r->metadata = metadata;
}

int bk_reader_has(const bk_reader *r, size_t n) {
  return r && r->pos <= r->size && n <= r->size - r->pos;
}

bk_status bk_reader_skip(bk_reader *r, size_t n) {
  if (!bk_reader_has(r, n))
    return BK_ERR_TRUNCATED;
  r->pos += n;
  return BK_OK;
}

bk_status bk_reader_read(bk_reader *r, void *out, size_t n) {
  if (!r || (!out && n))
    return BK_ERR_ARGUMENT;
  if (!bk_reader_has(r, n))
    return BK_ERR_TRUNCATED;
  if (n)
    memcpy(out, r->data + r->pos, n);
  r->pos += n;
  return BK_OK;
}

bk_status bk_reader_u8(bk_reader *r, uint8_t *v) {
  if (!v)
    return BK_ERR_ARGUMENT;
  if (!bk_reader_has(r, 1))
    return BK_ERR_TRUNCATED;
  *v = r->data[r->pos++];
  return BK_OK;
}

bk_status bk_reader_le16(bk_reader *r, uint16_t *v) {
  const uint8_t *p;
  if (!v)
    return BK_ERR_ARGUMENT;
  if (!bk_reader_has(r, 2))
    return BK_ERR_TRUNCATED;
  p = r->data + r->pos;
  *v = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
  r->pos += 2;
  return BK_OK;
}

bk_status bk_reader_le32(bk_reader *r, uint32_t *v) {
  const uint8_t *p;
  if (!v)
    return BK_ERR_ARGUMENT;
  if (!bk_reader_has(r, 4))
    return BK_ERR_TRUNCATED;
  p = r->data + r->pos;
  *v = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
       ((uint32_t)p[3] << 24);
  r->pos += 4;
  return BK_OK;
}

bk_status bk_reader_sle32(bk_reader *r, int32_t *v) {
  uint32_t u;
  bk_status st = bk_reader_le32(r, &u);
  if (st != BK_OK)
    return st;
  *v = (int32_t)u;
  return BK_OK;
}

bk_status bk_reader_be16(bk_reader *r, uint16_t *v) {
  const uint8_t *p;
  if (!v)
    return BK_ERR_ARGUMENT;
  if (!bk_reader_has(r, 2))
    return BK_ERR_TRUNCATED;
  p = r->data + r->pos;
  *v = (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
  r->pos += 2;
  return BK_OK;
}

bk_status bk_reader_be32(bk_reader *r, uint32_t *v) {
  const uint8_t *p;
  if (!v)
    return BK_ERR_ARGUMENT;
  if (!bk_reader_has(r, 4))
    return BK_ERR_TRUNCATED;
  p = r->data + r->pos;
  *v = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
       (uint32_t)p[3];
  r->pos += 4;
  return BK_OK;
}

size_t bk_reader_remaining(const bk_reader *r) {
  if (!r || r->pos > r->size)
    return 0;
  return r->size - r->pos;
}

const uint8_t *bk_reader_ptr(const bk_reader *r) {
  if (!r || r->pos > r->size)
    return NULL;
  return r->data + r->pos;
}
