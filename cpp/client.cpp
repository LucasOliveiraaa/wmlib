#include "wmlib-cpp/client.hpp"
#include "wmlib-cpp/screen.hpp"
#include "wmlib/client.h"
#include "wmlib/wm.h"
#include <stdexcept>

namespace wmlib {

void Client::update_title() { wm_client_update_title(wm_.wm(), client_); }
void Client::update_hints() { wm_client_update_hints(wm_.wm(), client_); }
void Client::update_transient() {
  wm_client_update_transient(wm_.wm(), client_);
}
void Client::update_window_type() {
  wm_client_update_window_type(wm_.wm(), client_);
}
void Client::update_strut() { wm_client_update_strut(wm_.wm(), client_); }
void Client::configure() { wm_client_configure(wm_.wm(), client_); }

void Client::select_input(long event_mask) {
  wm_client_select_input(wm_.wm(), client_, event_mask);
}
long Client::get_state() const {
  long state;
  wm_client_get_state(wm_.wm(), client_, &state);
  return state;
}
void Client::set_state(long state) {
  wm_client_set_state(wm_.wm(), client_, state);
}

void Client::set_net_state(NetAtom atom, bool enabled) {
  wm_client_set_net_state(wm_.wm(), client_, static_cast<int>(atom),
                          enabled ? 1 : 0);
}
void Client::set_fullscreen(bool enabled) {
  wm_client_set_fullscreen(wm_.wm(), client_, enabled ? 1 : 0);
}

void Client::focus() { wm_client_focus(wm_.wm(), client_); }
void Client::unfocus() { wm_client_unfocus(wm_.wm(), client_); }

void Client::map() { wm_client_map(wm_.wm(), client_); }
void Client::unmap() { wm_client_unmap(wm_.wm(), client_); }

void Client::close() { wm_client_close(wm_.wm(), client_); }
void Client::ping() { wm_client_ping(wm_.wm(), client_); }

/* Geometry */

void Client::set_geometry(Rect rect) {
  wm_client_set_geometry(wm_.wm(), client_, rect.x, rect.y, rect.width,
                         rect.height);
}
Rect Client::get_geometry() const {
  Rect rect;
  wm_client_get_geometry(wm_.wm(), client_, &rect.x, &rect.y, &rect.width,
                         &rect.height);
  return rect;
}
Rect Client::get_old_geometry() const {
  Rect rect;
  wm_client_get_old_geometry(client_, &rect);
  return rect;
}
void Client::save_geometry() { wm_client_save_geometry(wm_.wm(), client_); }
void Client::restore_geometry() {
  wm_client_restore_geometry(wm_.wm(), client_);
}

void Client::apply_size_hints(int &width, int &height) const {
  wm_client_apply_size_hints(wm_.wm(), client_, &width, &height);
}

void Client::begin_move(int x_root, int y_root) {
  wm_client_begin_move(wm_.wm(), client_, x_root, y_root);
}
void Client::begin_resize(int x_root, int y_root,
                          MoveResizeDirection direction) {
  wm_client_begin_resize(wm_.wm(), client_, x_root, y_root,
                         static_cast<WMMoveResizeDirection>(direction));
}

void Client::frame(Extents ext) { wm_client_frame(wm_.wm(), client_, ext); }
void Client::unframe() { wm_client_unframe(wm_.wm(), client_); }

void Client::raise() { wm_client_raise(wm_.wm(), client_); }
void Client::lower() { wm_client_lower(wm_.wm(), client_); }
void Client::restack(Client &sibling, bool above) {
  wm_client_restack(wm_.wm(), client_, sibling.client_, above ? 1 : 0);
}

void Client::set_border_width(int width) {
  wm_client_set_border_width(wm_.wm(), client_, width);
}
void Client::set_border_color(Color color) {
  wm_client_set_border_color(wm_.wm(), client_, color);
}
int Client::get_border_width() const {
  return wm_client_get_border_width(client_);
}

void Client::set_desktop(unsigned desktop) {
  wm_client_set_desktop(wm_.wm(), client_, desktop);
}

bool Client::is_visible() const {
  return wm_client_is_visible(wm_.wm(), client_) != 0;
}

Monitor Client::get_monitor() const {
  WMMonitor *monitor = wm_client_monitor(wm_.wm(), client_);
  if (!monitor) {
    throw std::runtime_error("Client is not on any monitor");
  }
  return Monitor(wm_, monitor);
}

Window Client::get_window() const { return wm_client_get_window(client_); }

Window Client::get_frame() const { return wm_client_get_frame(client_); }

Screen Client::get_screen() const {
  WMScreen *screen = wm_client_get_screen(client_);
  if (!screen) {
    throw std::runtime_error("Client is not on any screen");
  }
  return Screen(wm_, screen);
}

void *Client::get_userdata() const { return wm_client_get_userdata(client_); }

void Client::set_userdata(void *userdata) {
  wm_client_set_userdata(client_, userdata);
}

Extents Client::get_frame_extents() const {
  WMExtents ext;
  wm_client_get_frame_extents(client_, &ext);
  return Extents(ext);
}

void Client::get_base_size(int &width, int &height) const {
  wm_client_get_base_size(client_, &width, &height);
}

void Client::get_min_size(int &width, int &height) const {
  wm_client_get_min_size(client_, &width, &height);
}

void Client::get_max_size(int &width, int &height) const {
  wm_client_get_max_size(client_, &width, &height);
}

void Client::get_size_increments(int &width, int &height) const {
  wm_client_get_size_increments(client_, &width, &height);
}

void Client::get_aspect_ratio(float &min_aspect, float &max_aspect) const {
  wm_client_get_aspect_ratios(client_, &min_aspect, &max_aspect);
}

std::string Client::get_title() const {
  const char *title = wm_client_get_title(client_);
  if (!title) {
    throw std::runtime_error("Failed to get client title");
  }
  return std::string(title);
}
WindowType Client::get_window_type() const {
  return static_cast<WindowType>(wm_client_get_window_type(client_));
}
Window Client::get_transient_for() const {
  return wm_client_get_transient_for(client_);
}
Strut Client::get_strut() const {
  WMStrut strut = wm_client_get_strut(client_);
  return Strut(strut);
}
long Client::get_event_mask() const {
  return wm_client_get_event_mask(client_);
}

unsigned Client::get_desktop() const { return wm_client_get_desktop(client_); }
unsigned long Client::get_user_time() const {
  unsigned long user_time;
  wm_client_get_user_time(client_, &user_time);
  return user_time;
}

void Client::set_floating(bool floating) {
  wm_client_set_floating(wm_.wm(), client_, floating ? 1 : 0);
}
bool Client::is_floating() const { return wm_client_is_floating(client_) != 0; }
bool Client::is_fixed() const { return wm_client_is_fixed(client_) != 0; }
bool Client::is_fullscreen() const {
  return wm_client_is_fullscreen(client_) != 0;
}
bool Client::is_maximized_horizontally() const {
  return wm_client_is_maximized_horizontally(client_) != 0;
}
bool Client::is_maximized_vertically() const {
  return wm_client_is_maximized_vertically(client_) != 0;
}
bool Client::is_above() const { return wm_client_is_above(client_) != 0; }
bool Client::is_below() const { return wm_client_is_below(client_) != 0; }
bool Client::is_sticky() const { return wm_client_is_sticky(client_) != 0; }
bool Client::is_hidden() const { return wm_client_is_hidden(client_) != 0; }
bool Client::is_shaded() const { return wm_client_is_shaded(client_) != 0; }
bool Client::is_demanding_attention() const {
  return wm_client_is_demanding_attention(client_) != 0;
}
bool Client::is_urgent() const { return wm_client_is_urgent(client_) != 0; }
bool Client::never_takes_focus() const {
  return wm_client_never_takes_focus(client_) != 0;
}
bool Client::has_pending_ping() const {
  return wm_client_has_pending_ping(client_) != 0;
}
bool Client::is_mapped() const { return wm_client_is_mapped(client_) != 0; }

} // namespace wmlib