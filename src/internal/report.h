#ifndef WMLIB_REPORT_H_INTERNAL
#define WMLIB_REPORT_H_INTERNAL

#include "wmlib/wm.h"

#define WM_TRACE(wm) wm_i_report_trace((wm), __func__)

void wm_i_report(WM *wm, const char *format, ...);

/* Remembers the library call being executed, for wm_set_debug_sync(). */
void wm_i_report_trace(WM *wm, const char *name);

int wm_i_report_xerror_start(Display *dpy, XErrorEvent *ee);
int wm_i_report_xerror_ignore(Display *dpy, XErrorEvent *ee);
int wm_i_report_xerror(Display *dpy, XErrorEvent *ee);

void wm_i_report_set_xerror_default(int (*handler)(Display *, XErrorEvent *));
void wm_i_report_set_xerror_target(WM *wm);
WM *wm_i_report_get_xerror_target(void);
int wm_i_report_xerror_other_wm(void);
void wm_i_report_xerror_reset(void);
void wm_i_report_set_current_call(const char *name);
const char *wm_i_report_get_current_call(void);

#endif