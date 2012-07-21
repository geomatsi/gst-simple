#include <stdlib.h>
#include <stdio.h>

#include "xutil.h"

int xutil_create_display(Display **xdisp, Window *xwin, int w, int h)
{
	XVisualInfo *x11Visual = 0;
	Colormap x11Colormap = 0;
	Display *x11Display	= 0;
	Window x11Window = 0;
	long x11Screen = 0;

    XSetWindowAttributes sWA;
	unsigned int ui32Mask;
	Window sRootWindow;
	int	i32Depth;

	// Initializes the display and screen
	x11Display = XOpenDisplay(0);

	if (!x11Display) {
		fprintf(stderr, "Error: Unable to open X display\n");
		return -1;
	}

	x11Screen = XDefaultScreen(x11Display);

	// Gets the window parameters
	i32Depth = DefaultDepth(x11Display, x11Screen);
	sRootWindow = RootWindow(x11Display, x11Screen);
	x11Visual = (XVisualInfo *) calloc(1, sizeof(XVisualInfo));
	XMatchVisualInfo(x11Display, x11Screen, i32Depth, TrueColor, x11Visual);

	if (!x11Visual) {
		fprintf(stderr, "Error: Unable to acquire visual\n");
		return -1;
	}

    x11Colormap = XCreateColormap(x11Display, sRootWindow, x11Visual->visual, AllocNone);
    sWA.colormap = x11Colormap;

    // Add to these for handling other events
    sWA.event_mask = StructureNotifyMask | ExposureMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask;
    ui32Mask = CWBackPixel | CWBorderPixel | CWEventMask | CWColormap;

	// Creates the X11 window
    x11Window = XCreateWindow(x11Display, RootWindow(x11Display, x11Screen), 0, 0, w, h,
		0, CopyFromParent, InputOutput, CopyFromParent, ui32Mask, &sWA);
	XMapWindow(x11Display, x11Window);
	XFlush(x11Display);

	*xdisp = x11Display;
	*xwin = x11Window;

	return 1;
}
