#ifndef WMLIB_SCREEN_H
#define WMLIB_SCREEN_H

#include "wmlib/types.h"

#include "wmlib/geometry.h"
#include "wmlib/wm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WMScreen WMScreen;

/*
 * A monitor of a screen. The work area is the geometry minus the space
 * reserved by the struts of mapped clients.
 */
struct WMMonitor {
  WMRect geometry;
  WMRect workarea;
};

typedef void (*WMKeyCallback)(WM *wm, void *arg);

typedef struct {
  WMScreen *screen;
  unsigned modifiers;
  KeySym keysym;
  WMKeyCallback callback; /* NULL for plain grabs, see wm_bind_key() */
  void *arg;
} WMKeyGrab;

/*
 * Root window desktop properties. Pagers' _NET_CURRENT_DESKTOP and
 * _NET_SHOWING_DESKTOP requests are applied and trigger a layout; mapping and
 * unmapping clients on desktop switches is up to the user.
 */
int wm_screen_set_desktops(WM *wm, WMScreen *screen, unsigned count);
int wm_screen_set_current_desktop(WM *wm, WMScreen *screen, unsigned desktop);
/*
 * Overrides the computed work area. From then on the library stops updating
 * screen->workarea and _NET_WORKAREA; monitor work areas stay computed.
 */
int wm_screen_set_workarea(WM *wm, WMScreen *screen, WMRect workarea);

/* ============================================================= */
/*  Screen-bound functions                                       */
/* ============================================================= */

/*
 * Grabs a key on the root window of a screen for every NumLock/CapsLock
 * combination. The grab is re-applied when the keyboard mapping changes.
 */
int wm_grab_key(WM *wm, WMScreen *screen, unsigned modifiers, KeySym keysym);
/*
 * Like wm_grab_key(), and calls callback(wm, arg) when the key is pressed
 * with exactly those modifiers (after wm_clean_modifiers()). The KeyPress is
 * not passed to the KeyPress handler.
 */
int wm_bind_key(WM *wm, WMScreen *screen, unsigned modifiers, KeySym keysym,
                WMKeyCallback callback, void *arg);
int wm_ungrab_keys(WM *wm, WMScreen *screen);

/*
 * Asks for on_layout to run for a screen after the current events.
 */
int wm_request_layout(WM *wm, WMScreen *screen);

/*
 * Returns NULL when no monitor contains the point.
 */
WMMonitor *wm_monitor_at(WM *wm, WMScreen *screen, int x, int y);

int wm_screen_get_number(const WMScreen *screen);
Window wm_screen_get_root(const WMScreen *screen);

int wm_screen_get_width(const WMScreen *screen);
int wm_screen_get_height(const WMScreen *screen);

Window wm_screen_get_check_window(const WMScreen *screen);
Atom wm_screen_get_selection(const WMScreen *screen);

size_t wm_screen_get_monitor_count(const WMScreen *screen);
WMMonitor *wm_screen_get_monitor(WMScreen *screen, size_t index);
const WMMonitor *wm_screen_get_monitor_const(const WMScreen *screen,
                                             size_t index);

unsigned wm_screen_get_desktop_count(const WMScreen *screen);
unsigned wm_screen_get_current_desktop(const WMScreen *screen);
int wm_screen_is_showing_desktop(const WMScreen *screen);

WMRect wm_screen_get_workarea(const WMScreen *screen);
WMClient *wm_screen_get_focused(const WMScreen *screen);

int wm_screen_set_desktops(WM *wm, WMScreen *screen, unsigned count);

int wm_screen_set_current_desktop(WM *wm, WMScreen *screen, unsigned desktop);

int wm_screen_set_workarea(WM *wm, WMScreen *screen, WMRect workarea);

WMRect wm_monitor_get_geometry(const WMMonitor *monitor);
WMRect wm_monitor_get_workarea(const WMMonitor *monitor);

#ifdef __cplusplus
}
#endif

#endif