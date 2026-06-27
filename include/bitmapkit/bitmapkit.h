#ifndef BITMAPKIT_BITMAPKIT_H
#define BITMAPKIT_BITMAPKIT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BK_VERSION_MAJOR 0
#define BK_VERSION_MINOR 1
#define BK_VERSION_PATCH 0

#define BK_MAX_DIMENSION 65535u
#define BK_MAX_PIXELS 268435456u
#define BK_MAX_METADATA 64u
#define BK_MAX_WARNINGS 128u
#define BK_MAX_FRAMES 256u

typedef enum bk_status {
  BK_OK = 0,
  BK_ERR_ARGUMENT = 1,
  BK_ERR_OOM = 2,
  BK_ERR_TRUNCATED = 3,
  BK_ERR_BAD_MAGIC = 4,
  BK_ERR_UNSUPPORTED = 5,
  BK_ERR_DIMENSIONS = 6,
  BK_ERR_CORRUPT = 7,
  BK_ERR_LIMIT = 8,
  BK_ERR_IO = 9,
  BK_ERR_INTERNAL = 10
} bk_status;

typedef enum bk_format {
  BK_FORMAT_UNKNOWN = 0,
  BK_FORMAT_BMP,
  BK_FORMAT_TGA,
  BK_FORMAT_PCX,
  BK_FORMAT_PNM,
  BK_FORMAT_ICO,
  BK_FORMAT_CUR,
  BK_FORMAT_QOI,
  BK_FORMAT_FARBFELD,
  BK_FORMAT_XBM,
  BK_FORMAT_GIF,
  BK_FORMAT_PSD,
  BK_FORMAT_TIFF,
  BK_FORMAT_DDS,
  BK_FORMAT_RAS,
  BK_FORMAT_SGI,
  BK_FORMAT_PNG,
  BK_FORMAT_JPEG,
  BK_FORMAT_IFF,
  BK_FORMAT_XWD,
  BK_FORMAT_WEBP,
  BK_FORMAT_EXR
} bk_format;

typedef enum bk_pixel_format { BK_PIXEL_RGBA8 = 1 } bk_pixel_format;

typedef enum bk_severity {
  BK_NOTE = 0,
  BK_WARNING = 1,
  BK_ERROR = 2
} bk_severity;

typedef struct bk_warning {
  bk_severity severity;
  char code[32];
  char message[160];
  size_t offset;
} bk_warning;

typedef struct bk_metadata_pair {
  char key[48];
  char value[160];
} bk_metadata_pair;

typedef struct bk_metadata {
  bk_metadata_pair pairs[BK_MAX_METADATA];
  size_t count;
  bk_warning warnings[BK_MAX_WARNINGS];
  size_t warning_count;
} bk_metadata;

typedef struct bk_image {
  uint32_t width;
  uint32_t height;
  uint32_t stride;
  bk_pixel_format format;
  uint8_t *pixels;
  size_t pixel_size;
  bk_format source_format;
  bk_metadata metadata;
} bk_image;

typedef struct bk_decode_options {
  uint32_t max_width;
  uint32_t max_height;
  uint32_t max_pixels;
  int keep_metadata;
  int tolerate_repair;
} bk_decode_options;

typedef struct bk_info {
  bk_format format;
  uint32_t width;
  uint32_t height;
  uint32_t bits_per_pixel;
  uint32_t frame_count;
  int has_alpha;
  int indexed;
  bk_metadata metadata;
} bk_info;

typedef struct bk_recovery_hit {
  bk_format format;
  size_t offset;
  size_t estimated_size;
  uint32_t width;
  uint32_t height;
  uint32_t confidence;
} bk_recovery_hit;

typedef struct bk_recovery_report {
  bk_recovery_hit *hits;
  size_t count;
  size_t capacity;
  bk_metadata metadata;
} bk_recovery_report;

const char *bk_status_string(bk_status status);
const char *bk_format_name(bk_format format);
void bk_default_options(bk_decode_options *options);

bk_format bk_detect_format(const uint8_t *data, size_t size);
bk_status bk_probe_memory(const uint8_t *data, size_t size, bk_info *info);
bk_status bk_decode_memory(const uint8_t *data, size_t size,
                           const bk_decode_options *options, bk_image *image);
bk_status bk_decode_as(const uint8_t *data, size_t size, bk_format format,
                       const bk_decode_options *options, bk_image *image);
void bk_image_free(bk_image *image);
void bk_info_clear(bk_info *info);
void bk_metadata_clear(bk_metadata *metadata);
bk_status bk_metadata_add(bk_metadata *metadata, const char *key,
                          const char *value);
bk_status bk_metadata_add_u32(bk_metadata *metadata, const char *key,
                              uint32_t value);
bk_status bk_metadata_warn(bk_metadata *metadata, bk_severity severity,
                           const char *code, const char *message,
                           size_t offset);

bk_status bk_recover_memory(const uint8_t *data, size_t size,
                            bk_recovery_report *report);
void bk_recovery_report_free(bk_recovery_report *report);

bk_status bk_encode_bmp(const bk_image *image, uint8_t **out, size_t *out_size);
bk_status bk_encode_tga(const bk_image *image, uint8_t **out, size_t *out_size);
bk_status bk_encode_pnm(const bk_image *image, uint8_t **out, size_t *out_size);

bk_status bk_image_flip_vertical(bk_image *image);
bk_status bk_image_to_grayscale(bk_image *image);
bk_status bk_image_premultiply_alpha(bk_image *image);
bk_status bk_image_unpremultiply_alpha(bk_image *image);
bk_status bk_image_crop(const bk_image *source, uint32_t x, uint32_t y,
                        uint32_t width, uint32_t height, bk_image *out);

#ifdef __cplusplus
}
#endif

#endif
