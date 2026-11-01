#ifndef WMLIB_DRAG_H_INTERNAL
#define WMLIB_DRAG_H_INTERNAL

#include "wm.h"

__attribute__((visibility("hidden"))) int
wm_i_handle_net_wm_moveresize(WM *wm, XClientMessageEvent *event);

#endif
