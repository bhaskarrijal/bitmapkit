#include "bitmapkit/bitmapkit.h"
#include "bitmapkit/ops.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  bk_info info;
  bk_audit_report report;
  (void)bk_probe_memory(data, size, &info);
  (void)bk_audit_memory(data, size, &report);
  return 0;
}
