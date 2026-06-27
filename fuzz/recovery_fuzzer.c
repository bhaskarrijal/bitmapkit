#include "bitmapkit/bitmapkit.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  bk_recovery_report report;
  if (bk_recover_memory(data, size, &report) == BK_OK)
    bk_recovery_report_free(&report);
  return 0;
}
