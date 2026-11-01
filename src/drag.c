#include "wmlib/client.h"
#include "internal/client.h"
#include "internal/drag.h"
#include "internal/layout.h"
#include "internal/report.h"
#include "internal/screen.h"
#include "internal/util.h"
#include "internal/wm.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <string.h>

int wm_i_handle_net_wm_moveresize(WM *wm, XClientMessageEvent *event) {
  WMClient *client = wm_client_find(wm, event->window);
  if (!client) {
    return 0;
  }
  int x_root = (int)event->data.l[0];
  int y_root = (int)event->data.l[1];
  WMMoveResizeDirection direction = (WMMoveResizeDirection)event->data.l[2];
  if (wm->on_move_resize) {
    wm->on_move_resize(wm, client, x_root, y_root, direction,
                       (unsigned)event->data.l[3]);
    return 1;
  }
  if (!client->is_floating) {
    return 0;
  }
  if (direction == WM_MOVERESIZE_MOVE) {
    wm_client_begin_move(wm, client, x_root, y_root);
    return 1;
  }
  if (direction <= WM_MOVERESIZE_SIZE_LEFT) {
    wm_client_begin_resize(wm, client, x_root, y_root, direction);
    return 1;
  }
  return 0;
}

int wm_client_begin_move(WM *wm, WMClient *client, int x_root, int y_root) {
  return wm_i_client_drag(wm, client, x_root, y_root, NULL);
}

int wm_client_begin_resize(WM *wm, WMClient *client, int x_root, int y_root,
                           WMMoveResizeDirection direction) {
  if ((int)direction < WM_MOVERESIZE_SIZE_TOPLEFT ||
      direction > WM_MOVERESIZE_SIZE_LEFT) {
    return -1;
  }
  return wm_i_client_drag(wm, client, x_root, y_root,
                          &wm_resize_edges[direction]);
}

Window wm_client_get_window(const WMClient *client) {
  return client ? client->window : None;
}

Window wm_client_get_frame(const WMClient *client) {
  return client ? client->frame : None;
}

WMScreen *wm_client_get_screen(const WMClient *client) {
  return client ? client->screen : NULL;
}

void *wm_client_get_userdata(const WMClient *client) {
  return client ? client->userdata : NULL;
}

void wm_client_set_userdata(WMClient *client, void *userdata) {
  if (client)
    client->userdata = userdata;
}
