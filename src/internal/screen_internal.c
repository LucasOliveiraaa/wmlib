#include "screen.h"
#include "layout.h"
#include "client.h"
#include "report.h"
#include "util.h"
#include "wm.h"
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

WMScreen *wm_i_screen_from_root(WM *wm, Window root) {
  for (size_t i = 0; i < wm->screen_count; ++i) {
    if (wm->screens[i].root == root) {
      return &wm->screens[i];
    }
  }
  return NULL;
}

WMScreen *wm_i_screen_from_window(WM *wm, Window window) {
  XWindowAttributes attrs;
  if (!XGetWindowAttributes(wm->display, window, &attrs)) {
    return NULL;
  }
  return wm_i_screen_from_root(wm, attrs.root);
}

int wm_i_screen_update_client_list_stacking(WM *wm, WMScreen *screen) {
  Window root_return, parent_return;
  Window *children = NULL;
  unsigned int count = 0;

  if (XQueryTree(wm->display, screen->root, &root_return, &parent_return,
                 &children, &count) == 0) {
    wm_i_report(wm, "XQueryTree failed on screen %d", screen->index);
    return -1;
  }

  Window *windows = (Window *)malloc((count + 1) * sizeof(Window));
  if (!windows) {
    if (children) {
      XFree(children);
    }
    wm_i_report(wm, "out of memory building client list");
    return -1;
  }

  size_t length = 0;
  for (unsigned int i = 0; i < count; ++i) {
    WMClient *client = wm_client_find(wm, children[i]);
    if (client && client->screen == screen &&
        wm_i_client_outer(client) == children[i]) {
      windows[length++] = client->window;
    }
  }

  XChangeProperty(wm->display, screen->root,
                  wm->net_atoms[NetClientListStacking], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)windows, (int)length);

  free(windows);
  if (children) {
    XFree(children);
  }
  return 0;
}

int wm_i_screen_update_client_list(WM *wm, WMScreen *screen) {
  size_t capacity = 0;
  for (WMClientNode *node = wm->client_nodes; node; node = node->next) {
    if (node->client->screen == screen) {
      capacity++;
    }
  }

  Window *windows = (Window *)malloc((capacity + 1) * sizeof(Window));
  if (!windows) {
    wm_i_report(wm, "out of memory building client list");
    return -1;
  }

  size_t length = 0;
  for (WMClientNode *node = wm->client_nodes; node; node = node->next) {
    if (node->client->screen == screen) {
      windows[length++] = node->client->window;
    }
  }

  XChangeProperty(wm->display, screen->root, wm->net_atoms[NetClientList],
                  XA_WINDOW, 32, PropModeReplace, (unsigned char *)windows,
                  (int)length);
  free(windows);

  return wm_i_screen_update_client_list_stacking(wm, screen);
}

int wm_i_screen_publish_workarea(WM *wm, WMScreen *screen) {
  size_t length = (size_t)screen->desktop_count * 4;
  long *values = (long *)malloc(length * sizeof(long));
  if (!values) {
    wm_i_report(wm, "out of memory publishing work area");
    return -1;
  }

  for (size_t i = 0; i < screen->desktop_count; ++i) {
    values[i * 4 + 0] = screen->workarea.x;
    values[i * 4 + 1] = screen->workarea.y;
    values[i * 4 + 2] = screen->workarea.width;
    values[i * 4 + 3] = screen->workarea.height;
  }
  wm_i_set_cardinals(wm, screen->root, wm->net_atoms[NetWorkarea], values,
                     (int)length);

  free(values);
  return 0;
}

void wm_i_screen_publish_current_desktop(WM *wm, WMScreen *screen) {
  long current = screen->current_desktop;
  wm_i_set_cardinals(wm, screen->root, wm->net_atoms[NetCurrentDesktop],
                     &current, 1);
}

void wm_i_screen_publish_showing_desktop(WM *wm, WMScreen *screen) {
  long showing = screen->showing_desktop;
  wm_i_set_cardinals(wm, screen->root, wm->net_atoms[NetShowingDesktop],
                     &showing, 1);
}

int wm_i_screen_publish_desktops(WM *wm, WMScreen *screen) {
  long count = screen->desktop_count;
  wm_i_set_cardinals(wm, screen->root, wm->net_atoms[NetNumberOfDesktops],
                     &count, 1);
  wm_i_screen_publish_current_desktop(wm, screen);
  wm_i_screen_publish_showing_desktop(wm, screen);
  return wm_i_screen_publish_workarea(wm, screen);
}

/* The monitor geometry minus the struts of the mapped clients that cover it. */
static WMRect wm_i_screen_monitor_workarea(WM *wm, WMScreen *screen,
                                           const WMRect *g) {
  int left = 0, right = 0, top = 0, bottom = 0;
  int g_right = g->x + g->width - 1;
  int g_bottom = g->y + g->height - 1;

  for (WMClientNode *node = wm->client_nodes; node; node = node->next) {
    const WMClient *c = node->client;
    if (c->screen != screen || !c->is_mapped) {
      continue;
    }

    const WMStrut *s = &c->strut;
    if (s->left &&
        wm_i_ranges_overlap(s->left_start_y, s->left_end_y, g->y, g_bottom)) {
      left = MAX(left, s->left - g->x);
    }
    if (s->right &&
        wm_i_ranges_overlap(s->right_start_y, s->right_end_y, g->y, g_bottom)) {
      right = MAX(right, g->x + g->width - (screen->width - s->right));
    }
    if (s->top &&
        wm_i_ranges_overlap(s->top_start_x, s->top_end_x, g->x, g_right)) {
      top = MAX(top, s->top - g->y);
    }
    if (s->bottom && wm_i_ranges_overlap(s->bottom_start_x, s->bottom_end_x,
                                         g->x, g_right)) {
      bottom = MAX(bottom, g->y + g->height - (screen->height - s->bottom));
    }
  }

  return (WMRect){g->x + left, g->y + top, MAX(0, g->width - left - right),
                  MAX(0, g->height - top - bottom)};
}

/* Recomputes the monitor work areas and, unless overridden, republishes
 * _NET_WORKAREA when it changed. */
void wm_i_screen_update_workarea(WM *wm, WMScreen *screen) {
  WMRect bounds = screen->workarea;

  for (size_t i = 0; i < screen->monitor_count; ++i) {
    WMMonitor *monitor = &screen->monitors[i];
    monitor->workarea =
        wm_i_screen_monitor_workarea(wm, screen, &monitor->geometry);
    bounds =
        i == 0 ? monitor->workarea : wm_i_rect_union(bounds, monitor->workarea);
  }

  if (screen->workarea_fixed || wm_i_rect_equal(bounds, screen->workarea)) {
    return;
  }

  screen->workarea = bounds;
  wm_i_screen_publish_workarea(wm, screen);
}

int wm_i_screen_update_monitors(WM *wm, WMScreen *screen) {
  int count = 0;
  XRRMonitorInfo *info = NULL;

  if (wm->randr_event_base) {
    info = XRRGetMonitors(wm->display, screen->root, True, &count);
  }

  int has_info = info && count > 0;
  size_t monitor_count = has_info ? (size_t)count : 1;
  WMMonitor *monitors = (WMMonitor *)calloc(monitor_count, sizeof(WMMonitor));
  if (!monitors) {
    if (info) {
      XRRFreeMonitors(info);
    }
    wm_i_report(wm, "out of memory reading monitors");
    return -1;
  }

  if (has_info) {
    for (int i = 0; i < count; ++i) {
      monitors[i].geometry =
          (WMRect){info[i].x, info[i].y, info[i].width, info[i].height};
    }
  } else {
    monitors[0].geometry = (WMRect){0, 0, screen->width, screen->height};
  }

  if (info) {
    XRRFreeMonitors(info);
  }

  free(screen->monitors);
  screen->monitors = monitors;
  screen->monitor_count = monitor_count;
  wm_i_screen_update_workarea(wm, screen);

  return 0;
}

int wm_i_screen_grab_key_variants(WM *wm, WMScreen *screen, unsigned modifiers,
                                  KeySym keysym) {
  KeyCode code = XKeysymToKeycode(wm->display, keysym);
  if (!code) {
    return -1;
  }

  unsigned variants[4];
  wm_i_lock_variants(wm, variants);
  for (size_t i = 0; i < LENGTH(variants); ++i) {
    XGrabKey(wm->display, code, modifiers | variants[i], screen->root, True,
             GrabModeAsync, GrabModeAsync);
  }
  return 0;
}

int wm_i_screen_register_key(WM *wm, WMScreen *screen, unsigned modifiers,
                             KeySym keysym, WMKeyCallback callback, void *arg) {
  if (!wm || !screen) {
    return -1;
  }
  WM_TRACE(wm);

  WMKeyGrab *grabs = (WMKeyGrab *)realloc(
      wm->key_grabs, (wm->key_grab_count + 1) * sizeof(WMKeyGrab));
  if (!grabs) {
    wm_i_report(wm, "out of memory registering key grab");
    return -1;
  }
  wm->key_grabs = grabs;

  if (wm_i_screen_grab_key_variants(wm, screen, modifiers, keysym) != 0) {
    wm_i_report(wm, "no keycode for keysym 0x%lx", (unsigned long)keysym);
    return -1;
  }

  grabs[wm->key_grab_count++] =
      (WMKeyGrab){screen, modifiers, keysym, callback, arg};
  return 0;
}

static void wm_i_screen_create_check_window(WM *wm, WMScreen *screen) {
  const char *name = wm->name ? wm->name : WM_DEFAULT_NAME;
  Window check =
      XCreateSimpleWindow(wm->display, screen->root, -1, -1, 1, 1, 0, 0, 0);

  screen->check_window = check;
  XChangeProperty(wm->display, check, wm->net_atoms[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&check, 1);
  XChangeProperty(wm->display, check, wm->net_atoms[NetWMName],
                  wm->wm_atoms[WMUtf8String], 8, PropModeReplace,
                  (const unsigned char *)name, (int)strlen(name));
  XChangeProperty(wm->display, screen->root, wm->net_atoms[NetWMCheck],
                  XA_WINDOW, 32, PropModeReplace, (unsigned char *)&check, 1);
}

/* ICCCM 2.8: the WM_Sn selection, announced with a MANAGER message. */
int wm_i_screen_acquire_selection(WM *wm, WMScreen *screen) {
  char name[32];
  snprintf(name, sizeof(name), "WM_S%d", screen->index);
  screen->wm_selection = XInternAtom(wm->display, name, False);

  if (XGetSelectionOwner(wm->display, screen->wm_selection) != None) {
    wm_i_report(wm, "selection %s is already owned", name);
    return -1;
  }

  Time time = wm_i_server_time(wm, screen->check_window);
  XSetSelectionOwner(wm->display, screen->wm_selection, screen->check_window,
                     time);
  if (XGetSelectionOwner(wm->display, screen->wm_selection) !=
      screen->check_window) {
    wm_i_report(wm, "cannot acquire selection %s", name);
    return -1;
  }

  XEvent event;
  memset(&event, 0, sizeof(event));
  event.xclient.type = ClientMessage;
  event.xclient.window = screen->root;
  event.xclient.message_type = wm->wm_atoms[WMManager];
  event.xclient.format = 32;
  event.xclient.data.l[0] = (long)time;
  event.xclient.data.l[1] = (long)screen->wm_selection;
  event.xclient.data.l[2] = (long)screen->check_window;
  XSendEvent(wm->display, screen->root, False, StructureNotifyMask, &event);

  return 0;
}

int wm_i_screen_setup(WM *wm, WMScreen *screen) {
  screen->desktop_count = 1;
  screen->current_desktop = 0;
  screen->workarea = (WMRect){0, 0, screen->width, screen->height};

  if (wm_i_screen_update_monitors(wm, screen) != 0) {
    return -1;
  }
  if (wm->randr_event_base) {
    XRRSelectInput(wm->display, screen->root,
                   RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask);
  }

  wm_i_screen_create_check_window(wm, screen);
  if (wm_i_screen_acquire_selection(wm, screen) != 0) {
    return -1;
  }
  XChangeProperty(wm->display, screen->root, wm->net_atoms[NetSupported],
                  XA_ATOM, 32, PropModeReplace, (unsigned char *)wm->net_atoms,
                  NetLast);

  if (wm_i_screen_publish_desktops(wm, screen) != 0) {
    return -1;
  }
  return wm_i_screen_update_client_list(wm, screen);
}
void wm_i_screen_teardown(WM *wm, WMScreen *screen) {
  static const int properties[] = {
      NetSupported,          NetWMCheck,          NetClientList,
      NetClientListStacking, NetNumberOfDesktops, NetCurrentDesktop,
      NetShowingDesktop,     NetWorkarea,         NetActiveWindow,
  };

  for (size_t i = 0; i < LENGTH(properties); ++i) {
    XDeleteProperty(wm->display, screen->root, wm->net_atoms[properties[i]]);
  }
  if (screen->check_window) {
    XDestroyWindow(wm->display, screen->check_window);
    screen->check_window = None;
  }

  free(screen->monitors);
  screen->monitors = NULL;
  screen->monitor_count = 0;
}

