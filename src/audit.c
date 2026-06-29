#include "bitmapkit/internal.h"
#include "bitmapkit/ops.h"

static void bk_audit_add(bk_audit_report *report, bk_severity severity,
                         const char *rule, const char *message, size_t offset) {
  bk_audit_entry *entry;
  if (!report || report->count >= 256u)
    return;
  entry = &report->entries[report->count++];
  entry->severity = severity;
  entry->offset = offset;
  snprintf(entry->rule, sizeof(entry->rule), "%s", rule ? rule : "rule");
  snprintf(entry->message, sizeof(entry->message), "%s",
           message ? message : "");
}

static uint16_t rd16le(const uint8_t *p) {
  return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t rd32le(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}
static uint16_t rd16be(const uint8_t *p) {
  return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static const void *bk_memmem_local(const void *haystack, size_t haystack_len,
                                   const void *needle, size_t needle_len) {
  const uint8_t *h = (const uint8_t *)haystack;
  const uint8_t *n = (const uint8_t *)needle;
  size_t i;
  if (needle_len == 0)
    return haystack;
  if (!haystack || !needle || needle_len > haystack_len)
    return NULL;
  for (i = 0; i + needle_len <= haystack_len; ++i) {
    if (memcmp(h + i, n, needle_len) == 0)
      return h + i;
  }
  return NULL;
}

static void audit_common_tail(const uint8_t *data, size_t size,
                              bk_audit_report *report) {
  size_t i;
  size_t zeros = 0;
  size_t ff = 0;
  for (i = 0; i < size; ++i) {
    zeros += data[i] == 0;
    ff += data[i] == 255;
  }
  if (size > 64 && zeros > size * 9u / 10u)
    bk_audit_add(report, BK_WARNING, "mostly-zero",
                 "file is dominated by zero bytes", 0);
  if (size > 64 && ff > size * 9u / 10u)
    bk_audit_add(report, BK_WARNING, "mostly-ff",
                 "file is dominated by 0xff bytes", 0);
  if (size == 0)
    bk_audit_add(report, BK_ERROR, "empty", "input is empty", 0);
}

static void audit_bmp(const uint8_t *data, size_t size,
                      bk_audit_report *report) {
  uint32_t file_size, off, dib, width, height, compression, image_size, colors;
  uint16_t planes, bpp;
  if (size < 14) {
    bk_audit_add(report, BK_ERROR, "bmp-short-file-header",
                 "BMP file header is truncated", size);
    return;
  }
  file_size = rd32le(data + 2);
  off = rd32le(data + 10);
  if (file_size && file_size != size)
    bk_audit_add(report, BK_NOTE, "bmp-size-mismatch",
                 "file size field does not match actual byte count", 2);
  if (size < 18) {
    bk_audit_add(report, BK_ERROR, "bmp-short-dib", "DIB header is truncated",
                 size);
    return;
  }
  dib = rd32le(data + 14);
  if (!(dib == 12 || dib == 40 || dib == 52 || dib == 56 || dib == 108 ||
        dib == 124))
    bk_audit_add(report, BK_WARNING, "bmp-unusual-dib",
                 "DIB header size is uncommon", 14);
  if (dib < 12) {
    bk_audit_add(report, BK_ERROR, "bmp-bad-dib-size",
                 "DIB header is too small", 14);
    return;
  }
  if (dib > size - 14u) {
    bk_audit_add(report, BK_ERROR, "bmp-dib-past-eof",
                 "DIB header extends past end of file", 14);
    return;
  }
  if (dib == 12) {
    width = rd16le(data + 18);
    height = rd16le(data + 20);
    planes = rd16le(data + 22);
    bpp = rd16le(data + 24);
    compression = 0;
    image_size = 0;
    colors = 0;
  } else {
    if (dib < 40) {
      bk_audit_add(report, BK_ERROR, "bmp-short-info-dib",
                   "BMP info header is too short", 14);
      return;
    }
    width = rd32le(data + 18);
    height = rd32le(data + 22);
    planes = rd16le(data + 26);
    bpp = rd16le(data + 28);
    compression = rd32le(data + 30);
    image_size = rd32le(data + 34);
    colors = rd32le(data + 46);
  }
  if (width == 0 || height == 0)
    bk_audit_add(report, BK_ERROR, "bmp-zero-dimension",
                 "BMP width or height is zero", 18);
  if (width > BK_MAX_DIMENSION || height > BK_MAX_DIMENSION)
    bk_audit_add(report, BK_WARNING, "bmp-large-dimension",
                 "BMP dimensions exceed normal asset limits", 18);
  if (planes != 1)
    bk_audit_add(report, BK_ERROR, "bmp-planes", "BMP planes field is not one",
                 dib == 12 ? 22 : 26);
  if (!(bpp == 1 || bpp == 4 || bpp == 8 || bpp == 16 || bpp == 24 ||
        bpp == 32))
    bk_audit_add(report, BK_ERROR, "bmp-bpp", "BMP bit depth is unsupported",
                 dib == 12 ? 24 : 28);
  if (compression > 6)
    bk_audit_add(report, BK_ERROR, "bmp-compression",
                 "BMP compression mode is unsupported", 30);
  if (compression == 1 && bpp != 8)
    bk_audit_add(report, BK_ERROR, "bmp-rle8-bpp",
                 "RLE8 compression requires 8-bit pixels", 30);
  if (compression == 2 && bpp != 4)
    bk_audit_add(report, BK_ERROR, "bmp-rle4-bpp",
                 "RLE4 compression requires 4-bit pixels", 30);
  if (off < 14u + dib)
    bk_audit_add(report, BK_ERROR, "bmp-pixels-overlap-header",
                 "pixel array offset overlaps the header", 10);
  if (off > size)
    bk_audit_add(report, BK_ERROR, "bmp-pixels-past-eof",
                 "pixel array offset is past end of file", 10);
  if (colors > 256 && bpp <= 8)
    bk_audit_add(report, BK_WARNING, "bmp-large-palette",
                 "palette entry count is larger than expected", 46);
  if (image_size && (off > size || image_size > size - off))
    bk_audit_add(report, BK_WARNING, "bmp-image-size-overflow",
                 "declared image data extends past end of file", 34);
}

static void audit_tga(const uint8_t *data, size_t size,
                      bk_audit_report *report) {
  uint8_t idlen, cmap, type, cmap_depth, depth, desc;
  uint16_t cmap_first, cmap_count, width, height;
  size_t offset;
  if (size < 18) {
    bk_audit_add(report, BK_ERROR, "tga-short-header",
                 "TGA header is truncated", size);
    return;
  }
  idlen = data[0];
  cmap = data[1];
  type = data[2];
  cmap_first = rd16le(data + 3);
  cmap_count = rd16le(data + 5);
  cmap_depth = data[7];
  width = rd16le(data + 12);
  height = rd16le(data + 14);
  depth = data[16];
  desc = data[17];
  if (cmap > 1)
    bk_audit_add(report, BK_ERROR, "tga-cmap-type",
                 "TGA color-map type is not 0 or 1", 1);
  if (!(type == 1 || type == 2 || type == 3 || type == 9 || type == 10 ||
        type == 11))
    bk_audit_add(report, BK_ERROR, "tga-type", "TGA image type is unsupported",
                 2);
  if (width == 0 || height == 0)
    bk_audit_add(report, BK_ERROR, "tga-zero-dimension",
                 "TGA width or height is zero", 12);
  if (!(depth == 8 || depth == 15 || depth == 16 || depth == 24 || depth == 32))
    bk_audit_add(report, BK_ERROR, "tga-depth",
                 "TGA pixel depth is unsupported", 16);
  if (cmap == 0 && cmap_count != 0)
    bk_audit_add(report, BK_WARNING, "tga-cmap-without-type",
                 "color map length is set while map type is zero", 5);
  if (cmap == 1 && cmap_count == 0)
    bk_audit_add(report, BK_ERROR, "tga-empty-cmap",
                 "color-mapped TGA declares no palette entries", 5);
  if (cmap_depth && !(cmap_depth == 15 || cmap_depth == 16 ||
                      cmap_depth == 24 || cmap_depth == 32))
    bk_audit_add(report, BK_WARNING, "tga-cmap-depth",
                 "palette entry depth is unusual", 7);
  if (desc & 0xc0u)
    bk_audit_add(report, BK_WARNING, "tga-desc-reserved",
                 "descriptor reserved bits are set", 17);
  offset = 18u + idlen;
  if (offset > size)
    bk_audit_add(report, BK_ERROR, "tga-id-past-eof",
                 "image id field extends past end of file", 0);
  if (cmap == 1) {
    size_t cmap_bytes = ((size_t)cmap_count * ((size_t)cmap_depth + 7u)) / 8u;
    if (offset + cmap_bytes > size)
      bk_audit_add(report, BK_ERROR, "tga-cmap-past-eof",
                   "color map extends past end of file", 5);
    if (cmap_first && cmap_first + cmap_count > 256u)
      bk_audit_add(report, BK_WARNING, "tga-cmap-index-range",
                   "palette range exceeds 8-bit indexes", 3);
  }
}

static void audit_pcx(const uint8_t *data, size_t size,
                      bk_audit_report *report) {
  uint16_t xmin, ymin, xmax, ymax, bpl;
  uint8_t version, encoding, bpp, planes;
  if (size < 128) {
    bk_audit_add(report, BK_ERROR, "pcx-short-header",
                 "PCX header is truncated", size);
    return;
  }
  version = data[1];
  encoding = data[2];
  bpp = data[3];
  xmin = rd16le(data + 4);
  ymin = rd16le(data + 6);
  xmax = rd16le(data + 8);
  ymax = rd16le(data + 10);
  planes = data[65];
  bpl = rd16le(data + 66);
  if (data[0] != 0x0a)
    bk_audit_add(report, BK_ERROR, "pcx-manufacturer",
                 "PCX manufacturer byte is not 0x0a", 0);
  if (!(version == 0 || version == 2 || version == 3 || version == 4 ||
        version == 5))
    bk_audit_add(report, BK_WARNING, "pcx-version",
                 "PCX version byte is uncommon", 1);
  if (encoding > 1)
    bk_audit_add(report, BK_ERROR, "pcx-encoding",
                 "PCX encoding is unsupported", 2);
  if (!(bpp == 1 || bpp == 2 || bpp == 4 || bpp == 8))
    bk_audit_add(report, BK_ERROR, "pcx-bpp",
                 "PCX bits per plane is unsupported", 3);
  if (xmax < xmin || ymax < ymin)
    bk_audit_add(report, BK_ERROR, "pcx-bounds",
                 "PCX image bounds are inverted", 4);
  if (planes == 0 || planes > 4)
    bk_audit_add(report, BK_ERROR, "pcx-planes",
                 "PCX plane count is unsupported", 65);
  if (bpl == 0)
    bk_audit_add(report, BK_ERROR, "pcx-bytes-per-line",
                 "PCX bytes-per-line field is zero", 66);
  if (size >= 769 && data[size - 769] == 12 && !(bpp == 8 && planes == 1))
    bk_audit_add(report, BK_NOTE, "pcx-extra-vga-palette",
                 "VGA palette marker appears on a non-8-bit indexed PCX",
                 size - 769);
}

static void audit_pnm(const uint8_t *data, size_t size,
                      bk_audit_report *report) {
  size_t i = 0;
  unsigned token_count = 0;
  if (size < 2) {
    bk_audit_add(report, BK_ERROR, "pnm-short", "PNM magic is truncated", size);
    return;
  }
  if (data[0] != 'P' || data[1] < '1' || data[1] > '7')
    bk_audit_add(report, BK_ERROR, "pnm-magic", "PNM magic is invalid", 0);
  while (i < size && token_count < 8) {
    while (i < size && (data[i] == ' ' || data[i] == '\n' || data[i] == '\r' ||
                        data[i] == '\t'))
      ++i;
    if (i < size && data[i] == '#') {
      while (i < size && data[i] != '\n')
        ++i;
      continue;
    }
    if (i < size) {
      ++token_count;
      while (i < size && !(data[i] == ' ' || data[i] == '\n' ||
                           data[i] == '\r' || data[i] == '\t'))
        ++i;
    }
  }
  if (data[1] == '7') {
    if (size < 12 || !bk_memmem_local(data, size, "ENDHDR", 6))
      bk_audit_add(report, BK_ERROR, "pam-endhdr",
                   "PAM header has no ENDHDR marker", 0);
  } else if (token_count < 4 && data[1] != '1' && data[1] != '4') {
    bk_audit_add(report, BK_ERROR, "pnm-header-tokens",
                 "PNM header does not contain width, height, and max value", 0);
  }
}

static void audit_ico(const uint8_t *data, size_t size,
                      bk_audit_report *report) {
  uint16_t type, count;
  uint16_t i;
  if (size < 6) {
    bk_audit_add(report, BK_ERROR, "ico-short-header",
                 "ICO/CUR header is truncated", size);
    return;
  }
  type = rd16le(data + 2);
  count = rd16le(data + 4);
  if (rd16le(data) != 0)
    bk_audit_add(report, BK_ERROR, "ico-reserved",
                 "ICO reserved field is not zero", 0);
  if (!(type == 1 || type == 2))
    bk_audit_add(report, BK_ERROR, "ico-type",
                 "ICO type is neither icon nor cursor", 2);
  if (count == 0)
    bk_audit_add(report, BK_ERROR, "ico-empty", "ICO/CUR contains no images",
                 4);
  if (count > 256)
    bk_audit_add(report, BK_ERROR, "ico-count",
                 "ICO/CUR image count is too large", 4);
  if (size < 6u + (size_t)count * 16u) {
    bk_audit_add(report, BK_ERROR, "ico-dir-truncated",
                 "ICO directory is truncated", size);
    return;
  }
  for (i = 0; i < count && i < 256; ++i) {
    size_t off = 6u + (size_t)i * 16u;
    uint32_t bytes = rd32le(data + off + 8u);
    uint32_t img_off = rd32le(data + off + 12u);
    if (bytes == 0)
      bk_audit_add(report, BK_ERROR, "ico-entry-empty",
                   "ICO directory entry has zero payload bytes", off + 8u);
    if (img_off < 6u + (size_t)count * 16u)
      bk_audit_add(report, BK_ERROR, "ico-entry-overlap",
                   "ICO image payload overlaps the directory", off + 12u);
    if ((uint64_t)img_off + bytes > size)
      bk_audit_add(report, BK_ERROR, "ico-entry-past-eof",
                   "ICO image payload extends past end of file", off + 12u);
  }
}

bk_status bk_audit_memory(const uint8_t *data, size_t size,
                          bk_audit_report *report) {
  bk_format fmt;
  if (!data || !report)
    return BK_ERR_ARGUMENT;
  memset(report, 0, sizeof(*report));
  fmt = bk_detect_format(data, size);
  report->detected_format = fmt;
  audit_common_tail(data, size, report);
  switch (fmt) {
  case BK_FORMAT_BMP:
    audit_bmp(data, size, report);
    break;
  case BK_FORMAT_TGA:
    audit_tga(data, size, report);
    break;
  case BK_FORMAT_PCX:
    audit_pcx(data, size, report);
    break;
  case BK_FORMAT_PNM:
    audit_pnm(data, size, report);
    break;
  case BK_FORMAT_ICO:
  case BK_FORMAT_CUR:
    audit_ico(data, size, report);
    break;
  default:
    bk_audit_add(report, BK_ERROR, "unknown-format",
                 "bitmapkit did not recognize this input", 0);
    break;
  }
  return BK_OK;
}
