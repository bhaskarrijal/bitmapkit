#include "bitmapkit/internal.h"

static bk_status bk_recovery_add(bk_recovery_report *report, bk_format format,
                                 size_t offset, size_t estimated,
                                 uint32_t width, uint32_t height,
                                 uint32_t confidence) {
  bk_recovery_hit *next;
  if (report->count == report->capacity) {
    size_t cap = report->capacity ? report->capacity * 2u : 16u;
    if (cap > 4096u)
      return BK_ERR_LIMIT;
    next = (bk_recovery_hit *)realloc(report->hits, cap * sizeof(*next));
    if (!next)
      return BK_ERR_OOM;
    report->hits = next;
    report->capacity = cap;
  }
  report->hits[report->count].format = format;
  report->hits[report->count].offset = offset;
  report->hits[report->count].estimated_size = estimated;
  report->hits[report->count].width = width;
  report->hits[report->count].height = height;
  report->hits[report->count].confidence = confidence;
  report->count++;
  return BK_OK;
}

static void bk_try_probe_at(const uint8_t *data, size_t size, size_t off,
                            bk_recovery_report *report) {
  bk_info info;
  bk_status st;
  bk_info_clear(&info);
  st = bk_probe_memory(data + off, size - off, &info);
  if (st == BK_OK && info.width && info.height) {
    size_t estimate = 0;
    if (info.bits_per_pixel)
      estimate =
          (size_t)info.width * info.height * ((info.bits_per_pixel + 7u) / 8u);
    if (estimate > size - off)
      estimate = size - off;
    bk_recovery_add(report, info.format, off, estimate, info.width, info.height,
                    info.format == BK_FORMAT_BMP || info.format == BK_FORMAT_PNM
                        ? 90u
                        : 70u);
  }
}

bk_status bk_recover_memory(const uint8_t *data, size_t size,
                            bk_recovery_report *report) {
  size_t i;
  if (!data || !report)
    return BK_ERR_ARGUMENT;
  memset(report, 0, sizeof(*report));
  for (i = 0; i < size; ++i) {
    if (i + 2 <= size && data[i] == 'B' && data[i + 1] == 'M')
      bk_try_probe_at(data, size, i, report);
    else if (i + 2 <= size && data[i] == 'P' && data[i + 1] >= '1' &&
             data[i + 1] <= '7')
      bk_try_probe_at(data, size, i, report);
    else if (i + 4 <= size && data[i] == 0 && data[i + 1] == 0 &&
             (data[i + 2] == 1 || data[i + 2] == 2) && data[i + 3] == 0)
      bk_try_probe_at(data, size, i, report);
    else if (i + 18 <= size && data[i + 1] <= 1 &&
             (data[i + 2] == 2 || data[i + 2] == 10 || data[i + 2] == 3 ||
              data[i + 2] == 11) &&
             (data[i + 16] == 8 || data[i + 16] == 24 || data[i + 16] == 32))
      bk_try_probe_at(data, size, i, report);
    else if (i + 4 <= size && data[i] == 0x0a && data[i + 2] == 1 &&
             (data[i + 3] == 1 || data[i + 3] == 2 || data[i + 3] == 4 ||
              data[i + 3] == 8))
      bk_try_probe_at(data, size, i, report);
  }
  bk_metadata_add_u32(&report->metadata, "scan_bytes",
                      (uint32_t)(size > UINT32_MAX ? UINT32_MAX : size));
  bk_metadata_add_u32(&report->metadata, "hits", (uint32_t)report->count);
  return BK_OK;
}

void bk_recovery_report_free(bk_recovery_report *report) {
  if (!report)
    return;
  free(report->hits);
  memset(report, 0, sizeof(*report));
}
