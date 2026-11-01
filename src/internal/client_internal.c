#include "client.h"
#include "report.h"
#include "screen.h"
#include "util.h"
#include "wm.h"
#include <X11/Xatom.h>
#include <stdlib.h>
#include <string.h>

void wm_i_client_read_net_state(WM *wm, WMClient *client) {
  unsigned long count;
  Atom *atoms =
      wm_i_get_atom_list(wm, client->window, wm->net_atoms[NetWMState], &count);

  for (size_t i = 0; i < LENGTH(wm_net_states); ++i) {
    *wm_i_client_state_flag(client, wm_net_states[i]) =
        wm_i_atom_list_contains(atoms, count, wm->net_atoms[wm_net_states[i]]);
  }

  if (atoms) {
    XFree(atoms);
  }
}

WMClientNode *wm_i_client_get_tip(WM *wm) {
  WMClientNode *tip = wm->client_nodes;
  while (tip && tip->next) {
    tip = tip->next;
  }
  return tip;
}

void wm_i_client_add_node(WM *wm, WMClientNode *node) {
  if (!wm || !node) {
    return;
  }

  node->next = NULL;
  WMClientNode *tip = wm_i_client_get_tip(wm);
  if (tip) {
    tip->next = node;
  } else {
    wm->client_nodes = node;
  }
}

WMClientNode *wm_i_client_find_node(WM *wm, Window window) {
  if (!wm) {
    return NULL;
  }

  for (WMClientNode *cur = wm->client_nodes; cur; cur = cur->next) {
    WMClient *client = cur->client;
    if (client->window == window ||
        (client->frame && client->frame == window)) {
      return cur;
    }
  }
  return NULL;
}

WMClientNode *wm_i_client_find_node_by_client(WM *wm, WMClient *client) {
  for (WMClientNode *cur = wm->client_nodes; cur; cur = cur->next) {
    if (cur->client == client) {
      return cur;
    }
  }
  return NULL;
}

void wm_i_client_remove_node(WM *wm, WMClientNode *node) {
  if (!wm || !node) {
    return;
  }

  WMClientNode *prev = NULL;
  for (WMClientNode *cur = wm->client_nodes; cur; prev = cur, cur = cur->next) {
    if (cur == node) {
      if (prev) {
        prev->next = cur->next;
      } else {
        wm->client_nodes = cur->next;
      }
      free(cur);
      return;
    }
  }
}

WMClientNode *wm_i_client_create_node(WM *wm, WMClient *client) {
  WMClientNode *node = (WMClientNode *)malloc(sizeof(WMClientNode));
  if (!node) {
    return NULL;
  }

  node->client = client;
  node->next = NULL;
  wm_i_client_add_node(wm, node);
  return node;
}

int *wm_i_client_state_flag(WMClient *client, int net_atom) {
  switch (net_atom) {
  case NetWMFullscreen:
    return &client->is_fullscreen;
  case NetWMMaximizedVert:
    return &client->is_maximized_vert;
  case NetWMMaximizedHorz:
    return &client->is_maximized_horz;
  case NetWMAbove:
    return &client->is_above;
  case NetWMBelow:
    return &client->is_below;
  case NetWMSticky:
    return &client->is_sticky;
  case NetWMHidden:
    return &client->is_hidden;
  case NetWMDemandsAttention:
    return &client->is_demanding_attention;
  case NetWMStateShaded:
    return &client->is_shaded;
  default:
    return NULL;
  }
}

static int wm_i_client_supports_protocol(WM *wm, WMClient *client, Atom proto) {
  Atom *protocols = NULL;
  int n = 0, exists = 0;

  if (XGetWMProtocols(wm->display, client->window, &protocols, &n)) {
    for (int i = 0; i < n && !exists; ++i) {
      exists = protocols[i] == proto;
    }
    XFree(protocols);
  }
  return exists;
}

/* Sends a WM_PROTOCOLS message: data.l[0] = proto, l[1] = data1, l[2] =
 * data2. Returns whether the client supports the protocol and the message
 * was sent. */
int wm_i_client_send_protocol(WM *wm, WMClient *client, Atom proto, long data1,
                              long data2) {
  if (!wm_i_client_supports_protocol(wm, client, proto)) {
    return 0;
  }

  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ClientMessage;
  ev.xclient.window = client->window;
  ev.xclient.message_type = wm->wm_atoms[WMProtocols];
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = (long)proto;
  ev.xclient.data.l[1] = data1;
  ev.xclient.data.l[2] = data2;

  return XSendEvent(wm->display, client->window, False, NoEventMask, &ev) != 0;
}

/* ICCCM 4.2.3: root coordinates; a reparented window has no border of its
 * own. */
int wm_i_client_send_configure_notify(WM *wm, WMClient *client) {
  XConfigureEvent ce;
  memset(&ce, 0, sizeof(ce));
  ce.type = ConfigureNotify;
  ce.display = wm->display;
  ce.event = client->window;
  ce.window = client->window;
  ce.x = client->x;
  ce.y = client->y;
  ce.width = client->width;
  ce.height = client->height;
  ce.border_width = client->frame ? 0 : client->border_width;
  ce.above = None;
  ce.override_redirect = False;

  if (!XSendEvent(wm->display, client->window, False, StructureNotifyMask,
                  (XEvent *)&ce)) {
    wm_i_report(wm, "failed to send ConfigureNotify to 0x%lx", client->window);
    return -1;
  }
  return 0;
}

void wm_i_client_publish_frame_extents(WM *wm, WMClient *client) {
  int border = client->border_width;
  WMExtents e = client->frame ? client->frame_extents : (WMExtents){0, 0, 0, 0};
  long extents[4] = {e.left + border, e.right + border, e.top + border,
                     e.bottom + border};
  wm_i_set_cardinals(wm, client->window, wm->net_atoms[NetFrameExtents],
                     extents, 4);
}

void wm_i_client_publish_allowed_actions(WM *wm, WMClient *client) {
  Atom actions[WM_MAX_ACTIONS];
  size_t length = 0;

#define ALLOW(index) actions[length++] = wm->net_atoms[index]

  ALLOW(NetWMActionMove);
  if (!client->is_fixed) {
    ALLOW(NetWMActionResize);
    ALLOW(NetWMActionMaximizeHorz);
    ALLOW(NetWMActionMaximizeVert);
  }
  if (client->frame) {
    ALLOW(NetWMActionShade);
  }
  ALLOW(NetWMActionStick);
  ALLOW(NetWMActionFullscreen);
  ALLOW(NetWMActionChangeDesktop);
  ALLOW(NetWMActionClose);

#undef ALLOW

  XChangeProperty(wm->display, client->window,
                  wm->net_atoms[NetWMAllowedActions], XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)actions, (int)length);
}

/* Gives the window back to the root and drops the frame, without touching
 * any property (the window may already be gone). */
void wm_i_client_release_frame(WM *wm, WMClient *client) {
  if (!client->frame) {
    return;
  }

  XReparentWindow(wm->display, client->window, client->screen->root, client->x,
                  client->y);
  XDestroyWindow(wm->display, client->frame);
  client->frame = None;
  client->frame_extents = (WMExtents){0, 0, 0, 0};
}