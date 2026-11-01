#include "wmlib-cpp/wm.hpp"
#include "wmlib-cpp/client.hpp"
#include "wmlib-cpp/screen.hpp"
#include "wmlib/screen.h"
#include "wmlib/wm.h"
#include <stdexcept>

namespace wmlib {

Manager::CallbackContext *Manager::get_context(::WM *wm) {
  return static_cast<CallbackContext *>(::wm_get_userdata(wm));
}

#define DEFINE_WM_TRAMPOLINE(name)                                             \
  void Manager::name##_trampoline(::WM *wm) {                                       \
    auto *callback_context = get_context(wm);                                  \
    if (callback_context && callback_context->name) {                          \
      callback_context->name(callback_context->owner);                         \
    }                                                                          \
  }

DEFINE_WM_TRAMPOLINE(on_start)
DEFINE_WM_TRAMPOLINE(on_stop)
DEFINE_WM_TRAMPOLINE(on_loop)
DEFINE_WM_TRAMPOLINE(on_post)

#undef DEFINE_WM_TRAMPOLINE

void Manager::on_manage_trampoline(::WM *wm, ::WMClient *client) {
  auto *callback_context = get_context(wm);
  if (callback_context && callback_context->on_manage) {
    callback_context->on_manage(Client(callback_context->owner, client));
  }
}

void Manager::on_unmanage_trampoline(::WM *wm, ::WMClient *client) {
  auto *callback_context = get_context(wm);
  if (callback_context && callback_context->on_unmanage) {
    callback_context->on_unmanage(Client(callback_context->owner, client));
  }
}

void Manager::on_focus_trampoline(::WM *wm, ::WMClient *client) {
  auto *callback_context = get_context(wm);
  if (callback_context && callback_context->on_focus) {
    callback_context->on_focus(Client(callback_context->owner, client));
  }
}

void Manager::on_unfocus_trampoline(::WM *wm, ::WMClient *client) {
  auto *callback_context = get_context(wm);
  if (callback_context && callback_context->on_unfocus) {
    callback_context->on_unfocus(Client(callback_context->owner, client));
  }
}

void Manager::on_frame_expose_trampoline(::WM *wm, ::WMClient *client) {
  auto *callback_context = get_context(wm);
  if (callback_context && callback_context->on_frame_expose) {
    callback_context->on_frame_expose(Client(callback_context->owner, client));
  }
}

void Manager::on_layout_trampoline(::WM *wm, ::WMScreen *screen) {
  auto *callback_context = get_context(wm);
  if (callback_context && callback_context->on_layout) {
    callback_context->on_layout(callback_context->owner.get_screen(
        wm_screen_get_number(screen)));
  }
}

void Manager::on_monitors_changed_trampoline(::WM *wm, ::WMScreen *screen) {
  auto *callback_context = get_context(wm);
  if (callback_context && callback_context->on_monitors_changed) {
    callback_context->on_monitors_changed(callback_context->owner.get_screen(
        wm_screen_get_number(screen)));
  }
}

void Manager::on_client_changed_trampoline(::WM *wm, ::WMClient *client,
                                      unsigned change) {
  auto *callback_context = get_context(wm);
  if (callback_context && callback_context->on_client_changed) {
    callback_context->on_client_changed(Client(callback_context->owner, client),
                                        change);
  }
}

void Manager::on_state_request_trampoline(::WM *wm, ::WMClient *client, int action,
                                     int state) {
  auto *callback_context = get_context(wm);
  if (callback_context && callback_context->on_state_request) {
    callback_context->on_state_request(Client(callback_context->owner, client),
                                       action, state);
  }
}

void Manager::on_activate_request_trampoline(::WM *wm, ::WMClient *client,
                                        ::WMSource source) {
  auto *callback_context = get_context(wm);
  if (callback_context && callback_context->on_activate_request) {
    callback_context->on_activate_request(
        Client(callback_context->owner, client), static_cast<Source>(source));
  }
}

void Manager::on_move_resize_trampoline(::WM *wm, ::WMClient *client, int x, int y,
                                   ::WMMoveResizeDirection direction,
                                   unsigned edges) {
  auto *callback_context = get_context(wm);
  if (callback_context && callback_context->on_move_resize) {
    callback_context->on_move_resize(
        Client(callback_context->owner, client), x, y,
        static_cast<MoveResizeDirection>(direction), edges);
  }
}

void Manager::on_error_trampoline(::WM *wm, const char *str) {
  auto *callback_context = get_context(wm);
  if (callback_context && callback_context->on_error) {
    callback_context->on_error(callback_context->owner, str ? str : "");
  }
}

void Manager::event_handler_trampoline(::WM *wm, ::XEvent *event) {
  auto *callback_context = get_context(wm);
  if (callback_context && event && event->type >= 0 &&
      event->type < LASTEvent &&
      callback_context->event_handlers[event->type]) {
    callback_context->event_handlers[event->type](callback_context->owner,
                                                  event);
  }
}
std::vector<Client> Manager::get_clients() {
  std::vector<Client> clients;
  ::WMClientNode *node = ::wm_get_first_client_node(wm_);
  while (node) {
    clients.emplace_back(*this, node->client);
    node = node->next;
  }
  return clients;
}

Manager::Manager(const std::string &name)
    : wm_(::wm_create()), context_(*this) {
  if (!wm_) {
    throw std::runtime_error("Failed to create WM instance");
  }
  ::wm_set_userdata(wm_, &context_);
  if (::wm_set_name(wm_, name.c_str()) != 0) {
    ::wm_destroy(wm_);
    throw std::runtime_error("Failed to set WM name");
  }
}

Manager::~Manager() {
  if (wm_) {
    ::wm_destroy(wm_);
    wm_ = nullptr;
  }
}

void Manager::init() {
  if (::wm_init(wm_) != 0) {
    throw std::runtime_error("Failed to initialize WM");
  }
}

void Manager::run() { ::wm_run(wm_); }

void Manager::stop() { ::wm_stop(wm_); }

Client Manager::find_client(Window window) {
  ::WMClient *client = ::wm_client_find(wm_, window);
  if (!client) {
    throw std::runtime_error("Client not found for the given window");
  }
  return Client(*this, client);
}

void *Manager::get_userdata() const { return context_.application_userdata; }
void Manager::set_userdata(void *userdata) {
  context_.application_userdata = userdata;
}

std::string Manager::get_name() const {
  const char *name = ::wm_get_name(wm_);
  if (!name) {
    throw std::runtime_error("Failed to get WM name");
  }
  return std::string(name);
}

Extents Manager::get_frame_extents() const { return ::wm_get_frame_extents(wm_); }
void Manager::set_frame_extents(const Extents &extents) {
  ::wm_set_frame_extents(wm_, extents);
}

int Manager::get_loop_timeout() const { return ::wm_get_loop_timeout(wm_); }
void Manager::set_loop_timeout(int timeout_ms) {
  ::wm_set_loop_timeout(wm_, timeout_ms);
}

::Atom Manager::get_net_atom(NetAtom atom) const {
  return ::wm_get_net_atom(wm_, atom);
}
::Atom Manager::get_atom(Atom atom) const { return ::wm_get_atom(wm_, atom); }

bool Manager::is_running() const { return ::wm_is_running(wm_) != 0; }
bool Manager::get_debug_sync() const { return ::wm_get_debug_sync(wm_) != 0; }
void Manager::set_debug_sync(bool enabled) {
  ::wm_set_debug_sync(wm_, enabled ? 1 : 0);
}

Display *Manager::get_display() const { return ::wm_get_display(wm_); }

Screen &Manager::get_screen(size_t index) {
  ::WMScreen *screen = ::wm_get_screen(wm_, index);
  if (!screen) {
    throw std::runtime_error("Screen index out of range");
  }
  int num = wm_screen_get_number(screen);
  auto [it, inserted] =
      screens_.try_emplace(num, std::make_unique<Screen>(*this, screen));
  if (!inserted && it->second->screen() != screen) {
    throw std::runtime_error("Screen identity changed unexpectedly");
  }
  return *it->second;
}
size_t Manager::get_screen_count() const { return ::wm_get_screen_count(wm_); }
std::vector<std::reference_wrapper<Screen>> Manager::get_screens() {
  std::vector<std::reference_wrapper<Screen>> screens;
  for (size_t i = 0; i < get_screen_count(); ++i) {
    screens.push_back(get_screen(i));
  }
  return screens;
}

ClientNode *Manager::get_first_client_node() const {
  return ::wm_get_first_client_node(wm_);
}

void Manager::set_event_handler(EventType type,
                           std::function<void(Manager &, ::XEvent *)> handler) {
  const int event_type = static_cast<int>(type);
  if (event_type < 0 || event_type >= LASTEvent) {
    throw std::out_of_range("Event type out of range");
  }
  context_.event_handlers[event_type] = std::move(handler);
  ::wm_set_event_handler(wm_, event_type, &Manager::event_handler_trampoline);
}

} // namespace wmlib