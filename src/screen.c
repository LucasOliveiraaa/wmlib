#include "wmlib/screen.h"
#include "internal/client.h"
#include "internal/layout.h"
#include "internal/report.h"
#include "internal/screen.h"
#include "internal/util.h"
#include "internal/wm.h"
#include "wmlib/types.h"

int wm_screen_set_desktops(WM *wm, WMScreen *screen, unsigned count) {
  if (!wm || !screen || count == 0) {
    return -1;
  }
  WM_TRACE(wm);

  screen->desktop_count = count;
  if (screen->current_desktop >= count) {
    screen->current_desktop = count - 1;
  }

  for (WMClientNode *node = wm->client_nodes; node; node = node->next) {
    WMClient *client = node->client;
    if (client->screen == screen && client->desktop != WM_DESKTOP_ALL &&
        client->desktop >= count) {
      wm_client_set_desktop(wm, client, count - 1);
    }
  }

  wm_request_layout(wm, screen);
  return wm_i_screen_publish_desktops(wm, screen);
}

int wm_screen_set_current_desktop(WM *wm, WMScreen *screen, unsigned desktop) {
  if (!wm || !screen || desktop >= screen->desktop_count) {
    return -1;
  }
  WM_TRACE(wm);

  screen->current_desktop = desktop;
  wm_i_screen_publish_current_desktop(wm, screen);
  wm_request_layout(wm, screen);

  return 0;
}

int wm_screen_set_workarea(WM *wm, WMScreen *screen, WMRect workarea) {
  if (!wm || !screen) {
    return -1;
  }
  WM_TRACE(wm);

  screen->workarea = workarea;
  screen->workarea_fixed = 1;
  return wm_i_screen_publish_workarea(wm, screen);
}

int wm_grab_key(WM *wm, WMScreen *screen, unsigned modifiers, KeySym keysym) {
  return wm_i_screen_register_key(wm, screen, modifiers, keysym, NULL, NULL);
}

int wm_bind_key(WM *wm, WMScreen *screen, unsigned modifiers, KeySym keysym,
                WMKeyCallback callback, void *arg) {
  if (!callback) {
    return -1;
  }
  return wm_i_screen_register_key(wm, screen, modifiers, keysym, callback, arg);
}

int wm_ungrab_keys(WM *wm, WMScreen *screen) {
  if (!wm || !screen) {
    return -1;
  }
  WM_TRACE(wm);

  XUngrabKey(wm->display, AnyKey, AnyModifier, screen->root);

  size_t kept = 0;
  for (size_t i = 0; i < wm->key_grab_count; ++i) {
    if (wm->key_grabs[i].screen != screen) {
      wm->key_grabs[kept++] = wm->key_grabs[i];
    }
  }
  wm->key_grab_count = kept;

  return 0;
}

int wm_grab_button(WM *wm, Window window, unsigned modifiers, unsigned button,
                   unsigned event_mask) {
  if (!wm) {
    return -1;
  }
  WM_TRACE(wm);

  unsigned variants[4];
  wm_i_lock_variants(wm, variants);
  for (size_t i = 0; i < LENGTH(variants); ++i) {
    XGrabButton(wm->display, button, modifiers | variants[i], window, False,
                event_mask, GrabModeAsync, GrabModeAsync, None, None);
  }

  return 0;
}

int wm_ungrab_buttons(WM *wm, Window window) {
  if (!wm) {
    return -1;
  }
  WM_TRACE(wm);

  XUngrabButton(wm->display, AnyButton, AnyModifier, window);
  return 0;
}

unsigned wm_clean_modifiers(WM *wm, unsigned state) {
  unsigned modifiers = ShiftMask | ControlMask | Mod1Mask | Mod2Mask |
                       Mod3Mask | Mod4Mask | Mod5Mask;
  return state & ~(wm->numlock_mask | LockMask) & modifiers;
}

WMMonitor *wm_monitor_at(WM *wm, WMScreen *screen, int x, int y) {
  if (!wm || !screen) {
    return NULL;
  }

  for (size_t i = 0; i < screen->monitor_count; ++i) {
    WMRect g = screen->monitors[i].geometry;
    if (x >= g.x && x < g.x + g.width && y >= g.y && y < g.y + g.height) {
      return &screen->monitors[i];
    }
  }
  return NULL;
}

int wm_scan_screen(WM *wm, WMScreen *screen, WMScanCallback callback,
                   void *userdata) {
  if (!wm || !screen) {
    return -1;
  }

  Window root_return, parent_return;
  Window *windows = NULL;
  unsigned int count = 0;

  if (XQueryTree(wm->display, screen->root, &root_return, &parent_return,
                 &windows, &count) == 0) {
    wm_i_report(wm, "XQueryTree failed on screen %d", screen->index);
    return -1;
  }

  int ret =
      wm_i_client_scan_pass(wm, screen, windows, count, 0, callback, userdata);
  if (ret == 0) {
    ret = wm_i_client_scan_pass(wm, screen, windows, count, 1, callback,
                                userdata);
  }

  if (windows) {
    XFree(windows);
  }

  return ret;
}

int wm_screen_get_number(const WMScreen *screen) {
  return screen ? screen->index : -1;
}

Window wm_screen_get_root(const WMScreen *screen) {
  return screen ? screen->root : None;
}

int wm_screen_get_width(const WMScreen *screen) {
  return screen ? screen->width : 0;
}

int wm_screen_get_height(const WMScreen *screen) {
  return screen ? screen->height : 0;
}

Window wm_screen_get_check_window(const WMScreen *screen) {
  return screen ? screen->check_window : None;
}

Atom wm_screen_get_selection(const WMScreen *screen) {
  return screen ? screen->wm_selection : None;
}

size_t wm_screen_get_monitor_count(const WMScreen *screen) {
  return screen ? screen->monitor_count : 0;
}

WMMonitor *wm_screen_get_monitor(WMScreen *screen, size_t index) {
  if (!screen || index >= screen->monitor_count)
    return NULL;

  return &screen->monitors[index];
}

const WMMonitor *wm_screen_get_monitor_const(const WMScreen *screen,
                                             size_t index) {
  if (!screen || index >= screen->monitor_count)
    return NULL;

  return &screen->monitors[index];
}

unsigned wm_screen_get_desktop_count(const WMScreen *screen) {
  return screen ? screen->desktop_count : 0;
}

unsigned wm_screen_get_current_desktop(const WMScreen *screen) {
  return screen ? screen->current_desktop : 0;
}

int wm_screen_is_showing_desktop(const WMScreen *screen) {
  return screen ? screen->showing_desktop : 0;
}

WMRect wm_screen_get_workarea(const WMScreen *screen) {
  return screen ? screen->workarea : (WMRect){0};
}

WMClient *wm_screen_get_focused(const WMScreen *screen) {
  return screen ? screen->focused : NULL;
}

WMRect wm_monitor_get_geometry(const WMMonitor *monitor) {
  return monitor ? monitor->geometry : (WMRect){0};
}

WMRect wm_monitor_get_workarea(const WMMonitor *monitor) {
  return monitor ? monitor->workarea : (WMRect){0};
}