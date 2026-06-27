#include "bitmapkit/internal.h"
#include <ctype.h>

typedef struct bk_manifest_item {
  char input[160];
  char output[160];
  char transform[160];
  uint32_t flags;
} bk_manifest_item;

typedef struct bk_manifest {
  bk_manifest_item items[128];
  uint32_t count;
} bk_manifest;

static const char *mf_skip_ws(const char *p) {
  while (*p == ' ' || *p == '\t')
    ++p;
  return p;
}

static const char *mf_line_end(const char *p) {
  while (*p && *p != '\n' && *p != '\r')
    ++p;
  return p;
}

static void mf_copy_token(char *dst, size_t dst_size, const char *start,
                          const char *end) {
  size_t n = 0;
  while (start < end && n + 1 < dst_size)
    dst[n++] = *start++;
  dst[n] = 0;
}

static int mf_key_value(const char *start, const char *end, char *key,
                        size_t key_size, char *value, size_t value_size) {
  const char *eq = start;
  while (eq < end && *eq != '=')
    ++eq;
  if (eq == end)
    return 0;
  mf_copy_token(key, key_size, start, eq);
  mf_copy_token(value, value_size, eq + 1, end);
  return 1;
}

bk_status bk_manifest_parse(const char *text, bk_manifest *manifest) {
  const char *p;
  bk_manifest_item *current = NULL;
  if (!text || !manifest)
    return BK_ERR_ARGUMENT;
  memset(manifest, 0, sizeof(*manifest));
  p = text;
  while (*p) {
    const char *line_start;
    const char *line_end;
    char key[64];
    char value[192];
    p = mf_skip_ws(p);
    line_start = p;
    line_end = mf_line_end(p);
    if (line_start != line_end && *line_start != '#') {
      if (strncmp(line_start, "asset", 5) == 0 &&
          isspace((unsigned char)line_start[5])) {
        if (manifest->count >= 128u)
          return BK_ERR_LIMIT;
        current = &manifest->items[manifest->count++];
        memset(current, 0, sizeof(*current));
        mf_copy_token(current->input, sizeof(current->input),
                      mf_skip_ws(line_start + 5), line_end);
      } else if (current && mf_key_value(line_start, line_end, key, sizeof(key),
                                         value, sizeof(value))) {
        if (strcmp(key, "input") == 0)
          snprintf(current->input, sizeof(current->input), "%s", value);
        else if (strcmp(key, "output") == 0)
          snprintf(current->output, sizeof(current->output), "%s", value);
        else if (strcmp(key, "transform") == 0)
          snprintf(current->transform, sizeof(current->transform), "%s", value);
        else if (strcmp(key, "required") == 0)
          current->flags |= strcmp(value, "true") == 0 ? 1u : 0u;
        else if (strcmp(key, "lossless") == 0)
          current->flags |= strcmp(value, "true") == 0 ? 2u : 0u;
      } else {
        return BK_ERR_CORRUPT;
      }
    }
    p = line_end;
    while (*p == '\n' || *p == '\r')
      ++p;
  }
  return BK_OK;
}

uint32_t bk_manifest_count_required(const bk_manifest *manifest) {
  uint32_t i, n = 0;
  if (!manifest)
    return 0;
  for (i = 0; i < manifest->count; ++i)
    if (manifest->items[i].flags & 1u)
      ++n;
  return n;
}

uint32_t bk_manifest_count_lossless(const bk_manifest *manifest) {
  uint32_t i, n = 0;
  if (!manifest)
    return 0;
  for (i = 0; i < manifest->count; ++i)
    if (manifest->items[i].flags & 2u)
      ++n;
  return n;
}

bk_status bk_manifest_validate_paths(const bk_manifest *manifest) {
  uint32_t i;
  if (!manifest)
    return BK_ERR_ARGUMENT;
  for (i = 0; i < manifest->count; ++i) {
    const char *in = manifest->items[i].input;
    const char *out = manifest->items[i].output;
    if (!*in)
      return BK_ERR_CORRUPT;
    if (strstr(in, "..") || strstr(out, ".."))
      return BK_ERR_CORRUPT;
    if (strchr(in, '\\') || strchr(out, '\\'))
      return BK_ERR_UNSUPPORTED;
  }
  return BK_OK;
}
