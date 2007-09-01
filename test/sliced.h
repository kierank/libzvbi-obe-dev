/*
 *  libzvbi test
 *
 *  Copyright (C) 2005 Michael H. Schimek
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

/* $Id: sliced.h,v 1.8 2007/09/01 15:06:55 mschimek Exp $ */

/* For libzvbi version 0.2.x / 0.3.x. */

#include <stdio.h>
#include <errno.h>
#include <sys/time.h>

#include "src/macros.h"
#include "src/sliced.h"

/* Reader and write for old test/capture --sliced output.
   Attn: this code is not reentrant. */

extern vbi_bool
write_sliced_xml		(vbi_sliced *		sliced,
				 unsigned int		n_lines,
				 unsigned int		n_frame_lines,
				 int64_t		stream_time,
				 struct timeval		capture_time);
extern vbi_bool
write_sliced			(vbi_sliced *		sliced,
				 unsigned int		n_lines,
				 double			timestamp);
extern vbi_bool
open_sliced_write		(FILE *			fp,
				 double			timestamp);
#if 0
extern int
read_sliced			(vbi_sliced *		sliced,
				 double *		timestamp,
				 unsigned int		max_lines);
extern vbi_bool
open_sliced_read		(FILE *			fp);
#endif

/* Helpers. */

#ifndef CLEAR
#  define CLEAR(var) memset (&(var), 0, sizeof (var))
#endif

#ifndef N_ELEMENTS
#  define N_ELEMENTS(array) (sizeof (array) / sizeof (*(array)))
#endif

enum file_format {
	FILE_FORMAT_SLICED = 1,
	FILE_FORMAT_XML,
	FILE_FORMAT_DVB_PES,
	FILE_FORMAT_DVB_TS
};

typedef vbi_bool
stream_callback_fn		(const vbi_sliced *	sliced,
				 unsigned int		n_lines,
				 double			sample_time,
				 int64_t		stream_time);

struct stream;

#ifndef HAVE_PROGRAM_INVOCATION_NAME
extern char *			program_invocation_name;
extern char *			program_invocation_short_name;
#endif

extern vbi_bool			option_quiet;
extern unsigned long		option_ts_pid;
extern unsigned int		option_log_mask;

extern void
vprint_error			(const char *		template,
				 va_list		ap);
extern void
error_msg			(const char *		template,
				 ...);
extern void
error_exit			(const char *		template,
				 ...);
extern void
write_error_exit		(const char *		msg);
extern void
no_mem_exit			(void);
extern vbi_bool
read_stream_loop		(struct stream *	st);
extern void
read_stream_delete		(struct stream *	st);
struct stream *
read_stream_new			(enum file_format	file_format,
				 stream_callback_fn *	callback);
extern void
parse_option_ts			(void);
extern void
init_helpers			(int			argc,
				 char **		argv);
