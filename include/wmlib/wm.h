#ifndef WMLIB_WM_H
#define WMLIB_WM_H

#include <X11/X.h>
#include <X11/Xlib.h>
#include <stddef.h>

#include "wmlib/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WMLIB_VERSION_MAJOR 2
#define WMLIB_VERSION_MINOR 0
#define WMLIB_VERSION_PATCH 0

/* Value of WMClient.desktop for clients shown on every desktop. */
#define WM_DESKTOP_ALL 0xFFFFFFFFu

typedef enum {
  WMProtocols,
  WMDeleteWindow,
  WMState,
  WMHints,
  WMNormalHints,
  WMTransientFor,
  WMTakeFocus,
  WMName,
  WMUtf8String,
  WMManager,
  WMLast
} WMAtom;
typedef enum {
  NetSupported,
  NetWMName,
  NetWMState,
  NetWMCheck,
  NetWMFullscreen,
  NetActiveWindow,
  NetWMWindowType,
  NetWMWindowTypeDialog,
  NetClientList,
  NetClientListStacking,
  NetNumberOfDesktops,
  NetCurrentDesktop,
  NetWorkarea,
  NetWMWindowTypeDock,
  NetWMWindowTypeDesktop,
  NetWMWindowTypeSplash,
  NetWMWindowTypeUtility,
  NetWMWindowTypeMenu,
  NetWMStrutPartial,
  NetWMMaximizedVert,
  NetWMMaximizedHorz,
  NetWMAbove,
  NetWMBelow,
  NetWMSticky,
  NetWMHidden,
  NetWMDemandsAttention,
  NetCloseWindow,
  NetMoveResizeWindow,
  NetWMMoveResize,
  NetFrameExtents,
  NetWMPing,
  NetWMDesktop,
  NetShowingDesktop,
  NetRequestFrameExtents,
  NetWMAllowedActions,
  NetWMStrut,
  NetWMUserTime,
  NetWMStateShaded,
  NetWMActionMove,
  NetWMActionResize,
  NetWMActionShade,
  NetWMActionStick,
  NetWMActionMaximizeHorz,
  NetWMActionMaximizeVert,
  NetWMActionFullscreen,
  NetWMActionChangeDesktop,
  NetWMActionClose,
  NetLast
} WMNetAtom;

/**
 * Represents the state of the window manager.
 */
typedef struct WM WM;

/* _NET_WM_STRUT_PARTIAL; all zero when the client reserves no space. */
struct WMStrut {
  int left, right, top, bottom;
  int left_start_y, left_end_y;
  int right_start_y, right_end_y;
  int top_start_x, top_end_x;
  int bottom_start_x, bottom_end_x;
};

typedef enum {
  WM_WINDOW_NORMAL,
  WM_WINDOW_DIALOG,
  WM_WINDOW_DOCK,
  WM_WINDOW_DESKTOP,
  WM_WINDOW_SPLASH,
  WM_WINDOW_UTILITY,
  WM_WINDOW_MENU,
} WMWindowType;

/* Values of the direction field of _NET_WM_MOVERESIZE. */
typedef enum {
  WM_MOVERESIZE_SIZE_TOPLEFT,
  WM_MOVERESIZE_SIZE_TOP,
  WM_MOVERESIZE_SIZE_TOPRIGHT,
  WM_MOVERESIZE_SIZE_RIGHT,
  WM_MOVERESIZE_SIZE_BOTTOMRIGHT,
  WM_MOVERESIZE_SIZE_BOTTOM,
  WM_MOVERESIZE_SIZE_BOTTOMLEFT,
  WM_MOVERESIZE_SIZE_LEFT,
  WM_MOVERESIZE_MOVE,
  WM_MOVERESIZE_SIZE_KEYBOARD,
  WM_MOVERESIZE_MOVE_KEYBOARD,
  WM_MOVERESIZE_CANCEL,
} WMMoveResizeDirection;

/* Fields of a client that changed, see WM.on_client_changed. */
typedef enum {
  WM_CHANGE_TITLE = 1 << 0,
  WM_CHANGE_HINTS = 1 << 1, /* size hints or WM_HINTS input model */
  WM_CHANGE_URGENCY = 1 << 2,
  WM_CHANGE_WINDOW_TYPE = 1 << 3,
  WM_CHANGE_TRANSIENT = 1 << 4,
  WM_CHANGE_STRUT = 1 << 5,
} WMClientChange;

/* Source indication of a _NET_ACTIVE_WINDOW request. */
typedef enum {
  WM_SOURCE_NONE,
  WM_SOURCE_APPLICATION,
  WM_SOURCE_PAGER,
} WMSource;

typedef void (*WMFdCallback)(WM *wm, int fd);
typedef void (*WMTimerCallback)(WM *wm, int id);

/**
 * Creates a new window manager instance.
 * Returns a pointer to the new instance, or NULL on failure.
 */
WM *wm_create(void);

/*
 * Initializes the window manager state, selects SubstructureRedirect on every
 * root window, acquires the WM_Sn manager selections and publishes the root
 * EWMH properties.
 * Returns 0 on success, or -1 on failure (including another WM running).
 */
int wm_init(WM *wm);
/*
 * Cleans up the window manager state: releases all clients, removes the
 * properties published on the root windows and closes the display.
 */
void wm_cleanup(WM *wm);
/*
 * Cleans up the window manager state and frees the instance itself.
 */
void wm_destroy(WM *wm);

/*
 * Makes X errors synchronous: XSync-like behavior after every request, so
 * on_error is called during the library call that caused the error and the
 * message names it. Meant for debugging.
 */
int wm_set_debug_sync(WM *wm, int enabled);

/*
 * Grabs a button on a window for every NumLock/CapsLock combination.
 */
int wm_grab_button(WM *wm, Window window, unsigned modifiers, unsigned button,
                   unsigned event_mask);
int wm_ungrab_buttons(WM *wm, Window window);
/*
 * Strips NumLock, CapsLock and non-modifier bits from an event state so it
 * can be compared with the modifiers passed to wm_grab_key().
 */
unsigned wm_clean_modifiers(WM *wm, unsigned state);

/*
 * Calls callback(wm, fd) from the main loop whenever fd is readable.
 * Registering an fd again replaces its callback.
 */
int wm_watch_fd(WM *wm, int fd, WMFdCallback callback);
int wm_unwatch_fd(WM *wm, int fd);

/*
 * Calls callback(wm, id) from the main loop after interval_ms, once or every
 * interval_ms when repeat is set. Returns the timer id, or -1 on failure.
 */
int wm_add_timer(WM *wm, unsigned interval_ms, int repeat,
                 WMTimerCallback callback);
int wm_cancel_timer(WM *wm, int id);

/*
 * Starts the window manager.
 */
void wm_run(WM *wm);
/*
 * Makes wm_run() return after the current loop iteration.
 */
void wm_stop(WM *wm);

/* ============================================================ */
/*  State and configuration                                     */
/* ============================================================ */

/* User data */
void *wm_get_userdata(const WM *wm);
void wm_set_userdata(WM *wm, void *userdata);

/* Configuration */
const char *wm_get_name(const WM *wm);
int wm_set_name(WM *wm, const char *name);

WMExtents wm_get_frame_extents(const WM *wm);
int wm_set_frame_extents(WM *wm, WMExtents extents);

int wm_get_loop_timeout(const WM *wm);
int wm_set_loop_timeout(WM *wm, int timeout_ms);

/* Atoms */
Atom wm_get_net_atom(const WM *wm, WMNetAtom atom);
Atom wm_get_atom(const WM *wm, WMAtom atom);

/* Read-only state */
int wm_is_running(const WM *wm);
int wm_get_debug_sync(const WM *wm);

Display *wm_get_display(const WM *wm);
size_t wm_get_screen_count(const WM *wm);
WMScreen *wm_get_screen(WM *wm, size_t index);
const WMScreen *wm_get_screen_const(const WM *wm, size_t index);

WMClientNode *wm_get_first_client_node(WM *wm);

/* Lifecycle and debug synchronization already exist */
int wm_set_debug_sync(WM *wm, int enabled);

/* Callback registration */
void wm_set_event_handler(WM *wm, int type, void (*callback)(WM *, XEvent *));

void wm_set_on_start(WM *wm, void (*callback)(WM *));
void wm_set_on_stop(WM *wm, void (*callback)(WM *));
void wm_set_on_loop(WM *wm, void (*callback)(WM *));
void wm_set_on_post(WM *wm, void (*callback)(WM *));

void wm_set_on_manage(
    WM *wm, void (*callback)(WM *, WMClient *));

void wm_set_on_unmanage(
    WM *wm, void (*callback)(WM *, WMClient *));

void wm_set_on_layout(
    WM *wm, void (*callback)(WM *, WMScreen *));

void wm_set_on_focus(
    WM *wm, void (*callback)(WM *, WMClient *));

void wm_set_on_unfocus(
    WM *wm, void (*callback)(WM *, WMClient *));

void wm_set_on_client_changed(
    WM *wm, void (*callback)(WM *, WMClient *, unsigned));

void wm_set_on_state_request(
    WM *wm, void (*callback)(WM *, WMClient *, int, int));

void wm_set_on_activate_request(
    WM *wm, void (*callback)(WM *, WMClient *, WMSource));

void wm_set_on_frame_expose(
    WM *wm, void (*callback)(WM *, WMClient *));

void wm_set_on_monitors_changed(
    WM *wm, void (*callback)(WM *, WMScreen *));

void wm_set_on_move_resize(
    WM *wm,
    void (*callback)(
        WM *, WMClient *, int, int, WMMoveResizeDirection, unsigned));

void wm_set_on_error(
    WM *wm, void (*callback)(WM *, const char *));

#ifdef __cplusplus
}
#endif

#endif