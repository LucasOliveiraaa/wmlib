#include "wmlib/client.h"
#include "wmlib/screen.h"
#include "wmlib/wm.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define BAR_HEIGHT 20
#define BAR_PADDING 6

typedef struct {
  Window window;
  GC gc;
  XFontStruct *font;
  int width;
} Bar;

static Bar bar;

static void draw_bar(WM *wm) {
  WMClient *focused = wm_screen_get_focused(wm_get_screen(wm, 0));
  const char *title = focused ? wm_client_get_title(focused) : "";
  char clock[64];
  time_t now = time(NULL);
  strftime(clock, sizeof(clock), "%a %d %b %H:%M", localtime(&now));

  int baseline = (BAR_HEIGHT + bar.font->ascent - bar.font->descent) / 2;
  int clock_width = XTextWidth(bar.font, clock, (int)strlen(clock));

  XClearWindow(wm_get_display(wm), bar.window);
  XDrawString(wm_get_display(wm), bar.window, bar.gc, BAR_PADDING, baseline,
              title, (int)strlen(title));
  XDrawString(wm_get_display(wm), bar.window, bar.gc,
              bar.width - clock_width - BAR_PADDING, baseline, clock,
              (int)strlen(clock));
}

static void create_bar(WM *wm) {
  WMScreen *screen = wm_get_screen(wm, 0);
  Display *display = wm_get_display(wm);
  unsigned long black = BlackPixel(display, wm_screen_get_number(screen));
  unsigned long white = WhitePixel(display, wm_screen_get_number(screen));

  bar.width = wm_monitor_get_geometry(wm_screen_get_monitor(screen, 0)).width;
  bar.window =
      XCreateSimpleWindow(display, wm_screen_get_root(screen), 0, 0,
                          (unsigned)bar.width, BAR_HEIGHT, 0, black, black);
  bar.font = XLoadQueryFont(display, "fixed");
  bar.gc = XCreateGC(display, bar.window, 0, NULL);
  XSetForeground(display, bar.gc, white);
  XSetFont(display, bar.gc, bar.font->fid);

  Atom type = wm_get_net_atom(wm, NetWMWindowTypeDock);
  XChangeProperty(display, bar.window, wm_get_net_atom(wm, NetWMWindowType),
                  XA_ATOM, 32, PropModeReplace, (unsigned char *)&type, 1);

  long strut[12] = {0, 0, BAR_HEIGHT, 0, 0, 0, 0, 0, 0, bar.width - 1, 0, 0};
  XChangeProperty(display, bar.window, wm_get_net_atom(wm, NetWMStrutPartial),
                  XA_CARDINAL, 32, PropModeReplace, (unsigned char *)strut, 12);

  XMapWindow(display, bar.window);
  wm_client_create(wm, screen, bar.window);
}

static int is_visible_on(WM *wm, WMClient *c, WMScreen *screen,
                         WMMonitor *monitor) {
  return wm_client_get_screen(c) == screen && !wm_client_is_floating(c) &&
         wm_client_is_visible(wm, c) && wm_client_monitor(wm, c) == monitor;
}

/* One full-size window per monitor, the focused one on top. */
static void on_layout(WM *wm, WMScreen *screen) {
  for (size_t i = 0; i < wm_screen_get_monitor_count(screen); ++i) {
    WMMonitor *monitor = wm_screen_get_monitor(screen, i);

    for (WMClientNode *node = wm_get_first_client_node(wm); node;
         node = node->next) {
      WMClient *c = node->client;
      if (!is_visible_on(wm, c, screen, monitor))
        continue;

      WMRect area = wm_monitor_get_workarea(monitor);
      wm_client_set_geometry(wm, c, area.x, area.y,
                             area.width - 2 * wm_client_get_border_width(c),
                             area.height - 2 * wm_client_get_border_width(c));
    }
  }
}

static void on_manage(WM *wm, WMClient *client) {
  if (wm_client_get_window(client) == bar.window) {
    wm_client_select_input(wm, client, ExposureMask);
    wm_client_map(wm, client);
    return;
  }

  wm_client_map(wm, client);
  wm_client_focus(wm, client);
}

static void on_focus(WM *wm, WMClient *client) {
  wm_client_raise(wm, client);
  draw_bar(wm);
}

static void on_unfocus(WM *wm, WMClient *client) {
  (void)client;
  draw_bar(wm);
}

static void on_client_changed(WM *wm, WMClient *client, unsigned change) {
  WMScreen *screen = wm_client_get_screen(client);
  WMClient *focused = wm_screen_get_focused(screen);
  if ((change & WM_CHANGE_TITLE) && client == focused) {
    draw_bar(wm);
  }
}

static void on_tick(WM *wm, int id) {
  (void)id;
  draw_bar(wm);
}

static void on_expose(WM *wm, XEvent *event) {
  if (event->xexpose.window == bar.window && event->xexpose.count == 0)
    draw_bar(wm);
}

int main(void) {
  WM *wm = wm_create();
  if (!wm || wm_init(wm) != 0) {
    wm_destroy(wm);
    return 1;
  }

  wm_set_on_manage(wm, on_manage);
  wm_set_on_layout(wm, on_layout);
  wm_set_on_focus(wm, on_focus);
  wm_set_on_unfocus(wm, on_unfocus);
  wm_set_on_client_changed(wm, on_client_changed);

  wm_set_event_handler(wm, Expose, on_expose);

  for (size_t i = 0; i < wm_get_screen_count(wm); ++i)
    wm_scan_screen(wm, wm_get_screen(wm, i), NULL, NULL);

  create_bar(wm);
  wm_add_timer(wm, 1000, 1, on_tick);

  wm_run(wm);
  wm_destroy(wm);
  return 0;
}