// Minimal example of using the wmlib-cpp to create a simple window manager.

#include "wmlib-cpp/geometry.hpp"
#include "wmlib-cpp/screen.hpp"
#include "wmlib-cpp/types.hpp"
#include <X11/X.h>
#include <wmlib-cpp/wm.hpp>

inline bool is_tiled_on(wmlib::Client &client, wmlib::Screen &screen,
                        wmlib::Monitor &monitor) {
  return client.get_screen() == screen && !client.is_floating() &&
         client.is_visible() && client.get_monitor() == monitor;
}

void arrange_monitor(wmlib::Manager &wm, wmlib::Screen &screen,
                     wmlib::Monitor &monitor) {
  int n = 0;
  auto clients = wm.get_clients();
  for (auto &client : clients) {
    if (is_tiled_on(client, screen, monitor))
      n++;
  }

  if (n == 0)
    return;

  wmlib::Rect area = monitor.get_workarea();
  int master_w = n > 1 ? area.width / 2 : area.width;
  int i = 0;

  for (auto &client : clients) {
    if (!is_tiled_on(client, screen, monitor))
      continue;

    int x = area.x, y = area.y, w = master_w, h = area.height;
    if (i > 0) {
      h = area.height / (n - 1);
      x = area.x + master_w;
      y = area.y + (i - 1) * h;
      w = area.width - master_w;
    }

    w -= 2 * client.get_border_width();
    h -= 2 * client.get_border_width();
    client.apply_size_hints(w, h);
    client.set_geometry(wmlib::Rect{x, y, w, h});
    i++;
  }
}

int main() {
  wmlib::Manager wm("Example WM");
  wm.init();

  wm.on_manage([&](wmlib::Client client) {
    client.select_input(EnterWindowMask);
    client.map();
    client.focus();
  });
  wm.on_layout([&](wmlib::Screen &screen) {
    for (auto &monitor : screen.get_monitors())
      arrange_monitor(wm, screen, monitor);
  });
  wm.set_event_handler(wmlib::EventType::EnterNotifyEvent,
                       [](wmlib::Manager &wm, XEvent *event) {
                         XCrossingEvent *e = &event->xcrossing;
                         if (e->mode != NotifyNormal ||
                             e->detail == NotifyInferior)
                           return;

                         wmlib::Client client = wm.find_client(e->window);
                         client.focus();
                       });

  for (auto &screen_ref : wm.get_screens()) {
    auto &screen = screen_ref.get();
    screen.scan(nullptr);
  }

  wm.run();
  return 0;
}