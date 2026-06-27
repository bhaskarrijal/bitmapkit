#include "bitmapkit/bitmapkit.h"
#include "bitmapkit/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void) {
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  bitmapkit info <file>\n");
  fprintf(stderr, "  bitmapkit validate <file>\n");
  fprintf(stderr, "  bitmapkit convert <input> <output.{bmp,tga,ppm,pnm}>\n");
  fprintf(stderr, "  bitmapkit recover <file>\n");
}

static const char *ext_of(const char *path) {
  const char *dot = strrchr(path, '.');
  return dot ? dot + 1 : "";
}

static int print_info(const char *path) {
  uint8_t *data = NULL;
  size_t size = 0;
  bk_info info;
  bk_status st = bk_read_file(path, &data, &size);
  if (st != BK_OK) {
    fprintf(stderr, "%s: %s\n", path, bk_status_string(st));
    return 1;
  }
  st = bk_probe_memory(data, size, &info);
  free(data);
  if (st != BK_OK) {
    fprintf(stderr, "%s: %s\n", path, bk_status_string(st));
    return 1;
  }
  printf("format: %s\n", bk_format_name(info.format));
  printf("size: %ux%u\n", info.width, info.height);
  printf("bits_per_pixel: %u\n", info.bits_per_pixel);
  printf("frames: %u\n", info.frame_count);
  printf("indexed: %s\n", info.indexed ? "yes" : "no");
  printf("alpha: %s\n", info.has_alpha ? "yes" : "no");
  for (size_t i = 0; i < info.metadata.count; ++i)
    printf("%s: %s\n", info.metadata.pairs[i].key,
           info.metadata.pairs[i].value);
  return 0;
}

static int validate_file(const char *path) {
  uint8_t *data = NULL;
  size_t size = 0;
  bk_image image;
  bk_status st = bk_read_file(path, &data, &size);
  if (st != BK_OK) {
    fprintf(stderr, "%s: %s\n", path, bk_status_string(st));
    return 1;
  }
  st = bk_decode_memory(data, size, NULL, &image);
  free(data);
  if (st != BK_OK) {
    fprintf(stderr, "%s: %s\n", path, bk_status_string(st));
    return 1;
  }
  printf("ok: %s %ux%u\n", bk_format_name(image.source_format), image.width,
         image.height);
  bk_image_free(&image);
  return 0;
}

static int convert_file(const char *in_path, const char *out_path) {
  uint8_t *data = NULL, *out = NULL;
  size_t size = 0, out_size = 0;
  bk_image image;
  bk_status st;
  const char *ext = ext_of(out_path);
  st = bk_read_file(in_path, &data, &size);
  if (st != BK_OK) {
    fprintf(stderr, "%s: %s\n", in_path, bk_status_string(st));
    return 1;
  }
  st = bk_decode_memory(data, size, NULL, &image);
  free(data);
  if (st != BK_OK) {
    fprintf(stderr, "%s: %s\n", in_path, bk_status_string(st));
    return 1;
  }
  if (strcmp(ext, "bmp") == 0)
    st = bk_encode_bmp(&image, &out, &out_size);
  else if (strcmp(ext, "tga") == 0)
    st = bk_encode_tga(&image, &out, &out_size);
  else if (strcmp(ext, "ppm") == 0 || strcmp(ext, "pnm") == 0)
    st = bk_encode_pnm(&image, &out, &out_size);
  else
    st = BK_ERR_UNSUPPORTED;
  if (st == BK_OK)
    st = bk_write_file(out_path, out, out_size);
  free(out);
  bk_image_free(&image);
  if (st != BK_OK) {
    fprintf(stderr, "%s: %s\n", out_path, bk_status_string(st));
    return 1;
  }
  return 0;
}

static int recover_file(const char *path) {
  uint8_t *data = NULL;
  size_t size = 0;
  bk_recovery_report report;
  bk_status st = bk_read_file(path, &data, &size);
  if (st != BK_OK) {
    fprintf(stderr, "%s: %s\n", path, bk_status_string(st));
    return 1;
  }
  st = bk_recover_memory(data, size, &report);
  free(data);
  if (st != BK_OK) {
    fprintf(stderr, "%s: %s\n", path, bk_status_string(st));
    return 1;
  }
  for (size_t i = 0; i < report.count; ++i) {
    printf("%zu: offset=%zu format=%s size=%ux%u confidence=%u\n", i,
           report.hits[i].offset, bk_format_name(report.hits[i].format),
           report.hits[i].width, report.hits[i].height,
           report.hits[i].confidence);
  }
  bk_recovery_report_free(&report);
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    usage();
    return 2;
  }
  if (strcmp(argv[1], "info") == 0 && argc == 3)
    return print_info(argv[2]);
  if (strcmp(argv[1], "validate") == 0 && argc == 3)
    return validate_file(argv[2]);
  if (strcmp(argv[1], "convert") == 0 && argc == 4)
    return convert_file(argv[2], argv[3]);
  if (strcmp(argv[1], "recover") == 0 && argc == 3)
    return recover_file(argv[2]);
  usage();
  return 2;
}
