#include "bitmapkit/internal.h"

uint32_t bk_crc32(const uint8_t *data, size_t size) {
  uint32_t crc = 0xffffffffu;
  size_t i;
  for (i = 0; i < size; ++i) {
    unsigned bit;
    crc ^= data[i];
    for (bit = 0; bit < 8; ++bit) {
      uint32_t mask = 0u - (crc & 1u);
      crc = (crc >> 1) ^ (0xedb88320u & mask);
    }
  }
  return ~crc;
}

uint32_t bk_adler32(const uint8_t *data, size_t size) {
  uint32_t a = 1;
  uint32_t b = 0;
  size_t i;
  for (i = 0; i < size; ++i) {
    a = (a + data[i]) % 65521u;
    b = (b + a) % 65521u;
  }
  return (b << 16) | a;
}
