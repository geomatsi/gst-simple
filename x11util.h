#ifndef X11UTIL_H
#define X11UTIL_H

#include "X11/Xlib.h"
#include "X11/Xutil.h"

int xutil_create_display(Display **xdisp, Window *xwin, int w, int h);

#endif	/* X11UTIL_H */
