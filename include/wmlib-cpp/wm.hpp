#pragma once

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <wmlib/wm.h>

#include "wmlib-cpp/types.hpp"
#include "wmlib-cpp/geometry.hpp"

namespace wmlib {

struct Manager {
public:
  Manager(const std::string &name);
  ~Manager();

  Manager(const Manager &) = delete;
  Manager &operator=(const Manager &) = delete;

  Manager(Manager &&) = delete;
  Manager &operator=(Manager &&) = delete;

  ::WM *wm() const { return wm_; }

  void init();
  void run();
  void stop();

  Client find_client(Window window);

  void *get_userdata() const;
  void set_userdata(void *userdata);

  std::string get_name() const;

  Extents get_frame_extents() const;
  void set_frame_extents(const Extents &extents);

  int get_loop_timeout() const;
  void set_loop_timeout(int timeout_ms);

  ::Atom get_net_atom(NetAtom atom) const;
  ::Atom get_atom(Atom atom) const;

  bool is_running() const;
  bool get_debug_sync() const;
  void set_debug_sync(bool enabled);

  Display *get_display() const;

  Screen &get_screen(size_t index);
  size_t get_screen_count() const;
  std::vector<std::reference_wrapper<Screen>> get_screens();

  ClientNode *get_first_client_node() const;
  std::vector<Client> get_clients();

  void set_event_handler(EventType type,
                         std::function<void(Manager &, ::XEvent *)> handler);

#define WM_DEFINE_CALLBACK_SETTER(name, type)                                  \
  void name(std::function<type> callback) {                                    \
    context_.name = std::move(callback);                                       \
    ::wm_set_##name(wm_, &Manager::name##_trampoline);                         \
  }

  WM_DEFINE_CALLBACK_SETTER(on_start, void(Manager &))
  WM_DEFINE_CALLBACK_SETTER(on_stop, void(Manager &))
  WM_DEFINE_CALLBACK_SETTER(on_loop, void(Manager &))
  WM_DEFINE_CALLBACK_SETTER(on_post, void(Manager &))

  WM_DEFINE_CALLBACK_SETTER(on_manage, void(Client))
  WM_DEFINE_CALLBACK_SETTER(on_unmanage, void(Client))

  WM_DEFINE_CALLBACK_SETTER(on_layout, void(Screen &))

  WM_DEFINE_CALLBACK_SETTER(on_focus, void(Client))
  WM_DEFINE_CALLBACK_SETTER(on_unfocus, void(Client))

  WM_DEFINE_CALLBACK_SETTER(on_client_changed, void(Client, unsigned))
  WM_DEFINE_CALLBACK_SETTER(on_state_request, void(Client, int, int))
  WM_DEFINE_CALLBACK_SETTER(on_activate_request, void(Client, Source))
  WM_DEFINE_CALLBACK_SETTER(on_frame_expose, void(Client))
  WM_DEFINE_CALLBACK_SETTER(on_monitors_changed, void(Screen &))
  WM_DEFINE_CALLBACK_SETTER(on_move_resize, void(Client, int, int,
                                                 MoveResizeDirection, unsigned))
  WM_DEFINE_CALLBACK_SETTER(on_error, void(Manager &, const std::string &))

#undef WM_DEFINE_CALLBACK_SETTER

private:
  struct CallbackContext {
    CallbackContext(Manager &owner) : owner(owner) {}

    Manager &owner;
    void *application_userdata = nullptr;

    std::array<std::function<void(Manager &, ::XEvent *)>, LASTEvent>
        event_handlers;

    std::function<void(Manager &)> on_start = nullptr;
    std::function<void(Manager &)> on_stop = nullptr;
    std::function<void(Manager &)> on_loop = nullptr;
    std::function<void(Manager &)> on_post = nullptr;

    std::function<void(Client)> on_manage = nullptr;
    std::function<void(Client)> on_unmanage = nullptr;

    std::function<void(Screen &)> on_layout = nullptr;

    std::function<void(Client)> on_focus = nullptr;
    std::function<void(Client)> on_unfocus = nullptr;

    std::function<void(Client, unsigned)> on_client_changed = nullptr;
    std::function<void(Client, int, int)> on_state_request = nullptr;
    std::function<void(Client, Source)> on_activate_request = nullptr;
    std::function<void(Client)> on_frame_expose = nullptr;
    std::function<void(Screen &)> on_monitors_changed = nullptr;
    std::function<void(Client, int, int, MoveResizeDirection, unsigned)>
        on_move_resize = nullptr;
    std::function<void(Manager &, const std::string &)> on_error = nullptr;
  };

  static void on_start_trampoline(::WM *);
  static void on_stop_trampoline(::WM *);
  static void on_loop_trampoline(::WM *);
  static void on_post_trampoline(::WM *);
  static void on_manage_trampoline(::WM *, ::WMClient *);
  static void on_unmanage_trampoline(::WM *, ::WMClient *);
  static void on_focus_trampoline(::WM *, ::WMClient *);
  static void on_unfocus_trampoline(::WM *, ::WMClient *);
  static void on_frame_expose_trampoline(::WM *, ::WMClient *);
  static void on_layout_trampoline(::WM *, ::WMScreen *);
  static void on_monitors_changed_trampoline(::WM *, ::WMScreen *);

  static void on_client_changed_trampoline(::WM *, ::WMClient *, unsigned);
  static void on_state_request_trampoline(::WM *, ::WMClient *, int, int);
  static void on_activate_request_trampoline(::WM *, ::WMClient *,
                                              ::WMSource);
  static void on_move_resize_trampoline(::WM *, ::WMClient *, int, int,
                                        ::WMMoveResizeDirection, unsigned);
  static void on_error_trampoline(::WM *, const char *);
  static void event_handler_trampoline(::WM *, ::XEvent *);
  static CallbackContext *get_context(::WM *);

  ::WM *wm_;
  CallbackContext context_;
  std::map<int, std::unique_ptr<Screen>> screens_;
};

} // namespace wmlib