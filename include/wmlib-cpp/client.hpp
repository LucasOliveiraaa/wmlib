#pragma once

#include "wmlib-cpp/types.hpp"
#include "wmlib-cpp/wm.hpp"
#include "wmlib/wm.h"
#include <string>

namespace wmlib {

struct Client {
public:
  Client(Manager &wm, ::WMClient *client) : wm_(wm), client_(client) {}
  ~Client() = default;

  ::WMClient *client() const { return client_; }
  Manager &wm() const { return wm_; }

  void update_title();
  void update_hints();
  void update_transient();
  void update_window_type();
  void update_strut();
  void configure();

  void select_input(long event_mask);
  long get_state() const;
  void set_state(long state);

  void set_net_state(NetAtom atom, bool enabled);
  void set_fullscreen(bool enabled);

  void focus();
  void unfocus();

  void map();
  void unmap();

  void close();
  void ping();

  /* Geometry */

  void set_geometry(Rect rect);
  Rect get_geometry() const;
  Rect get_old_geometry() const;
  void save_geometry();
  void restore_geometry();

  void apply_size_hints(int &width, int &height) const;


  void begin_move(int x_root, int y_root);
  void begin_resize(int x_root, int y_root, MoveResizeDirection direction);

  void frame(Extents ext);
  void unframe();

  void raise();
  void lower();
  void restack(Client &sibling, bool above);

  void set_border_width(int width);
  void set_border_color(Color color);
  int get_border_width() const;

  void set_desktop(unsigned desktop);

  bool is_visible() const;

  Monitor get_monitor() const;

  Window get_window() const;
  Window get_frame() const;
  Screen get_screen() const;

  void *get_userdata() const;
  void set_userdata(void *userdata);

  Extents get_frame_extents() const;

  void get_base_size(int &width, int &height) const;
  void get_min_size(int &width, int &height) const;
  void get_max_size(int &width, int &height) const;
  void get_size_increments(int &width, int &height) const;
  void get_aspect_ratio(float &min_aspect, float &max_aspect) const;

  std::string get_title() const;
  WindowType get_window_type() const;
  Window get_transient_for() const;
  Strut get_strut() const;
  long get_event_mask() const;

  unsigned get_desktop() const;
  unsigned long get_user_time() const;

  void set_floating(bool floating);
  bool is_floating() const;
  bool is_fixed() const;
  bool is_fullscreen() const;
  bool is_maximized_horizontally() const;
  bool is_maximized_vertically() const;
  bool is_above() const;
  bool is_below() const;
  bool is_sticky() const;
  bool is_hidden() const;
  bool is_shaded() const;
  bool is_demanding_attention() const;
  bool is_urgent() const;
  bool never_takes_focus() const;
  bool has_pending_ping() const;
  bool is_mapped() const;

private:
  Manager &wm_;
  ::WMClient *client_;
};

} // namespace wmlib