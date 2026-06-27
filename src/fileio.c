#include "bitmapkit/internal.h"

bk_status bk_read_file(const char *path, uint8_t **data, size_t *size) {
  FILE *f;
  long len;
  uint8_t *buf;
  size_t got;
  if (!path || !data || !size)
    return BK_ERR_ARGUMENT;
  *data = NULL;
  *size = 0;
  f = fopen(path, "rb");
  if (!f)
    return BK_ERR_IO;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return BK_ERR_IO;
  }
  len = ftell(f);
  if (len < 0) {
    fclose(f);
    return BK_ERR_IO;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return BK_ERR_IO;
  }
  buf = (uint8_t *)malloc((size_t)len ? (size_t)len : 1u);
  if (!buf) {
    fclose(f);
    return BK_ERR_OOM;
  }
  got = fread(buf, 1, (size_t)len, f);
  fclose(f);
  if (got != (size_t)len) {
    free(buf);
    return BK_ERR_IO;
  }
  *data = buf;
  *size = got;
  return BK_OK;
}

bk_status bk_write_file(const char *path, const uint8_t *data, size_t size) {
  FILE *f;
  if (!path || (!data && size))
    return BK_ERR_ARGUMENT;
  f = fopen(path, "wb");
  if (!f)
    return BK_ERR_IO;
  if (size && fwrite(data, 1, size, f) != size) {
    fclose(f);
    return BK_ERR_IO;
  }
  if (fclose(f) != 0)
    return BK_ERR_IO;
  return BK_OK;
}
