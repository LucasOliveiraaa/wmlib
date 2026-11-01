#include "wmlib/client.h"
#include "internal/client.h"
#include "internal/layout.h"
#include "internal/layout.h"
#include "internal/report.h"
#include "internal/screen.h"
#include "internal/util.h"
#include "internal/wm.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <string.h>

WMMonitor *wm_client_monitor(WM *wm, WMClient *client) {
  if (!wm || !client || client->screen->monitor_count == 0) {
    return NULL;
  }

  WMScreen *screen = client->screen;
  WMMonitor *best = NULL;
  long long best_area = -1;

  for (size_t i = 0; i < screen->monitor_count; ++i) {
    WMRect g = screen->monitors[i].geometry;
    int width =
        MIN(client->x + client->width, g.x + g.width) - MAX(client->x, g.x);
    int height =
        MIN(client->y + client->height, g.y + g.height) - MAX(client->y, g.y);
    long long area = width > 0 && height > 0 ? (long long)width * height : 0;
    if (area > best_area) {
      best = &screen->monitors[i];
      best_area = area;
    }
  }
  return best;
}

WMClient *wm_client_create(WM *wm, WMScreen *screen, Window window) {
  if (!wm || !screen) {
    return NULL;
  }

  WMClient *existing = wm_client_find(wm, window);
  if (existing) {
    return existing;
  }

  XWindowAttributes attrs;
  Window transient_for = None;

  /* Not required to be mapped: this is used for MapRequest too. */
  if (!wm_i_get_manageable(wm, window, &attrs)) {
    return NULL;
  }

  XGetTransientForHint(wm->display, window, &transient_for);

  return wm_i_client_manage_window(wm, screen, window, &attrs, transient_for);
}

void wm_client_destroy(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return;
  }

  WMScreen *screen = client->screen;
  WMClientNode *node = wm_i_client_find_node_by_client(wm, client);
  if (node) {
    if (screen->focused == client) {
      wm_client_unfocus(wm, client);
    }
    if (wm->on_unmanage) {
      wm->on_unmanage(wm, client);
    }
    wm_i_client_release_frame(wm, client);
    wm_i_client_remove_node(wm, node);
  }

  free(client);

  if (node && wm->display) {
    wm_i_screen_update_client_list(wm, screen);
    wm_request_layout(wm, screen);
  }
}

WMClient *wm_client_find(WM *wm, Window window) {
  WMClientNode *node = wm_i_client_find_node(wm, window);
  return node ? node->client : NULL;
}

int wm_client_update_title(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return -1;
  }

  if (wm_i_get_text_prop(wm, client->window, wm->net_atoms[NetWMName],
                         client->title, sizeof(client->title)) != 0) {
    wm_i_get_text_prop(wm, client->window, XA_WM_NAME, client->title,
                       sizeof(client->title));
  }
  if (client->title[0] == '\0') {
    strcpy(client->title, "Untitled");
  }

  return 0;
}

int wm_client_update_hints(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return -1;
  }

  long msize;
  XSizeHints hints;
  memset(&hints, 0, sizeof(hints));

  if (!XGetWMNormalHints(wm->display, client->window, &hints, &msize)) {
    hints.flags = PSize;
  }

  if (hints.flags & PBaseSize) {
    client->base_width = MAX(0, hints.base_width);
    client->base_height = MAX(0, hints.base_height);
  } else if (hints.flags & PMinSize) {
    client->base_width = MAX(0, hints.min_width);
    client->base_height = MAX(0, hints.min_height);
  } else {
    client->base_width = 0;
    client->base_height = 0;
  }

  if (hints.flags & PResizeInc) {
    client->increment_width = MAX(0, hints.width_inc);
    client->increment_height = MAX(0, hints.height_inc);
  } else {
    client->increment_width = 0;
    client->increment_height = 0;
  }

  if (hints.flags & PMaxSize) {
    client->max_width = MAX(0, hints.max_width);
    client->max_height = MAX(0, hints.max_height);
  } else {
    client->max_width = 0;
    client->max_height = 0;
  }

  if (hints.flags & PMinSize) {
    client->min_width = MAX(0, hints.min_width);
    client->min_height = MAX(0, hints.min_height);
  } else if (hints.flags & PBaseSize) {
    client->min_width = MAX(0, hints.base_width);
    client->min_height = MAX(0, hints.base_height);
  } else {
    client->min_width = 0;
    client->min_height = 0;
  }

  if ((hints.flags & PAspect) && hints.min_aspect.x > 0 &&
      hints.min_aspect.y > 0 && hints.max_aspect.x > 0 &&
      hints.max_aspect.y > 0) {
    client->min_aspect = (float)hints.min_aspect.x / hints.min_aspect.y;
    client->max_aspect = (float)hints.max_aspect.x / hints.max_aspect.y;
  } else {
    client->min_aspect = 0.0f;
    client->max_aspect = 0.0f;
  }

  client->is_fixed = (client->max_width > 0 && client->max_height > 0 &&
                      client->min_width == client->max_width &&
                      client->min_height == client->max_height);

  XWMHints *wmh = XGetWMHints(wm->display, client->window);
  if (wmh) {
    client->never_focus = (wmh->flags & InputHint) ? !wmh->input : 0;
    client->is_urgent = (wmh->flags & XUrgencyHint) != 0;
    XFree(wmh);
  } else {
    client->never_focus = 0;
    client->is_urgent = 0;
  }

  return 0;
}

int wm_client_update_transient(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return -1;
  }

  Window transient_for = None;
  if (XGetTransientForHint(wm->display, client->window, &transient_for)) {
    client->transient_for = transient_for;
  } else {
    client->transient_for = None;
  }

  if (client->transient_for != None) {
    client->is_floating = 1;
  }

  return 0;
}

int wm_client_update_window_type(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return -1;
  }

  unsigned long count;
  Atom *types = wm_i_get_atom_list(wm, client->window,
                                   wm->net_atoms[NetWMWindowType], &count);

  client->window_type = WM_WINDOW_NORMAL;
  for (unsigned long i = 0; i < count; ++i) {
    WMWindowType type = wm_i_window_type_from_atom(wm, types[i]);
    if (type != WM_WINDOW_NORMAL) {
      client->window_type = type;
      break;
    }
  }

  if (types) {
    XFree(types);
  }
  if (client->window_type != WM_WINDOW_NORMAL) {
    client->is_floating = 1;
  }

  return 0;
}

int wm_client_update_strut(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return -1;
  }

  long v[WM_STRUT_PARTIAL_LENGTH];

  memset(&client->strut, 0, sizeof(client->strut));

  if (wm_i_get_cardinals(wm, client->window, wm->net_atoms[NetWMStrutPartial],
                         WM_STRUT_PARTIAL_LENGTH, v)) {
    client->strut = (WMStrut){
        (int)v[0], (int)v[1], (int)v[2], (int)v[3], (int)v[4],  (int)v[5],
        (int)v[6], (int)v[7], (int)v[8], (int)v[9], (int)v[10], (int)v[11],
    };
  } else if (wm_i_get_cardinals(wm, client->window, wm->net_atoms[NetWMStrut],
                                WM_STRUT_LENGTH, v)) {
    /* The legacy property reserves the whole edge. */
    int last_x = client->screen->width - 1;
    int last_y = client->screen->height - 1;
    client->strut =
        (WMStrut){(int)v[0], (int)v[1], (int)v[2], (int)v[3], 0, last_y,
                  0,         last_y,    0,         last_x,    0, last_x};
  }

  return 0;
}

int wm_client_select_input(WM *wm, WMClient *client, long mask) {
  if (!wm || !client) {
    return -1;
  }
  WM_TRACE(wm);

  client->event_mask = mask;
  XSelectInput(wm->display, client->window, WM_CLIENT_BASE_MASK | mask);
  if (client->frame) {
    XSelectInput(wm->display, client->frame,
                 WM_FRAME_MASK | (mask & WM_FRAME_INPUT_MASK));
  }

  return 0;
}

int wm_client_configure(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return -1;
  }
  WM_TRACE(wm);

  if (client->frame) {
    wm_i_configure_frame(wm, client);
  } else {
    XWindowChanges changes;
    changes.x = client->x;
    changes.y = client->y;
    changes.width = client->width;
    changes.height = client->height;
    changes.border_width = client->border_width;

    XConfigureWindow(wm->display, client->window,
                     CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &changes);
  }
  wm_i_client_publish_frame_extents(wm, client);

  return wm_i_client_send_configure_notify(wm, client);
}

int wm_client_get_state(WM *wm, WMClient *client, long *state) {
  if (!wm || !client || !state) {
    return -1;
  }

  *state = client->state;
  return 0;
}

int wm_client_set_state(WM *wm, WMClient *client, long state) {
  if (!wm || !client) {
    return -1;
  }
  WM_TRACE(wm);

  client->state = state;

  /* ICCCM: type WM_STATE, data = { state, icon window }. */
  long data[2] = {state, None};
  XChangeProperty(wm->display, client->window, wm->wm_atoms[WMState],
                  wm->wm_atoms[WMState], 32, PropModeReplace,
                  (unsigned char *)data, 2);

  return 0;
}

int wm_client_set_net_state(WM *wm, WMClient *client, int net_atom,
                            int enabled) {
  if (!wm || !client) {
    return -1;
  }
  WM_TRACE(wm);

  int *flag = wm_i_client_state_flag(client, net_atom);
  if (!flag) {
    return -1;
  }

  unsigned long count;
  Atom atom = wm->net_atoms[net_atom];
  Atom *current =
      wm_i_get_atom_list(wm, client->window, wm->net_atoms[NetWMState], &count);
  Atom *merged = (Atom *)malloc((count + 1) * sizeof(Atom));
  if (!merged) {
    if (current) {
      XFree(current);
    }
    wm_i_report(wm, "out of memory updating _NET_WM_STATE");
    return -1;
  }

  size_t length = 0;
  for (unsigned long i = 0; i < count; ++i) {
    if (current[i] != atom) {
      merged[length++] = current[i];
    }
  }
  if (enabled) {
    merged[length++] = atom;
  }

  *flag = enabled != 0;
  XChangeProperty(wm->display, client->window, wm->net_atoms[NetWMState],
                  XA_ATOM, 32, PropModeReplace, (unsigned char *)merged,
                  (int)length);

  free(merged);
  if (current) {
    XFree(current);
  }

  if (net_atom == NetWMStateShaded && client->frame) {
    wm_client_configure(wm, client);
  }
  wm_request_layout(wm, client->screen);

  return 0;
}

int wm_client_set_fullscreen(WM *wm, WMClient *client, int fullscreen) {
  return wm_client_set_net_state(wm, client, NetWMFullscreen, fullscreen);
}

int wm_client_set_desktop(WM *wm, WMClient *client, unsigned desktop) {
  if (!wm || !client) {
    return -1;
  }
  WM_TRACE(wm);

  if (desktop != WM_DESKTOP_ALL && desktop >= client->screen->desktop_count) {
    return -1;
  }

  long value = (long)desktop;
  client->desktop = desktop;
  wm_i_set_cardinals(wm, client->window, wm->net_atoms[NetWMDesktop], &value,
                     1);
  wm_request_layout(wm, client->screen);

  return 0;
}

int wm_client_is_visible(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return 0;
  }

  WMScreen *screen = client->screen;
  if (!client->is_mapped || client->is_hidden) {
    return 0;
  }
  if (screen->showing_desktop && client->window_type != WM_WINDOW_DOCK &&
      client->window_type != WM_WINDOW_DESKTOP) {
    return 0;
  }

  return client->is_sticky || client->desktop == WM_DESKTOP_ALL ||
         client->desktop == screen->current_desktop;
}

/* ------------------------------------------------------------------------ */
/* Focus / map / close / geometry / stacking                                */
/* ------------------------------------------------------------------------ */

int wm_client_focus(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return -1;
  }
  WM_TRACE(wm);

  WMScreen *screen = client->screen;
  WMClient *previous = screen->focused;

  if (!client->never_focus) {
    XSetInputFocus(wm->display, client->window, RevertToPointerRoot,
                   CurrentTime);
    XChangeProperty(wm->display, screen->root, wm->net_atoms[NetActiveWindow],
                    XA_WINDOW, 32, PropModeReplace,
                    (unsigned char *)&client->window, 1);
  }
  wm_i_client_send_protocol(wm, client, wm->wm_atoms[WMTakeFocus], CurrentTime,
                            0);

  if (previous == client) {
    return 0;
  }

  screen->focused = client;
  if (previous && wm->on_unfocus) {
    wm->on_unfocus(wm, previous);
  }
  if (wm->on_focus) {
    wm->on_focus(wm, client);
  }

  return 0;
}

int wm_client_unfocus(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return -1;
  }
  WM_TRACE(wm);

  WMScreen *screen = client->screen;
  Window focused;
  int revert;
  XGetInputFocus(wm->display, &focused, &revert);

  int has_input_focus = focused == client->window;
  if (has_input_focus) {
    XSetInputFocus(wm->display, screen->root, RevertToPointerRoot, CurrentTime);
  }
  if (has_input_focus || screen->focused == client) {
    XDeleteProperty(wm->display, screen->root, wm->net_atoms[NetActiveWindow]);
  }

  if (screen->focused == client) {
    screen->focused = NULL;
    if (wm->on_unfocus) {
      wm->on_unfocus(wm, client);
    }
  }

  return 0;
}

int wm_client_map(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return -1;
  }
  WM_TRACE(wm);

  XMapWindow(wm->display, client->window);
  if (client->frame) {
    XMapWindow(wm->display, client->frame);
  }
  client->is_mapped = 1;
  wm_client_set_state(wm, client, NormalState);
  wm_request_layout(wm, client->screen);

  return 0;
}

int wm_client_unmap(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return -1;
  }
  WM_TRACE(wm);

  if (!client->is_mapped) {
    return 0; /* No UnmapNotify would come back; don't leave a stale count. */
  }

  /* Unmapping the frame hides the client without an UnmapNotify for it. */
  if (client->frame) {
    XUnmapWindow(wm->display, client->frame);
  } else {
    client->ignore_unmap++;
    XUnmapWindow(wm->display, client->window);
  }
  client->is_mapped = 0;
  wm_request_layout(wm, client->screen);

  return 0;
}

int wm_client_close(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return -1;
  }
  WM_TRACE(wm);

  if (!wm_i_client_send_protocol(wm, client, wm->wm_atoms[WMDeleteWindow],
                                 CurrentTime, 0)) {
    XGrabServer(wm->display);
    XSetErrorHandler(wm_i_report_xerror_ignore);
    XSetCloseDownMode(wm->display, DestroyAll);
    XKillClient(wm->display, client->window);
    XSync(wm->display, False);
    XSetErrorHandler(wm_i_report_xerror);
    XUngrabServer(wm->display);
  }

  return 0;
}

int wm_client_ping(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return -1;
  }
  WM_TRACE(wm);

  if (!wm_i_client_send_protocol(wm, client, wm->net_atoms[NetWMPing],
                                 CurrentTime, (long)client->window)) {
    return -1;
  }

  client->ping_pending = 1;
  return 0;
}

int wm_client_raise(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return -1;
  }
  WM_TRACE(wm);

  XRaiseWindow(wm->display, wm_i_client_outer(client));
  return wm_i_screen_update_client_list_stacking(wm, client->screen);
}

int wm_client_lower(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return -1;
  }
  WM_TRACE(wm);

  XLowerWindow(wm->display, wm_i_client_outer(client));
  return wm_i_screen_update_client_list_stacking(wm, client->screen);
}

int wm_client_restack(WM *wm, WMClient *client, WMClient *sibling, int above) {
  if (!wm || !client || !sibling) {
    return -1;
  }
  WM_TRACE(wm);

  XWindowChanges changes;
  changes.sibling = wm_i_client_outer(sibling);
  changes.stack_mode = above ? Above : Below;

  XConfigureWindow(wm->display, wm_i_client_outer(client),
                   CWSibling | CWStackMode, &changes);
  return wm_i_screen_update_client_list_stacking(wm, client->screen);
}

int wm_client_set_border_width(WM *wm, WMClient *client, int width) {
  if (!wm || !client) {
    return -1;
  }

  client->border_width = width;
  return wm_client_configure(wm, client);
}

int wm_client_set_border_color(WM *wm, WMClient *client, unsigned long pixel) {
  if (!wm || !client) {
    return -1;
  }
  WM_TRACE(wm);

  XSetWindowBorder(wm->display, wm_i_client_outer(client), pixel);
  return 0;
}

int wm_client_get_border_width(const WMClient *client) {
  return client ? client->border_width : 0;
}

int wm_client_get_old_geometry(const WMClient *client, WMRect *geometry) {
  if (!client || !geometry)
    return -1;

  geometry->x = client->old_x;
  geometry->y = client->old_y;
  geometry->width = client->old_width;
  geometry->height = client->old_height;
  return 0;
}

int wm_client_get_frame_extents(const WMClient *client, WMExtents *extents) {
  if (!client || !extents)
    return -1;

  *extents = client->frame_extents;
  return 0;
}

const char *wm_client_get_title(const WMClient *client) {
  return client ? client->title : NULL;
}

WMWindowType wm_client_get_window_type(const WMClient *client) {
  return client ? client->window_type : WM_WINDOW_NORMAL;
}

Window wm_client_get_transient_for(const WMClient *client) {
  return client ? client->transient_for : None;
}

WMStrut wm_client_get_strut(const WMClient *client) {
  return client ? client->strut : (WMStrut){0};
}

long wm_client_get_event_mask(const WMClient *client) {
  return client ? client->event_mask : 0;
}

unsigned wm_client_get_desktop(const WMClient *client) {
  return client ? client->desktop : WM_DESKTOP_ALL;
}

int wm_client_is_floating(const WMClient *client) {
  return client ? !!client->is_floating : 0;
}

int wm_client_is_fixed(const WMClient *client) {
  return client ? !!client->is_fixed : 0;
}

int wm_client_is_fullscreen(const WMClient *client) {
  return client ? !!client->is_fullscreen : 0;
}

int wm_client_is_maximized_horizontally(const WMClient *client) {
  return client ? !!client->is_maximized_horz : 0;
}

int wm_client_is_maximized_vertically(const WMClient *client) {
  return client ? !!client->is_maximized_vert : 0;
}

int wm_client_is_above(const WMClient *client) {
  return client ? !!client->is_above : 0;
}

int wm_client_is_below(const WMClient *client) {
  return client ? !!client->is_below : 0;
}

int wm_client_is_sticky(const WMClient *client) {
  return client ? !!client->is_sticky : 0;
}

int wm_client_is_hidden(const WMClient *client) {
  return client ? !!client->is_hidden : 0;
}

int wm_client_is_shaded(const WMClient *client) {
  return client ? !!client->is_shaded : 0;
}

int wm_client_is_demanding_attention(const WMClient *client) {
  return client ? !!client->is_demanding_attention : 0;
}

int wm_client_is_urgent(const WMClient *client) {
  return client ? !!client->is_urgent : 0;
}

int wm_client_never_takes_focus(const WMClient *client) {
  return client ? !!client->never_focus : 0;
}

int wm_client_has_pending_ping(const WMClient *client) {
  return client ? !!client->ping_pending : 0;
}

int wm_client_is_mapped(const WMClient *client) {
  return client ? !!client->is_mapped : 0;
}

int wm_client_get_min_size(const WMClient *client, int *width, int *height) {
  if (!client || !width || !height)
    return -1;

  *width = client->min_width;
  *height = client->min_height;

  return 0;
}

int wm_client_get_base_size(const WMClient *client, int *width, int *height) {
  if (!client || !width || !height)
    return -1;

  *width = client->base_width;
  *height = client->base_height;
  return 0;
}

int wm_client_get_max_size(const WMClient *client, int *width, int *height) {
  if (!client || !width || !height)
    return -1;

  *width = client->max_width;
  *height = client->max_height;
  return 0;
}

int wm_client_get_size_increments(const WMClient *client, int *width,
                                  int *height) {
  if (!client || !width || !height)
    return -1;

  *width = client->increment_width;
  *height = client->increment_height;

  return 0;
}

int wm_client_get_aspect_ratios(const WMClient *client, float *min_aspect,
                                float *max_aspect) {
  if (!client || !min_aspect || !max_aspect)
    return -1;

  *min_aspect = client->min_aspect;
  *max_aspect = client->max_aspect;

  return 0;
}

int wm_client_get_user_time(const WMClient *client,
                            unsigned long *user_time) {
  if (!client || !user_time)
    return -1;

  *user_time = client->user_time;
  return client->has_user_time ? 0 : -1;
}

int wm_client_set_floating(WM *wm, WMClient *client, int floating) {
  if (!wm || !client)
    return -1;

  client->is_floating = !!floating;
  return wm_client_configure(wm, client);
}