/*
 *  libzvbi test
 *
 *  Copyright (C) 2000, 2001 Michael H. Schimek
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

/* $Id: export.c,v 1.7 2004/10/25 16:56:30 mschimek Exp $ */

#undef NDEBUG

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "src/libzvbi.h"

vbi_decoder *		vbi;
vbi_bool		quit = FALSE;
vbi_pgno		pgno;
vbi_export *		ex;
char *			extension;

static void
handler(vbi_event *ev, void *unused)
{
	FILE *fp;
	vbi_page page;

	fprintf(stderr, "\rPage %03x.%02x ",
		ev->ev.ttx_page.pgno,
		ev->ev.ttx_page.subno & 0xFF);

	if (pgno != -1 && ev->ev.ttx_page.pgno != pgno)
		return;

	fprintf(stderr, "\nSaving... ");
	if (isatty(STDERR_FILENO))
		fputc('\n', stderr);
	fflush(stderr);

	/* Fetching & exporting here is a bad idea,
	   but this is only a test. */
	assert(vbi_fetch_vt_page(vbi, &page,
				 ev->ev.ttx_page.pgno,
				 ev->ev.ttx_page.subno,
				 VBI_WST_LEVEL_3p5, 25, TRUE));

	/* Just for fun */
	if (pgno == -1) {
		char name[256];
		
		snprintf(name, sizeof(name) - 1, "test-%03x-%02x.%s",
			 ev->ev.ttx_page.pgno,
			 ev->ev.ttx_page.subno,
			 extension);

		assert((fp = fopen(name, "w")));
	} else
		fp = stdout;

	if (!vbi_export_stdio(ex, fp, &page)) {
		fprintf(stderr, "failed: %s\n", vbi_export_errstr(ex));
		exit(EXIT_FAILURE);
	} else {
		fprintf(stderr, "done\n");
	}

	vbi_unref_page(&page);

	if (pgno == -1)
		assert(fclose(fp) == 0);
	else
		quit = TRUE;
}

static void
stream(void)
{
	char buf[256];
	double time = 0.0, dt;
	int index, items, i;
	vbi_sliced *s, sliced[40];

	while (!quit) {
		if (ferror(stdin) || !fgets(buf, 255, stdin))
			goto abort;

		dt = strtod(buf, NULL);
		items = fgetc(stdin);

		assert(items < 40);

		for (s = sliced, i = 0; i < items; s++, i++) {
			index = fgetc(stdin);
			s->line = (fgetc(stdin) + 256 * fgetc(stdin)) & 0xFFF;

			if (index < 0)
				goto abort;

			switch (index) {
			case 0:
				s->id = VBI_SLICED_TELETEXT_B;
				fread(s->data, 1, 42, stdin);
				break;
			case 1:
				s->id = VBI_SLICED_CAPTION_625; 
				fread(s->data, 1, 2, stdin);
				break; 
			case 2:
				s->id = VBI_SLICED_VPS; 
				fread(s->data, 1, 13, stdin);
				break;
			case 3:
				s->id = VBI_SLICED_WSS_625; 
				fread(s->data, 1, 2, stdin);
				break;
			case 4:
				s->id = VBI_SLICED_WSS_CPR1204; 
				fread(s->data, 1, 3, stdin);
				break;
			case 7:
				s->id = VBI_SLICED_CAPTION_525; 
				fread(s->data, 1, 2, stdin);
				break;
			default:
				fprintf(stderr, "\nOops! Unknown data %d "
					"in sample file\n", index);
				exit(EXIT_FAILURE);
			}
		}

		if (feof(stdin) || ferror(stdin))
			goto abort;

		vbi_decode(vbi, sliced, items, time);

		time += dt;
	}

	return;

abort:
	fprintf(stderr, "\rEnd of stream, page %03x not found\n", pgno);
}

int
main(int argc, char **argv)
{
	char *module, *t;
	vbi_export_info *xi;

	if (argc < 3) {
		fprintf(stderr, "Usage: %s module[;options] pgno <vbi data >file\n"
				"module eg. \"ppm\", pgno eg. 100 (hex)\n",
			argv[0]);
		exit(EXIT_FAILURE);
	}

	if (isatty(STDIN_FILENO)) {
		fprintf(stderr, "No vbi data on stdin\n");
		exit(EXIT_FAILURE);
	}


	module = argv[1];
	pgno = strtol(argv[2], NULL, 16);

	if (!(ex = vbi_export_new(module, &t))) {
		fprintf(stderr, "Failed to open export module '%s': %s\n",
			module, t);
		exit(EXIT_FAILURE);
	}


	assert((xi = vbi_export_info_export(ex)));
	assert((extension = strdup(xi->extension)));
	extension = strtok_r(extension, ",", &t);

	assert((vbi = vbi_decoder_new()));

vbi_teletext_set_default_region(vbi,48);

	assert(vbi_event_handler_add(vbi, VBI_EVENT_TTX_PAGE, handler, NULL)); 

	stream();

	exit(EXIT_SUCCESS);
}
