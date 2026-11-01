#include "wmlib/client.h"
#include "wmlib/screen.h"
#include "wmlib/wm.h"

static int is_tiled_on(WM *wm, WMClient *c, WMScreen *screen,
                       WMMonitor *monitor) {
  return wm_client_get_screen(c) == screen && !wm_client_is_floating(c) &&
         wm_client_is_visible(wm, c) && wm_client_monitor(wm, c) == monitor;
}

static void arrange_monitor(WM *wm, WMScreen *screen, WMMonitor *monitor) {
  int n = 0;
  for (WMClientNode *node = wm_get_first_client_node(wm); node;
       node = node->next)
    if (is_tiled_on(wm, node->client, screen, monitor))
      n++;
  if (n == 0)
    return;

  WMRect area = monitor->workarea;
  int master_w = n > 1 ? area.width / 2 : area.width;
  int i = 0;

  for (WMClientNode *node = wm_get_first_client_node(wm); node;
       node = node->next) {
    WMClient *c = node->client;
    if (!is_tiled_on(wm, c, screen, monitor))
      continue;

    int x = area.x, y = area.y, w = master_w, h = area.height;
    if (i > 0) {
      h = area.height / (n - 1);
      x = area.x + master_w;
      y = area.y + (i - 1) * h;
      w = area.width - master_w;
    }

    w -= 2 * wm_client_get_border_width(c);
    h -= 2 * wm_client_get_border_width(c);
    wm_client_apply_size_hints(wm, c, &w, &h);
    wm_client_set_geometry(wm, c, x, y, w, h);
    i++;
  }
}

static void on_layout(WM *wm, WMScreen *screen) {
  for (size_t i = 0; i < wm_screen_get_monitor_count(screen); ++i)
    arrange_monitor(wm, screen, wm_screen_get_monitor(screen, i));
}

static void on_enter(WM *wm, XEvent *event) {
  XCrossingEvent *e = &event->xcrossing;
  if (e->mode != NotifyNormal || e->detail == NotifyInferior)
    return;

  WMClient *c = wm_client_find(wm, e->window);
  if (c)
    wm_client_focus(wm, c);
}

static void on_manage(WM *wm, WMClient *client) {
  wm_client_select_input(wm, client, EnterWindowMask);
  wm_client_map(wm, client);
  wm_client_focus(wm, client);
}

int main(void) {
  WM *wm = wm_create();
  if (!wm || wm_init(wm) != 0) {
    wm_destroy(wm);
    return 1;
  }

  wm_set_on_manage(wm, on_manage);
  wm_set_on_layout(wm, on_layout);

  wm_set_event_handler(wm, EnterNotify, on_enter);

  for (size_t i = 0; i < wm_get_screen_count(wm); ++i)
    wm_scan_screen(wm, wm_get_screen(wm, i), NULL, NULL);

  wm_run(wm);
  wm_destroy(wm);
  return 0;
}