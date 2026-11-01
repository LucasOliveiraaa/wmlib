#include "report.h"
#include "wm.h"
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <stdarg.h>
#include <stdio.h>

static int (*wm_xerror_default)(Display *, XErrorEvent *) = NULL;
static int wm_other_wm_detected = 0;
static WM *wm_error_target = NULL;
static const char *wm_current_call = NULL;

void wm_i_report(WM *wm, const char *format, ...) {
  char message[256];
  va_list args;

  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);

  if (wm && wm->on_error) {
    wm->on_error(wm, message);
  } else {
    fprintf(stderr, "wmlib: %s\n", message);
  }
}

/* Remembers the library call being executed, for wm_set_debug_sync(). */
void wm_i_report_trace(WM *wm, const char *name) {
  if (wm && wm->debug_sync) {
    wm_current_call = name;
  }
}

int wm_i_report_xerror_start(Display *dpy, XErrorEvent *ee) {
  (void)dpy;
  (void)ee;
  wm_other_wm_detected = 1; /* BadAccess on SubstructureRedirect */
  return 0;
}

int wm_i_report_xerror_ignore(Display *dpy, XErrorEvent *ee) {
  (void)dpy;
  (void)ee;
  return 0;
}

/* Windows can vanish at any time; those races are not fatal. */
int wm_i_report_xerror(Display *dpy, XErrorEvent *ee) {
  if (ee->error_code == BadWindow ||
      (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch) ||
      (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch) ||
      (ee->request_code == X_GrabButton && ee->error_code == BadAccess) ||
      (ee->request_code == X_GrabKey && ee->error_code == BadAccess)) {
    return 0;
  }

  if (wm_error_target && wm_error_target->on_error) {
    char text[128];
    XGetErrorText(dpy, ee->error_code, text, sizeof(text));
    if (wm_error_target->debug_sync && wm_current_call) {
      wm_i_report(wm_error_target, "X error: %s (request %d) in %s", text,
                  ee->request_code, wm_current_call);
    } else {
      wm_i_report(wm_error_target, "X error: %s (request %d)", text,
                  ee->request_code);
    }
    return 0;
  }

  fprintf(stderr, "wmlib: X error: request=%d code=%d\n", ee->request_code,
          ee->error_code);
  return wm_xerror_default ? wm_xerror_default(dpy, ee) : 0;
}

void wm_i_report_set_xerror_default(int (*handler)(Display *, XErrorEvent *)) {
  wm_xerror_default = handler;
}
void wm_i_report_set_xerror_target(WM *wm) { wm_error_target = wm; }
WM *wm_i_report_get_xerror_target(void) { return wm_error_target; }
int wm_i_report_xerror_other_wm(void) { return wm_other_wm_detected; }
void wm_i_report_xerror_reset(void) { wm_other_wm_detected = 0; }
void wm_i_report_set_current_call(const char *name) { wm_current_call = name; }
const char *wm_i_report_get_current_call(void) { return wm_current_call; }