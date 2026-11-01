#include "wm.h"
#include "drag.h"
#include "layout.h"
#include "client.h"
#include "report.h"
#include "screen.h"
#include "util.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int wm_i_net_state_from_atom(WM *wm, Atom atom) {
  for (size_t i = 0; i < LENGTH(wm_net_states); ++i) {
    if (wm->net_atoms[wm_net_states[i]] == atom) {
      return wm_net_states[i];
    }
  }
  return -1;
}

/* Returns an XFree()able list, or NULL (with *count == 0) when the property
 * is missing or not a list of atoms. */
Atom *wm_i_get_atom_list(WM *wm, Window window, Atom property,
                         unsigned long *count) {
  Atom actual_type;
  int actual_format;
  unsigned long bytes_after;
  unsigned char *data = NULL;

  *count = 0;
  if (XGetWindowProperty(wm->display, window, property, 0, WM_MAX_ATOMS, False,
                         XA_ATOM, &actual_type, &actual_format, count,
                         &bytes_after, &data) != Success ||
      !data) {
    *count = 0;
    return NULL;
  }

  if (actual_type != XA_ATOM || actual_format != 32) {
    XFree(data);
    *count = 0;
    return NULL;
  }

  return (Atom *)data;
}

void wm_i_set_cardinals(WM *wm, Window window, Atom property,
                        const long *values, int count) {
  XChangeProperty(wm->display, window, property, XA_CARDINAL, 32,
                  PropModeReplace, (const unsigned char *)values, count);
}

int wm_i_atom_list_contains(const Atom *atoms, unsigned long count, Atom atom) {
  for (unsigned long i = 0; i < count; ++i) {
    if (atoms[i] == atom) {
      return 1;
    }
  }
  return 0;
}

long wm_i_get_state(WM *wm, Window window) {
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *prop = NULL;
  long state = WithdrawnState;

  int status = XGetWindowProperty(wm->display, window, wm->wm_atoms[WMState], 0,
                                  2, False, wm->wm_atoms[WMState], &actual_type,
                                  &actual_format, &nitems, &bytes_after, &prop);
  if (status == Success && actual_type == wm->wm_atoms[WMState] &&
      actual_format == 32 && nitems >= 1 && prop) {
    state = *(long *)prop;
  }

  if (prop) {
    XFree(prop);
  }

  return state;
}

/* Reads exactly `length` cardinals; returns whether the property matched. */
int wm_i_get_cardinals(WM *wm, Window window, Atom property,
                       unsigned long length, long *values) {
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *data = NULL;
  int found = 0;

  if (XGetWindowProperty(wm->display, window, property, 0, (long)length, False,
                         XA_CARDINAL, &actual_type, &actual_format, &nitems,
                         &bytes_after, &data) == Success &&
      data) {
    if (actual_type == XA_CARDINAL && actual_format == 32 && nitems == length) {
      memcpy(values, data, length * sizeof(long));
      found = 1;
    }
    XFree(data);
  }

  return found;
}

void wm_i_lock_variants(WM *wm, unsigned variants[4]) {
  variants[0] = 0;
  variants[1] = LockMask;
  variants[2] = wm->numlock_mask;
  variants[3] = wm->numlock_mask | LockMask;
}

void wm_i_regrab_keys(WM *wm) {
  for (size_t i = 0; i < wm->screen_count; ++i) {
    XUngrabKey(wm->display, AnyKey, AnyModifier, wm->screens[i].root);
  }
  for (size_t i = 0; i < wm->key_grab_count; ++i) {
    WMKeyGrab *grab = &wm->key_grabs[i];
    wm_i_screen_grab_key_variants(wm, grab->screen, grab->modifiers,
                                  grab->keysym);
  }
}

/* Gets a timestamp from the server with a zero-length property append. */
Time wm_i_server_time(WM *wm, Window window) {
  XEvent event;

  XSelectInput(wm->display, window, PropertyChangeMask);
  XChangeProperty(wm->display, window, wm->wm_atoms[WMName], XA_STRING, 8,
                  PropModeAppend, NULL, 0);
  XWindowEvent(wm->display, window, PropertyChangeMask, &event);
  XSelectInput(wm->display, window, NoEventMask);

  return event.xproperty.time;
}

WMWindowType wm_i_window_type_from_atom(WM *wm, Atom atom) {
  for (size_t i = 0; i < LENGTH(wm_window_types); ++i) {
    if (wm->net_atoms[wm_window_types[i].net_atom] == atom) {
      return wm_window_types[i].type;
    }
  }
  return WM_WINDOW_NORMAL;
}

/* Window exists, and is not override-redirect. */
int wm_i_get_manageable(WM *wm, Window window, XWindowAttributes *attrs) {
  if (!XGetWindowAttributes(wm->display, window, attrs)) {
    return 0;
  }
  return !attrs->override_redirect;
}

int wm_i_is_scannable(WM *wm, Window window, XWindowAttributes *attrs) {
  if (!wm_i_get_manageable(wm, window, attrs)) {
    return 0;
  }
  return attrs->map_state == IsViewable ||
         wm_i_get_state(wm, window) == IconicState;
}

void wm_i_update_numlock_mask(WM *wm) {
  wm->numlock_mask = 0;

  KeyCode numlock = XKeysymToKeycode(wm->display, XK_Num_Lock);
  XModifierKeymap *map = XGetModifierMapping(wm->display);
  if (!map) {
    return;
  }

  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < map->max_keypermod; ++j) {
      if (numlock && map->modifiermap[i * map->max_keypermod + j] == numlock) {
        wm->numlock_mask = 1u << i;
      }
    }
  }
  XFreeModifiermap(map);
}

/* ------------------------------------------------------------------------ */
/* Event handlers                                                           */
/*                                                                          */
/* Each returns 0 to let the user's handler run, nonzero to swallow the     */
/* event.                                                                   */
/* ------------------------------------------------------------------------ */

/* Registers the client so the user's handler can wm_client_find() it. The
 * library never maps the window: that decision belongs to the user. */
static int wm_handle_map_request(WM *wm, XMapRequestEvent *event) {
  if (wm_client_find(wm, event->window)) {
    return 0;
  }

  WMScreen *screen = wm_i_screen_from_window(wm, event->window);
  if (!screen || !wm_client_create(wm, screen, event->window)) {
    return 1; /* window vanished or is not manageable */
  }

  return 0;
}

static int wm_handle_unmap_notify(WM *wm, XUnmapEvent *event) {
  WMClient *client = wm_client_find(wm, event->window);
  if (!client) {
    return 0;
  }

  if (event->window == client->frame) {
    return 1;
  }

  if (client->ignore_unmap > 0) {
    client->ignore_unmap--; /* caused by wm_client_unmap() or reparenting */
    return 1;
  }

  /* The client withdrew itself
   * Keep the client; the user decides whether to destroy it. */
  client->is_mapped = 0;
  wm_client_set_state(wm, client, WithdrawnState);
  if (client->frame) {
    XUnmapWindow(wm->display, client->frame);
  }
  wm_request_layout(wm, client->screen);

  return 0;
}

/* The client is released after the user's handler ran (see wm_handle_event). */
static int wm_handle_destroy_notify(WM *wm, XDestroyWindowEvent *event) {
  (void)wm;
  (void)event;
  return 0;
}

static int wm_handle_configure_request(WM *wm, XConfigureRequestEvent *event) {
  WMClient *client = wm_client_find(wm, event->window);

  if (!client) {
    /* Not managed: honour the request as-is (ICCCM). */
    XWindowChanges wc;
    wc.x = event->x;
    wc.y = event->y;
    wc.width = event->width;
    wc.height = event->height;
    wc.border_width = event->border_width;
    wc.sibling = event->above;
    wc.stack_mode = event->detail;
    XConfigureWindow(wm->display, event->window, (unsigned)event->value_mask,
                     &wc);
    return 1;
  }

  /* Managed: the geometry policy is the user's. Without a user handler,
   * deny but tell the client its real geometry, as ICCCM requires. */
  if (!wm->event_handlers[ConfigureRequest]) {
    wm_i_client_send_configure_notify(wm, client);
    return 1;
  }

  return 0;
}

static void wm_notify_client_changed(WM *wm, WMClient *client,
                                     unsigned change) {
  if (change & (WM_CHANGE_HINTS | WM_CHANGE_WINDOW_TYPE | WM_CHANGE_TRANSIENT |
                WM_CHANGE_STRUT)) {
    wm_request_layout(wm, client->screen);
  }
  if (change & WM_CHANGE_HINTS) {
    wm_i_client_publish_allowed_actions(wm, client);
  }
  if (wm->on_client_changed) {
    wm->on_client_changed(wm, client, change);
  }
}

static int wm_handle_property_notify(WM *wm, XPropertyEvent *event) {
  WMClient *client = wm_client_find(wm, event->window);
  if (!client) {
    return 0; /* e.g. root properties: the user may care */
  }

  Atom a = event->atom;
  unsigned change = 0;

  if (a == wm->net_atoms[NetWMName] || a == wm->wm_atoms[WMName]) {
    wm_client_update_title(wm, client);
    change |= WM_CHANGE_TITLE;
  } else if (a == wm->wm_atoms[WMState]) {
    client->state = wm_i_get_state(wm, client->window);
  } else if (a == wm->wm_atoms[WMNormalHints] || a == wm->wm_atoms[WMHints]) {
    int was_urgent = client->is_urgent;
    int could_focus = !client->never_focus;

    wm_client_update_hints(wm, client);
    if (a == wm->wm_atoms[WMNormalHints] ||
        could_focus == client->never_focus) {
      change |= WM_CHANGE_HINTS;
    }
    if (was_urgent != client->is_urgent) {
      change |= WM_CHANGE_URGENCY;
    }
  } else if (a == wm->wm_atoms[WMTransientFor]) {
    wm_client_update_transient(wm, client);
    change |= WM_CHANGE_TRANSIENT;
  } else if (a == wm->net_atoms[NetWMWindowType]) {
    wm_client_update_window_type(wm, client);
    change |= WM_CHANGE_WINDOW_TYPE;
  } else if (a == wm->net_atoms[NetWMStrutPartial] ||
             a == wm->net_atoms[NetWMStrut]) {
    wm_client_update_strut(wm, client);
    change |= WM_CHANGE_STRUT;
  } else if (a == wm->net_atoms[NetWMUserTime]) {
    long user_time = 0;
    client->has_user_time = wm_i_get_cardinals(
        wm, client->window, wm->net_atoms[NetWMUserTime], 1, &user_time);
    client->user_time = (unsigned long)user_time;
  }

  if (change) {
    wm_notify_client_changed(wm, client, change);
  }

  return 0;
}

/* _NET_WM_STATE: data.l[0] = action (0 remove, 1 add, 2 toggle),
 * l[1], l[2] = properties. The flags and the property are updated, geometry
 * is left to the user's handler. */
static int wm_handle_net_wm_state(WM *wm, XClientMessageEvent *event) {
  WMClient *client = wm_client_find(wm, event->window);
  if (!client) {
    return 0;
  }

  long action = event->data.l[0];
  for (int i = 1; i <= 2; ++i) {
    int net_atom = wm_i_net_state_from_atom(wm, (Atom)event->data.l[i]);
    if (net_atom < 0) {
      continue;
    }

    int current = *wm_i_client_state_flag(client, net_atom);
    int want = current;
    if (action == 0) {
      want = 0;
    } else if (action == 1) {
      want = 1;
    } else if (action == 2) {
      want = !current;
    }
    wm_client_set_net_state(wm, client, net_atom, want);

    if (wm->on_state_request) {
      wm->on_state_request(wm, client, net_atom, want);
    }
  }

  return wm->on_state_request != NULL;
}

/* An application asking for the focus with a timestamp older than the last
 * user activity of the focused client is stealing it. */
static int wm_is_focus_steal(const WMClient *client, WMSource source,
                             unsigned long timestamp) {
  const WMClient *focused = client->screen->focused;

  if (source != WM_SOURCE_APPLICATION || timestamp == 0 || !focused ||
      !focused->has_user_time) {
    return 0;
  }
  return (int32_t)((uint32_t)timestamp - (uint32_t)focused->user_time) < 0;
}

static int wm_handle_net_active_window(WM *wm, XClientMessageEvent *event) {
  WMClient *client = wm_client_find(wm, event->window);
  if (!client) {
    return 0;
  }

  WMSource source = (WMSource)event->data.l[0];
  if (wm_is_focus_steal(client, source, (unsigned long)event->data.l[1])) {
    wm_client_set_net_state(wm, client, NetWMDemandsAttention, 1);
    return 1;
  }

  if (!wm->on_activate_request) {
    return 0;
  }

  wm->on_activate_request(wm, client, source);
  return 1;
}

static int wm_handle_net_close_window(WM *wm, XClientMessageEvent *event) {
  WMClient *client = wm_client_find(wm, event->window);
  if (client) {
    wm_client_close(wm, client);
  }
  return 1;
}

/* Only floating clients are moved here; tiled geometry is the user's. The
 * gravity in data.l[0] is ignored. */
static int wm_handle_net_moveresize_window(WM *wm, XClientMessageEvent *event) {
  WMClient *client = wm_client_find(wm, event->window);
  if (!client || !client->is_floating) {
    return 0;
  }

  long flags = event->data.l[0];
  int x = client->x, y = client->y;
  int width = client->width, height = client->height;

  if (flags & WM_MOVERESIZE_FLAG_X) {
    x = (int)event->data.l[1];
  }
  if (flags & WM_MOVERESIZE_FLAG_Y) {
    y = (int)event->data.l[2];
  }
  if (flags & WM_MOVERESIZE_FLAG_WIDTH) {
    width = (int)event->data.l[3];
  }
  if (flags & WM_MOVERESIZE_FLAG_HEIGHT) {
    height = (int)event->data.l[4];
  }

  wm_client_set_geometry(wm, client, x, y, width, height);
  return 0;
}

/* Ping replies arrive as WM_PROTOCOLS messages on the root window. */
static int wm_handle_wm_protocols(WM *wm, XClientMessageEvent *event) {
  if ((Atom)event->data.l[0] != wm->net_atoms[NetWMPing]) {
    return 0;
  }

  WMClient *client = wm_client_find(wm, (Window)event->data.l[2]);
  if (client) {
    client->ping_pending = 0;
  }
  return 1;
}

static int wm_handle_net_current_desktop(WM *wm, XClientMessageEvent *event) {
  WMScreen *screen = wm_i_screen_from_root(wm, event->window);
  if (!screen) {
    return 0;
  }

  wm_screen_set_current_desktop(wm, screen, (unsigned)event->data.l[0]);
  return 1;
}

static int wm_handle_net_wm_desktop(WM *wm, XClientMessageEvent *event) {
  WMClient *client = wm_client_find(wm, event->window);
  if (!client) {
    return 0;
  }

  wm_client_set_desktop(wm, client, (unsigned)event->data.l[0]);
  return 1;
}

static int wm_handle_net_showing_desktop(WM *wm, XClientMessageEvent *event) {
  WMScreen *screen = wm_i_screen_from_root(wm, event->window);
  if (!screen) {
    return 0;
  }

  screen->showing_desktop = event->data.l[0] != 0;
  wm_i_screen_publish_showing_desktop(wm, screen);
  wm_request_layout(wm, screen);
  return 1;
}

/* Answered for windows that are not managed yet, with the extents the user
 * announced in wm->frame_extents. */
static int wm_handle_request_frame_extents(WM *wm, XClientMessageEvent *event) {
  WMClient *client = wm_client_find(wm, event->window);
  if (client) {
    wm_i_client_publish_frame_extents(wm, client);
    return 1;
  }

  const WMExtents *e = &wm->frame_extents;
  long extents[4] = {e->left, e->right, e->top, e->bottom};
  wm_i_set_cardinals(wm, event->window, wm->net_atoms[NetFrameExtents], extents,
                     4);
  return 1;
}

static int wm_handle_client_message(WM *wm, XClientMessageEvent *event) {
  Atom type = event->message_type;

  if (type == wm->net_atoms[NetWMState]) {
    return wm_handle_net_wm_state(wm, event);
  } else if (type == wm->net_atoms[NetActiveWindow]) {
    return wm_handle_net_active_window(wm, event);
  } else if (type == wm->net_atoms[NetCloseWindow]) {
    return wm_handle_net_close_window(wm, event);
  } else if (type == wm->net_atoms[NetMoveResizeWindow]) {
    return wm_handle_net_moveresize_window(wm, event);
  } else if (type == wm->net_atoms[NetWMMoveResize]) {
    return wm_i_handle_net_wm_moveresize(wm, event);
  } else if (type == wm->net_atoms[NetCurrentDesktop]) {
    return wm_handle_net_current_desktop(wm, event);
  } else if (type == wm->net_atoms[NetWMDesktop]) {
    return wm_handle_net_wm_desktop(wm, event);
  } else if (type == wm->net_atoms[NetShowingDesktop]) {
    return wm_handle_net_showing_desktop(wm, event);
  } else if (type == wm->net_atoms[NetRequestFrameExtents]) {
    return wm_handle_request_frame_extents(wm, event);
  } else if (type == wm->wm_atoms[WMProtocols]) {
    return wm_handle_wm_protocols(wm, event);
  }

  return 0;
}

static int wm_handle_configure_notify(WM *wm, XConfigureEvent *event) {
  WMScreen *screen = wm_i_screen_from_root(wm, event->window);
  if (screen) {
    screen->width = event->width;
    screen->height = event->height;
    return 0;
  }

  /* The geometry of a framed client is owned by the library. */
  WMClient *client = wm_client_find(wm, event->window);
  if (!client || client->frame) {
    return 0;
  }

  client->x = event->x;
  client->y = event->y;
  client->width = event->width;
  client->height = event->height;
  client->border_width = event->border_width;

  return 0;
}

static int wm_handle_map_notify(WM *wm, XMapEvent *event) {
  WMClient *client = wm_client_find(wm, event->window);
  if (client && event->window == client->window) {
    client->is_mapped = 1;
    wm_request_layout(wm, client->screen);
  }
  return 0;
}

static int wm_handle_mapping_notify(WM *wm, XMappingEvent *event) {
  XRefreshKeyboardMapping(event);

  if (event->request == MappingKeyboard || event->request == MappingModifier) {
    wm_i_update_numlock_mask(wm);
    wm_i_regrab_keys(wm);
  }

  return 0;
}

static int wm_handle_key_press(WM *wm, XKeyEvent *event) {
  unsigned state = wm_clean_modifiers(wm, event->state);

  for (size_t i = 0; i < wm->key_grab_count; ++i) {
    WMKeyGrab *grab = &wm->key_grabs[i];
    if (!grab->callback || grab->screen->root != event->root ||
        XKeysymToKeycode(wm->display, grab->keysym) != event->keycode ||
        wm_clean_modifiers(wm, grab->modifiers) != state) {
      continue;
    }

    WMKeyCallback callback = grab->callback;
    void *arg = grab->arg;
    callback(wm, arg);
    return 1;
  }

  return 0;
}

static int wm_handle_expose(WM *wm, XExposeEvent *event) {
  WMClient *client = wm_client_find(wm, event->window);
  if (!client || client->frame != event->window || event->count > 0) {
    return 0;
  }

  if (wm->on_frame_expose) {
    wm->on_frame_expose(wm, client);
  }
  return 0;
}

/* Another window manager asked for the manager selection. */
static int wm_handle_selection_clear(WM *wm, XSelectionClearEvent *event) {
  for (size_t i = 0; i < wm->screen_count; ++i) {
    if (event->window == wm->screens[i].check_window) {
      wm_i_report(wm, "replaced by another window manager");
      wm_stop(wm);
      return 1;
    }
  }
  return 0;
}

static int wm_is_randr_event(WM *wm, int type) {
  return wm->randr_event_base &&
         (type == wm->randr_event_base + RRScreenChangeNotify ||
          type == wm->randr_event_base + RRNotify);
}

static void wm_handle_randr_event(WM *wm, XEvent *event) {
  WMScreen *screen = wm_i_screen_from_root(wm, event->xany.window);
  if (!screen) {
    return;
  }

  if (event->type == wm->randr_event_base + RRScreenChangeNotify) {
    XRRUpdateConfiguration(event);
    screen->width = ((XRRScreenChangeNotifyEvent *)event)->width;
    screen->height = ((XRRScreenChangeNotifyEvent *)event)->height;
  }

  if (wm_i_screen_update_monitors(wm, screen) != 0) {
    return;
  }
  wm_request_layout(wm, screen);
  if (wm->on_monitors_changed) {
    wm->on_monitors_changed(wm, screen);
  }
}

void wm_i_handle_event(WM *wm, XEvent *event) {
  if (!wm || !event) {
    return;
  }

  if (wm_is_randr_event(wm, event->type)) {
    wm_handle_randr_event(wm, event);
    return;
  }

  int ret = 0;
  switch (event->type) {
  case MapRequest:
    ret = wm_handle_map_request(wm, &event->xmaprequest);
    break;
  case UnmapNotify:
    ret = wm_handle_unmap_notify(wm, &event->xunmap);
    break;
  case DestroyNotify:
    ret = wm_handle_destroy_notify(wm, &event->xdestroywindow);
    break;
  case ConfigureRequest:
    ret = wm_handle_configure_request(wm, &event->xconfigurerequest);
    break;
  case PropertyNotify:
    ret = wm_handle_property_notify(wm, &event->xproperty);
    break;
  case ClientMessage:
    ret = wm_handle_client_message(wm, &event->xclient);
    break;
  case ConfigureNotify:
    ret = wm_handle_configure_notify(wm, &event->xconfigure);
    break;
  case MapNotify:
    ret = wm_handle_map_notify(wm, &event->xmap);
    break;
  case MappingNotify:
    ret = wm_handle_mapping_notify(wm, &event->xmapping);
    break;
  case KeyPress:
    ret = wm_handle_key_press(wm, &event->xkey);
    break;
  case Expose:
    ret = wm_handle_expose(wm, &event->xexpose);
    break;
  case SelectionClear:
    ret = wm_handle_selection_clear(wm, &event->xselectionclear);
    break;
  default:
    break;
  }

  if (event->type < LASTEvent && wm->event_handlers[event->type] && !ret) {
    wm->event_handlers[event->type](wm, event);
  }

  /* Release the client only after the user had a chance to look at it. */
  if (event->type == DestroyNotify) {
    WMClient *client = wm_client_find(wm, event->xdestroywindow.window);
    if (client && client->window == event->xdestroywindow.window) {
      wm_client_destroy(wm, client);
    }
  }
}

/* Grabs the pointer, unless no button is held: the drag would never end. */
static int wm_grab_pointer(WM *wm, WMScreen *screen) {
  if (XGrabPointer(wm->display, screen->root, False,
                   ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                   GrabModeAsync, GrabModeAsync, None, None,
                   CurrentTime) != GrabSuccess) {
    return -1;
  }

  Window root, child;
  int root_x, root_y, win_x, win_y;
  unsigned mask;
  XQueryPointer(wm->display, screen->root, &root, &child, &root_x, &root_y,
                &win_x, &win_y, &mask);
  if (!(mask & WM_BUTTON_MASK)) {
    XUngrabPointer(wm->display, CurrentTime);
    return -1;
  }

  return 0;
}

/* Moves the client, or resizes it by the given edges (NULL to move). */
static void wm_drag_step(WM *wm, WMClient *client, const WMRect *start, int dx,
                         int dy, const WMEdges *edges) {
  if (!edges) {
    wm_client_set_geometry(wm, client, start->x + dx, start->y + dy,
                           start->width, start->height);
    return;
  }

  int w = start->width + (edges->right ? dx : 0) - (edges->left ? dx : 0);
  int h = start->height + (edges->bottom ? dy : 0) - (edges->top ? dy : 0);
  wm_client_apply_size_hints(wm, client, &w, &h);

  int x = edges->left ? start->x + start->width - w : start->x;
  int y = edges->top ? start->y + start->height - h : start->y;
  wm_client_set_geometry(wm, client, x, y, w, h);
}

int wm_i_client_drag(WM *wm, WMClient *client, int x_root, int y_root,
                     const WMEdges *edges) {
  if (!wm || !client || client->is_fullscreen) {
    return -1;
  }
  WM_TRACE(wm);

  if (wm_grab_pointer(wm, client->screen) != 0) {
    return -1;
  }

  WMRect start = {client->x, client->y, client->width, client->height};
  XEvent event;

  do {
    XMaskEvent(wm->display, WM_DRAG_MASK, &event);
    switch (event.type) {
    case ConfigureRequest:
    case MapRequest:
    case Expose:
      wm_i_handle_event(wm, &event);
      break;
    case MotionNotify:
      wm_drag_step(wm, client, &start, event.xmotion.x_root - x_root,
                   event.xmotion.y_root - y_root, edges);
      break;
    default:
      break;
    }
  } while (event.type != ButtonRelease);

  XUngrabPointer(wm->display, CurrentTime);
  return 0;
}

int wm_i_get_text_prop(WM *wm, Window window, Atom atom, char *text,
                       unsigned size) {
  char **list = NULL;
  int count = 0;
  XTextProperty prop;

  if (!text || size == 0) {
    return -1;
  }
  text[0] = '\0';

  prop.value = NULL;
  if (!XGetTextProperty(wm->display, window, &prop, atom)) {
    return -1;
  }
  if (!prop.value || prop.nitems == 0) {
    if (prop.value) {
      XFree(prop.value);
    }
    return -1;
  }

  if (prop.encoding == XA_STRING) {
    strncpy(text, (char *)prop.value, size - 1);
  } else if (XmbTextPropertyToTextList(wm->display, &prop, &list, &count) >=
             Success) {
    if (count > 0 && list && list[0]) {
      strncpy(text, list[0], size - 1);
    }
    if (list) {
      XFreeStringList(list);
    }
  }

  text[size - 1] = '\0';
  XFree(prop.value);
  return text[0] ? 0 : -1;
}

void wm_i_configure_frame(WM *wm, WMClient *client) {
  const WMExtents *e = &client->frame_extents;
  XWindowChanges changes;

  changes.x = client->x - e->left - client->border_width;
  changes.y = client->y - e->top - client->border_width;
  changes.width = client->width + e->left + e->right;
  changes.height =
      client->is_shaded ? MAX(1, e->top) : client->height + e->top + e->bottom;
  changes.border_width = client->border_width;

  XConfigureWindow(wm->display, client->frame,
                   CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &changes);
  XMoveResizeWindow(wm->display, client->window, e->left, e->top,
                    (unsigned)client->width, (unsigned)client->height);
}