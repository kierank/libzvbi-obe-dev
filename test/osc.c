/*
 *  libzvbi test
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: osc.c,v 1.2 2002/01/15 03:20:25 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>

#include <libzvbi.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/xpm.h>

vbi_capture *		cap;
vbi_raw_decoder *	par;
int			src_w, src_h;
vbi_sliced *		sliced;
int			slines;
vbi_bool		quit;

int			do_sim;

Display *		display;
int			screen;
Colormap		cmap;
Window			window;
int			dst_w, dst_h;
GC			gc;
XEvent			event;
XImage *		ximage;
void *			ximgdata;
unsigned char 		*raw1, *raw2;
int			palette[256];
int			depth;
int			draw_row, draw_offset;
int			draw_count = -1;

#include "sim.c"

static void
draw(unsigned char *raw)
{
	int rem = src_w - draw_offset;
	unsigned char buf[256];
	unsigned char *data = raw;
	int i, v, h0, field, end, line;
        XTextItem xti;

	if (draw_count == 0)
		return;

	if (draw_count > 0)
		draw_count--;

	memcpy(raw2, raw, src_w * src_h);

	if (depth == 24) {
		unsigned int *p = ximgdata;
		
		for (i = src_w * src_h; i >= 0; i--)
			*p++ = palette[(int) *data++];
	} else {
		unsigned short *p = ximgdata; // 64 bit safe?

		for (i = src_w * src_h; i >= 0; i--)
			*p++ = palette[(int) *data++];
	}

	XPutImage(display, window, gc, ximage,
		draw_offset, 0, 0, 0, rem, src_h);

	XSetForeground(display, gc, 0);

	if (rem < dst_w)
		XFillRectangle(display, window, gc,
			rem, 0, dst_w, src_h);

	if ((v = dst_h - src_h) <= 0)
		return;

	XSetForeground(display, gc, 0);
	XFillRectangle(display, window, gc,
		0, src_h, dst_w, dst_h);

	XSetForeground(display, gc, ~0);

	field = (draw_row >= par->count[0]);

	if (par->start[field] < 0) {
		xti.nchars = snprintf(buf, 255, "Row %d Line ?", draw_row);
		line = -1;
	} else if (field == 0) {
		line = draw_row + par->start[0];
		xti.nchars = snprintf(buf, 255, "Row %d Line %d", draw_row, line);
	} else {
		line = draw_row - par->count[0] + par->start[1];
		xti.nchars = snprintf(buf, 255, "Row %d Line %d", draw_row, line);
	}

	for (i = 0; i < slines; i++)
		if (sliced[i].line == line)
			break;
	if (i < slines) {
		xti.nchars += snprintf(buf + xti.nchars, 255 - xti.nchars,
				       " %s", vbi_sliced_name(sliced[i].id) ?: "???");
	} else {
		int s = 0, sd = 0;

		data = raw + draw_row * src_w;

		for (i = 0; i < src_w; i++)
			s += data[i];
		s /= src_w;

		for (i = 0; i < src_w; i++)
			sd += abs(data[i] - s);

		sd /= src_w;

		xti.nchars += snprintf(buf + xti.nchars, 255 - xti.nchars,
				       (sd < 5) ? " Blank" : " Unknown signal");
	}

	xti.chars = buf;
	xti.delta = 0;
	xti.font = 0;

	XDrawText(display, window, gc, 4, src_h + 12, &xti, 1);

	data = raw + draw_offset + draw_row * src_w;
	h0 = dst_h - (data[0] * v) / 256;
	end = src_w - draw_offset;
	if (dst_w < end)
		end = dst_w;

	for (i = 1; i < end; i++) {
		int h = dst_h - (data[i] * v) / 256;

		XDrawLine(display, window, gc, i - 1, h0, i, h);
		h0 = h;
	}
}

static void
xevent(void)
{
	while (XPending(display)) {
		XNextEvent(display, &event);

		switch (event.type) {
		case KeyPress:
		{
			switch (XLookupKeysym(&event.xkey, 0)) {
			case 'g':
				draw_count = 1;
				break;

			case 'l':
				draw_count = -1;
				break;

			case 'q':
			case 'c':
				quit = TRUE;
				break;

			case XK_Up:
			    if (draw_row > 0)
				    draw_row--;
			    goto redraw;

			case XK_Down:
			    if (draw_row < (src_h - 1))
				    draw_row++;
			    goto redraw;

			case XK_Left:
			    if (draw_offset > 0)
				    draw_offset -= 10;
			    goto redraw;

			case XK_Right:
			    if (draw_offset < (src_w - 10))
				    draw_offset += 10;
			    goto redraw;  
			}

			break;
		}

		case ConfigureNotify:
			dst_w = event.xconfigurerequest.width;
			dst_h = event.xconfigurerequest.height;
redraw:
			if (draw_count == 0) {
				draw_count = 1;
				draw(raw2);
			}

			break;

		case ClientMessage:
			exit(EXIT_SUCCESS);
		}
	}
}

static void
init_window(int ac, char **av, char *dev_name)
{
	char buf[256];
	Atom delete_window_atom;
	XWindowAttributes wa;
	int i;

	if (!(display = XOpenDisplay(NULL))) {
		fprintf(stderr, "No display\n");
		exit(EXIT_FAILURE);
	}

	screen = DefaultScreen(display);
	cmap = DefaultColormap(display, screen);
 
	window = XCreateSimpleWindow(display,
		RootWindow(display, screen),
		0, 0,		// x, y
		dst_w = 768, dst_h = src_h + 110,
				// w, h
		2,		// borderwidth
		0xffffffff,	// fgd
		0x00000000);	// bgd 

	if (!window) {
		fprintf(stderr, "No window\n");
		exit(EXIT_FAILURE);
	}
			
	XGetWindowAttributes(display, window, &wa);
	depth = wa.depth;
			
	if (depth != 15 && depth != 16 && depth != 24) {
		fprintf(stderr, "Sorry, cannot run at colour depth %d\n", depth);
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < 256; i++) {
		switch (depth) {
		case 15:
			palette[i] = ((i & 0xF8) << 7)
				   + ((i & 0xF8) << 2)
				   + ((i & 0xF8) >> 3);
				break;

		case 16:
			palette[i] = ((i & 0xF8) << 8)
				   + ((i & 0xFC) << 3)
				   + ((i & 0xF8) >> 3);
				break;

		case 24:
			palette[i] = (i << 16) + (i << 8) + i;
				break;
		}
	}

	if (depth == 24) {
		if (!(ximgdata = malloc(src_w * src_h * 4))) {
			fprintf(stderr, "Virtual memory exhausted\n");
			exit(EXIT_FAILURE);
		}
	} else {
		if (!(ximgdata = malloc(src_w * src_h * 2))) {
			fprintf(stderr, "Virtual memory exhausted\n");
			exit(EXIT_FAILURE);
		}
	}

	if (!(raw1 = malloc(src_w * src_h))) {
		fprintf(stderr, "Virtual memory exhausted\n");
		exit(EXIT_FAILURE);
	}

	if (!(raw2 = malloc(src_w * src_h))) {
		fprintf(stderr, "Virtual memory exhausted\n");
		exit(EXIT_FAILURE);
	}

	ximage = XCreateImage(display,
		DefaultVisual(display, screen),
		DefaultDepth(display, screen),
		ZPixmap, 0, (char *) ximgdata,
		src_w, src_h,
		8, 0);

	if (!ximage) {
		fprintf(stderr, "No ximage\n");
		exit(EXIT_FAILURE);
	}

	delete_window_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);

	XSelectInput(display, window, KeyPressMask | ExposureMask | StructureNotifyMask);
	XSetWMProtocols(display, window, &delete_window_atom, 1);
	snprintf(buf, sizeof(buf) - 1, "%s - [cursor] [g]rab [l]ive", dev_name);
	XStoreName(display, window, buf);

	gc = XCreateGC(display, window, 0, NULL);

	XMapWindow(display, window);
	       
	XSync(display, False);
}

static void
mainloop(void)
{
	double timestamp;
	struct timeval tv;

	tv.tv_sec = 2;
	tv.tv_usec = 0;

	assert((sliced = malloc(sizeof(vbi_sliced) * src_h)));

	for (quit = FALSE; !quit;) {
		int r;

		if (do_sim) {
			read_sim(raw1, sliced, &slines, &timestamp);
			r = 1;
		} else {
			r = vbi_capture_read(cap, raw1, sliced,
					     &slines, &timestamp, &tv);
		}

		switch (r) {
		case -1:
			fprintf(stderr, "VBI read error: %d, %s\n",
				errno, strerror(errno));
			exit(EXIT_FAILURE);
		case 0: 
			fprintf(stderr, "VBI read timeout\n");
			exit(EXIT_FAILURE);
		case 1:
			break;
		default:
			assert(!"reached");
		}

		draw(raw1);

/*		printf("raw: %f; sliced: %d\n", timestamp, slines); */

		xevent();
	}
}

static const struct option
long_options[] = {
	{ "device",	required_argument,	NULL,		'd' },
	{ "ntsc",	no_argument,		NULL,		'n' },
	{ "pal",	no_argument,		NULL,		'p' },
	{ "sim",	no_argument,		&do_sim,	TRUE },
	{ "verbose",	no_argument,		NULL,		'v' },
};

int
main(int argc, char **argv)
{
	char *dev_name = "/dev/vbi";
	char *errstr;
	unsigned int services;
	int scanning = 625;
	vbi_bool verbose = FALSE;
	int c, index;

	while ((c = getopt_long(argc, argv, "d:npv", long_options, &index)) != -1)
		switch (c) {
		case 0: /* set flag */
			break;
		case 'd':
			dev_name = optarg;
			break;
		case 'n':
			scanning = 525;
			break;
		case 'p':
			scanning = 625;
			break;
		case 'v':
			verbose ^= TRUE;
			break;
		default:
			fprintf(stderr, "Unknown option\n");
			exit(EXIT_FAILURE);
		}

	services = VBI_SLICED_VBI_525 | VBI_SLICED_VBI_625
		| VBI_SLICED_TELETEXT_B | VBI_SLICED_CAPTION_525
		| VBI_SLICED_CAPTION_625 | VBI_SLICED_VPS
		| VBI_SLICED_WSS_625 | VBI_SLICED_WSS_CPR1204;

	if (do_sim) {
		par = init_sim(scanning, services);
	} else {
		cap = vbi_capture_v4l2_new(dev_name, /* buffers */ 5,
			&services, /* strict */ -1, &errstr, /* trace */ verbose);

		if (!cap) {
			fprintf(stderr, "Cannot capture vbi data "
				"with v4l2 interface:\n%s\n", errstr);

			if (errstr)
				free(errstr);

			cap = vbi_capture_v4l_new(dev_name, scanning,
				&services, /* strict */ -1, &errstr,
				/* trace */ verbose);

			if (!cap) {
				fprintf(stderr, "Cannot capture vbi data "
					"with v4l interface:\n%s\n", errstr);
		
				exit(EXIT_FAILURE);
			}
		}

		assert((par = vbi_capture_parameters(cap)));
	}

	assert(par->sampling_format == VBI_PIXFMT_YUV420);

	src_w = par->bytes_per_line / 1;
	src_h = par->count[0] + par->count[1];

	init_window(argc, argv, dev_name);

	mainloop();

	if (!do_sim)
		vbi_capture_delete(cap);

	exit(EXIT_SUCCESS);	
}
