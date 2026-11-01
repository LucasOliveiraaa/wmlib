// Minimal example of using the wmlib to create a simple window manager.

#include "wmlib/client.h"
#include "wmlib/wm.h"

static void show(WM *wm, WMClient *client) {
  wm_client_set_geometry(wm, client, 0, 0,
                         wm_screen_get_width(wm_get_screen_const(wm, 0)),
                         wm_screen_get_height(wm_get_screen_const(wm, 0)));
  wm_client_map(wm, client);
  wm_client_focus(wm, client);
}

static void on_manage(WM *wm, WMClient *client) { show(wm, client); }

static void on_scan(WM *wm, WMClient *client, WMScanKind kind, void *userdata) {
  (void)kind;
  (void)userdata;
  show(wm, client);
}

int main(void) {
  WM *wm = wm_create();
  if (!wm || wm_init(wm) != 0) {
    wm_destroy(wm);
    return 1;
  }

  wm_set_on_manage(wm, on_manage);
  wm_set_loop_timeout(wm, 1000);

  for (size_t i = 0; i < wm_get_screen_count(wm); ++i) {
    wm_scan_screen(wm, wm_get_screen(wm, i), on_scan, NULL);
  }

  wm_run(wm);
  wm_destroy(wm);
  return 0;
}