/*
 *  zvbi-sliced2pes -- Sliced VBI file converter
 *
 *  Copyright (C) 2004, 2007 Michael H. Schimek
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

/* $Id: sliced2pes.c,v 1.8 2007/09/01 01:45:42 mschimek Exp $ */

/* For libzvbi version 0.2.x / 0.3.x. */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <limits.h>
#include <assert.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif

#include "src/version.h"
#if 2 == VBI_VERSION_MINOR
#  include "src/dvb_mux.h"
#else
#  include "src/zvbi.h"
#endif

#include "sliced.h"

#undef _
#define _(x) x /* i18n TODO */

/* Will be installed one day. */
#define PROGRAM_NAME "sliced2pes"

static unsigned long		option_ts_output_pid;
static unsigned long		option_data_identifier;
static unsigned long		option_min_pes_packet_size;
static unsigned long		option_max_pes_packet_size;

static vbi_dvb_mux *		mx;

static void
write_error_exit		(void)
{
	error_exit (_("Write error: %s."), strerror (errno));
}

static vbi_bool
ts_pes_cb			(vbi_dvb_mux *		mx,
				 void *			user_data,
				 const uint8_t *	packet,
				 unsigned int		packet_size)
{
	size_t actual;

	mx = mx; /* unused */
	user_data = user_data;

	actual = fwrite (packet, 1, packet_size, stdout);
	if (actual < 1)
		write_error_exit ();

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
				     (VBI_SLICED_CAPTION_625 |
				      VBI_SLICED_TELETEXT_B_625 |
				      VBI_SLICED_VPS |
				      VBI_SLICED_WSS_625),
				     /* raw */ NULL,
				     /* sp */ NULL,
				     /* pts */ stream_time);
	if (!success) {
		error_exit (_("Maximum PES packet size %lu bytes "
			      "is too small for this input stream."),
			    option_max_pes_packet_size);
	}

	return TRUE;
}

static void
usage				(FILE *			fp)
{
	vbi_dvb_mux *mx;

	mx = vbi_dvb_pes_mux_new (/* callback */ NULL,
				   /* user_data */ NULL);
	if (NULL == mx)
		no_mem_exit ();

	fprintf (fp, "\
%s %s -- VBI stream converter\n\n\
Copyright (C) 2004, 2007 Michael H. Schimek\n\
This program is licensed under GPLv2 or later. NO WARRANTIES.\n\n\
Usage: %s [options] < sliced VBI data > PES or TS stream\n\n\
-d | --data-identifier n          0x10 ... 0x1F for compatibility with\n\
                                  ETS 300 472 compliant decoders, or\n\
                                  0x99 ... 0x9B as defined in EN 301 775\n\
                                  (default 0x%02x)\n\
-h | --help | --usage             Print this message and exit\n\
-m | --max | --max-packet-size n  Maximum PES packet size (%u bytes)\n\
-n | --min | --min-packet-size n  Minimum PES packet size (%u bytes)\n\
-p | --pes-output                 Generate a DVB PES stream\n\
-q | --quiet                      Suppress progress and error messages\n\
-t | --ts-output pid              Generate a DVB TS stream with this PID\n\
-P | --pes | --pes-input          Source is a DVB PES stream\n\
-T | --ts | --ts-input pid        Source is a DVB TS stream\n\
-V | --version                    Print the program version and exit\n\
",
		 PROGRAM_NAME, VERSION, program_invocation_name,
		 vbi_dvb_mux_get_data_identifier (mx),
		 vbi_dvb_mux_get_max_pes_packet_size (mx),
		 vbi_dvb_mux_get_min_pes_packet_size (mx));

	vbi_dvb_mux_delete (mx);
	mx = NULL;
}

static const char
short_options [] = "d:hm:n:pqt:PT:V";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "data-identifier",	required_argument,	NULL,	'd' },
	{ "help",		no_argument,		NULL,	'h' },
	{ "usage",		no_argument,		NULL,	'h' },
	{ "max-packet-size",	required_argument,	NULL,	'm' },
	{ "min-packet-size",	required_argument,	NULL,	'n' },
	{ "pes-output",		no_argument,		NULL,	'p' },
	{ "quiet",		no_argument,		NULL,	'q' },
	{ "ts-output",		required_argument,	NULL,	't' },
	{ "pes-input",		no_argument,		NULL,	'P' },
	{ "ts-input",		required_argument,	NULL,	'T' },
	{ "version",		no_argument,		NULL,	'V' },
	{ NULL, 0, 0, 0 }
};
#else
#  define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static int			option_index;

static void
parse_option_ts_output		(void)
{
	const char *s = optarg;
	char *end;

	assert (NULL != optarg);

	option_ts_output_pid = strtoul (s, &end, 0);

	if (option_ts_output_pid <= 0x000F ||
	    option_ts_output_pid >= 0x1FFF) {
		error_exit (_("Invalid PID %u."),
			    option_ts_output_pid);
	}
}

static void
parse_option_data_identifier	(void)
{
	const char *s = optarg;
	char *end;

	assert (NULL != optarg);

	option_data_identifier = strtoul (s, &end, 0);

	if (option_data_identifier > 0xFF) {
		error_exit (_("Invalid data identifier 0x%02lx."),
			    option_data_identifier);
	}
}

int
main				(int			argc,
				 char **		argv)
{
	enum file_format in_file_format;
	enum file_format out_file_format;

	init_helpers (argc, argv);

	mx = vbi_dvb_pes_mux_new (ts_pes_cb,
				   /* user_data */ NULL);
	if (NULL == mx)
		no_mem_exit ();

	option_data_identifier =
		vbi_dvb_mux_get_data_identifier (mx);

	option_min_pes_packet_size =
		vbi_dvb_mux_get_min_pes_packet_size (mx);

	option_max_pes_packet_size =
		vbi_dvb_mux_get_max_pes_packet_size (mx);

	vbi_dvb_mux_delete (mx);
	mx = NULL;

	in_file_format = FILE_FORMAT_SLICED;
	out_file_format = FILE_FORMAT_DVB_PES;

	for (;;) {
		int c;

		c = getopt_long (argc, argv, short_options,
				 long_options, &option_index);
		if (-1 == c)
			break;

		switch (c) {
		case 0: /* getopt_long() flag */
			break;

		case 'd':
			parse_option_data_identifier ();
			break;

		case 'h':
			usage (stdout);
			exit (EXIT_SUCCESS);

		case 'm':
			assert (NULL != optarg);
			option_max_pes_packet_size =
				strtoul (optarg, NULL, 0);
			if (option_max_pes_packet_size > UINT_MAX)
				option_max_pes_packet_size = UINT_MAX;
			break;

		case 'n':
			assert (NULL != optarg);
			option_min_pes_packet_size =
				strtoul (optarg, NULL, 0);
			if (option_min_pes_packet_size > UINT_MAX)
				option_min_pes_packet_size = UINT_MAX;
			break;

		case 'p':
			out_file_format = FILE_FORMAT_DVB_PES;
			break;

		case 'q':
			option_quiet ^= TRUE;
			break;

		case 't':
			parse_option_ts_output ();
			out_file_format = FILE_FORMAT_DVB_TS;
			break;

		case 'P':
			in_file_format = FILE_FORMAT_DVB_PES;
			break;

		case 'T':
			parse_option_ts ();
			in_file_format = FILE_FORMAT_DVB_TS;
			break;

		case 'V':
			printf (PROGRAM_NAME " " VERSION "\n");
			exit (EXIT_SUCCESS);

		default:
			usage (stderr);
			exit (EXIT_FAILURE);
		}
	}

	switch (out_file_format) {
	case FILE_FORMAT_DVB_PES:
		mx = vbi_dvb_pes_mux_new (ts_pes_cb,
					   /* user_data */ NULL);
		break;

	case FILE_FORMAT_DVB_TS:
		mx = vbi_dvb_ts_mux_new (option_ts_output_pid,
					  ts_pes_cb,
					  /* user_data */ NULL);
		break;

	default:
		assert (0);
	}

	if (NULL == mx)
		no_mem_exit ();

	if (!vbi_dvb_mux_set_data_identifier (mx, option_data_identifier)) {
		error_exit (_("Invalid data identifier 0x%02lx."),
			    option_data_identifier);
	}

	if (!vbi_dvb_mux_set_pes_packet_size (mx,
					       option_min_pes_packet_size,
					       option_max_pes_packet_size))
		no_mem_exit ();

	{
		struct stream *st;

		st = read_stream_new (in_file_format, decode_frame);
		read_stream_loop (st);
		read_stream_delete (st);
	}

	if (0 != fflush (stdout))
		write_error_exit ();

	exit (EXIT_SUCCESS);
}
