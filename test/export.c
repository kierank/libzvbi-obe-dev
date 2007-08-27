/*
 *  libzvbi test
 *
 *  Copyright (C) 2000, 2001, 2007 Michael H. Schimek
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

/* $Id: export.c,v 1.15 2007/08/27 06:43:12 mschimek Exp $ */

#undef NDEBUG

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif

#include "src/decoder.h"
#include "src/export.h"
#include "src/dvb_demux.h"
#include "src/vt.h"
#include "src/cache.h"
#include "src/vbi.h"
#include "sliced.h"

#define PROGRAM_NAME "zvbi-export"

#undef _
#define _(x) x /* TODO */

static vbi_decoder *		vbi;
static vbi_bool			quit;
static vbi_pgno			pgno;
static vbi_export *		ex;
static char *			extension;
static int			cr;

static void
event_handler			(vbi_event *		ev,
				 void *			user_data)
{
	FILE *fp;
	vbi_page page;
	vbi_bool success;

	user_data = user_data;

	if (pgno != -1 && ev->ev.ttx_page.pgno != pgno)
		return;

	/* Fetching & exporting here is a bad idea,
	   but this is only a test. */
	success = vbi_fetch_vt_page (vbi, &page,
				     ev->ev.ttx_page.pgno,
				     ev->ev.ttx_page.subno,
				     VBI_WST_LEVEL_3p5,
				     /* n_rows */ 25,
				     /* navigation */ TRUE);
	if (!success) {
		/* Shouldn't happen. */
		error_exit (_("Unknown error."));
	}

	if (-1 == pgno) {
		char name[256];
		
		snprintf (name, sizeof (name) - 1,
			  "test-%03x.%02x.%s",
			  ev->ev.ttx_page.pgno,
			  ev->ev.ttx_page.subno,
			  extension);

		fp = fopen (name, "w");
		if (NULL == fp) {
			error_exit (_("Cannot open output file '%s': %s."),
				    name, strerror (errno));
		}
	} else {
		fp = stdout;
	}

	success = vbi_export_stdio (ex, fp, &page);
	if (!success) {
		error_exit (_("Cannot export page %x: %s."),
			    ev->ev.ttx_page.pgno,
			    vbi_export_errstr (ex));
	}

	vbi_unref_page (&page);

	if (-1 == pgno) {
		if (0 != fclose (fp)) {
			error_exit (_("Write error: %s."),
				    strerror (errno));
		}
	} else {
		quit = TRUE;
	}

	if (!option_quiet) {
		fprintf (stderr,
			 _("Saved page %x.\n"),
			 ev->ev.ttx_page.pgno);
	}
}

static vbi_bool
decode_frame			(const vbi_sliced *	sliced,
				 unsigned int		n_lines,
				 double			sample_time,
				 int64_t		stream_time)
{
	stream_time = stream_time;

	vbi_decode (vbi, sliced, n_lines, sample_time);

	return !quit;
}

static vbi_export *
export_new			(const char *		module)
{
	vbi_export *ex;
	char *errmsg;

	ex = vbi_export_new (module, &errmsg);
	if (NULL == ex) {
		error_exit (_("Cannot open export module '%s': %s."),
			    module, errmsg);
		/* Attn: free (errmsg) if you don't exit here. */
	}

	return ex;
}

static void
list_options			(const vbi_export *	ex)
{
	vbi_option_info *oi;
	unsigned int i;

	for (i = 0; (oi = vbi_export_option_info_enum (ex, i)); ++i) {
		printf ("  Option '%s' - %s\n",
			oi->keyword, _(oi->tooltip));
	}
}

static void
list_modules			(void)
{
	const vbi_export_info *xi;
	unsigned int i;

	for (i = 0; (xi = vbi_export_info_enum (i)); ++i) {
		vbi_export *ex;

		printf ("'%s' - %s\n",
			_(xi->keyword),
			_(xi->tooltip));

		ex = export_new (xi->keyword);
		list_options (ex);
		vbi_export_delete (ex);
		ex = NULL;
	}
}

static void
usage				(FILE *			fp)
{
	fprintf (fp, "\
%s %s -- Export Teletext pages in various formats\n\n\
Copyright (C) 2000, 2001, 2007 Michael H. Schimek\n\
This program is licensed under GPLv2 or later. NO WARRANTIES.\n\n\
Usage: %s module[;options] page_number < sliced VBI data > file\n\n\
-h | --help | --usage  Print this message and exit\n\
-q | --quiet           Suppress progress and error messages\n\
-P | --pes             Source is a DVB PES stream\n\
-T | --ts pid          Source is a DVB TS stream\n\
-V | --version         Print the program version and exit\n\
",
		 PROGRAM_NAME, VERSION, program_invocation_name);
}

static const char
short_options [] = "1hqPT:V";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "all-pages",  no_argument,		NULL,		'1' },
	{ "help",	no_argument,		NULL,		'h' },
	{ "usage",	no_argument,		NULL,		'h' },
	{ "quiet",	no_argument,		NULL,		'q' },
	{ "pes",	no_argument,		NULL,		'P' },
	{ "ts",		required_argument,	NULL,		'T' },
	{ "version",	no_argument,		NULL,		'V' },
	{ NULL, 0, 0, 0 }
};
#else
#  define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static int			option_index;

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

int
main				(int			argc,
				 char **		argv)
{
	enum file_format file_format;
	const vbi_export_info *xi;
	struct stream *st;
	char *errmsg;
	const char *module;
	vbi_bool success;
	vbi_bool all_pages;
	int c;

	init_helpers (argc, argv);

	file_format = FILE_FORMAT_SLICED;
	all_pages = FALSE;

	while (-1 != (c = getopt_long (argc, argv, short_options,
				       long_options, &option_index))) {
		switch (c) {
		case 0: /* getopt_long() flag */
			break;

		case '1':
			/* Compatibility. */
			all_pages = TRUE;
			break;

		case 'h':
			usage (stdout);
			exit (EXIT_SUCCESS);

		case 'l':
			if (0)
				list_modules ();
			exit (EXIT_SUCCESS);

		case 'q':
			option_quiet ^= TRUE;
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

	if (argc - optind < (all_pages ? 1 : 2)) {
		usage (stderr);
		exit (EXIT_FAILURE);
	}

	module = argv[optind++];

	if (all_pages) {
		pgno = -1;
	} else {
		pgno = strtol (argv[optind++], NULL, 16);
	}

	if (-1 != pgno && !is_valid_pgno (pgno))
		invalid_pgno_exit (argv[2]);

	ex = export_new (module);

	xi = vbi_export_info_export (ex);
	if (NULL == xi)
		no_mem_exit ();

	extension = strdup (xi->extension);
	if (NULL == extension)
		no_mem_exit ();

	extension = strtok_r (extension, ",", &errmsg);

	vbi = vbi_decoder_new ();
	if (NULL == vbi)
		no_mem_exit ();

	success = vbi_event_handler_add (vbi, VBI_EVENT_TTX_PAGE,
					 event_handler, NULL);
	if (!success)
		no_mem_exit ();

	cr = isatty (STDERR_FILENO) ? '\r' : '\n';

	st = read_stream_new (file_format, decode_frame);

	read_stream_loop (st);

	read_stream_delete (st);

	exit (EXIT_SUCCESS);

	return 0;
}
