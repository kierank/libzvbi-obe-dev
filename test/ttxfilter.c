/*
 *  zvbi-ttxfilter -- Teletext filter
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

/* $Id: ttxfilter.c,v 1.12 2007/09/01 15:06:55 mschimek Exp $ */

/* For libzvbi version 0.2.x / 0.3.x. */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <locale.h>
#include <math.h>
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

#define DEBUG 0

static vbi_bool			option_abort_on_error;
static vbi_bool			option_experimental_output;
static vbi_bool			option_keep_ttx_system_pages;
static double			option_start_time;
static double			option_end_time;

static vbi_sliced_filter *	sf;

/* Data is all zero, hopefully ignored due to hamming and parity error. */
static vbi_sliced		sliced_blank;

static vbi_bool			started;

static vbi_bool
filter_frame			(const vbi_sliced *	sliced_in,
				 unsigned int		n_lines,
				 double			sample_time,
				 int64_t		stream_time)
{
	vbi_sliced sliced_out[64];
	vbi_sliced *s;
	unsigned int n_lines_prev_in;
	unsigned int n_lines_prev_out;
	unsigned int n_lines_in;
	unsigned int n_lines_out;
	vbi_bool success;

	stream_time = stream_time; /* unused */

	if (!started) {
		option_start_time += sample_time;
		option_end_time += sample_time;

		started = TRUE;
	}

	if (sample_time < option_start_time
	    || sample_time >= option_end_time)
		return TRUE;

	if (0 == n_lines)
		return TRUE;

	n_lines_prev_in = 0;
	n_lines_prev_out = 0;

	do {
		const unsigned int max_lines_out = N_ELEMENTS (sliced_out);

		n_lines_in = n_lines - n_lines_prev_in;

		success = vbi_sliced_filter_cor
			(sf,
			 sliced_out + n_lines_prev_out,
			 &n_lines_out,
			 max_lines_out - n_lines_prev_out,
			 sliced_in + n_lines_prev_in,
			 &n_lines_in);

		if (success)
			break;

		error_msg (vbi_sliced_filter_errstr (sf));

		if (option_abort_on_error) {
			exit (EXIT_FAILURE);
		}

		assert (n_lines_in > 0);

		n_lines_prev_in += n_lines_in;
		n_lines_prev_out += n_lines_out;

	} while (n_lines_prev_in < n_lines);

	n_lines_in += n_lines_prev_in;
	n_lines_out += n_lines_prev_out;

	s = sliced_out;

	if (0 == n_lines_out) {
		if (0) {
			/* Decoder may assume data loss without
			   continuous timestamps. */
			s = &sliced_blank;
			n_lines_out = 1;
		} else {
			return TRUE;
		}
	}

	if (option_experimental_output) {
		int64_t stream_time;
		struct timeval capture_time;
		double intpart;

		stream_time = sample_time * 90000;

		capture_time.tv_usec =
			(int)(1e6 * modf (sample_time, &intpart));
		capture_time.tv_sec = (int) intpart;

		success = write_sliced_xml (s, n_lines_out, 625,
					    stream_time, capture_time);
	} else {
		success = write_sliced (s, n_lines_out, sample_time);
	}

	if (!success)
		write_error_exit (/* msg: errno */ NULL);

	fflush (stdout);

	return TRUE;
}

static void
usage				(FILE *			fp)
{
	fprintf (fp, _("\
%s %s -- Teletext filter\n\n\
Copyright (C) 2005-2007 Michael H. Schimek\n\
This program is licensed under GPLv2. NO WARRANTIES.\n\n\
Usage: %s [options] [page numbers] < sliced VBI data > sliced VBI data\n\n\
-h | --help | --usage  Print this message and exit\n\
-q | --quiet           Suppress progress and error messages\n\
-V | --version         Print the program version and exit\n\
Input options:\n\
-P | --pes             Source is a DVB PES stream\n\
-T | --ts pid          Source is a DVB TS stream\n\
Filter options:\n\
-s | --system          Keep system pages (page inventories, DRCS etc)\n\
-t | --time from-to    Keep pages in this time interval, in seconds\n\
                       since the start of the stream\n\
Valid page numbers are 100 to 899. You can also specify a range like\n\
150-299.\n\
"),
		 PROGRAM_NAME, VERSION, program_invocation_name);
}

static const char
short_options [] = "ahst:qxPT:V";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "abort-on-error",	no_argument,		NULL,	'a' },
	{ "help",		no_argument,		NULL,	'h' },
	{ "usage",		no_argument,		NULL,	'h' },
	{ "system",		no_argument,		NULL,	's' },
	{ "time",		no_argument,		NULL,	't' },
	{ "quiet",		no_argument,		NULL,	'q' },
	{ "experimental",	no_argument,		NULL,	'x' },
	{ "pes",		no_argument,		NULL,	'P' },
	{ "ts",			required_argument,	NULL,	'T' },
	{ "version",		no_argument,		NULL,	'V' },
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
valid_pgno			(vbi_pgno		pgno)
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

		if (!valid_pgno (first_pgno))
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

			if (!valid_pgno (last_pgno))
				invalid_pgno_exit (argv[i]);
		} else if (0 != *s) {
			invalid_pgno_exit (argv[i]);
		}

		success = vbi_sliced_filter_keep_ttx_pages
			(sf, first_pgno, last_pgno);
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
	enum file_format file_format;
	struct stream *st;
	int c;

	option_log_mask = DEBUG ? -1 : 0;

	init_helpers (argc, argv);

	file_format = FILE_FORMAT_SLICED;

	option_start_time = 0.0;
	option_end_time	= 1e30;

	while (-1 != (c = getopt_long (argc, argv, short_options,
				       long_options, &option_index))) {
		switch (c) {
		case 0: /* getopt_long() flag */
			break;

		case 'a':
			option_abort_on_error ^= TRUE;
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

		case 'q':
			option_quiet ^= TRUE;
			break;

		case 'x':
			option_experimental_output ^= TRUE;
			break;

		case 'P':
			file_format = FILE_FORMAT_DVB_PES;
			break;

		case 'T':
			parse_option_ts ();
			file_format = FILE_FORMAT_DVB_TS;
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

#if 0
	if (DEBUG) {
		vbi_sliced_filter_set_log_fn (sf, /* mask */ -1,
					      vbi_log_on_stderr,
					      /* user_data */ NULL);
	}
#endif

	vbi_sliced_filter_keep_ttx_system_pages
		(sf, option_keep_ttx_system_pages);

	assert (argc >= optind);
	parse_page_numbers (argc - optind, &argv[optind]);

	sliced_blank.id = VBI_SLICED_TELETEXT_B_L10_625;
	sliced_blank.line = 7;

	st = read_stream_new (file_format, filter_frame);

	{
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

		success = open_sliced_write (stdout, timestamp);
		if (!success) {
			error_exit (_("Cannot open output stream: %s."),
				    strerror (errno));
		}
	}

	read_stream_loop (st);

	read_stream_delete (st);

	error_msg (_("End of stream."));

	exit (EXIT_SUCCESS);

	return 0;
}
