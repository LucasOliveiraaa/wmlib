#ifndef WMLIB_LAYOUT_H_INTERNAL
#define WMLIB_LAYOUT_H_INTERNAL

#include "wm.h"
#include "screen.h"

int wm_request_layout(WM *wm, WMScreen *screen);
int wm_i_layout_pending(WM *wm);
void wm_i_flush_layouts(WM *wm);

#endif
