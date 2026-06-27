#include "bitmapkit/internal.h"

static int path_is_sep(char c) { return c == '/' || c == '\\'; }

bk_status bk_path_normalize(const char *input, char *out, size_t out_size) {
  size_t n = 0;
  int last_sep = 0;
  if (!input || !out || out_size == 0)
    return BK_ERR_ARGUMENT;
  for (size_t i = 0; input[i]; ++i) {
    char c = input[i];
    if (c == '\\')
      c = '/';
    if ((unsigned char)c < 32)
      return BK_ERR_CORRUPT;
    if (c == '/') {
      if (last_sep)
        continue;
      last_sep = 1;
    } else {
      last_sep = 0;
    }
    if (n + 1 >= out_size)
      return BK_ERR_LIMIT;
    out[n++] = c;
  }
  while (n > 1 && out[n - 1] == '/')
    --n;
  out[n] = 0;
  return BK_OK;
}

bk_status bk_path_reject_traversal(const char *path) {
  const char *p = path;
  if (!path || !*path)
    return BK_ERR_ARGUMENT;
  if (path[0] == '/' || path[0] == '~')
    return BK_ERR_CORRUPT;
  while (*p) {
    while (path_is_sep(*p))
      ++p;
    if (p[0] == '.' && (path_is_sep(p[1]) || p[1] == 0))
      return BK_ERR_CORRUPT;
    if (p[0] == '.' && p[1] == '.' && (path_is_sep(p[2]) || p[2] == 0))
      return BK_ERR_CORRUPT;
    while (*p && !path_is_sep(*p))
      ++p;
  }
  return BK_OK;
}

bk_status bk_path_extension(const char *path, char *out, size_t out_size) {
  const char *dot = NULL;
  const char *p;
  size_t n;
  if (!path || !out || out_size == 0)
    return BK_ERR_ARGUMENT;
  for (p = path; *p; ++p) {
    if (*p == '.')
      dot = p;
    if (path_is_sep(*p))
      dot = NULL;
  }
  if (!dot || !dot[1]) {
    out[0] = 0;
    return BK_OK;
  }
  n = strlen(dot + 1);
  if (n + 1 > out_size)
    return BK_ERR_LIMIT;
  memcpy(out, dot + 1, n + 1);
  for (size_t i = 0; out[i]; ++i)
    if (out[i] >= 'A' && out[i] <= 'Z')
      out[i] = (char)(out[i] - 'A' + 'a');
  return BK_OK;
}

bk_status bk_path_join(const char *base, const char *leaf, char *out,
                       size_t out_size) {
  size_t n;
  if (!base || !leaf || !out || out_size == 0)
    return BK_ERR_ARGUMENT;
  if (bk_path_reject_traversal(leaf) != BK_OK)
    return BK_ERR_CORRUPT;
  n = strlen(base);
  if (n + strlen(leaf) + 2 > out_size)
    return BK_ERR_LIMIT;
  memcpy(out, base, n);
  if (n && !path_is_sep(out[n - 1]))
    out[n++] = '/';
  strcpy(out + n, leaf);
  return bk_path_normalize(out, out, out_size);
}

bk_status bk_path_change_extension(const char *path, const char *ext, char *out,
                                   size_t out_size) {
  size_t cut = 0;
  if (!path || !ext || !out || out_size == 0)
    return BK_ERR_ARGUMENT;
  for (size_t i = 0; path[i]; ++i) {
    if (path[i] == '.')
      cut = i;
    if (path_is_sep(path[i]))
      cut = 0;
  }
  if (cut == 0)
    cut = strlen(path);
  if (cut + 1 + strlen(ext) + 1 > out_size)
    return BK_ERR_LIMIT;
  memcpy(out, path, cut);
  out[cut] = '.';
  strcpy(out + cut + 1, ext);
  return BK_OK;
}

bk_format bk_path_guess_format(const char *path) {
  char ext[32];
  if (bk_path_extension(path, ext, sizeof(ext)) != BK_OK)
    return BK_FORMAT_UNKNOWN;
  if (strcmp(ext, "bmp") == 0 || strcmp(ext, "dib") == 0)
    return BK_FORMAT_BMP;
  if (strcmp(ext, "tga") == 0 || strcmp(ext, "vda") == 0 ||
      strcmp(ext, "icb") == 0)
    return BK_FORMAT_TGA;
  if (strcmp(ext, "pcx") == 0)
    return BK_FORMAT_PCX;
  if (strcmp(ext, "pbm") == 0 || strcmp(ext, "pgm") == 0 ||
      strcmp(ext, "ppm") == 0 || strcmp(ext, "pnm") == 0 ||
      strcmp(ext, "pam") == 0)
    return BK_FORMAT_PNM;
  if (strcmp(ext, "ico") == 0)
    return BK_FORMAT_ICO;
  if (strcmp(ext, "cur") == 0)
    return BK_FORMAT_CUR;
  if (strcmp(ext, "gif") == 0)
    return BK_FORMAT_GIF;
  if (strcmp(ext, "psd") == 0)
    return BK_FORMAT_PSD;
  if (strcmp(ext, "tif") == 0 || strcmp(ext, "tiff") == 0)
    return BK_FORMAT_TIFF;
  if (strcmp(ext, "dds") == 0)
    return BK_FORMAT_DDS;
  if (strcmp(ext, "ras") == 0)
    return BK_FORMAT_RAS;
  if (strcmp(ext, "rgb") == 0 || strcmp(ext, "sgi") == 0)
    return BK_FORMAT_SGI;
  if (strcmp(ext, "png") == 0)
    return BK_FORMAT_PNG;
  if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0)
    return BK_FORMAT_JPEG;
  if (strcmp(ext, "iff") == 0 || strcmp(ext, "ilbm") == 0)
    return BK_FORMAT_IFF;
  if (strcmp(ext, "xwd") == 0)
    return BK_FORMAT_XWD;
  if (strcmp(ext, "webp") == 0)
    return BK_FORMAT_WEBP;
  if (strcmp(ext, "exr") == 0)
    return BK_FORMAT_EXR;
  if (strcmp(ext, "qoi") == 0)
    return BK_FORMAT_QOI;
  if (strcmp(ext, "ff") == 0 || strcmp(ext, "farbfeld") == 0)
    return BK_FORMAT_FARBFELD;
  if (strcmp(ext, "xbm") == 0)
    return BK_FORMAT_XBM;
  return BK_FORMAT_UNKNOWN;
}
