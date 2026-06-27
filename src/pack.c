#include "bitmapkit/internal.h"

typedef struct bk_pack_rect {
  uint32_t id, w, h, x, y;
  int placed;
} bk_pack_rect;
typedef struct bk_pack_node {
  uint32_t x, y, w, h;
} bk_pack_node;
typedef struct bk_pack_result {
  uint32_t width, height, placed, failed;
} bk_pack_result;

static int rect_fits(const bk_pack_node *n, const bk_pack_rect *r) {
  return r->w <= n->w && r->h <= n->h;
}

static void split_node(bk_pack_node *nodes, uint32_t *count, uint32_t max_nodes,
                       bk_pack_node n, const bk_pack_rect *r) {
  if (*count + 2u > max_nodes)
    return;
  if (n.w > r->w) {
    nodes[(*count)++] = (bk_pack_node){n.x + r->w, n.y, n.w - r->w, r->h};
  }
  if (n.h > r->h) {
    nodes[(*count)++] = (bk_pack_node){n.x, n.y + r->h, n.w, n.h - r->h};
  }
}

static void prune_nodes(bk_pack_node *nodes, uint32_t *count) {
  for (uint32_t i = 0; i < *count; ++i) {
    for (uint32_t j = i + 1u; j < *count; ++j) {
      bk_pack_node a = nodes[i], b = nodes[j];
      int a_in_b = a.x >= b.x && a.y >= b.y && a.x + a.w <= b.x + b.w &&
                   a.y + a.h <= b.y + b.h;
      int b_in_a = b.x >= a.x && b.y >= a.y && b.x + b.w <= a.x + a.w &&
                   b.y + b.h <= a.y + a.h;
      if (a_in_b) {
        nodes[i] = nodes[--(*count)];
        --i;
        break;
      }
      if (b_in_a) {
        nodes[j] = nodes[--(*count)];
        --j;
      }
    }
  }
}

bk_status bk_pack_rects(bk_pack_rect *rects, uint32_t rect_count,
                        uint32_t width, uint32_t height,
                        bk_pack_result *result) {
  bk_pack_node nodes[1024];
  uint32_t node_count = 1;
  if (!rects || !result || width == 0 || height == 0)
    return BK_ERR_ARGUMENT;
  memset(result, 0, sizeof(*result));
  result->width = width;
  result->height = height;
  nodes[0] = (bk_pack_node){0, 0, width, height};
  for (uint32_t i = 0; i < rect_count; ++i) {
    uint32_t best = UINT32_MAX, best_area = UINT32_MAX;
    for (uint32_t n = 0; n < node_count; ++n)
      if (rect_fits(&nodes[n], &rects[i])) {
        uint32_t area = nodes[n].w * nodes[n].h;
        if (area < best_area) {
          best_area = area;
          best = n;
        }
      }
    if (best == UINT32_MAX) {
      result->failed++;
      continue;
    }
    bk_pack_node node = nodes[best];
    rects[i].x = node.x;
    rects[i].y = node.y;
    rects[i].placed = 1;
    result->placed++;
    nodes[best] = nodes[--node_count];
    split_node(nodes, &node_count, 1024, node, &rects[i]);
    prune_nodes(nodes, &node_count);
  }
  return result->failed ? BK_ERR_LIMIT : BK_OK;
}

bk_status bk_pack_sort_by_area(bk_pack_rect *rects, uint32_t count) {
  if (!rects)
    return BK_ERR_ARGUMENT;
  for (uint32_t i = 1; i < count; ++i) {
    bk_pack_rect key = rects[i];
    uint32_t j = i;
    while (j > 0 && rects[j - 1u].w * rects[j - 1u].h < key.w * key.h) {
      rects[j] = rects[j - 1u];
      --j;
    }
    rects[j] = key;
  }
  return BK_OK;
}
