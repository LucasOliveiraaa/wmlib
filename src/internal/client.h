#ifndef WMLIB_CLIENT_H_INTERNAL
#define WMLIB_CLIENT_H_INTERNAL

#include "wm.h"
#include "wmlib/client.h"
#include "wmlib/screen.h"
#include <X11/Xlib.h>

struct WMClient {
  Window window;
  WMScreen *screen;

  /*
   * Arbitrary data owned by the user.
   */
  void *userdata;

  /*
   * The geometry of the client window in root coordinates, excluding the
   * frame. border_width is the border of the client window, or of the frame
   * when the client has one.
   */
  int x, y;
  int width, height;
  int border_width;

  int old_x, old_y;
  int old_width, old_height;
  int old_border_width;

  int base_width, base_height;
  int max_width, max_height;
  int min_width, min_height;
  int increment_width, increment_height;
  /* width / height ratios; 0 when unset. */
  float min_aspect, max_aspect;

  Window transient_for;

  char title[256];
  long state; /* ICCCM WM_STATE: WithdrawnState, NormalState, IconicState */

  WMWindowType window_type;
  WMStrut strut;

  /*
   * The reparenting frame, or None. See wm_client_frame().
   */
  Window frame;
  WMExtents frame_extents;

  /*
   * Extra event mask selected with wm_client_select_input().
   */
  long event_mask;

  /*
   * The desktop of the client, or WM_DESKTOP_ALL.
   */
  unsigned desktop;

  /*
   * _NET_WM_USER_TIME, valid when has_user_time is set.
   */
  unsigned long user_time;
  int has_user_time;

  int is_floating;
  int is_fixed;
  int is_fullscreen; /* _NET_WM_STATE_FULLSCREEN requested/active */
  int is_maximized_horz;
  int is_maximized_vert;
  int is_above;
  int is_below;
  int is_sticky;
  int is_hidden;
  int is_shaded;
  int is_demanding_attention; /* _NET_WM_STATE_DEMANDS_ATTENTION */
  int is_urgent;              /* XUrgencyHint in WM_HINTS */
  int never_focus;            /* WM_HINTS input == False */
  int ping_pending;           /* _NET_WM_PING sent, no reply yet */
  unsigned ignore_unmap;
  int is_mapped;
};

static inline Window wm_i_client_outer(const struct WMClient *client) {
  return client->frame ? client->frame : client->window;
}

void wm_i_client_read_net_state(WM *wm, WMClient *client);

WMClientNode *wm_i_client_get_tip(WM *wm);
void wm_i_client_add_node(WM *wm, WMClientNode *node);
WMClientNode *wm_i_client_find_node(WM *wm, Window window);
WMClientNode *wm_i_client_find_node_by_client(WM *wm, WMClient *client);
void wm_i_client_remove_node(WM *wm, WMClientNode *node);
WMClientNode *wm_i_client_create_node(WM *wm, WMClient *client);

int wm_i_client_send_protocol(WM *wm, WMClient *client, Atom proto, long data1,
                              long data2);

int *wm_i_client_state_flag(WMClient *client, int net_atom);
int wm_i_client_send_configure_notify(WM *wm, WMClient *client);
void wm_i_client_publish_frame_extents(WM *wm, WMClient *client);
void wm_i_client_publish_allowed_actions(WM *wm, WMClient *client);

void wm_i_client_release_frame(WM *wm, WMClient *client);

WMClient *wm_i_client_manage_window(WM *wm, WMScreen *screen, Window window,
                                    const XWindowAttributes *attrs,
                                    Window transient_for);
int wm_i_client_scan_pass(WM *wm, WMScreen *screen, Window *windows,
                          unsigned int count, int want_transient,
                          WMScanCallback callback, void *userdata);

#endif