#ifndef WMLIB_UTIL_H_INTERNAL
#define WMLIB_UTIL_H_INTERNAL

#include "wmlib/geometry.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define LENGTH(array) (sizeof(array) / sizeof((array)[0]))

static inline int wm_i_ranges_overlap(int start_a, int end_a, int start_b,
                                      int end_b) {
  return start_a <= end_b && start_b <= end_a;
}

static inline WMRect wm_i_rect_union(WMRect a, WMRect b) {
  int x1 = MIN(a.x, b.x);
  int y1 = MIN(a.y, b.y);
  int x2 = MAX(a.x + a.width, b.x + b.width);
  int y2 = MAX(a.y + a.height, b.y + b.height);
  return (WMRect){x1, y1, x2 - x1, y2 - y1};
}

static inline int wm_i_rect_equal(WMRect a, WMRect b) {
  return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

#endif