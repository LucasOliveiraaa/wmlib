#include "wmlib-cpp/client.hpp"
#include "wmlib-cpp/screen.hpp"
#include "wmlib-cpp/wm.hpp"

#include <stdexcept>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<wmlib::Manager>);
static_assert(!std::is_move_constructible_v<wmlib::Manager>);
static_assert(!std::is_copy_assignable_v<wmlib::Manager>);
static_assert(!std::is_move_assignable_v<wmlib::Manager>);

int main() {
  try {
    wmlib::Manager manager("wmlib-cpp-test");
    int loops = 0;
    int stops = 0;
    manager.set_userdata(&loops);
    manager.on_start([&](wmlib::Manager &wm) {
      ++loops;
      wm.stop();
    });
    manager.on_stop([&](wmlib::Manager &) { ++stops; });
    manager.init();
    if (manager.get_screen_count() == 0 || manager.get_userdata() != &loops) {
      return 1;
    }
    manager.run();
    return loops == 1 && stops == 1 ? 0 : 1;
  } catch (const std::exception &) {
    return 77;
  }
}
