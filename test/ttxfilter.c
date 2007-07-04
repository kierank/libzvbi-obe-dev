/*
 *  libzvbi test
 *
 *  Copyright (C) 2005, 2007 Michael H. Schimek
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

/* $Id: ttxfilter.c,v 1.7 2007/07/04 05:08:46 mschimek Exp $ */

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

#define PROGRAM_NAME "zvbi-ttxfilter"

#define _(x) x /* TODO */

#define N_ELEMENTS(array) (sizeof (array) / sizeof (*(array)))

static struct {
	vbi_pgno			first;
	vbi_pgno			last;
}				filter_pages[30];

#ifndef HAVE_PROGRAM_INVOCATION_NAME
static char *			program_invocation_name;
static char *			program_invocation_short_name;
#endif

static vbi_bool			option_source_is_pes;
static vbi_bool			option_keep_system_pages;
static double			option_start_time;
static double			option_end_time;

static vbi_dvb_demux *		dx;

/* Data is all zero, hopefully ignored due to hamming and parity error. */
static vbi_sliced		sliced_blank;

static unsigned int		pass_this;
static unsigned int		pass_next;

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
invalid_pgno_exit		(const char *		arg)
{
	error_exit (_("Invalid page number '%s'."), arg);
}

static void
no_mem_exit			(void)
{
	error_exit ("%s.", strerror (ENOMEM));
}

static vbi_bool
decode_packet_0			(const uint8_t		buffer[42],
				 unsigned int		magazine)
{
	int page;
	int flags;
	vbi_pgno pgno;
	vbi_bool match;
	unsigned int mag_set;

	page = vbi_unham16p (buffer + 2);
	if (page < 0) {
		fprintf (stderr, "Hamming error in packet 0 page number\n");
		return FALSE;
	}

	if (0xFF == page) {
		/* Filler, discard. */
		pass_this = 0;
		return TRUE;
	}

	pgno = magazine * 0x100 + page;

	flags = vbi_unham16p (buffer + 4)
		| (vbi_unham16p (buffer + 6) << 8)
		| (vbi_unham16p (buffer + 8) << 16);
	if (flags < 0) {
		fprintf (stderr, "Hamming error in packet 0 flags\n");
		return FALSE;
	}

	if (flags & 0x100000 /* VBI_SERIAL */) {
		mag_set = -1;
	} else {
		mag_set = 1 << magazine;
	}

	match = FALSE;

	if (!vbi_is_bcd (pgno)) {
		/* Page inventories and TOP pages (e.g. to
		   find subtitles), DRCS and object pages, etc. */
		match = option_keep_system_pages;
	} else {
		unsigned int i;

		for (i = 0; i < N_ELEMENTS (filter_pages); ++i) {
			if (pgno >= filter_pages[i].first
			    && pgno <= filter_pages[i].last) {
				match = TRUE;
				break;
			}
		}
	}

	if (match) {
		pass_this |= mag_set;
		pass_next = pass_this;
	} else {
		if (pass_this & mag_set) {
			/* Terminate page. */
			pass_next = pass_this & ~mag_set;
		} else {
			pass_this &= ~mag_set;
			pass_next = pass_this;
		}
	}

	return TRUE;
}

static vbi_bool
teletext			(const uint8_t		buffer[42],
				 unsigned int		line)
{
	int pmag;
	unsigned int magazine;
	unsigned int packet;

	line = line;

	pass_this = pass_next;

	pmag = vbi_unham16p (buffer);
	if (pmag < 0) {
		fprintf (stderr, "Hamming error in packet number\n");
		return FALSE;
	}

	magazine = pmag & 7;
	if (0 == magazine)
		magazine = 8;

	packet = pmag >> 3;

	switch (packet) {
	case 0:
		/* Page header. */

		if (!decode_packet_0 (buffer, magazine))
			return FALSE;
		break;

	case 1 ... 25:
		/* Page body. */
	case 26:
		/* Page enhancement packet. */
	case 27:
		/* Page linking. */
	case 28:
	case 29:
		/* Level 2.5/3.5 enhancement. */
		break;

	case 30:
	case 31:
		/* IDL packet (ETS 300 708). */
		return FALSE;

	default:
		assert (0);
	}

	return !!(pass_this & (1 << magazine));
}

static void
decode				(vbi_sliced *		sliced,
				 unsigned int *		lines,
				 double			sample_time,
				 int64_t		stream_time)
{
	vbi_sliced *sliced_out;
	unsigned int n_lines_in;
	unsigned int n_lines_out;
	vbi_bool pass_through;

	sample_time = sample_time;
	stream_time = stream_time;

	sliced_out = sliced;

	n_lines_in = *lines;
	n_lines_out = 0;

	while (n_lines_in > 0) {
		switch (sliced->id) {
		case VBI_SLICED_TELETEXT_B_L10_625:
		case VBI_SLICED_TELETEXT_B_L25_625:
		case VBI_SLICED_TELETEXT_B_625:
			pass_through = teletext (sliced->data, sliced->line);
			break;

		default:
			pass_through = FALSE;
			break;
		}

		if (pass_through) {
			memcpy (sliced_out, sliced, sizeof (*sliced_out));
			++sliced_out;
			++n_lines_out;
		}

		++sliced;
		--n_lines_in;
	}

	*lines = n_lines_out;
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
			if (n_lines > 0) {
				decode (sliced, &n_lines, 0, pts);

				if (n_lines > 0) {
					double timestamp = (pts - c_start_time) / 90000.0;
					vbi_bool success;

					success = write_sliced (sliced, n_lines, timestamp);
					assert (success);

					fflush (stdout);
				}
			}
			}
		}
	}

	fprintf (stderr, "\rEnd of stream\n");
}

static void
old_mainloop			(void)
{
	vbi_bool start = TRUE;

	for (;;) {
		vbi_sliced sliced[40];
		double timestamp;
		unsigned int n_lines;
		vbi_bool success;

		n_lines = read_sliced (sliced, &timestamp, /* max_lines */ 40);
		if ((int) n_lines < 0)
			break; /* eof */

		if (start) {
			option_start_time += timestamp;
			option_end_time += timestamp;
			start = FALSE;
		}

		decode (sliced, &n_lines, timestamp, /* sample_time */ 0);

		if (timestamp >= option_start_time
		    && timestamp < option_end_time) {
		if (n_lines > 0) {
			success = write_sliced (sliced, n_lines, timestamp);
			assert (success);

			fflush (stdout);
		} else if (0) {
			/* Decoder may assume data loss without
			   continuous timestamps. */
			success = write_sliced (&sliced_blank,
						/* n_lines */ 1,
						timestamp);
			assert (success);

			fflush (stdout);
		}
		}
	}

	fprintf (stderr, "\rEnd of stream\n");
}

static void
usage				(FILE *			fp)
{
	fprintf (fp, _("\
%s %s -- Teletext filter\n\n\
Copyright (C) 2005-2006 Michael H. Schimek\n\
This program is licensed under GPL 2. NO WARRANTIES.\n\n\
Usage: %s [options] [page numbers] < sliced vbi data > sliced vbi data\n\n\
-h | --help | --usage  Print this message and exit\n\
-s | --system          Keep system pages (page inventories, DRCS etc)\n\
-t | --time from-to    Keep pages in this time interval (in seconds)\n\
-P | --pes             Source is a DVB PES\n\
-V | --version         Print the program version and exit\n\n\
Valid page numbers are 100 to 899. You can also specify a range like\n\
150-299.\n\
"),
		 PROGRAM_NAME, VERSION, program_invocation_name);
}

static const char
short_options [] = "hst:PV";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "help",	no_argument,		NULL,		'h' },
	{ "usage",	no_argument,		NULL,		'h' },
	{ "system",	no_argument,		NULL,		's' },
	{ "time",	no_argument,		NULL,		't' },
	{ "pes",	no_argument,		NULL,		'P' },
	{ "version",	no_argument,		NULL,		'V' },
	{ NULL, 0, 0, 0 }
};
#else
#  define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static int			option_index;

static void
error_usage_exit		(const char *		template,
				 ...)
{
	va_list ap;

	va_start (ap, template);
	vprint_error (template, ap);
	va_end (ap);

	usage (stderr);

	exit (EXIT_FAILURE);
}

static vbi_bool
is_valid_pgno			(vbi_pgno		pgno)
{
	if (!vbi_is_bcd (pgno))
		return FALSE;

	if (pgno >= 0x100 && pgno <= 0x899)
		return TRUE;

	return FALSE;
}

static void
option_time			(void)
{
	char *s = optarg;

	option_start_time = strtod (s, &s);

	while (isspace (*s))
		++s;

	if ('-' != *s++)
		goto invalid;

	option_end_time = strtod (s, &s);

	if (option_start_time < 0
	    || option_end_time < 0
	    || option_end_time <= option_start_time)
		goto invalid;

	return;

 invalid:
	error_usage_exit (_("Invalid time range '%s'."), optarg);
}

int
main				(int			argc,
				 char **		argv)
{
	int c;

#ifndef HAVE_PROGRAM_INVOCATION_NAME
	{
		unsigned int i;

		for (i = strlen (argv[0]); i > 0; --i) {
			if ('/' == argv[0][i - 1])
				break;
		}

		program_invocation_name = argv[0];
		program_invocation_short_name = &argv[0][i];
	}
#endif

	setlocale (LC_ALL, "");

	option_start_time = 0.0;
	option_end_time = 1e40;

	while (-1 != (c = getopt_long (argc, argv, short_options,
				       long_options, &option_index))) {
		switch (c) {
		case 0:
			break;

		case 'h':
			usage (stdout);
			exit (EXIT_SUCCESS);

		case 's':
			option_keep_system_pages ^= TRUE;
			break;

		case 't':
			assert (NULL != optarg);
			option_time ();
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

	if (argc > optind) {
		unsigned int n_pages;
		int i;

		n_pages = 0;

		for (i = optind; i < argc; ++i) {
			const char *s;
			char *end;
			vbi_pgno pgno;

			if (n_pages >= N_ELEMENTS (filter_pages))
				break;

			s = argv[i];

			pgno = strtoul (s, &end, 16);
			s = end;

			if (!is_valid_pgno (pgno))
				invalid_pgno_exit (argv[i]);

			filter_pages[n_pages].first = pgno;

			while (*s && isspace (*s))
				++s;

			if ('-' == *s) {
				++s;

				while (*s && isspace (*s))
					++s;

				pgno = strtoul (s, &end, 16);
				s = end;

				if (!is_valid_pgno (pgno))
					invalid_pgno_exit (argv[i]);
			} else if (0 != *s) {
				usage (stderr);
				exit (EXIT_FAILURE);
			}

			filter_pages[n_pages++].last = pgno;
		}
	}

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
