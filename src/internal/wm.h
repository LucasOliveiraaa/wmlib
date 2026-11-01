#ifndef WMLIB_WM_H_INTERNAL
#define WMLIB_WM_H_INTERNAL

#include "wmlib/client.h"
#include "wmlib/screen.h"
#include "wmlib/wm.h"

#define WM_DEFAULT_NAME "wmlib"
#define WM_STRUT_LENGTH 4
#define WM_STRUT_PARTIAL_LENGTH 12

#define WM_MAX_ACTIONS 10
#define WM_MAX_ATOMS 1024

#define WM_CLIENT_BASE_MASK PropertyChangeMask
#define WM_FRAME_MASK                                                          \
  (SubstructureRedirectMask | SubstructureNotifyMask | ExposureMask)
#define WM_FRAME_INPUT_MASK                                                    \
  (EnterWindowMask | LeaveWindowMask | ButtonPressMask | ButtonReleaseMask |   \
   PointerMotionMask)
#define WM_DRAG_MASK                                                           \
  (ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ExposureMask |    \
   SubstructureRedirectMask)
#define WM_BUTTON_MASK                                                         \
  (Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask)

/* _NET_MOVERESIZE_WINDOW flags: bits 8-11 say which fields are present. */
#define WM_MOVERESIZE_FLAG_X (1L << 8)
#define WM_MOVERESIZE_FLAG_Y (1L << 9)
#define WM_MOVERESIZE_FLAG_WIDTH (1L << 10)
#define WM_MOVERESIZE_FLAG_HEIGHT (1L << 11)

static const struct {
  int net_atom;
  WMWindowType type;
} wm_window_types[] = {
    {NetWMWindowTypeDialog, WM_WINDOW_DIALOG},
    {NetWMWindowTypeDock, WM_WINDOW_DOCK},
    {NetWMWindowTypeDesktop, WM_WINDOW_DESKTOP},
    {NetWMWindowTypeSplash, WM_WINDOW_SPLASH},
    {NetWMWindowTypeUtility, WM_WINDOW_UTILITY},
    {NetWMWindowTypeMenu, WM_WINDOW_MENU},
};

static const int wm_net_states[] = {
    NetWMFullscreen,    NetWMMaximizedVert,
    NetWMMaximizedHorz, NetWMAbove,
    NetWMBelow,         NetWMSticky,
    NetWMHidden,        NetWMDemandsAttention,
    NetWMStateShaded,
};

typedef struct {
  int fd;
  WMFdCallback callback;
} WMFdWatch;

typedef struct {
  int id;
  long long due_ms;
  unsigned interval_ms;
  int repeat;
  WMTimerCallback callback;
} WMTimer;

typedef struct {
  int left, right, top, bottom;
} WMEdges;

/* Indexed by the resize directions of _NET_WM_MOVERESIZE. */
static const WMEdges wm_resize_edges[] = {
    {1, 0, 1, 0}, {0, 0, 1, 0}, {0, 1, 1, 0}, {0, 1, 0, 0},
    {0, 1, 0, 1}, {0, 0, 0, 1}, {1, 0, 0, 1}, {1, 0, 0, 0},
};

struct WM {
  /*
   * The X11 display connection.
   */
  Display *display;

  /*
   * Arbitrary data owned by the user.
   */
  void *userdata;

  /*
   * The name published in _NET_WM_NAME on the supporting check window.
   * Defaults to "wmlib" when NULL. Must be set before wm_init().
   */
  char *name;

  /*
   * Frame extents published (_NET_FRAME_EXTENTS) in answer to
   * _NET_REQUEST_FRAME_EXTENTS for windows that are not managed yet. Set it
   * to the extents passed to wm_client_frame() when using frames.
   */
  WMExtents frame_extents;

  /*
   * A list of screens managed by the window manager.
   */
  WMScreen *screens;
  size_t screen_count;

  /*
   * A flag indicating whether the window manager is running.
   */
  int running;

  /*
   * Maximum time in milliseconds wm_run() waits for events before running
   * the next loop iteration. -1 (the default) waits indefinitely. Timers
   * and watched file descriptors wake the loop on their own.
   */
  int loop_timeout_ms;

  /*
   * When set (see wm_set_debug_sync()) the connection is synchronous, so X
   * errors are reported by on_error during the request that caused them.
   */
  int debug_sync;

  Atom wm_atoms[WMLast];
  Atom net_atoms[NetLast];

  /*
   * The modifier bit NumLock is mapped to, or 0. Kept up to date on
   * MappingNotify. See wm_clean_modifiers().
   */
  unsigned numlock_mask;

  /*
   * Key grabs registered with wm_grab_key() and wm_bind_key(), re-applied
   * when the keyboard mapping changes.
   */
  WMKeyGrab *key_grabs;
  size_t key_grab_count;

  /*
   * File descriptors and timers registered with wm_watch_fd() and
   * wm_add_timer().
   */
  WMFdWatch *fd_watches;
  size_t fd_watch_count;
  WMTimer *timers;
  size_t timer_count;
  int next_timer_id;

  /*
   * The RandR event base, or 0 when RandR 1.5 is unavailable.
   */
  int randr_event_base;

  /*
   * A list of clients managed by the window manager.
   */
  WMClientNode *client_nodes;

  /*
   * An array of function pointers for handling X11 events.
   * The index corresponds to the event type (e.g., KeyPress, ButtonPress).
   *
   * For events the library tracks internally (MapRequest, UnmapNotify,
   * DestroyNotify, ConfigureRequest, PropertyNotify, ClientMessage,
   * ConfigureNotify, MapNotify, MappingNotify, Expose on frames, KeyPress
   * matching a wm_bind_key() binding) the library updates its own state first
   * and then calls the handler, unless the event was fully consumed
   * internally (e.g. an UnmapNotify caused by wm_client_unmap()).
   * For DestroyNotify the client is still valid inside the handler; it is
   * released right after the handler returns.
   * Use wm_client_find() to get the WMClient for event->window; it matches
   * both the client window and its frame.
   */
  void (*event_handlers[LASTEvent])(WM *wm, XEvent *event);

  /*
   * Function pointer called when the window manager starts.
   */
  void (*on_start)(WM *wm);
  /*
   * Function pointer called when the window manager stops.
   */
  void (*on_stop)(WM *wm);
  /*
   * Function pointer called on each iteration of the main loop.
   */
  void (*on_loop)(WM *wm);
  /*
   * Function pointer called after each iteration of the main loop, after
   * on_layout.
   */
  void (*on_post)(WM *wm);

  /*
   * Function pointer called after a client has been registered, for managed
   * map requests as well as for clients found by wm_scan_screen(). The client
   * pointer stays valid until on_unmanage returns.
   */
  void (*on_manage)(WM *wm, WMClient *client);
  /*
   * Function pointer called right before a client is released, either because
   * its window was destroyed or because wm_client_destroy() was called. The
   * client is still valid inside the callback.
   */
  void (*on_unmanage)(WM *wm, WMClient *client);
  /*
   * Function pointer called at most once per loop iteration, after the
   * events were processed, for every screen that asked for a layout with
   * wm_request_layout(). The client list is consistent and the work areas are
   * up to date. The library requests a layout on manage, unmanage, map,
   * unmap, strut, state, desktop and monitor changes.
   */
  void (*on_layout)(WM *wm, WMScreen *screen);
  /*
   * Function pointers called when wm_client_focus() moves the focus to a
   * client and when a client loses it (wm_client_unfocus(), another client
   * gaining the focus, or the focused client being destroyed).
   */
  void (*on_focus)(WM *wm, WMClient *client);
  void (*on_unfocus)(WM *wm, WMClient *client);
  /*
   * Function pointer called after the library re-read properties of a client.
   * The change argument is a bitmask of WMClientChange values.
   */
  void (*on_client_changed)(WM *wm, WMClient *client, unsigned change);
  /*
   * Function pointer called for each _NET_WM_STATE change requested by a
   * client or pager. net_atom is the index of the state (NetWMFullscreen,
   * NetWMMaximizedVert, ...). The client flag and the property are already
   * updated; geometry is up to the callback. When unset, the request is
   * passed to the ClientMessage handler after the flags were updated.
   */
  void (*on_state_request)(WM *wm, WMClient *client, int net_atom, int enabled);
  /*
   * Function pointer called when a client asks to be activated
   * (_NET_ACTIVE_WINDOW). Requests from applications that are older than the
   * focused client's _NET_WM_USER_TIME are turned into
   * _NET_WM_STATE_DEMANDS_ATTENTION instead. When unset, the request is passed
   * to the ClientMessage handler.
   */
  void (*on_activate_request)(WM *wm, WMClient *client, WMSource source);
  /*
   * Function pointer called when a frame window needs to be redrawn.
   */
  void (*on_frame_expose)(WM *wm, WMClient *client);
  /*
   * Function pointer called after the monitor layout of a screen changed.
   * The monitor work areas are already recomputed.
   */
  void (*on_monitors_changed)(WM *wm, WMScreen *screen);
  /*
   * Function pointer called when a client asks for an interactive move or
   * resize (_NET_WM_MOVERESIZE). When unset, floating clients are moved or
   * resized with wm_client_begin_move() and wm_client_begin_resize().
   */
  void (*on_move_resize)(WM *wm, WMClient *client, int x_root, int y_root,
                         WMMoveResizeDirection direction, unsigned button);
  /*
   * Function pointer called with a description of every failure the library
   * detects, including X protocol errors that are not benign races. When set,
   * X protocol errors no longer terminate the process; when unset they are
   * printed to stderr and handled by Xlib's default error handler.
   */
  void (*on_error)(WM *wm, const char *message);
};

long wm_i_get_state(WM *wm, Window window);
Atom *wm_i_get_atom_list(WM *wm, Window window, Atom property,
                         unsigned long *count);
int wm_i_atom_list_contains(const Atom *atoms, unsigned long count, Atom atom);
void wm_i_set_cardinals(WM *wm, Window window, Atom property,
                        const long *values, int count);
int wm_i_get_cardinals(WM *wm, Window window, Atom property,
                       unsigned long length, long *values);
void wm_i_lock_variants(WM *wm, unsigned variants[4]);
Time wm_i_server_time(WM *wm, Window window);
WMWindowType wm_i_window_type_from_atom(WM *wm, Atom atom);
int wm_i_net_state_from_atom(WM *wm, Atom atom);

void wm_i_regrab_keys(WM *wm);

void wm_i_update_numlock_mask(WM *wm);

int wm_i_get_manageable(WM *wm, Window window, XWindowAttributes *attrs);
int wm_i_is_scannable(WM *wm, Window window, XWindowAttributes *attrs);

int wm_i_get_text_prop(WM *wm, Window window, Atom atom, char *text,
                       unsigned size);
void wm_i_configure_frame(WM *wm, WMClient *client);

int wm_i_client_drag(WM *wm, WMClient *client, int x_root, int y_root,
                     const WMEdges *edges);

void wm_i_handle_event(WM *wm, XEvent *event);

#endif