#include "bitmapkit/bitmapkit.h"
#include "bitmapkit/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require(int cond, const char *msg) {
  if (!cond) {
    fprintf(stderr, "test failed: %s\n", msg);
    exit(1);
  }
}

static void make_image(bk_image *image) {
  require(bk_image_alloc(image, 2, 2) == BK_OK, "alloc image");
  bk_set_pixel(image, 0, 0, 255, 0, 0, 255);
  bk_set_pixel(image, 1, 0, 0, 255, 0, 255);
  bk_set_pixel(image, 0, 1, 0, 0, 255, 255);
  bk_set_pixel(image, 1, 1, 255, 255, 255, 128);
}

static void roundtrip_bmp(void) {
  bk_image image, decoded;
  uint8_t *data = NULL;
  size_t size = 0;
  make_image(&image);
  require(bk_encode_bmp(&image, &data, &size) == BK_OK, "encode bmp");
  require(bk_detect_format(data, size) == BK_FORMAT_BMP, "detect bmp");
  require(bk_decode_memory(data, size, NULL, &decoded) == BK_OK, "decode bmp");
  require(decoded.width == 2 && decoded.height == 2, "bmp dimensions");
  bk_image_free(&decoded);
  bk_image_free(&image);
  free(data);
}

static void roundtrip_tga(void) {
  bk_image image, decoded;
  uint8_t *data = NULL;
  size_t size = 0;
  make_image(&image);
  require(bk_encode_tga(&image, &data, &size) == BK_OK, "encode tga");
  require(bk_detect_format(data, size) == BK_FORMAT_TGA, "detect tga");
  require(bk_decode_memory(data, size, NULL, &decoded) == BK_OK, "decode tga");
  require(decoded.width == 2 && decoded.height == 2, "tga dimensions");
  bk_image_free(&decoded);
  bk_image_free(&image);
  free(data);
}

static void decode_pnm(void) {
  static const unsigned char ppm[] = "P6\n2 1\n255\n\xff\0\0\0\xff\0";
  bk_image image;
  require(bk_detect_format(ppm, sizeof(ppm) - 1) == BK_FORMAT_PNM,
          "detect pnm");
  require(bk_decode_memory(ppm, sizeof(ppm) - 1, NULL, &image) == BK_OK,
          "decode pnm");
  require(image.width == 2 && image.height == 1, "pnm dimensions");
  bk_image_free(&image);
}

static void recover_embedded(void) {
  bk_image image;
  uint8_t *bmp = NULL;
  size_t bmp_size = 0;
  unsigned char junk[512];
  bk_recovery_report report;
  make_image(&image);
  require(bk_encode_bmp(&image, &bmp, &bmp_size) == BK_OK,
          "encode recovery bmp");
  memset(junk, 0xa5, sizeof(junk));
  require(80 + bmp_size < sizeof(junk), "fixture fits");
  memcpy(junk + 80, bmp, bmp_size);
  require(bk_recover_memory(junk, sizeof(junk), &report) == BK_OK,
          "recover memory");
  require(report.count >= 1, "recovery hit");
  bk_recovery_report_free(&report);
  bk_image_free(&image);
  free(bmp);
}

int main(void) {
  roundtrip_bmp();
  roundtrip_tga();
  decode_pnm();
  recover_embedded();
  return 0;
}
