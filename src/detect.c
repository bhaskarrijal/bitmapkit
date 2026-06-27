#include "bitmapkit/internal.h"

static int bk_is_pnm_magic(const uint8_t *data, size_t size) {
  return size >= 2 && data[0] == 'P' && data[1] >= '1' && data[1] <= '7';
}

bk_format bk_detect_format(const uint8_t *data, size_t size) {
  if (!data || size == 0)
    return BK_FORMAT_UNKNOWN;
  if (size >= 2 && data[0] == 'B' && data[1] == 'M')
    return BK_FORMAT_BMP;
  if (size >= 14 && memcmp(data, "qoif", 4) == 0)
    return BK_FORMAT_QOI;
  if (size >= 16 && memcmp(data, "farbfeld", 8) == 0)
    return BK_FORMAT_FARBFELD;
  if (size >= 6 &&
      (memcmp(data, "GIF87a", 6) == 0 || memcmp(data, "GIF89a", 6) == 0))
    return BK_FORMAT_GIF;
  if (size >= 4 && memcmp(data, "8BPS", 4) == 0)
    return BK_FORMAT_PSD;
  if (size >= 4 &&
      ((data[0] == 'I' && data[1] == 'I' && data[2] == 42 && data[3] == 0) ||
       (data[0] == 'M' && data[1] == 'M' && data[2] == 0 && data[3] == 42)))
    return BK_FORMAT_TIFF;
  if (size >= 4 && memcmp(data, "DDS ", 4) == 0)
    return BK_FORMAT_DDS;
  if (size >= 4 && data[0] == 0x59 && data[1] == 0xa6 && data[2] == 0x6a &&
      data[3] == 0x95)
    return BK_FORMAT_RAS;
  if (size >= 2 && data[0] == 0x01 && data[1] == 0xda)
    return BK_FORMAT_SGI;
  if (size >= 8 && data[0] == 137 && data[1] == 80 && data[2] == 78 &&
      data[3] == 71)
    return BK_FORMAT_PNG;
  if (size >= 2 && data[0] == 0xff && data[1] == 0xd8)
    return BK_FORMAT_JPEG;
  if (size >= 12 && memcmp(data, "FORM", 4) == 0 &&
      (memcmp(data + 8, "ILBM", 4) == 0 || memcmp(data + 8, "PBM ", 4) == 0))
    return BK_FORMAT_IFF;
  if (size >= 12 && memcmp(data, "RIFF", 4) == 0 &&
      memcmp(data + 8, "WEBP", 4) == 0)
    return BK_FORMAT_WEBP;
  if (size >= 4 && data[0] == 0x76 && data[1] == 0x2f && data[2] == 0x31 &&
      data[3] == 0x01)
    return BK_FORMAT_EXR;
  if (size >= 9 && memcmp(data, "#define ", 8) == 0)
    return BK_FORMAT_XBM;
  if (size >= 22 && data[0] == 0 && data[1] == 0 &&
      (data[2] == 1 || data[2] == 2) && data[3] == 0) {
    uint32_t count = (uint32_t)data[4] | ((uint32_t)data[5] << 8);
    if (count > 0 && count <= 256 && size >= 6u + count * 16u) {
      return data[2] == 1 ? BK_FORMAT_ICO : BK_FORMAT_CUR;
    }
  }
  if (size >= 4 && data[0] == 0x0a && data[1] <= 5 && data[2] == 1 &&
      (data[3] == 1 || data[3] == 2 || data[3] == 4 || data[3] == 8))
    return BK_FORMAT_PCX;
  if (bk_is_pnm_magic(data, size))
    return BK_FORMAT_PNM;
  if (size >= 18) {
    uint8_t image_type = data[2];
    uint8_t cmap_type = data[1];
    uint8_t depth = data[16];
    if ((cmap_type == 0 || cmap_type == 1) &&
        (image_type == 1 || image_type == 2 || image_type == 3 ||
         image_type == 9 || image_type == 10 || image_type == 11) &&
        (depth == 8 || depth == 15 || depth == 16 || depth == 24 ||
         depth == 32))
      return BK_FORMAT_TGA;
  }
  return BK_FORMAT_UNKNOWN;
}

static bk_decode_context bk_make_context(const bk_decode_options *options,
                                         bk_format forced, int metadata_only) {
  bk_decode_context ctx;
  bk_default_options(&ctx.options);
  if (options)
    ctx.options = *options;
  ctx.forced_format = forced;
  ctx.metadata_only = metadata_only;
  return ctx;
}

bk_status bk_probe_memory(const uint8_t *data, size_t size, bk_info *info) {
  bk_decode_context ctx;
  bk_format fmt;
  if (!data || !info)
    return BK_ERR_ARGUMENT;
  bk_info_clear(info);
  fmt = bk_detect_format(data, size);
  if (fmt == BK_FORMAT_UNKNOWN)
    return BK_ERR_BAD_MAGIC;
  ctx = bk_make_context(NULL, fmt, 1);
  switch (fmt) {
  case BK_FORMAT_BMP:
    return bk_decode_bmp(data, size, &ctx, NULL, info);
  case BK_FORMAT_TGA:
    return bk_decode_tga(data, size, &ctx, NULL, info);
  case BK_FORMAT_PCX:
    return bk_decode_pcx(data, size, &ctx, NULL, info);
  case BK_FORMAT_PNM:
    return bk_decode_pnm(data, size, &ctx, NULL, info);
  case BK_FORMAT_ICO:
  case BK_FORMAT_CUR:
    return bk_decode_ico(data, size, &ctx, NULL, info);
  case BK_FORMAT_QOI:
    return bk_decode_qoi(data, size, &ctx, NULL, info);
  case BK_FORMAT_FARBFELD:
    return bk_decode_farbfeld(data, size, &ctx, NULL, info);
  case BK_FORMAT_XBM:
    return bk_decode_xbm(data, size, &ctx, NULL, info);
  case BK_FORMAT_GIF:
    return bk_decode_gif(data, size, &ctx, NULL, info);
  case BK_FORMAT_PSD:
    return bk_decode_psd(data, size, &ctx, NULL, info);
  case BK_FORMAT_TIFF:
    return bk_decode_tiff(data, size, &ctx, NULL, info);
  case BK_FORMAT_DDS:
    return bk_decode_dds(data, size, &ctx, NULL, info);
  case BK_FORMAT_RAS:
    return bk_decode_ras(data, size, &ctx, NULL, info);
  case BK_FORMAT_SGI:
    return bk_decode_sgi(data, size, &ctx, NULL, info);
  case BK_FORMAT_PNG:
    return bk_decode_png(data, size, &ctx, NULL, info);
  case BK_FORMAT_JPEG:
    return bk_decode_jpeg(data, size, &ctx, NULL, info);
  case BK_FORMAT_IFF:
    return bk_decode_iff(data, size, &ctx, NULL, info);
  case BK_FORMAT_XWD:
    return bk_decode_xwd(data, size, &ctx, NULL, info);
  case BK_FORMAT_WEBP:
    return bk_decode_webp(data, size, &ctx, NULL, info);
  case BK_FORMAT_EXR:
    return bk_decode_exr(data, size, &ctx, NULL, info);
  default:
    return BK_ERR_BAD_MAGIC;
  }
}

bk_status bk_decode_as(const uint8_t *data, size_t size, bk_format format,
                       const bk_decode_options *options, bk_image *image) {
  bk_decode_context ctx;
  if (!data || !image)
    return BK_ERR_ARGUMENT;
  memset(image, 0, sizeof(*image));
  ctx = bk_make_context(options, format, 0);
  switch (format) {
  case BK_FORMAT_BMP:
    return bk_decode_bmp(data, size, &ctx, image, NULL);
  case BK_FORMAT_TGA:
    return bk_decode_tga(data, size, &ctx, image, NULL);
  case BK_FORMAT_PCX:
    return bk_decode_pcx(data, size, &ctx, image, NULL);
  case BK_FORMAT_PNM:
    return bk_decode_pnm(data, size, &ctx, image, NULL);
  case BK_FORMAT_ICO:
  case BK_FORMAT_CUR:
    return bk_decode_ico(data, size, &ctx, image, NULL);
  case BK_FORMAT_QOI:
    return bk_decode_qoi(data, size, &ctx, image, NULL);
  case BK_FORMAT_FARBFELD:
    return bk_decode_farbfeld(data, size, &ctx, image, NULL);
  case BK_FORMAT_XBM:
    return bk_decode_xbm(data, size, &ctx, image, NULL);
  case BK_FORMAT_GIF:
    return bk_decode_gif(data, size, &ctx, image, NULL);
  case BK_FORMAT_PSD:
    return bk_decode_psd(data, size, &ctx, image, NULL);
  case BK_FORMAT_TIFF:
    return bk_decode_tiff(data, size, &ctx, image, NULL);
  case BK_FORMAT_DDS:
    return bk_decode_dds(data, size, &ctx, image, NULL);
  case BK_FORMAT_RAS:
    return bk_decode_ras(data, size, &ctx, image, NULL);
  case BK_FORMAT_SGI:
    return bk_decode_sgi(data, size, &ctx, image, NULL);
  case BK_FORMAT_PNG:
    return bk_decode_png(data, size, &ctx, image, NULL);
  case BK_FORMAT_JPEG:
    return bk_decode_jpeg(data, size, &ctx, image, NULL);
  case BK_FORMAT_IFF:
    return bk_decode_iff(data, size, &ctx, image, NULL);
  case BK_FORMAT_XWD:
    return bk_decode_xwd(data, size, &ctx, image, NULL);
  case BK_FORMAT_WEBP:
    return bk_decode_webp(data, size, &ctx, image, NULL);
  case BK_FORMAT_EXR:
    return bk_decode_exr(data, size, &ctx, image, NULL);
  default:
    return BK_ERR_BAD_MAGIC;
  }
}

bk_status bk_decode_memory(const uint8_t *data, size_t size,
                           const bk_decode_options *options, bk_image *image) {
  bk_format fmt;
  if (!data || !image)
    return BK_ERR_ARGUMENT;
  fmt = bk_detect_format(data, size);
  if (fmt == BK_FORMAT_UNKNOWN)
    return BK_ERR_BAD_MAGIC;
  return bk_decode_as(data, size, fmt, options, image);
}
