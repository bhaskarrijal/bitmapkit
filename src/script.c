#include "bitmapkit/internal.h"
#include "bitmapkit/ops.h"
#include <ctype.h>

typedef enum bk_script_op {
  BK_SCRIPT_NONE = 0,
  BK_SCRIPT_GRAYSCALE,
  BK_SCRIPT_FLIP,
  BK_SCRIPT_MIRROR,
  BK_SCRIPT_ROTATE90,
  BK_SCRIPT_ROTATE180,
  BK_SCRIPT_ROTATE270,
  BK_SCRIPT_RESIZE,
  BK_SCRIPT_CROP,
  BK_SCRIPT_PREMULTIPLY,
  BK_SCRIPT_UNPREMULTIPLY,
  BK_SCRIPT_PALETTE
} bk_script_op;

typedef struct bk_script_step {
  bk_script_op op;
  uint32_t args[4];
  char text[64];
} bk_script_step;

typedef struct bk_script_plan {
  bk_script_step steps[64];
  uint32_t count;
} bk_script_plan;

static const char *skip_ws(const char *p) {
  while (*p && isspace((unsigned char)*p))
    ++p;
  return p;
}

static int read_name(const char **pp, char *out, size_t out_size) {
  const char *p = skip_ws(*pp);
  size_t n = 0;
  while (*p && (isalpha((unsigned char)*p) || *p == '-' || *p == '_')) {
    if (n + 1 < out_size)
      out[n++] = *p;
    ++p;
  }
  out[n] = 0;
  *pp = p;
  return n != 0;
}

static int read_u32_arg(const char **pp, uint32_t *out) {
  const char *p = skip_ws(*pp);
  uint64_t v = 0;
  int any = 0;
  while (*p >= '0' && *p <= '9') {
    any = 1;
    v = v * 10u + (uint32_t)(*p - '0');
    if (v > UINT32_MAX)
      return 0;
    ++p;
  }
  *pp = p;
  *out = (uint32_t)v;
  return any;
}

static int consume_char(const char **pp, char want) {
  const char *p = skip_ws(*pp);
  if (*p != want)
    return 0;
  *pp = p + 1;
  return 1;
}

static bk_script_op op_from_name(const char *name) {
  if (strcmp(name, "grayscale") == 0)
    return BK_SCRIPT_GRAYSCALE;
  if (strcmp(name, "flip") == 0)
    return BK_SCRIPT_FLIP;
  if (strcmp(name, "mirror") == 0)
    return BK_SCRIPT_MIRROR;
  if (strcmp(name, "rotate90") == 0)
    return BK_SCRIPT_ROTATE90;
  if (strcmp(name, "rotate180") == 0)
    return BK_SCRIPT_ROTATE180;
  if (strcmp(name, "rotate270") == 0)
    return BK_SCRIPT_ROTATE270;
  if (strcmp(name, "resize") == 0)
    return BK_SCRIPT_RESIZE;
  if (strcmp(name, "crop") == 0)
    return BK_SCRIPT_CROP;
  if (strcmp(name, "premultiply") == 0)
    return BK_SCRIPT_PREMULTIPLY;
  if (strcmp(name, "unpremultiply") == 0)
    return BK_SCRIPT_UNPREMULTIPLY;
  if (strcmp(name, "palette") == 0)
    return BK_SCRIPT_PALETTE;
  return BK_SCRIPT_NONE;
}

static bk_status parse_script(const char *text, bk_script_plan *plan) {
  const char *p = text;
  memset(plan, 0, sizeof(*plan));
  while (*(p = skip_ws(p))) {
    char name[64];
    bk_script_step *step;
    if (plan->count >= 64u)
      return BK_ERR_LIMIT;
    if (!read_name(&p, name, sizeof(name)))
      return BK_ERR_CORRUPT;
    step = &plan->steps[plan->count];
    step->op = op_from_name(name);
    snprintf(step->text, sizeof(step->text), "%s", name);
    if (step->op == BK_SCRIPT_NONE)
      return BK_ERR_UNSUPPORTED;
    if (*skip_ws(p) == '(') {
      uint32_t argn = 0;
      consume_char(&p, '(');
      while (*skip_ws(p) && *skip_ws(p) != ')') {
        if (argn >= 4u)
          return BK_ERR_LIMIT;
        if (!read_u32_arg(&p, &step->args[argn++]))
          return BK_ERR_CORRUPT;
        if (*skip_ws(p) == ',')
          consume_char(&p, ',');
        else
          break;
      }
      if (!consume_char(&p, ')'))
        return BK_ERR_CORRUPT;
    }
    plan->count++;
    p = skip_ws(p);
    if (*p == '|' || *p == ';')
      ++p;
    else if (*p)
      return BK_ERR_CORRUPT;
  }
  return BK_OK;
}

bk_status bk_script_validate(const char *text) {
  bk_script_plan plan;
  if (!text)
    return BK_ERR_ARGUMENT;
  return parse_script(text, &plan);
}

bk_status bk_script_apply(const char *text, bk_image *image) {
  bk_script_plan plan;
  uint32_t i;
  bk_status st;
  if (!text || !image || !image->pixels)
    return BK_ERR_ARGUMENT;
  st = parse_script(text, &plan);
  if (st != BK_OK)
    return st;
  for (i = 0; i < plan.count; ++i) {
    bk_script_step *step = &plan.steps[i];
    bk_image tmp;
    memset(&tmp, 0, sizeof(tmp));
    switch (step->op) {
    case BK_SCRIPT_GRAYSCALE:
      st = bk_image_to_grayscale(image);
      break;
    case BK_SCRIPT_FLIP:
      st = bk_image_flip_vertical(image);
      break;
    case BK_SCRIPT_MIRROR:
      st = bk_image_mirror_horizontal(image);
      break;
    case BK_SCRIPT_ROTATE90:
      st = bk_image_rotate90(image, &tmp);
      if (st == BK_OK) {
        bk_image_free(image);
        *image = tmp;
      }
      break;
    case BK_SCRIPT_ROTATE180:
      st = bk_image_rotate180(image, &tmp);
      if (st == BK_OK) {
        bk_image_free(image);
        *image = tmp;
      }
      break;
    case BK_SCRIPT_ROTATE270:
      st = bk_image_rotate270(image, &tmp);
      if (st == BK_OK) {
        bk_image_free(image);
        *image = tmp;
      }
      break;
    case BK_SCRIPT_RESIZE:
      st = bk_image_resize_bilinear(image, step->args[0], step->args[1], &tmp);
      if (st == BK_OK) {
        bk_image_free(image);
        *image = tmp;
      }
      break;
    case BK_SCRIPT_CROP:
      st = bk_image_crop(image, step->args[0], step->args[1], step->args[2],
                         step->args[3], &tmp);
      if (st == BK_OK) {
        bk_image_free(image);
        *image = tmp;
      }
      break;
    case BK_SCRIPT_PREMULTIPLY:
      st = bk_image_premultiply_alpha(image);
      break;
    case BK_SCRIPT_UNPREMULTIPLY:
      st = bk_image_unpremultiply_alpha(image);
      break;
    case BK_SCRIPT_PALETTE: {
      bk_palette pal;
      st = bk_palette_from_image(image, step->args[0] ? step->args[0] : 32u,
                                 &pal);
      if (st == BK_OK) {
        st = bk_palette_apply(image, &pal, &tmp);
        if (st == BK_OK) {
          bk_image_free(image);
          *image = tmp;
        }
      }
      break;
    }
    default:
      st = BK_ERR_UNSUPPORTED;
      break;
    }
    if (st != BK_OK)
      return st;
  }
  return BK_OK;
}
