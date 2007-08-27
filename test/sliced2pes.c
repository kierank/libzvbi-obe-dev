/*
 *  libzvbi test
 *
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: sliced2pes.c,v 1.6 2007/08/27 06:46:10 mschimek Exp $ */

/* For libzvbi version 0.2.x / 0.3.x. */

#undef NDEBUG

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif

#include "src/version.h"
#if 2 == VBI_VERSION_MINOR
#  include "src/dvb_mux.h"
#else
#  include "src/zvbi.h"
#endif

#include "sliced.h"

static vbi_dvb_mux *		mx;

static vbi_bool
ts_pes_cb			(vbi_dvb_mux *		mx,
				 void *			user_data,
				 const uint8_t *	packet,
				 unsigned int		packet_size)
{
	mx = mx; /* unused */
	user_data = user_data;

	fwrite (packet, 1, packet_size, stdout);

	return TRUE;
}

static vbi_bool
decode_frame			(const vbi_sliced *	sliced,
				 unsigned int		n_lines,
				 double			sample_time,
				 int64_t		stream_time)
{
	vbi_bool success;

	sample_time = sample_time; /* unused */

	success = vbi_dvb_mux_feed (mx,
				    sliced, n_lines,
				    /* service_mask */ -1,
				    /* raw */ NULL,
				    /* sp */ NULL,
				    /* pts */ stream_time);

	return TRUE;
}

static void
usage				(FILE *			fp,
				 char **		argv)
{
	fprintf (fp,
		 "Usage: %s < sliced vbi data > pes stream\n",
		 argv[0]);
}

static const char
short_options [] = "h";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "help",	no_argument,		NULL,		'h' },
	{ NULL, 0, 0, 0 }
};
#else
#define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

int
main				(int			argc,
				 char **		argv)
{
	int index;
	int c;

	init_helpers (argc, argv);

	while (-1 != (c = getopt_long (argc, argv, short_options,
				       long_options, &index))) {
		switch (c) {
		case 0:
			break;

		case 'h':
			usage (stdout, argv);
			exit (EXIT_SUCCESS);

		default:
			usage (stderr, argv);
			exit (EXIT_FAILURE);
		}
	}

	mx = vbi_dvb_pes_mux_new (ts_pes_cb,
				  /* user_data */ NULL);
	assert (NULL != mx);

	{
		struct stream *st;

		st = read_stream_new (FILE_FORMAT_SLICED,
				      decode_frame);
		read_stream_loop (st);
		read_stream_delete (st);
	}

	exit (EXIT_SUCCESS);
}
