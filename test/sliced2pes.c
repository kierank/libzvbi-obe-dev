/*
 *  libzvbi test
 *
 *  Copyright (C) 2004-2005 Michael H. Schimek
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

/* $Id: sliced2pes.c,v 1.5 2006/09/29 09:30:16 mschimek Exp $ */

#undef NDEBUG

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif

#include "src/dvb_mux.h"
#include "sliced.h"

static vbi_dvb_mux *		mx;

static vbi_bool
binary_ts_pes			(vbi_dvb_mux *		mx,
				 void *			user_data,
				 const uint8_t *	packet,
				 unsigned int		packet_size)
{
	mx = mx; /* unused, no warning. */
	user_data = user_data;

	if (packet_size != fwrite (packet, 1, packet_size, stdout)) {
		perror ("Write error");
		exit (EXIT_FAILURE);
	}

	fflush (stdout);

	return TRUE;
}

static void
mainloop			(void)
{
	for (;;) {
		vbi_sliced sliced[40];
		double timestamp;
		int n_lines;

		n_lines = read_sliced (sliced, &timestamp, /* max_lines */ 40);
		if (n_lines < 0)
			break;

		if (!_vbi_dvb_mux_feed (mx, timestamp * 90000,
					sliced, n_lines,
					/* service_set: encode all */ -1))
			break;
	}

	fprintf (stderr, "\rEnd of stream\n");
}

static const char
short_options [] = "h";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "help",	no_argument,		NULL,		'h' },
	{ 0, 0, 0, 0 }
};
#else
#define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static void
usage				(FILE *			fp,
				 char **		argv)
{
	fprintf (fp,
 "Libzvbi test/sliced2pes version " VERSION "\n"
 "Copyright (C) 2004-2005 Michael H. Schimek\n"
 "This program is licensed under GPL 2. NO WARRANTIES.\n\n"
 "Converts old test/capture --sliced output to DVB PES format.\n\n"
 "Usage: %s < old file > PES file\n"
 "Options:\n"
 "-h | --help    Print this message\n",
		 argv[0]);
}

int
main				(int			argc,
				 char **		argv)
{
	int index;
	int c;

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

	if (isatty (STDIN_FILENO)) {
		fprintf (stderr, "No vbi data on stdin.\n");
		exit (EXIT_FAILURE);
	}

	mx = _vbi_dvb_mux_pes_new (/* data_identifier */ 0x10,
				   /* packet_size */ 8 * 184,
				   VBI_VIDEOSTD_SET_625_50,
				   binary_ts_pes,
				   /* user_data */ NULL);
	assert (NULL != mx);

	open_sliced_read (stdin);

	mainloop ();

	exit (EXIT_SUCCESS);
}
