#include "bitmapkit/bitmapkit.h"
#include <string.h>

const char *bk_status_string(bk_status status) {
  switch (status) {
  case BK_OK:
    return "ok";
  case BK_ERR_ARGUMENT:
    return "bad argument";
  case BK_ERR_OOM:
    return "out of memory";
  case BK_ERR_TRUNCATED:
    return "truncated input";
  case BK_ERR_BAD_MAGIC:
    return "bad magic";
  case BK_ERR_UNSUPPORTED:
    return "unsupported feature";
  case BK_ERR_DIMENSIONS:
    return "invalid dimensions";
  case BK_ERR_CORRUPT:
    return "corrupt input";
  case BK_ERR_LIMIT:
    return "configured limit exceeded";
  case BK_ERR_IO:
    return "i/o error";
  case BK_ERR_INTERNAL:
    return "internal error";
  default:
    return "unknown error";
  }
}

const char *bk_format_name(bk_format format) {
  switch (format) {
  case BK_FORMAT_BMP:
    return "BMP";
  case BK_FORMAT_TGA:
    return "TGA";
  case BK_FORMAT_PCX:
    return "PCX";
  case BK_FORMAT_PNM:
    return "PNM/PAM";
  case BK_FORMAT_ICO:
    return "ICO";
  case BK_FORMAT_CUR:
    return "CUR";
  case BK_FORMAT_QOI:
    return "QOI";
  case BK_FORMAT_FARBFELD:
    return "farbfeld";
  case BK_FORMAT_XBM:
    return "XBM";
  case BK_FORMAT_GIF:
    return "GIF";
  case BK_FORMAT_PSD:
    return "PSD";
  case BK_FORMAT_TIFF:
    return "TIFF";
  case BK_FORMAT_DDS:
    return "DDS";
  case BK_FORMAT_RAS:
    return "Sun raster";
  case BK_FORMAT_SGI:
    return "SGI RGB";
  case BK_FORMAT_PNG:
    return "PNG";
  case BK_FORMAT_JPEG:
    return "JPEG";
  case BK_FORMAT_IFF:
    return "IFF ILBM";
  case BK_FORMAT_XWD:
    return "XWD";
  case BK_FORMAT_WEBP:
    return "WebP";
  case BK_FORMAT_EXR:
    return "OpenEXR";
  default:
    return "unknown";
  }
}

void bk_default_options(bk_decode_options *options) {
  if (!options)
    return;
  options->max_width = BK_MAX_DIMENSION;
  options->max_height = BK_MAX_DIMENSION;
  options->max_pixels = BK_MAX_PIXELS;
  options->keep_metadata = 1;
  options->tolerate_repair = 0;
}

void bk_info_clear(bk_info *info) {
  if (!info)
    return;
  memset(info, 0, sizeof(*info));
}
