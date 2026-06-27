#include "bitmapkit/internal.h"

bk_status bk_sheet_compose(const bk_image *images, uint32_t count,
                           uint32_t columns, uint32_t cell_w, uint32_t cell_h,
                           bk_image *out) {
  if (!images || !out || count == 0 || columns == 0 || cell_w == 0 ||
      cell_h == 0)
    return BK_ERR_ARGUMENT;
  uint32_t rows = (count + columns - 1u) / columns;
  bk_status st = bk_image_alloc(out, columns * cell_w, rows * cell_h);
  if (st != BK_OK)
    return st;
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t ox = (i % columns) * cell_w, oy = (i / columns) * cell_h;
    uint32_t copy_w = images[i].width < cell_w ? images[i].width : cell_w;
    uint32_t copy_h = images[i].height < cell_h ? images[i].height : cell_h;
    for (uint32_t y = 0; y < copy_h; ++y)
      for (uint32_t x = 0; x < copy_w; ++x) {
        uint8_t r, g, b, a;
        bk_get_pixel(&images[i], x, y, &r, &g, &b, &a);
        bk_set_pixel(out, ox + x, oy + y, r, g, b, a);
      }
  }
  return BK_OK;
}

bk_status bk_sheet_extract(const bk_image *sheet, uint32_t columns,
                           uint32_t index, uint32_t cell_w, uint32_t cell_h,
                           bk_image *out) {
  if (!sheet || !sheet->pixels || !out || columns == 0)
    return BK_ERR_ARGUMENT;
  uint32_t x = (index % columns) * cell_w;
  uint32_t y = (index / columns) * cell_h;
  if (x + cell_w > sheet->width || y + cell_h > sheet->height)
    return BK_ERR_DIMENSIONS;
  return bk_image_crop(sheet, x, y, cell_w, cell_h, out);
}
