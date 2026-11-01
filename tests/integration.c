#include "wmlib/client.h"
#define _POSIX_C_SOURCE 200809L

#include "wmlib/wm.h"
#include "wmlib/geometry.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SKIP_EXIT 77
#define WAIT_TRIES 500
#define BIND_MODIFIERS Mod4Mask
#define DESKTOP_COUNT 4
#define DOCK_HEIGHT 30

#define REQUESTED_WIDTH 333
#define REQUESTED_HEIGHT 222
#define HINTED_WIDTH 330
#define HINTED_HEIGHT 220
#define WINDOW_X 10
#define WINDOW_Y 30

static const WMExtents frame_extents = {4, 4, 16, 4};

static volatile sig_atomic_t terminate;
static int failures;
static int grab_failed;

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,         \
              #condition);                                                     \
      failures++;                                                              \
    }                                                                          \
  } while (0)

/* Polls a condition for up to five seconds: the window manager runs in
 * another process. */
#define EVENTUALLY(condition)                                                  \
  do {                                                                         \
    int satisfied_ = 0;                                                        \
    for (int tries_ = 0; tries_ < WAIT_TRIES && !satisfied_; ++tries_) {       \
      satisfied_ = (condition);                                                \
      if (!satisfied_) {                                                       \
        sleep_ms(10);                                                          \
      }                                                                        \
    }                                                                          \
    CHECK(satisfied_);                                                         \
  } while (0)

static void sleep_ms(long ms) {
  struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
  nanosleep(&ts, NULL);
}

/* ------------------------------------------------------------------------ */
/* The window manager under test, run in a child process                    */
/* ------------------------------------------------------------------------ */

static void on_terminate(int signal_number) {
  (void)signal_number;
  terminate = 1;
}

static void wm_on_loop(WM *wm) {
  if (terminate) {
    wm_stop(wm);
  }
}

static void wm_on_manage(WM *wm, WMClient *client) {
  if (strcmp(wm_client_get_title(client), "framed") == 0) {
    wm_client_frame(wm, client, frame_extents);
  }

  int width = REQUESTED_WIDTH, height = REQUESTED_HEIGHT;
  wm_client_apply_size_hints(wm, client, &width, &height);
  wm_client_set_geometry(wm, client, WINDOW_X, WINDOW_Y, width, height);
  wm_client_map(wm, client);
}

static void wm_on_bound(WM *wm, void *arg) {
  (void)wm;
  (void)arg;
}

static int run_window_manager(void) {
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = on_terminate;
  sigaction(SIGTERM, &action, NULL);

  WM *wm = wm_create();
  if (!wm) {
    return 1;
  }

  wm_set_name(wm, "wmlib-test");
  if (wm_init(wm) != 0) {
    wm_destroy(wm);
    return 1;
  }

  wm_set_on_manage(wm, wm_on_manage);
  wm_set_on_loop(wm, wm_on_loop);
  wm_set_loop_timeout(wm, 50);

  wm_screen_set_desktops(wm, wm_get_screen(wm, 0), DESKTOP_COUNT);
  wm_bind_key(wm, wm_get_screen(wm, 0), BIND_MODIFIERS, XK_Return, wm_on_bound,
              NULL);

  wm_run(wm);
  wm_destroy(wm);
  return 0;
}

/* ------------------------------------------------------------------------ */
/* X client helpers                                                         */
/* ------------------------------------------------------------------------ */

static int ignore_error(Display *display, XErrorEvent *event) {
  (void)display;
  (void)event;
  return 0;
}

static int record_access_error(Display *display, XErrorEvent *event) {
  (void)display;
  if (event->error_code == BadAccess) {
    grab_failed = 1;
  }
  return 0;
}

static Atom atom(Display *display, const char *name) {
  return XInternAtom(display, name, False);
}

/* Returns the property items as longs (XFree them), or NULL. */
static long *get_items(Display *display, Window window, const char *name,
                       Atom type, unsigned long *count) {
  Atom actual_type;
  int actual_format;
  unsigned long bytes_after;
  unsigned char *data = NULL;

  *count = 0;
  if (XGetWindowProperty(display, window, atom(display, name), 0, 1024, False,
                         type, &actual_type, &actual_format, count,
                         &bytes_after, &data) != Success ||
      !data || actual_type != type) {
    if (data) {
      XFree(data);
    }
    *count = 0;
    return NULL;
  }
  return (long *)data;
}

static int has_item(Display *display, Window window, const char *name,
                    Atom type, long item) {
  unsigned long count;
  long *items = get_items(display, window, name, type, &count);
  int found = 0;

  for (unsigned long i = 0; i < count; ++i) {
    found |= items[i] == item;
  }
  if (items) {
    XFree(items);
  }
  return found;
}

static int has_atom(Display *display, Window window, const char *property,
                    const char *name) {
  return has_item(display, window, property, XA_ATOM,
                  (long)atom(display, name));
}

static int in_list(Display *display, const char *list, Window window) {
  return has_item(display, DefaultRootWindow(display), list, XA_WINDOW,
                  (long)window);
}

static int has_wm_state(Display *display, Window window) {
  unsigned long count;
  long *items = get_items(display, window, "WM_STATE", atom(display, "WM_STATE"),
                          &count);

  if (items) {
    XFree(items);
  }
  return count > 0;
}

/* The first cardinal of a property, or -1. */
static long get_cardinal(Display *display, Window window, const char *name) {
  unsigned long count;
  long *items = get_items(display, window, name, XA_CARDINAL, &count);
  long value = count ? items[0] : -1;

  if (items) {
    XFree(items);
  }
  return value;
}

static int frame_extents_are(Display *display, Window window, long left,
                             long right, long top, long bottom) {
  unsigned long count;
  long *items =
      get_items(display, window, "_NET_FRAME_EXTENTS", XA_CARDINAL, &count);
  int same = count == 4 && items[0] == left && items[1] == right &&
             items[2] == top && items[3] == bottom;

  if (items) {
    XFree(items);
  }
  return same;
}

static int geometry_is(Display *display, Window window, int x, int y, int width,
                       int height) {
  XWindowAttributes attrs;
  return XGetWindowAttributes(display, window, &attrs) && attrs.x == x &&
         attrs.y == y && attrs.width == width && attrs.height == height;
}

static Window parent_of(Display *display, Window window) {
  Window root, parent = None, *children = NULL;
  unsigned int count;

  if (XQueryTree(display, window, &root, &parent, &children, &count) &&
      children) {
    XFree(children);
  }
  return parent;
}

static Window create_window(Display *display, const char *title) {
  Window window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0,
                                      100, 100, 0, 0, 0);
  XSizeHints hints;
  memset(&hints, 0, sizeof(hints));
  hints.flags = PMinSize | PBaseSize | PResizeInc | PMaxSize;
  hints.min_width = hints.min_height = 10;
  hints.base_width = 20;
  hints.base_height = 10;
  hints.width_inc = 10;
  hints.height_inc = 5;
  hints.max_width = 800;
  hints.max_height = 600;

  XStoreName(display, window, title);
  XSetWMNormalHints(display, window, &hints);
  return window;
}

static void send_message(Display *display, Window window, const char *type,
                         long l0, long l1, long l2) {
  XEvent event;
  memset(&event, 0, sizeof(event));
  event.xclient.type = ClientMessage;
  event.xclient.window = window;
  event.xclient.message_type = atom(display, type);
  event.xclient.format = 32;
  event.xclient.data.l[0] = l0;
  event.xclient.data.l[1] = l1;
  event.xclient.data.l[2] = l2;

  XSendEvent(display, DefaultRootWindow(display), False,
             SubstructureNotifyMask | SubstructureRedirectMask, &event);
  XFlush(display);
}

/* ------------------------------------------------------------------------ */
/* Tests                                                                    */
/* ------------------------------------------------------------------------ */

static void test_root_properties(Display *display) {
  Window root = DefaultRootWindow(display);

  EVENTUALLY(get_cardinal(display, root, "_NET_NUMBER_OF_DESKTOPS") ==
             DESKTOP_COUNT);
  CHECK(has_atom(display, root, "_NET_SUPPORTED", "_NET_ACTIVE_WINDOW"));
  CHECK(has_atom(display, root, "_NET_SUPPORTED", "_NET_WM_STATE_SHADED"));

  unsigned long count;
  long *workarea = get_items(display, root, "_NET_WORKAREA", XA_CARDINAL, &count);
  CHECK(count == 4 * DESKTOP_COUNT);
  if (workarea) {
    XFree(workarea);
  }

  long *check = get_items(display, root, "_NET_SUPPORTING_WM_CHECK", XA_WINDOW,
                          &count);
  CHECK(check != NULL);
  if (!check) {
    return;
  }

  Window check_window = (Window)check[0];
  XFree(check);

  Atom utf8 = atom(display, "UTF8_STRING");
  unsigned long name_length;
  long *name = get_items(display, check_window, "_NET_WM_NAME", utf8,
                         &name_length);
  CHECK(name != NULL);
  if (name) {
    XFree(name);
  }
  CHECK(XGetSelectionOwner(display, atom(display, "WM_S0")) == check_window);
}

static void test_manage_and_size_hints(Display *display) {
  Window root = DefaultRootWindow(display);
  Window window = create_window(display, "plain");

  XMapWindow(display, window);
  EVENTUALLY(in_list(display, "_NET_CLIENT_LIST", window));
  EVENTUALLY(geometry_is(display, window, WINDOW_X, WINDOW_Y, HINTED_WIDTH,
                         HINTED_HEIGHT));
  EVENTUALLY(has_wm_state(display, window));
  EVENTUALLY(frame_extents_are(display, window, 0, 0, 0, 0));
  CHECK(has_atom(display, window, "_NET_WM_ALLOWED_ACTIONS",
                 "_NET_WM_ACTION_CLOSE"));
  CHECK(!has_atom(display, window, "_NET_WM_ALLOWED_ACTIONS",
                  "_NET_WM_ACTION_SHADE"));
  CHECK(get_cardinal(display, window, "_NET_WM_DESKTOP") == 0);
  CHECK(in_list(display, "_NET_CLIENT_LIST_STACKING", window));

  send_message(display, root, "_NET_CURRENT_DESKTOP", 2, 0, 0);
  EVENTUALLY(get_cardinal(display, root, "_NET_CURRENT_DESKTOP") == 2);
  send_message(display, window, "_NET_WM_DESKTOP", 1, 0, 0);
  EVENTUALLY(get_cardinal(display, window, "_NET_WM_DESKTOP") == 1);
  send_message(display, root, "_NET_CURRENT_DESKTOP", 0, 0, 0);
  EVENTUALLY(get_cardinal(display, root, "_NET_CURRENT_DESKTOP") == 0);

  send_message(display, window, "_NET_WM_STATE", 1,
               (long)atom(display, "_NET_WM_STATE_FULLSCREEN"), 0);
  EVENTUALLY(has_atom(display, window, "_NET_WM_STATE",
                      "_NET_WM_STATE_FULLSCREEN"));
  send_message(display, window, "_NET_WM_STATE", 2,
               (long)atom(display, "_NET_WM_STATE_FULLSCREEN"), 0);
  EVENTUALLY(!has_atom(display, window, "_NET_WM_STATE",
                       "_NET_WM_STATE_FULLSCREEN"));

  XDestroyWindow(display, window);
  EVENTUALLY(!in_list(display, "_NET_CLIENT_LIST", window));
}

static void test_frames(Display *display) {
  Window root = DefaultRootWindow(display);
  Window window = create_window(display, "framed");

  XMapWindow(display, window);
  EVENTUALLY(in_list(display, "_NET_CLIENT_LIST", window));
  EVENTUALLY(parent_of(display, window) != root &&
             parent_of(display, window) != None);

  Window frame = parent_of(display, window);
  CHECK(geometry_is(display, frame, WINDOW_X - frame_extents.left,
                    WINDOW_Y - frame_extents.top,
                    HINTED_WIDTH + frame_extents.left + frame_extents.right,
                    HINTED_HEIGHT + frame_extents.top + frame_extents.bottom));
  CHECK(geometry_is(display, window, frame_extents.left, frame_extents.top,
                    HINTED_WIDTH, HINTED_HEIGHT));
  EVENTUALLY(frame_extents_are(display, window, frame_extents.left,
                               frame_extents.right, frame_extents.top,
                               frame_extents.bottom));
  CHECK(has_atom(display, window, "_NET_WM_ALLOWED_ACTIONS",
                 "_NET_WM_ACTION_SHADE"));
  CHECK(in_list(display, "_NET_CLIENT_LIST_STACKING", window));
  CHECK(!in_list(display, "_NET_CLIENT_LIST_STACKING", frame));

  XDestroyWindow(display, window);
  EVENTUALLY(!in_list(display, "_NET_CLIENT_LIST", window));

  XWindowAttributes attrs;
  EVENTUALLY(!XGetWindowAttributes(display, frame, &attrs));
}

static void test_struts(Display *display) {
  Window root = DefaultRootWindow(display);
  int screen_width = DisplayWidth(display, DefaultScreen(display));
  int screen_height = DisplayHeight(display, DefaultScreen(display));

  Window dock = create_window(display, "dock");
  Atom type = atom(display, "_NET_WM_WINDOW_TYPE_DOCK");
  long strut[12] = {0, 0, DOCK_HEIGHT, 0, 0, 0, 0, 0, 0, screen_width - 1, 0, 0};

  XChangeProperty(display, dock, atom(display, "_NET_WM_WINDOW_TYPE"), XA_ATOM,
                  32, PropModeReplace, (unsigned char *)&type, 1);
  XChangeProperty(display, dock, atom(display, "_NET_WM_STRUT_PARTIAL"),
                  XA_CARDINAL, 32, PropModeReplace, (unsigned char *)strut, 12);
  XMapWindow(display, dock);

  unsigned long count;
  long *workarea = NULL;
  int reserved = 0;
  for (int tries = 0; tries < WAIT_TRIES && !reserved; ++tries) {
    workarea = get_items(display, root, "_NET_WORKAREA", XA_CARDINAL, &count);
    reserved = workarea && workarea[1] == DOCK_HEIGHT &&
               workarea[3] == screen_height - DOCK_HEIGHT;
    if (workarea) {
      XFree(workarea);
    }
    if (!reserved) {
      sleep_ms(10);
    }
  }
  CHECK(reserved);

  XDestroyWindow(display, dock);
  int released = 0;
  for (int tries = 0; tries < WAIT_TRIES && !released; ++tries) {
    workarea = get_items(display, root, "_NET_WORKAREA", XA_CARDINAL, &count);
    released = workarea && workarea[1] == 0 && workarea[3] == screen_height;
    if (workarea) {
      XFree(workarea);
    }
    if (!released) {
      sleep_ms(10);
    }
  }
  CHECK(released);
}

static void test_key_grab(Display *display) {
  Window root = DefaultRootWindow(display);
  KeyCode code = XKeysymToKeycode(display, XK_Return);

  XSetErrorHandler(record_access_error);
  grab_failed = 0;
  XGrabKey(display, code, BIND_MODIFIERS, root, False, GrabModeAsync,
           GrabModeAsync);
  XSync(display, False);
  CHECK(grab_failed);
  XSetErrorHandler(ignore_error);
}

int main(void) {
  pid_t pid = fork();
  if (pid < 0) {
    return 1;
  }
  if (pid == 0) {
    _exit(run_window_manager());
  }

  Display *display = XOpenDisplay(NULL);
  if (!display) {
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
    return SKIP_EXIT;
  }
  XSetErrorHandler(ignore_error);

  Window root = DefaultRootWindow(display);

  int ready = 0;
  for (int tries = 0; tries < WAIT_TRIES && !ready; ++tries) {
    unsigned long count;
    long *check = get_items(display, root, "_NET_SUPPORTING_WM_CHECK",
                            XA_WINDOW, &count);
    ready = check != NULL;
    if (check) {
      XFree(check);
    }
    if (!ready) {
      sleep_ms(10);
    }
  }
  CHECK(ready);

  if (ready) {
    test_root_properties(display);
    test_manage_and_size_hints(display);
    test_frames(display);
    test_struts(display);
    test_key_grab(display);
  }

  XCloseDisplay(display);
  kill(pid, SIGTERM);

  int status = 0;
  waitpid(pid, &status, 0);
  CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);

  if (failures) {
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  return 0;
}