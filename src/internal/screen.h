#ifndef WMLIB_SCREEN_H_INTERNAL
#define WMLIB_SCREEN_H_INTERNAL

#include "wmlib/client.h"
#include "wmlib/screen.h"
#include <X11/Xlib.h>

struct WMScreen {
  /*
   * The screen number (index) of this screen.
   */
  int index;
  /*
   * The root window of this screen.
   */
  Window root;

  /*
   * The width of this screen in pixels.
   */
  int width;
  /*
   * The height of this screen in pixels.
   */
  int height;

  /*
   * The window published as _NET_SUPPORTING_WM_CHECK. It also owns the WM_Sn
   * manager selection.
   */
  Window check_window;
  Atom wm_selection;

  /*
   * The monitors of this screen. Contains one entry covering the whole screen
   * when RandR 1.5 is unavailable.
   */
  WMMonitor *monitors;
  size_t monitor_count;

  unsigned desktop_count;
  unsigned current_desktop;
  int showing_desktop;

  /*
   * The bounding box of the monitor work areas, published as _NET_WORKAREA.
   * Computed from the client struts unless set with
   * wm_screen_set_workarea().
   */
  WMRect workarea;
  int workarea_fixed;

  /*
   * The client that has the focus, or NULL.
   */
  WMClient *focused;

  int layout_pending;
};

WMScreen *wm_i_screen_from_root(WM *wm, Window root);
int wm_i_screen_setup(WM *wm, WMScreen *screen);
void wm_i_screen_teardown(WM *wm, WMScreen *screen);
WMScreen *wm_i_screen_from_window(WM *wm, Window window);
int wm_i_screen_grab_key_variants(WM *wm, WMScreen *screen, unsigned modifiers,
                                  KeySym keysym);
void wm_i_screen_update_workarea(WM *wm, WMScreen *screen);
void wm_i_screen_publish_showing_desktop(WM *wm, WMScreen *screen);
int wm_i_screen_update_monitors(WM *wm, WMScreen *screen);
void wm_i_screen_publish_current_desktop(WM *wm, WMScreen *screen);
int wm_i_screen_publish_desktops(WM *wm, WMScreen *screen);
int wm_i_screen_publish_workarea(WM *wm, WMScreen *screen);
int wm_i_screen_register_key(WM *wm, WMScreen *screen, unsigned modifiers,
                             KeySym keysym, WMKeyCallback callback, void *arg);

int wm_i_screen_update_client_list_stacking(WM *wm, WMScreen *screen);
int wm_i_screen_update_client_list(WM *wm, WMScreen *screen);

#endif