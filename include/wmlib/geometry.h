#ifndef WMLIB_GEOMETRY_H
#define WMLIB_GEOMETRY_H

#include "wmlib/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct WMRect {
  int x, y;
  int width, height;
};

struct WMExtents {
  int left, right, top, bottom;
};

#ifdef __cplusplus
}
#endif

#endif