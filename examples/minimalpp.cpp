// Minimal example of using the wmlib-cpp to create a simple window manager.

#include "wmlib-cpp/geometry.hpp"
#include "wmlib-cpp/screen.hpp"
#include <wmlib-cpp/wm.hpp>

void show(wmlib::Client client, wmlib::Screen &screen) {
  client.set_geometry(wmlib::Rect{
      .width = screen.get_width(),
      .height = screen.get_height(),
  });
  client.map();
  client.focus();
}

int main() {
  wmlib::Manager wm("Example WM");
  wm.init();
  wmlib::Screen &screen = wm.get_screen(0);

  wm.on_manage([&](wmlib::Client client) { show(client, screen); });
  wm.set_loop_timeout(1000);

  for (auto &screen_ref : wm.get_screens()) {
    auto &screen = screen_ref.get();
    screen.scan([&](wmlib::Manager &wm, wmlib::Client client, wmlib::ScanKind kind,
                    void *userdata) { show(client, screen); });
  }

  wm.run();
  return 0;
}