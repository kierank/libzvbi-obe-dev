/*
 *  libzvbi - Device interfaces
 *
 *  Copyright (C) 2002 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: io.h,v 1.1 2002/01/12 16:19:07 mschimek Exp $ */

#ifndef IO_H
#define IO_H

#include "decoder.h"

/* Public */

#include <sys/time.h> /* struct timeval */

typedef struct vbi_capture_buffer {
	void *			data;
	int			size;
	double			timestamp;
} vbi_capture_buffer;

/**
 * vbi_capture:
 *
 * Opaque device interface handle.
 **/
typedef struct vbi_capture vbi_capture;

extern vbi_capture *	vbi_capture_v4l2_new(char *dev_name, int buffers,
					     unsigned int *services, int strict,
					     char **errorstr, vbi_bool trace);
extern int		vbi_capture_read_raw(vbi_capture *capture, void *data,
					     double *timestamp, struct timeval *timeout);
extern int		vbi_capture_read_sliced(vbi_capture *capture, vbi_sliced *data, int *lines,
						double *timestamp, struct timeval *timeout);
extern int		vbi_capture_read(vbi_capture *capture, void *raw_data,
					 vbi_sliced *sliced_data, int *lines,
					 double *timestamp, struct timeval *timeout);
extern int		vbi_capture_pull_raw(vbi_capture *capture, vbi_capture_buffer **buffer,
					     struct timeval *timeout);
extern int		vbi_capture_pull_sliced(vbi_capture *capture, vbi_capture_buffer **buffer,
						struct timeval *timeout);
extern int		vbi_capture_pull(vbi_capture *capture, vbi_capture_buffer **raw_buffer,
					 vbi_capture_buffer **sliced_buffer, struct timeval *timeout);
extern vbi_raw_decoder *vbi_capture_parameters(vbi_capture *capture);
extern void		vbi_capture_delete(vbi_capture *capture);

/* Private */

#ifndef _
#ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) gettext (String)
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#endif
#endif

struct vbi_capture {
	vbi_bool		(* read)(vbi_capture *, vbi_capture_buffer **,
					 vbi_capture_buffer **, struct timeval *);
	vbi_raw_decoder *	(* parameters)(vbi_capture *);
	void			(* delete)(vbi_capture *);
};

#endif /* IO_H */
