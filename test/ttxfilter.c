/*
 *  libzvbi -- Teletext filter
 *
 *  Copyright (C) 2005-2007 Michael H. Schimek
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

/* $Id: ttxfilter.c,v 1.8 2007/07/09 23:40:24 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <locale.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <ctype.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif

#include "src/dvb_demux.h"
#include "src/hamm.h"
#include "sliced.h"
#include "src/sliced_filter.h"

#define PROGRAM_NAME "zvbi-ttxfilter"

#define _(x) x /* TODO */

#define N_ELEMENTS(array) (sizeof (array) / sizeof (*(array)))

#ifndef HAVE_PROGRAM_INVOCATION_NAME
#  define HAVE_PROGRAM_INVOCATION_NAME 0
static char *			program_invocation_name;
static char *			program_invocation_short_name;
#endif

static vbi_bool			option_source_is_pes		= FALSE;
static vbi_bool			option_keep_ttx_system_pages	= FALSE;
static vbi_bool			option_experimental_output	= FALSE;
static double			option_start_time		= 0.0;
static double			option_end_time			= 1e30;

static vbi_dvb_demux *		dx;
static vbi_sliced_filter *	sf;

/* Data is all zero, hopefully ignored due to hamming and parity error. */
static vbi_sliced		sliced_blank;

static void
vprint_error			(const char *		template,
				 va_list		ap)
{
	fprintf (stderr, "%s: ", program_invocation_short_name);
	vfprintf (stderr, template, ap);
	fputc ('\n', stderr);
}

static void
error_exit			(const char *		template,
				 ...)
{
	va_list ap;

	va_start (ap, template);
	vprint_error (template, ap);
	va_end (ap);

	exit (EXIT_FAILURE);
}

static void
no_mem_exit			(void)
{
	error_exit ("%s.", strerror (ENOMEM));
}

static void
filter_frame			(const vbi_sliced *	sliced_in,
				 unsigned int		n_lines_in,
				 double			timestamp)
{
	vbi_sliced sliced_out[64];
	vbi_sliced *s;
	unsigned int n_lines_out;
	vbi_bool success;

	if (0 == n_lines_in)
		return;

	success = vbi_sliced_filter_cor	(sf,
					 sliced_out,
					 &n_lines_out,
					 /* max_lines_out */ 64,
					 sliced_in,
					 &n_lines_in);
	if (!success)
		return;

	s = sliced_out;

	if (0 == n_lines_out) {
		if (0) {
			/* Decoder may assume data loss without
			   continuous timestamps. */
			s = &sliced_blank;
			n_lines_out = 1;
		} else {
			return;
		}
	}

	if (option_experimental_output) {
		int64_t stream_time;
		struct timeval capture_time;
		double intpart;

		stream_time = timestamp * 90000;

		capture_time.tv_usec =
			(int)(1e6 * modf (timestamp, &intpart));
		capture_time.tv_sec = (int) intpart;

		success = write_sliced_xml (s, n_lines_out, 625,
					    stream_time, capture_time);
	} else {
		success = write_sliced (s, n_lines_out, timestamp);
	}

	if (!success)
		error_exit ("Write error.");

	fflush (stdout);
}

static void
pes_mainloop			(void)
{
	uint8_t buffer[2048];
	int64_t min_pts;
	double c_start_time;
	double c_end_time;

	min_pts = ((int64_t) 1) << 62;

	while (1 == fread (buffer, sizeof (buffer), 1, stdin)) {
		const uint8_t *bp;
		unsigned int left;

		bp = buffer;
		left = sizeof (buffer);

		while (left > 0) {
			vbi_sliced sliced[64];
			unsigned int n_lines;
			int64_t pts;

			n_lines = vbi_dvb_demux_cor (dx,
						     sliced, 64,
						     &pts,
						     &bp, &left);
			if (pts < 0) {
				/* WTF? */
				continue;
			}

			if (pts < min_pts) {
				c_start_time = option_start_time * 90000 + pts;
				c_end_time = option_end_time * 90000 + pts;
				min_pts = pts;
			}
			
			if (pts >= c_start_time
			    && pts < c_end_time) {
				double timestamp;

				timestamp = (pts - c_start_time) / 90000.0;

				filter_frame (sliced, n_lines, timestamp);
			}
		}
	}

	if (0)
		fprintf (stderr, "\rEnd of stream\n");
}

static void
old_mainloop			(void)
{
	vbi_bool start = TRUE;

	for (;;) {
		vbi_sliced sliced[64];
		double timestamp;
		unsigned int n_lines;

		n_lines = read_sliced (sliced, &timestamp, /* max_lines */ 64);
		if ((int) n_lines < 0)
			break; /* eof */

		if (start) {
			option_start_time += timestamp;
			option_end_time += timestamp;
			start = FALSE;
		}

		if (timestamp >= option_start_time
		    && timestamp < option_end_time) {
			filter_frame (sliced, n_lines, timestamp);
		}
	}

	if (0)
		fprintf (stderr, "\rEnd of stream\n");
}

static void
usage				(FILE *			fp)
{
	fprintf (fp, _("\
%s %s -- Teletext filter\n\n\
Copyright (C) 2005-2007 Michael H. Schimek\n\
This program is licensed under GPL 2. NO WARRANTIES.\n\n\
Usage: %s [options] [page numbers] < sliced vbi data > sliced vbi data\n\n\
-h | --help | --usage  Print this message and exit\n\
-s | --system          Keep system pages (page inventories, DRCS etc)\n\
-t | --time from-to    Keep pages in this time interval, in seconds\n\
                       since the start of the stream\n\
-P | --pes             Source is a DVB PES\n\
-V | --version         Print the program version and exit\n\n\
Valid page numbers are 100 to 899. You can also specify a range like\n\
150-299.\n\
"),
		 PROGRAM_NAME, VERSION, program_invocation_name);
}

static const char
short_options [] = "hst:xPV";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "help",	no_argument,		NULL,		'h' },
	{ "usage",	no_argument,		NULL,		'h' },
	{ "system",	no_argument,		NULL,		's' },
	{ "time",	no_argument,		NULL,		't' },
	{ "experimental", no_argument,		NULL,		'x' },
	{ "pes",	no_argument,		NULL,		'P' },
	/* -T --ts [pid] reserved for transport streams. */
	{ "version",	no_argument,		NULL,		'V' },
	{ NULL, 0, 0, 0 }
};
#else
#  define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static int			option_index;

static void
option_time			(void)
{
	const char *s = optarg;
	char *end;

	assert (NULL != optarg);

	option_start_time = strtod (s, &end);
	s = end;

	while (isspace (*s))
		++s;

	if ('-' != *s++)
		goto invalid;

	option_end_time = strtod (s, &end);
	s = end;

	if (option_start_time < 0
	    || option_end_time < 0
	    || option_end_time <= option_start_time)
		goto invalid;

	return;

 invalid:
	error_exit (_("Invalid time range '%s'."), optarg);
}

static vbi_bool
is_valid_pgno			(vbi_pgno		pgno)
{
	return (vbi_is_bcd (pgno)
		&& pgno >= 0x100
		&& pgno <= 0x899);
}

static void
invalid_pgno_exit		(const char *		arg)
{
	error_exit (_("Invalid page number '%s'."), arg);
}

static void
parse_page_numbers		(unsigned int		argc,
				 char **		argv)
{
	unsigned int i;

	for (i = 0; i < argc; ++i) {
		vbi_pgno first_pgno;
		vbi_pgno last_pgno;
		vbi_bool success;
		const char *s;
		char *end;

		s = argv[i];

		first_pgno = strtoul (s, &end, 16);
		s = end;

		if (!is_valid_pgno (first_pgno))
			invalid_pgno_exit (argv[i]);

		last_pgno = first_pgno;

		while (*s && isspace (*s))
			++s;

		if ('-' == *s) {
			++s;

			while (*s && isspace (*s))
				++s;

			last_pgno = strtoul (s, &end, 16);
			s = end;

			if (!is_valid_pgno (last_pgno))
				invalid_pgno_exit (argv[i]);
		} else if (0 != *s) {
			invalid_pgno_exit (argv[i]);
		}

		success = vbi_sliced_filter_keep_ttx_pages
			(sf, first_pgno, last_pgno + 1);
		if (!success)
			no_mem_exit ();
	}

	if (0 == i)
		error_exit (_("No page numbers specified."));
}

int
main				(int			argc,
				 char **		argv)
{
	int c;

	if (!HAVE_PROGRAM_INVOCATION_NAME) {
		unsigned int i;

		for (i = strlen (argv[0]); i > 0; --i) {
			if ('/' == argv[0][i - 1])
				break;
		}

		program_invocation_name = argv[0];
		program_invocation_short_name = &argv[0][i];
	}

	setlocale (LC_ALL, "");

	while (-1 != (c = getopt_long (argc, argv, short_options,
				       long_options, &option_index))) {
		switch (c) {
		case 0: /* getopt_long() flag */
			break;

		case 'h':
			usage (stdout);
			exit (EXIT_SUCCESS);

		case 's':
			option_keep_ttx_system_pages ^= TRUE;
			break;

		case 't':
			option_time ();
			break;

		case 'x':
			option_experimental_output ^= TRUE;
			break;

		case 'P':
			option_source_is_pes ^= TRUE;
			break;

		case 'V':
			printf (PROGRAM_NAME " " VERSION "\n");
			exit (EXIT_SUCCESS);

		default:
			usage (stderr);
			exit (EXIT_FAILURE);
		}
	}

	sf = vbi_sliced_filter_new (/* callback */ NULL,
				    /* user_data */ NULL);
	if (NULL == sf)
		no_mem_exit ();

	vbi_sliced_filter_keep_ttx_system_pages
		(sf, option_keep_ttx_system_pages);

	assert (argc >= optind);
	parse_page_numbers (argc - optind, &argv[optind]);

	sliced_blank.id = VBI_SLICED_TELETEXT_B_L10_625;
	sliced_blank.line = 7;

	if (isatty (STDIN_FILENO))
		error_exit (_("No VBI data on standard input."));

	c = getchar ();
	ungetc (c, stdin);

	if (0 == c)
		option_source_is_pes = TRUE;

	if (option_source_is_pes) {
		vbi_bool success;

		dx = vbi_dvb_pes_demux_new (/* callback */ NULL,
					    /* user_data */ NULL);
		if (NULL == dx)
			no_mem_exit ();

		success = open_sliced_write (stdout, 0);
		if (!success) {
			error_exit (_("Cannot open output stream: %s."),
				    strerror (errno));
		}

		pes_mainloop ();
	} else {
		struct timeval tv;
		double timestamp;
		vbi_bool success;
		int r;

		r = gettimeofday (&tv, NULL);
		if (-1 == r) {
			error_exit (_("Cannot determine system time: %s."),
				    strerror (errno));
		}

		timestamp = tv.tv_sec + tv.tv_usec * (1 / 1e6);

		success = open_sliced_read (stdin);
		if (!success) {
			error_exit (_("Cannot open input stream: %s."),
				    strerror (errno));
		}

		success = open_sliced_write (stdout, timestamp);
		if (!success) {
			error_exit (_("Cannot open output stream: %s."),
				    strerror (errno));
		}

		old_mainloop ();
	}

	exit (EXIT_SUCCESS);
}
