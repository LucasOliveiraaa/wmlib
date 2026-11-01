#ifndef WMLIB_CLIENT_H
#define WMLIB_CLIENT_H

#include "wmlib/screen.h"
#include "wmlib/wm.h"
#include <X11/Xlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WMClient WMClient;

typedef struct WMClientNode WMClientNode;

struct WMClientNode {
  WMClient *client;
  WMClientNode *next;
};

typedef enum {
  WM_SCAN_NORMAL,
  WM_SCAN_TRANSIENT,
} WMScanKind;

typedef void (*WMScanCallback)(WM *wm, WMClient *client, WMScanKind kind, void *userdata);

/*
 * Scans the windows on a given screen, registers every manageable window
 * (non-transient first, then transient ones) and calls on_manage for each.
 * The optional callback is then called with the same client. Clients are
 * already registered and owned by the library.
 * Returns 0 on success, or -1 on failure.
 */
int wm_scan_screen(WM *wm, WMScreen *screen, WMScanCallback callback, void *userdata);

/*
 * Creates and registers a client for an existing, non-override-redirect
 * window. The window does not need to be mapped yet. Returns NULL on failure.
 */
WMClient *wm_client_create(WM *wm, WMScreen *screen, Window window);
/*
 * Forgets a client and frees it. A frame is removed and the window returns to
 * the root.
 */
void wm_client_destroy(WM *wm, WMClient *client);
/*
 * Finds the managed client for a client window or its frame, or NULL.
 */
WMClient *wm_client_find(WM *wm, Window window);

int wm_client_update_title(WM *wm, WMClient *client);
int wm_client_update_hints(WM *wm, WMClient *client);
int wm_client_update_transient(WM *wm, WMClient *client);
int wm_client_update_window_type(WM *wm, WMClient *client);
/*
 * Reads _NET_WM_STRUT_PARTIAL, falling back to the legacy _NET_WM_STRUT.
 */
int wm_client_update_strut(WM *wm, WMClient *client);
/*
 * Applies the client's stored geometry/border to the X window (and to its
 * frame), publishes _NET_FRAME_EXTENTS and sends the synthetic
 * ConfigureNotify required by ICCCM.
 */
int wm_client_configure(WM *wm, WMClient *client);

/*
 * Selects events on the client window in addition to the ones the library
 * needs. For framed clients the pointer and crossing bits are selected on the
 * frame too.
 */
int wm_client_select_input(WM *wm, WMClient *client, long mask);

int wm_client_get_state(WM *wm, WMClient *client, long *state);
int wm_client_set_state(WM *wm, WMClient *client, long state);

/*
 * Sets or clears one _NET_WM_STATE value, identified by its net atom index.
 */
int wm_client_set_net_state(WM *wm, WMClient *client, int net_atom,
                            int enabled);
/*
 * Shorthand for wm_client_set_net_state() with NetWMFullscreen.
 */
int wm_client_set_fullscreen(WM *wm, WMClient *client, int fullscreen);

/*
 * Gives the input focus to a client and records it in screen->focused.
 */
int wm_client_focus(WM *wm, WMClient *client);
int wm_client_unfocus(WM *wm, WMClient *client);

int wm_client_map(WM *wm, WMClient *client);
int wm_client_unmap(WM *wm, WMClient *client);

/*
 * Politely asks the client to close (WM_DELETE_WINDOW), killing it if it does
 * not support the protocol.
 */
int wm_client_close(WM *wm, WMClient *client);
/*
 * Sends _NET_WM_PING and sets ping_pending until the client answers.
 * Returns -1 if the client does not support the protocol.
 */
int wm_client_ping(WM *wm, WMClient *client);

int wm_client_get_geometry(WM *wm, WMClient *client, int *x, int *y, int *width,
                           int *height);
/*
 * Stores the geometry and applies it (see wm_client_configure).
 */
int wm_client_set_geometry(WM *wm, WMClient *client, int x, int y, int width,
                           int height);
/*
 * Stores the current geometry and border in the old_* fields, and applies
 * them again.
 */
int wm_client_save_geometry(WM *wm, WMClient *client);
int wm_client_restore_geometry(WM *wm, WMClient *client);
/*
 * Adjusts a requested size to the client's size hints (base, min, max,
 * increments and aspect ratio), following ICCCM 4.1.2.3.
 */
int wm_client_apply_size_hints(WM *wm, WMClient *client, int *width,
                               int *height);

/*
 * Interactive move and resize.
 */
int wm_client_begin_move(WM *wm, WMClient *client, int x_root, int y_root);
int wm_client_begin_resize(WM *wm, WMClient *client, int x_root, int y_root,
                           WMMoveResizeDirection direction);

/*
 * Reparenting frames, opt-in per client.
 */
int wm_client_frame(WM *wm, WMClient *client, WMExtents extents);
int wm_client_unframe(WM *wm, WMClient *client);

/*
 * Stacking order. All of them keep _NET_CLIENT_LIST_STACKING up to date.
 */
int wm_client_raise(WM *wm, WMClient *client);
int wm_client_lower(WM *wm, WMClient *client);
int wm_client_restack(WM *wm, WMClient *client, WMClient *sibling, int above);

int wm_client_set_border_width(WM *wm, WMClient *client, int width);
/*
 * The color is a pixel value allocated by the caller.
 */
int wm_client_set_border_color(WM *wm, WMClient *client, unsigned long pixel);

/*
 * Assigns a client to a desktop (or WM_DESKTOP_ALL) and publishes
 * _NET_WM_DESKTOP.
 */
int wm_client_set_desktop(WM *wm, WMClient *client, unsigned desktop);
/*
 * Whether the client is mapped, not hidden, on the current desktop (or
 * sticky) and not covered by show-desktop mode.
 */
int wm_client_is_visible(WM *wm, WMClient *client);

/*
 * Returns the monitor with the largest overlap with the client, or the
 * first one when none overlaps.
 */
WMMonitor *wm_client_monitor(WM *wm, WMClient *client);

Window wm_client_get_window(const WMClient *client);
Window wm_client_get_frame(const WMClient *client);
WMScreen *wm_client_get_screen(const WMClient *client);

void *wm_client_get_userdata(const WMClient *client);
void wm_client_set_userdata(WMClient *client, void *userdata);

/* Geometry */
int wm_client_get_geometry(WM *wm, WMClient *client, int *x, int *y, int *width,
                           int *height);

int wm_client_get_border_width(const WMClient *client);

int wm_client_get_old_geometry(const WMClient *client, WMRect *geometry);

int wm_client_get_frame_extents(const WMClient *client, WMExtents *extents);

/* Hints */
int wm_client_get_base_size(const WMClient *client, int *width, int *height);

int wm_client_get_min_size(const WMClient *client, int *width, int *height);

int wm_client_get_max_size(const WMClient *client, int *width, int *height);

int wm_client_get_size_increments(const WMClient *client, int *width,
                                  int *height);

int wm_client_get_aspect_ratios(const WMClient *client, float *min_aspect,
                                float *max_aspect);

/* Properties */
const char *wm_client_get_title(const WMClient *client);
WMWindowType wm_client_get_window_type(const WMClient *client);
Window wm_client_get_transient_for(const WMClient *client);
WMStrut wm_client_get_strut(const WMClient *client);
long wm_client_get_event_mask(const WMClient *client);

/* Desktop */
unsigned wm_client_get_desktop(const WMClient *client);

/* User time */
int wm_client_get_user_time(const WMClient *client, unsigned long *user_time);

/* Window-management policy */
int wm_client_is_floating(const WMClient *client);
int wm_client_is_fixed(const WMClient *client);
int wm_client_is_fullscreen(const WMClient *client);
int wm_client_is_maximized_horizontally(const WMClient *client);
int wm_client_is_maximized_vertically(const WMClient *client);
int wm_client_is_above(const WMClient *client);
int wm_client_is_below(const WMClient *client);
int wm_client_is_sticky(const WMClient *client);
int wm_client_is_hidden(const WMClient *client);
int wm_client_is_shaded(const WMClient *client);
int wm_client_is_demanding_attention(const WMClient *client);
int wm_client_is_urgent(const WMClient *client);
int wm_client_never_takes_focus(const WMClient *client);
int wm_client_has_pending_ping(const WMClient *client);
int wm_client_is_mapped(const WMClient *client);

/* Policy setters */
int wm_client_set_floating(WM *wm, WMClient *client, int floating);

/* Existing authoritative operations */
int wm_client_set_geometry(WM *wm, WMClient *client, int x, int y, int width,
                           int height);

int wm_client_set_fullscreen(WM *wm, WMClient *client, int fullscreen);

int wm_client_set_desktop(WM *wm, WMClient *client, unsigned desktop);

#ifdef __cplusplus
}
#endif

#endif