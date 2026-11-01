#include "wmlib/client.h"
#include "internal/client.h"
#include "internal/layout.h"
#include "internal/report.h"
#include "internal/screen.h"
#include "internal/util.h"
#include "internal/wm.h"
#include <stdlib.h>

/* Allocates, initializes and registers a client. NULL on failure. */
WMClient *wm_i_client_manage_window(WM *wm, WMScreen *screen, Window window,
                                    const XWindowAttributes *attrs,
                                    Window transient_for) {
  WMClient *client = (WMClient *)calloc(1, sizeof(WMClient));
  if (!client) {
    wm_i_report(wm, "out of memory managing window 0x%lx", window);
    return NULL;
  }

  client->window = window;
  client->screen = screen;

  client->x = client->old_x = attrs->x;
  client->y = client->old_y = attrs->y;
  client->width = client->old_width = attrs->width;
  client->height = client->old_height = attrs->height;
  client->border_width = client->old_border_width = attrs->border_width;

  client->transient_for = transient_for;
  client->is_mapped = attrs->map_state != IsUnmapped;
  client->state = wm_i_get_state(wm, window);

  wm_client_update_title(wm, client);
  wm_client_update_hints(wm, client);
  wm_client_update_window_type(wm, client);
  wm_client_update_strut(wm, client);
  wm_i_client_read_net_state(wm, client);

  if (transient_for != None || client->is_fixed) {
    client->is_floating = 1;
  }

  long desktop = 0;
  client->desktop = screen->current_desktop;
  if (wm_i_get_cardinals(wm, window, wm->net_atoms[NetWMDesktop], 1,
                         &desktop) &&
      ((unsigned)desktop == WM_DESKTOP_ALL ||
       (unsigned)desktop < screen->desktop_count)) {
    client->desktop = (unsigned)desktop;
  }

  long user_time = 0;
  client->has_user_time = wm_i_get_cardinals(
      wm, window, wm->net_atoms[NetWMUserTime], 1, &user_time);
  client->user_time = (unsigned long)user_time;

  if (!wm_i_client_create_node(wm, client)) {
    wm_i_report(wm, "out of memory managing window 0x%lx", window);
    free(client);
    return NULL;
  }

  /* Substructure events come through the root; we only need property events
   * here (selecting StructureNotify too would duplicate Unmap/Destroy). */
  XSelectInput(wm->display, window, WM_CLIENT_BASE_MASK);

  wm_i_client_publish_frame_extents(wm, client);
  wm_i_client_publish_allowed_actions(wm, client);
  wm_client_set_desktop(wm, client, client->desktop);
  wm_i_screen_update_client_list(wm, screen);
  wm_request_layout(wm, screen);

  if (wm->on_manage) {
    wm->on_manage(wm, client);
  }

  return client;
}

int wm_i_client_scan_pass(WM *wm, WMScreen *screen, Window *windows,
                          unsigned int count, int want_transient,
                          WMScanCallback callback, void *userdata) {
  for (unsigned int i = 0; i < count; ++i) {
    XWindowAttributes attrs;
    Window transient_for = None;

    if (!wm_i_is_scannable(wm, windows[i], &attrs)) {
      continue;
    }

    int is_transient =
        XGetTransientForHint(wm->display, windows[i], &transient_for) != 0;
    if (is_transient != want_transient) {
      continue;
    }

    if (wm_client_find(wm, windows[i])) {
      continue;
    }

    WMClient *client = wm_i_client_manage_window(wm, screen, windows[i], &attrs,
                                                 transient_for);
    if (!client) {
      return -1;
    }

    if (callback) {
      callback(wm, client, want_transient ? WM_SCAN_TRANSIENT : WM_SCAN_NORMAL, userdata);
    }
  }
  return 0;
}