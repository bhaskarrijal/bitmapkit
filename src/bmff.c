#include "bitmapkit/internal.h"

static uint32_t bmff_be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | p[3];
}
static uint64_t bmff_be64(const uint8_t *p) {
  return ((uint64_t)bmff_be32(p) << 32) | bmff_be32(p + 4);
}

bk_status bk_bmff_scan_boxes(const uint8_t *data, size_t size,
                             bk_metadata *metadata) {
  size_t pos = 0;
  if (!data || !metadata)
    return BK_ERR_ARGUMENT;
  while (pos + 8 <= size && metadata->count < BK_MAX_METADATA) {
    uint64_t box_size = bmff_be32(data + pos);
    const uint8_t *type = data + pos + 4;
    size_t header = 8;
    if (box_size == 1) {
      if (pos + 16 > size)
        return BK_ERR_TRUNCATED;
      box_size = bmff_be64(data + pos + 8);
      header = 16;
    } else if (box_size == 0)
      box_size = size - pos;
    if (box_size < header || pos + box_size > size)
      return BK_ERR_TRUNCATED;
    char key[32], val[96];
    snprintf(key, sizeof(key), "box_%zu", metadata->count);
    snprintf(val, sizeof(val), "%c%c%c%c:%llu", type[0], type[1], type[2],
             type[3], (unsigned long long)box_size);
    bk_metadata_add(metadata, key, val);
    pos += (size_t)box_size;
  }
  return BK_OK;
}

bk_status bk_heif_probe(const uint8_t *data, size_t size, bk_info *info) {
  if (!data || !info)
    return BK_ERR_ARGUMENT;
  bk_metadata_clear(&info->metadata);
  if (bk_bmff_scan_boxes(data, size, &info->metadata) != BK_OK)
    return BK_ERR_BAD_MAGIC;
  info->format = BK_FORMAT_UNKNOWN;
  info->frame_count = 1;
  return BK_OK;
}
