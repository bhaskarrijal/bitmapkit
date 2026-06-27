#include "bitmapkit/internal.h"

typedef struct bk_atlas_tile {
  char name[64];
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
  uint32_t flags;
} bk_atlas_tile;

typedef struct bk_atlas {
  bk_atlas_tile tiles[512];
  uint32_t count;
  uint32_t image_width;
  uint32_t image_height;
} bk_atlas;

bk_status bk_atlas_grid(uint32_t image_width, uint32_t image_height,
                        uint32_t tile_width, uint32_t tile_height,
                        bk_atlas *atlas) {
  uint32_t x, y;
  if (!atlas || tile_width == 0 || tile_height == 0)
    return BK_ERR_ARGUMENT;
  memset(atlas, 0, sizeof(*atlas));
  atlas->image_width = image_width;
  atlas->image_height = image_height;
  for (y = 0; y + tile_height <= image_height; y += tile_height) {
    for (x = 0; x + tile_width <= image_width; x += tile_width) {
      bk_atlas_tile *tile;
      if (atlas->count >= 512u)
        return BK_ERR_LIMIT;
      tile = &atlas->tiles[atlas->count];
      snprintf(tile->name, sizeof(tile->name), "tile_%u_%u", x / tile_width,
               y / tile_height);
      tile->x = x;
      tile->y = y;
      tile->width = tile_width;
      tile->height = tile_height;
      atlas->count++;
    }
  }
  return BK_OK;
}

bk_status bk_atlas_extract_tile(const bk_image *image,
                                const bk_atlas_tile *tile, bk_image *out) {
  if (!image || !tile || !out)
    return BK_ERR_ARGUMENT;
  return bk_image_crop(image, tile->x, tile->y, tile->width, tile->height, out);
}

bk_status bk_atlas_validate(const bk_atlas *atlas) {
  uint32_t i, j;
  if (!atlas)
    return BK_ERR_ARGUMENT;
  for (i = 0; i < atlas->count; ++i) {
    const bk_atlas_tile *a = &atlas->tiles[i];
    if (a->width == 0 || a->height == 0)
      return BK_ERR_DIMENSIONS;
    if (a->x > atlas->image_width || a->y > atlas->image_height)
      return BK_ERR_DIMENSIONS;
    if (a->width > atlas->image_width - a->x ||
        a->height > atlas->image_height - a->y)
      return BK_ERR_DIMENSIONS;
    for (j = i + 1u; j < atlas->count; ++j) {
      const bk_atlas_tile *b = &atlas->tiles[j];
      int apart = a->x + a->width <= b->x || b->x + b->width <= a->x ||
                  a->y + a->height <= b->y || b->y + b->height <= a->y;
      if (!apart)
        return BK_ERR_CORRUPT;
    }
  }
  return BK_OK;
}

bk_status bk_atlas_parse_text(const char *text, bk_atlas *atlas) {
  const char *p = text;
  if (!text || !atlas)
    return BK_ERR_ARGUMENT;
  memset(atlas, 0, sizeof(*atlas));
  while (*p) {
    bk_atlas_tile *tile;
    int n;
    if (*p == '#') {
      while (*p && *p != '\n')
        ++p;
    }
    while (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t')
      ++p;
    if (!*p)
      break;
    if (atlas->count >= 512u)
      return BK_ERR_LIMIT;
    tile = &atlas->tiles[atlas->count];
    n = sscanf(p, "%63s %u %u %u %u", tile->name, &tile->x, &tile->y,
               &tile->width, &tile->height);
    if (n != 5)
      return BK_ERR_CORRUPT;
    if (tile->x + tile->width > atlas->image_width)
      atlas->image_width = tile->x + tile->width;
    if (tile->y + tile->height > atlas->image_height)
      atlas->image_height = tile->y + tile->height;
    atlas->count++;
    while (*p && *p != '\n')
      ++p;
  }
  return BK_OK;
}
