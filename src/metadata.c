#include "bitmapkit/internal.h"

static void bk_copy_text(char *dst, size_t dst_size, const char *src) {
  size_t i;
  if (!dst || dst_size == 0)
    return;
  if (!src)
    src = "";
  for (i = 0; i + 1 < dst_size && src[i]; ++i)
    dst[i] = src[i];
  dst[i] = '\0';
}

void bk_metadata_clear(bk_metadata *metadata) {
  if (!metadata)
    return;
  memset(metadata, 0, sizeof(*metadata));
}

bk_status bk_metadata_add(bk_metadata *metadata, const char *key,
                          const char *value) {
  bk_metadata_pair *pair;
  if (!metadata || !key || !value)
    return BK_ERR_ARGUMENT;
  if (metadata->count >= BK_MAX_METADATA)
    return BK_ERR_LIMIT;
  pair = &metadata->pairs[metadata->count++];
  bk_copy_text(pair->key, sizeof(pair->key), key);
  bk_copy_text(pair->value, sizeof(pair->value), value);
  return BK_OK;
}

bk_status bk_metadata_add_u32(bk_metadata *metadata, const char *key,
                              uint32_t value) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%u", value);
  return bk_metadata_add(metadata, key, buf);
}

bk_status bk_metadata_warn(bk_metadata *metadata, bk_severity severity,
                           const char *code, const char *message,
                           size_t offset) {
  bk_warning *warning;
  if (!metadata || !code || !message)
    return BK_ERR_ARGUMENT;
  if (metadata->warning_count >= BK_MAX_WARNINGS)
    return BK_ERR_LIMIT;
  warning = &metadata->warnings[metadata->warning_count++];
  warning->severity = severity;
  warning->offset = offset;
  bk_copy_text(warning->code, sizeof(warning->code), code);
  bk_copy_text(warning->message, sizeof(warning->message), message);
  return BK_OK;
}
