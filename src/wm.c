#include "internal/report.h"
#include "internal/util.h"
#define _POSIX_C_SOURCE 200809L

#include "internal/client.h"
#include "internal/layout.h"
#include "internal/screen.h"
#include "internal/wm.h"

#include "wmlib/screen.h"
#include "wmlib/wm.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>
#include <limits.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int wm_set_debug_sync(WM *wm, int enabled) {
  if (!wm || !wm->display) {
    return -1;
  }

  wm->debug_sync = enabled != 0;
  wm_i_report_set_current_call(NULL);
  XSynchronize(wm->display, wm->debug_sync);

  return 0;
}

WM *wm_create(void) {
  WM *wm = (WM *)calloc(1, sizeof(WM));
  if (!wm) {
    return NULL;
  }
  wm->loop_timeout_ms = -1;
  wm->next_timer_id = 1;
  return wm;
}

static void wm_fail_init(WM *wm) {
  free(wm->screens);
  wm->screens = NULL;
  wm->screen_count = 0;
  if (wm->display) {
    XCloseDisplay(wm->display);
    wm->display = NULL;
  }
  if (wm_i_report_get_xerror_target() == wm) {
    wm_i_report_set_xerror_target(NULL);
  }
}

static void wm_i_init_randr(WM *wm) {
  int error_base, major, minor;

  wm->randr_event_base = 0;
  if (!XRRQueryExtension(wm->display, &wm->randr_event_base, &error_base) ||
      !XRRQueryVersion(wm->display, &major, &minor) ||
      (major == 1 && minor < 5) || major < 1) {
    wm->randr_event_base = 0;
  }
}

int wm_init(WM *wm) {
  if (!wm) {
    return -1;
  }

  wm_i_report_set_xerror_target(wm);

  wm->display = XOpenDisplay(NULL);
  if (!wm->display) {
    wm_i_report(wm, "cannot open display");
    wm_i_report_set_xerror_target(NULL);
    return -1;
  }

  int count = ScreenCount(wm->display);
  if (count <= 0) {
    wm_i_report(wm, "display has no screens");
    wm_fail_init(wm);
    return -1;
  }
  wm->screen_count = (size_t)count;

  wm->screens = (WMScreen *)calloc(wm->screen_count, sizeof(WMScreen));
  if (!wm->screens) {
    wm_i_report(wm, "out of memory allocating screens");
    wm_fail_init(wm);
    return -1;
  }

  for (size_t i = 0; i < wm->screen_count; ++i) {
    wm->screens[i].index = (int)i;
    wm->screens[i].root = RootWindow(wm->display, (int)i);
    wm->screens[i].width = DisplayWidth(wm->display, (int)i);
    wm->screens[i].height = DisplayHeight(wm->display, (int)i);
  }

#define INTERN_ATOM(atom_name) XInternAtom(wm->display, atom_name, False)

  wm->wm_atoms[WMProtocols] = INTERN_ATOM("WM_PROTOCOLS");
  wm->wm_atoms[WMDeleteWindow] = INTERN_ATOM("WM_DELETE_WINDOW");
  wm->wm_atoms[WMState] = INTERN_ATOM("WM_STATE");
  wm->wm_atoms[WMHints] = INTERN_ATOM("WM_HINTS");
  wm->wm_atoms[WMNormalHints] = INTERN_ATOM("WM_NORMAL_HINTS");
  wm->wm_atoms[WMTransientFor] = INTERN_ATOM("WM_TRANSIENT_FOR");
  wm->wm_atoms[WMTakeFocus] = INTERN_ATOM("WM_TAKE_FOCUS");
  wm->wm_atoms[WMName] = INTERN_ATOM("WM_NAME");
  wm->wm_atoms[WMUtf8String] = INTERN_ATOM("UTF8_STRING");
  wm->wm_atoms[WMManager] = INTERN_ATOM("MANAGER");

  wm->net_atoms[NetSupported] = INTERN_ATOM("_NET_SUPPORTED");
  wm->net_atoms[NetWMName] = INTERN_ATOM("_NET_WM_NAME");
  wm->net_atoms[NetWMState] = INTERN_ATOM("_NET_WM_STATE");
  wm->net_atoms[NetWMCheck] = INTERN_ATOM("_NET_SUPPORTING_WM_CHECK");
  wm->net_atoms[NetWMFullscreen] = INTERN_ATOM("_NET_WM_STATE_FULLSCREEN");
  wm->net_atoms[NetActiveWindow] = INTERN_ATOM("_NET_ACTIVE_WINDOW");
  wm->net_atoms[NetWMWindowType] = INTERN_ATOM("_NET_WM_WINDOW_TYPE");
  wm->net_atoms[NetWMWindowTypeDialog] =
      INTERN_ATOM("_NET_WM_WINDOW_TYPE_DIALOG");
  wm->net_atoms[NetClientList] = INTERN_ATOM("_NET_CLIENT_LIST");
  wm->net_atoms[NetClientListStacking] =
      INTERN_ATOM("_NET_CLIENT_LIST_STACKING");
  wm->net_atoms[NetNumberOfDesktops] = INTERN_ATOM("_NET_NUMBER_OF_DESKTOPS");
  wm->net_atoms[NetCurrentDesktop] = INTERN_ATOM("_NET_CURRENT_DESKTOP");
  wm->net_atoms[NetWorkarea] = INTERN_ATOM("_NET_WORKAREA");
  wm->net_atoms[NetWMWindowTypeDock] = INTERN_ATOM("_NET_WM_WINDOW_TYPE_DOCK");
  wm->net_atoms[NetWMWindowTypeDesktop] =
      INTERN_ATOM("_NET_WM_WINDOW_TYPE_DESKTOP");
  wm->net_atoms[NetWMWindowTypeSplash] =
      INTERN_ATOM("_NET_WM_WINDOW_TYPE_SPLASH");
  wm->net_atoms[NetWMWindowTypeUtility] =
      INTERN_ATOM("_NET_WM_WINDOW_TYPE_UTILITY");
  wm->net_atoms[NetWMWindowTypeMenu] = INTERN_ATOM("_NET_WM_WINDOW_TYPE_MENU");
  wm->net_atoms[NetWMStrutPartial] = INTERN_ATOM("_NET_WM_STRUT_PARTIAL");
  wm->net_atoms[NetWMMaximizedVert] =
      INTERN_ATOM("_NET_WM_STATE_MAXIMIZED_VERT");
  wm->net_atoms[NetWMMaximizedHorz] =
      INTERN_ATOM("_NET_WM_STATE_MAXIMIZED_HORZ");
  wm->net_atoms[NetWMAbove] = INTERN_ATOM("_NET_WM_STATE_ABOVE");
  wm->net_atoms[NetWMBelow] = INTERN_ATOM("_NET_WM_STATE_BELOW");
  wm->net_atoms[NetWMSticky] = INTERN_ATOM("_NET_WM_STATE_STICKY");
  wm->net_atoms[NetWMHidden] = INTERN_ATOM("_NET_WM_STATE_HIDDEN");
  wm->net_atoms[NetWMDemandsAttention] =
      INTERN_ATOM("_NET_WM_STATE_DEMANDS_ATTENTION");
  wm->net_atoms[NetCloseWindow] = INTERN_ATOM("_NET_CLOSE_WINDOW");
  wm->net_atoms[NetMoveResizeWindow] = INTERN_ATOM("_NET_MOVERESIZE_WINDOW");
  wm->net_atoms[NetWMMoveResize] = INTERN_ATOM("_NET_WM_MOVERESIZE");
  wm->net_atoms[NetFrameExtents] = INTERN_ATOM("_NET_FRAME_EXTENTS");
  wm->net_atoms[NetWMPing] = INTERN_ATOM("_NET_WM_PING");
  wm->net_atoms[NetWMDesktop] = INTERN_ATOM("_NET_WM_DESKTOP");
  wm->net_atoms[NetShowingDesktop] = INTERN_ATOM("_NET_SHOWING_DESKTOP");
  wm->net_atoms[NetRequestFrameExtents] =
      INTERN_ATOM("_NET_REQUEST_FRAME_EXTENTS");
  wm->net_atoms[NetWMAllowedActions] = INTERN_ATOM("_NET_WM_ALLOWED_ACTIONS");
  wm->net_atoms[NetWMStrut] = INTERN_ATOM("_NET_WM_STRUT");
  wm->net_atoms[NetWMUserTime] = INTERN_ATOM("_NET_WM_USER_TIME");
  wm->net_atoms[NetWMStateShaded] = INTERN_ATOM("_NET_WM_STATE_SHADED");
  wm->net_atoms[NetWMActionMove] = INTERN_ATOM("_NET_WM_ACTION_MOVE");
  wm->net_atoms[NetWMActionResize] = INTERN_ATOM("_NET_WM_ACTION_RESIZE");
  wm->net_atoms[NetWMActionShade] = INTERN_ATOM("_NET_WM_ACTION_SHADE");
  wm->net_atoms[NetWMActionStick] = INTERN_ATOM("_NET_WM_ACTION_STICK");
  wm->net_atoms[NetWMActionMaximizeHorz] =
      INTERN_ATOM("_NET_WM_ACTION_MAXIMIZE_HORZ");
  wm->net_atoms[NetWMActionMaximizeVert] =
      INTERN_ATOM("_NET_WM_ACTION_MAXIMIZE_VERT");
  wm->net_atoms[NetWMActionFullscreen] =
      INTERN_ATOM("_NET_WM_ACTION_FULLSCREEN");
  wm->net_atoms[NetWMActionChangeDesktop] =
      INTERN_ATOM("_NET_WM_ACTION_CHANGE_DESKTOP");
  wm->net_atoms[NetWMActionClose] = INTERN_ATOM("_NET_WM_ACTION_CLOSE");

#undef INTERN_ATOM

  /* Become the window manager; fails with BadAccess if one is running. */
  wm_i_report_xerror_reset();
  wm_i_report_set_xerror_default(XSetErrorHandler(wm_i_report_xerror_start));
  for (size_t i = 0; i < wm->screen_count; ++i) {
    XSelectInput(wm->display, wm->screens[i].root,
                 SubstructureRedirectMask | SubstructureNotifyMask |
                     StructureNotifyMask | PropertyChangeMask);
  }
  XSync(wm->display, False);
  XSetErrorHandler(wm_i_report_xerror);
  XSync(wm->display, False);

  if (wm_i_report_xerror_other_wm()) {
    wm_i_report(wm, "another window manager is already running");
    wm_fail_init(wm);
    return -1;
  }

  wm_i_update_numlock_mask(wm);
  wm_i_init_randr(wm);

  for (size_t i = 0; i < wm->screen_count; ++i) {
    if (wm_i_screen_setup(wm, &wm->screens[i]) != 0) {
      wm_cleanup(wm);
      return -1;
    }
  }
  XSync(wm->display, False);

  return 0;
}

void wm_cleanup(WM *wm) {
  if (!wm) {
    return;
  }

  while (wm->client_nodes) {
    wm_client_destroy(wm, wm->client_nodes->client);
  }

  if (wm->display) {
    for (size_t i = 0; i < wm->screen_count; ++i) {
      wm_i_screen_teardown(wm, &wm->screens[i]);
    }
  }

  free(wm->key_grabs);
  wm->key_grabs = NULL;
  wm->key_grab_count = 0;

  free(wm->fd_watches);
  wm->fd_watches = NULL;
  wm->fd_watch_count = 0;

  free(wm->timers);
  wm->timers = NULL;
  wm->timer_count = 0;

  free(wm->screens);
  wm->screens = NULL;
  wm->screen_count = 0;

  if (wm->display) {
    XCloseDisplay(wm->display);
    wm->display = NULL;
  }
  if (wm_i_report_get_xerror_target() == wm) {
    wm_i_report_set_xerror_target(NULL);
  }
}

void wm_destroy(WM *wm) {
  if (!wm) {
    return;
  }

  wm_cleanup(wm);
  free(wm);
}

/* ------------------------------------------------------------------------ */
/* File descriptors and timers                                              */
/* ------------------------------------------------------------------------ */

static long long wm_now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int wm_watch_fd(WM *wm, int fd, WMFdCallback callback) {
  if (!wm || fd < 0 || !callback) {
    return -1;
  }

  for (size_t i = 0; i < wm->fd_watch_count; ++i) {
    if (wm->fd_watches[i].fd == fd) {
      wm->fd_watches[i].callback = callback;
      return 0;
    }
  }

  WMFdWatch *watches = (WMFdWatch *)realloc(
      wm->fd_watches, (wm->fd_watch_count + 1) * sizeof(WMFdWatch));
  if (!watches) {
    wm_i_report(wm, "out of memory watching fd %d", fd);
    return -1;
  }

  wm->fd_watches = watches;
  watches[wm->fd_watch_count++] = (WMFdWatch){fd, callback};
  return 0;
}

int wm_unwatch_fd(WM *wm, int fd) {
  if (!wm) {
    return -1;
  }

  for (size_t i = 0; i < wm->fd_watch_count; ++i) {
    if (wm->fd_watches[i].fd == fd) {
      memmove(&wm->fd_watches[i], &wm->fd_watches[i + 1],
              (wm->fd_watch_count - i - 1) * sizeof(WMFdWatch));
      wm->fd_watch_count--;
      return 0;
    }
  }
  return -1;
}

static WMFdCallback wm_find_fd_callback(WM *wm, int fd) {
  for (size_t i = 0; i < wm->fd_watch_count; ++i) {
    if (wm->fd_watches[i].fd == fd) {
      return wm->fd_watches[i].callback;
    }
  }
  return NULL;
}

int wm_add_timer(WM *wm, unsigned interval_ms, int repeat,
                 WMTimerCallback callback) {
  if (!wm || interval_ms == 0 || !callback) {
    return -1;
  }

  WMTimer *timers =
      (WMTimer *)realloc(wm->timers, (wm->timer_count + 1) * sizeof(WMTimer));
  if (!timers) {
    wm_i_report(wm, "out of memory adding timer");
    return -1;
  }

  int id = wm->next_timer_id++;
  wm->timers = timers;
  timers[wm->timer_count++] = (WMTimer){id, wm_now_ms() + interval_ms,
                                        interval_ms, repeat != 0, callback};
  return id;
}

int wm_cancel_timer(WM *wm, int id) {
  if (!wm) {
    return -1;
  }

  for (size_t i = 0; i < wm->timer_count; ++i) {
    if (wm->timers[i].id == id) {
      memmove(&wm->timers[i], &wm->timers[i + 1],
              (wm->timer_count - i - 1) * sizeof(WMTimer));
      wm->timer_count--;
      return 0;
    }
  }
  return -1;
}

static void wm_run_timers(WM *wm) {
  long long now = wm_now_ms();

  for (;;) {
    WMTimer *due = NULL;
    for (size_t i = 0; i < wm->timer_count && !due; ++i) {
      if (wm->timers[i].due_ms <= now) {
        due = &wm->timers[i];
      }
    }
    if (!due) {
      return;
    }

    int id = due->id;
    WMTimerCallback callback = due->callback;
    if (due->repeat) {
      due->due_ms = now + due->interval_ms;
    } else {
      wm_cancel_timer(wm, id);
    }

    callback(wm, id);
  }
}

/* ------------------------------------------------------------------------ */
/* Main loop                                                                */
/* ------------------------------------------------------------------------ */

static int wm_wait_timeout(WM *wm) {
  if (wm_i_layout_pending(wm)) {
    return 0;
  }

  int timeout = wm->loop_timeout_ms;
  long long now = wm_now_ms();

  for (size_t i = 0; i < wm->timer_count; ++i) {
    long long remaining = MAX(0, wm->timers[i].due_ms - now);
    if (timeout < 0 || remaining < timeout) {
      timeout = (int)MIN(remaining, INT_MAX);
    }
  }
  return timeout;
}

/* Blocks until the connection or a watched fd has data, a timer is due or the
 * loop timeout expires. XPending flushes the output buffer, which poll()
 * would not. */
static void wm_wait_for_events(WM *wm) {
  if (XPending(wm->display) > 0) {
    return;
  }

  int timeout = wm_wait_timeout(wm);
  struct pollfd display_fd = {ConnectionNumber(wm->display), POLLIN, 0};
  size_t count = wm->fd_watch_count + 1;
  struct pollfd *fds = (struct pollfd *)malloc(count * sizeof(struct pollfd));
  if (!fds) {
    wm_i_report(wm, "out of memory polling watched fds");
    poll(&display_fd, 1, timeout);
    return;
  }

  fds[0] = display_fd;
  for (size_t i = 0; i < wm->fd_watch_count; ++i) {
    fds[i + 1] = (struct pollfd){wm->fd_watches[i].fd, POLLIN, 0};
  }

  if (poll(fds, count, timeout) > 0) {
    for (size_t i = 1; i < count; ++i) {
      if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR))) {
        continue;
      }

      /* An earlier callback may have unwatched this fd. */
      WMFdCallback callback = wm_find_fd_callback(wm, fds[i].fd);
      if (callback) {
        callback(wm, fds[i].fd);
      }
    }
  }

  free(fds);
}

void wm_run(WM *wm) {
  if (!wm || !wm->display) {
    return;
  }

  wm->running = 1;

  if (wm->on_start) {
    wm->on_start(wm);
  }

  while (wm->running) {
    if (wm->on_loop) {
      wm->on_loop(wm);
    }
    if (!wm->running) {
      break;
    }

    wm_wait_for_events(wm);
    wm_run_timers(wm);

    while (wm->running && XPending(wm->display) > 0) {
      XEvent event;
      XNextEvent(wm->display, &event);
      wm_i_handle_event(wm, &event);
    }

    wm_i_flush_layouts(wm);

    if (wm->running && wm->on_post) {
      wm->on_post(wm);
    }
  }

  if (wm->on_stop) {
    wm->on_stop(wm);
  }
}

void wm_stop(WM *wm) {
  if (wm) {
    wm->running = 0;
  }
}

void *wm_get_userdata(const WM *wm) { return wm ? wm->userdata : NULL; }

void wm_set_userdata(WM *wm, void *userdata) {
  if (wm)
    wm->userdata = userdata;
}

const char *wm_get_name(const WM *wm) { return wm ? wm->name : NULL; }

int wm_set_name(WM *wm, const char *name) {
  if (!wm || wm->running)
    return -1;

  if (wm->name) {
    free(wm->name);
  }

  if (name) {
    wm->name = strdup(name);
    if (!wm->name) {
      return -1;
    }
  } else {
    wm->name = NULL;
  }
  return 0;
}

WMExtents wm_get_frame_extents(const WM *wm) {
  return wm ? wm->frame_extents : (WMExtents){0};
}

int wm_set_frame_extents(WM *wm, WMExtents extents) {
  if (!wm || wm->running)
    return -1;

  if (extents.left < 0 || extents.right < 0 || extents.top < 0 ||
      extents.bottom < 0)
    return -1;

  wm->frame_extents = extents;
  return 0;
}

int wm_get_loop_timeout(const WM *wm) { return wm ? wm->loop_timeout_ms : -1; }

int wm_set_loop_timeout(WM *wm, int timeout_ms) {
  if (!wm || timeout_ms < -1)
    return -1;

  wm->loop_timeout_ms = timeout_ms;
  return 0;
}

Atom wm_get_net_atom(const WM *wm, WMNetAtom atom) {
    if (!wm || atom < 0 || atom >= NetLast) {
        return None;
    }
    return wm->net_atoms[atom];
}
Atom wm_get_atom(const WM *wm, WMAtom atom) {
    if (!wm || atom < 0 || atom >= WMLast) {
        return None;
    }
    return wm->wm_atoms[atom];
}

int wm_is_running(const WM *wm) { return wm ? wm->running : 0; }

int wm_get_debug_sync(const WM *wm) { return wm ? wm->debug_sync : 0; }

size_t wm_get_screen_count(const WM *wm) { return wm ? wm->screen_count : 0; }

Display *wm_get_display(const WM *wm) {
  return wm ? wm->display : NULL;
}

WMScreen *wm_get_screen(WM *wm, size_t index) {
  if (!wm || index >= wm->screen_count)
    return NULL;

  return &wm->screens[index];
}

const WMScreen *wm_get_screen_const(const WM *wm, size_t index) {
  if (!wm || index >= wm->screen_count)
    return NULL;

  return &wm->screens[index];
}

WMClientNode *wm_get_first_client_node(WM *wm) {
  if (!wm)
    return NULL;

  return wm->client_nodes;
}

void wm_set_event_handler(WM *wm, int type, void (*callback)(WM *, XEvent *)) {
  if (wm && type >= 0 && type < 256) {
    wm->event_handlers[type] = callback;
  }
}

#define WM_DEFINE_CALLBACK_SETTER(name, member, type)                          \
  void name(WM *wm, type) {                                                    \
    if (wm)                                                                    \
      wm->member = callback;                                                   \
  }

WM_DEFINE_CALLBACK_SETTER(wm_set_on_start, on_start, void (*callback)(WM *))

WM_DEFINE_CALLBACK_SETTER(wm_set_on_stop, on_stop, void (*callback)(WM *))

WM_DEFINE_CALLBACK_SETTER(wm_set_on_loop, on_loop, void (*callback)(WM *))

WM_DEFINE_CALLBACK_SETTER(wm_set_on_post, on_post, void (*callback)(WM *))

WM_DEFINE_CALLBACK_SETTER(wm_set_on_manage, on_manage,
                          void (*callback)(WM *, WMClient *))

WM_DEFINE_CALLBACK_SETTER(wm_set_on_unmanage, on_unmanage,
                          void (*callback)(WM *, WMClient *))

WM_DEFINE_CALLBACK_SETTER(wm_set_on_layout, on_layout,
                          void (*callback)(WM *, WMScreen *))

WM_DEFINE_CALLBACK_SETTER(wm_set_on_focus, on_focus,
                          void (*callback)(WM *, WMClient *))

WM_DEFINE_CALLBACK_SETTER(wm_set_on_unfocus, on_unfocus,
                          void (*callback)(WM *, WMClient *))

WM_DEFINE_CALLBACK_SETTER(wm_set_on_client_changed, on_client_changed,
                          void (*callback)(WM *, WMClient *, unsigned))

WM_DEFINE_CALLBACK_SETTER(wm_set_on_state_request, on_state_request,
                          void (*callback)(WM *, WMClient *, int, int))

WM_DEFINE_CALLBACK_SETTER(wm_set_on_activate_request, on_activate_request,
                          void (*callback)(WM *, WMClient *, WMSource))

WM_DEFINE_CALLBACK_SETTER(wm_set_on_frame_expose, on_frame_expose,
                          void (*callback)(WM *, WMClient *))

WM_DEFINE_CALLBACK_SETTER(wm_set_on_monitors_changed, on_monitors_changed,
                          void (*callback)(WM *, WMScreen *))

WM_DEFINE_CALLBACK_SETTER(wm_set_on_move_resize, on_move_resize,
                          void (*callback)(WM *, WMClient *, int, int,
                                           WMMoveResizeDirection, unsigned))

WM_DEFINE_CALLBACK_SETTER(wm_set_on_error, on_error,
                          void (*callback)(WM *, const char *))

#undef WM_DEFINE_CALLBACK_SETTER