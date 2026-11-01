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

int wm_client_get_geometry(WM *wm, WMClient *client, int *x, int *y, int *width,
                           int *height) {
  if (!wm || !client || !x || !y || !width || !height) {
    return -1;
  }

  *x = client->x;
  *y = client->y;
  *width = client->width;
  *height = client->height;

  return 0;
}

int wm_client_set_geometry(WM *wm, WMClient *client, int x, int y, int width,
                           int height) {
  if (!wm || !client) {
    return -1;
  }

  client->x = x;
  client->y = y;
  client->width = width;
  client->height = height;

  return wm_client_configure(wm, client);
}

int wm_client_save_geometry(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return -1;
  }

  client->old_x = client->x;
  client->old_y = client->y;
  client->old_width = client->width;
  client->old_height = client->height;
  client->old_border_width = client->border_width;

  return 0;
}

int wm_client_restore_geometry(WM *wm, WMClient *client) {
  if (!wm || !client) {
    return -1;
  }

  client->border_width = client->old_border_width;
  return wm_client_set_geometry(wm, client, client->old_x, client->old_y,
                                client->old_width, client->old_height);
}

int wm_client_apply_size_hints(WM *wm, WMClient *client, int *width,
                               int *height) {
  if (!wm || !client || !width || !height) {
    return -1;
  }

  int w = MAX(1, *width);
  int h = MAX(1, *height);

  /* ICCCM 4.1.2.3: base size only counts for aspect when it is not the
   * minimum size. */
  int base_is_min = client->base_width == client->min_width &&
                    client->base_height == client->min_height;
  if (!base_is_min) {
    w -= client->base_width;
    h -= client->base_height;
  }

  if (client->min_aspect > 0 && client->max_aspect > 0) {
    if (client->max_aspect < (float)w / h) {
      w = (int)(h * client->max_aspect + 0.5f);
    } else if (client->min_aspect > (float)w / h) {
      h = (int)(w / client->min_aspect + 0.5f);
    }
  }

  if (base_is_min) {
    w -= client->base_width;
    h -= client->base_height;
  }

  if (client->increment_width) {
    w -= w % client->increment_width;
  }
  if (client->increment_height) {
    h -= h % client->increment_height;
  }

  w = MAX(w + client->base_width, client->min_width);
  h = MAX(h + client->base_height, client->min_height);
  if (client->max_width) {
    w = MIN(w, client->max_width);
  }
  if (client->max_height) {
    h = MIN(h, client->max_height);
  }

  *width = w;
  *height = h;

  return 0;
}

