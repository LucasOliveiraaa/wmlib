#include "wmlib-cpp/screen.hpp"
#include "wmlib-cpp/client.hpp"
#include "wmlib-cpp/wm.hpp"
#include "wmlib/client.h"
#include "wmlib/screen.h"
#include <stdexcept>

namespace wmlib {

Rect Monitor::get_geometry() const { return wm_monitor_get_geometry(monitor_); }

Rect Monitor::get_workarea() const { return wm_monitor_get_workarea(monitor_); }

void Screen::bind_key(unsigned modifiers, KeySym keysym,
                      std::function<void(Manager &, void *)> callback, void *arg) {
  auto binding = std::make_unique<KeyBind>(KeyBind{callback, arg, this});

  wm_bind_key(wm_.wm(), screen_, modifiers, keysym,
              &Screen::key_bind_trampoline, binding.get());

  key_bindings_.push_back(std::move(binding));
}

void Screen::scan(std::function<void(Manager &, Client, ScanKind, void *)> callback,
                  void *userdata) {
  auto context =
      std::make_unique<ScanContext>(ScanContext{callback, userdata, this});
  wm_scan_screen(wm_.wm(), screen_, &Screen::scan_trampoline, context.get());
  scan_context_ = std::move(context);
}

void Screen::set_desktops(unsigned count) {
  wm_screen_set_desktops(wm_.wm(), screen_, count);
}
void Screen::set_current_desktop(unsigned desktop) {
  wm_screen_set_current_desktop(wm_.wm(), screen_, desktop);
}
void Screen::set_workarea(Rect workarea) {
  wm_screen_set_workarea(wm_.wm(), screen_, workarea);
}

void Screen::grab_key(unsigned modifiers, KeySym keysym) {
  wm_grab_key(wm_.wm(), screen_, modifiers, keysym);
}
void Screen::ungrab_keys() { wm_ungrab_keys(wm_.wm(), screen_); }

void Screen::request_layout() { wm_request_layout(wm_.wm(), screen_); }

Monitor Screen::monitor_at(int x, int y) const {
  ::WMMonitor *monitor = wm_monitor_at(wm_.wm(), screen_, x, y);
  if (!monitor) {
    throw std::runtime_error("No monitor contains the point");
  }
  return Monitor(wm_, monitor);
}

int Screen::get_number() const { return wm_screen_get_number(screen_); }
Window Screen::get_root() const { return wm_screen_get_root(screen_); }

int Screen::get_width() const { return wm_screen_get_width(screen_); }
int Screen::get_height() const { return wm_screen_get_height(screen_); }

Window Screen::get_check_window() const {
  return wm_screen_get_check_window(screen_);
}
::Atom Screen::get_selection() const {
  return wm_screen_get_selection(screen_);
}

size_t Screen::get_monitor_count() const {
  return wm_screen_get_monitor_count(screen_);
}
Monitor Screen::get_monitor(size_t index) {
  ::WMMonitor *monitor = wm_screen_get_monitor(screen_, index);
  if (!monitor) {
    throw std::runtime_error("Monitor index out of range");
  }
  return Monitor(wm_, monitor);
}
std::vector<Monitor> Screen::get_monitors() {
  std::vector<Monitor> monitors;
  size_t count = get_monitor_count();
  for (size_t i = 0; i < count; ++i) {
    monitors.push_back(get_monitor(i));
  }
  return monitors;
}

unsigned Screen::get_desktop_count() const {
  return wm_screen_get_desktop_count(screen_);
}
unsigned Screen::get_current_desktop() const {
  return wm_screen_get_current_desktop(screen_);
}
bool Screen::is_showing_desktop() const {
  return wm_screen_is_showing_desktop(screen_);
}

Rect Screen::get_workarea() const { return wm_screen_get_workarea(screen_); }
Client Screen::get_focused() const {
  ::WMClient *client = wm_screen_get_focused(screen_);
  if (!client) {
    throw std::runtime_error("No focused client on this screen");
  }
  return Client(wm_, client);
}

Client Screen::create_client(Window window) {
  ::WMClient *client = wm_client_create(wm_.wm(), screen_, window);
  if (!client) {
    throw std::runtime_error("Failed to create client");
  }
  return Client(wm_, client);
}

} // namespace wmlib