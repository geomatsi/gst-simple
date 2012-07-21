#ifndef OFBP_XUTIL_H
#define OFBP_XUTIL_H

#include "X11/Xlib.h"
#include "X11/Xutil.h"

int xutil_create_display(Display **xdisp, Window *xwin, int w, int h);

#endif	/* OFBP_XUTIL_H */
