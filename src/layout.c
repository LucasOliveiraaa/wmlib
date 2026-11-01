#include "internal/layout.h"
#include "internal/report.h"
#include "internal/screen.h"

__attribute__((visibility("default"))) int wm_request_layout(WM *wm,
                                                              WMScreen *screen) {
  if (!wm || !screen) {
    return -1;
  }
  screen->layout_pending = 1;
  return 0;
}

int wm_i_layout_pending(WM *wm) {
  for (size_t i = 0; i < wm->screen_count; ++i) {
    if (wm->screens[i].layout_pending) {
      return 1;
    }
  }
  return 0;
}

void wm_i_flush_layouts(WM *wm) {
  for (size_t i = 0; i < wm->screen_count && wm->running; ++i) {
    WMScreen *screen = &wm->screens[i];
    if (!screen->layout_pending) {
      continue;
    }
    screen->layout_pending = 0;
    wm_i_screen_update_workarea(wm, screen);
    if (wm->on_layout) {
      wm->on_layout(wm, screen);
    }
  }
}
