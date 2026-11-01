#pragma once

#include "wmlib-cpp/client.hpp"
#include "wmlib-cpp/geometry.hpp"
#include "wmlib-cpp/types.hpp"
#include "wmlib/client.h"
#include "wmlib/screen.h"
#include "wmlib/types.h"
#include "wmlib/wm.h"
#include <X11/X.h>
#include <functional>
#include <memory>
#include <vector>

namespace wmlib {

struct Monitor {
public:
  Monitor(Manager &wm, ::WMMonitor *monitor) : wm_(wm), monitor_(monitor) {}
  ~Monitor() = default;

  ::WMMonitor *monitor() const { return monitor_; }
  Manager &wm() const { return wm_; }

  Rect get_geometry() const;
  Rect get_workarea() const;

  bool operator==(const Monitor &other) const {
    return monitor_ == other.monitor_;
  }

private:
  Manager &wm_;
  ::WMMonitor *monitor_;
};

enum class ScanKind : int {
  Transient = WM_SCAN_TRANSIENT,
  Normal = WM_SCAN_NORMAL,
};

struct Screen {
public:
  Screen(Manager &wm, ::WMScreen *screen) : wm_(wm), screen_(screen) {}
  ~Screen() = default;

  Screen(const Screen &) = delete;
  Screen &operator=(const Screen &) = delete;

  Screen(Screen &&) = delete;
  Screen &operator=(Screen &&) = delete;

  ::WMScreen *screen() const { return screen_; }
  Manager &wm() const { return wm_; }

  void scan(std::function<void(Manager &, Client, ScanKind, void *)> callback,
            void *userdata = nullptr);

  void set_desktops(unsigned count);
  void set_current_desktop(unsigned desktop);
  void set_workarea(Rect workarea);

  void grab_key(unsigned modifiers, KeySym keysym);
  void bind_key(unsigned modifiers, KeySym keysym,
                std::function<void(Manager &, void *)> callback, void *arg);
  void ungrab_keys();

  void request_layout();

  Monitor monitor_at(int x, int y) const;

  int get_number() const;
  Window get_root() const;

  int get_width() const;
  int get_height() const;

  Window get_check_window() const;
  ::Atom get_selection() const;

  size_t get_monitor_count() const;
  Monitor get_monitor(size_t index);
  std::vector<Monitor> get_monitors();

  unsigned get_desktop_count() const;
  unsigned get_current_desktop() const;
  bool is_showing_desktop() const;

  Rect get_workarea() const;
  Client get_focused() const;

  Client create_client(Window window);

  bool operator==(const Screen &other) const {
    return screen_ == other.screen_;
  }

private:
  static void key_bind_trampoline(::WM *wm, void *arg) {
    (void)wm;
    auto *binding = static_cast<KeyBind *>(arg);
    if (!binding || !binding->callback) {
      return;
    }
    binding->callback(binding->owner->wm(), binding->arg);
  }

  static void scan_trampoline(::WM *wm, ::WMClient *client, WMScanKind kind,
                              void *userdata) {
    (void)wm;
    auto *context = static_cast<ScanContext *>(userdata);
    if (!context || !context->callback) {
      return;
    }
    context->callback(context->owner->wm(), Client(context->owner->wm_, client),
                      (ScanKind)kind, context->userdata);
  }

  struct KeyBind {
    std::function<void(Manager &, void *)> callback;
    void *arg;
    Screen *owner;
  };

  struct ScanContext {
    std::function<void(Manager &, Client, ScanKind, void *)> callback;
    void *userdata;
    Screen *owner;
  };

  Manager &wm_;
  ::WMScreen *screen_;
  std::vector<std::unique_ptr<KeyBind>> key_bindings_;
  std::unique_ptr<ScanContext> scan_context_;
};

} // namespace wmlib