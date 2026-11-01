#include "wmlib/client.h"
#include "internal/client.h"
#include "internal/layout.h"
#include "internal/report.h"
#include "internal/screen.h"
#include "internal/util.h"
#include "internal/wm.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* Frames                                                                   */
/* ------------------------------------------------------------------------ */

int wm_client_frame(WM *wm, WMClient *client, WMExtents extents) {
  if (!wm || !client) {
    return -1;
  }
  WM_TRACE(wm);

  client->frame_extents = extents;
  if (client->frame) {
    return wm_client_configure(wm, client);
  }

  WMScreen *screen = client->screen;
  unsigned long black = BlackPixel(wm->display, screen->index);
  Window frame =
      XCreateSimpleWindow(wm->display, screen->root, client->x, client->y,
                          (unsigned)MAX(1, client->width),
                          (unsigned)MAX(1, client->height), 0, black, black);
  if (!frame) {
    wm_i_report(wm, "cannot create frame for 0x%lx", client->window);
    return -1;
  }

  XSetWindowAttributes attrs;
  attrs.override_redirect = True;
  attrs.event_mask = WM_FRAME_MASK | (client->event_mask & WM_FRAME_INPUT_MASK);
  XChangeWindowAttributes(wm->display, frame, CWOverrideRedirect | CWEventMask,
                          &attrs);

  /* Reparenting unmaps a mapped window; the UnmapNotify is not a withdraw. */
  if (client->is_mapped) {
    client->ignore_unmap++;
  }
  XSetWindowBorderWidth(wm->display, client->window, 0);
  XReparentWindow(wm->display, client->window, frame, extents.left,
                  extents.top);
  client->frame = frame;
  if (client->is_mapped) {
    XMapWindow(wm->display, frame);
  }

  wm_i_client_publish_allowed_actions(wm, client);
  return wm_client_configure(wm, client);
}

int wm_client_unframe(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return -1;
  }
  if (!client->frame) {
    return 0;
  }
  WM_TRACE(wm);

  if (client->is_mapped) {
    client->ignore_unmap++;
  }
  wm_i_client_release_frame(wm, client);
  XSetWindowBorderWidth(wm->display, client->window, client->border_width);

  wm_i_client_publish_allowed_actions(wm, client);
  return wm_client_configure(wm, client);
}

