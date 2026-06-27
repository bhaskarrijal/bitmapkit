#include "bitmapkit/internal.h"

static bk_status append_json_string(bk_buffer *b, const char *s) {
  bk_buffer_append_u8(b, '"');
  for (; s && *s; ++s) {
    if (*s == '"' || *s == '\\') {
      bk_buffer_append_u8(b, '\\');
      bk_buffer_append_u8(b, (uint8_t)*s);
    } else if ((unsigned char)*s < 32) {
      char tmp[8];
      snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned char)*s);
      bk_buffer_append(b, tmp, strlen(tmp));
    } else
      bk_buffer_append_u8(b, (uint8_t)*s);
  }
  bk_buffer_append_u8(b, '"');
  return BK_OK;
}

bk_status bk_info_to_json(const bk_info *info, char **out) {
  bk_buffer b;
  char num[96];
  if (!info || !out)
    return BK_ERR_ARGUMENT;
  bk_buffer_init(&b, 512);
  bk_buffer_append_u8(&b, '{');
  bk_buffer_append(&b, "\"format\":", 9);
  append_json_string(&b, bk_format_name(info->format));
  snprintf(num, sizeof(num),
           ",\"width\":%u,\"height\":%u,\"bits_per_pixel\":%u,\"frames\":%u",
           info->width, info->height, info->bits_per_pixel, info->frame_count);
  bk_buffer_append(&b, num, strlen(num));
  bk_buffer_append(&b, ",\"metadata\":{", 13);
  for (size_t i = 0; i < info->metadata.count; ++i) {
    if (i)
      bk_buffer_append_u8(&b, ',');
    append_json_string(&b, info->metadata.pairs[i].key);
    bk_buffer_append_u8(&b, ':');
    append_json_string(&b, info->metadata.pairs[i].value);
  }
  bk_buffer_append(&b, "}}", 2);
  bk_buffer_append_u8(&b, 0);
  *out = (char *)b.data;
  return BK_OK;
}

bk_status bk_recovery_to_text(const bk_recovery_report *report, char **out) {
  bk_buffer b;
  char line[192];
  if (!report || !out)
    return BK_ERR_ARGUMENT;
  bk_buffer_init(&b, 512);
  for (size_t i = 0; i < report->count; ++i) {
    snprintf(line, sizeof(line), "%zu\t%zu\t%s\t%ux%u\t%u\n", i,
             report->hits[i].offset, bk_format_name(report->hits[i].format),
             report->hits[i].width, report->hits[i].height,
             report->hits[i].confidence);
    bk_buffer_append(&b, line, strlen(line));
  }
  bk_buffer_append_u8(&b, 0);
  *out = (char *)b.data;
  return BK_OK;
}
