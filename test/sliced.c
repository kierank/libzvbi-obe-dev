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

/* $Id: sliced.c,v 1.5 2006/05/22 08:55:22 mschimek Exp $ */

#undef NDEBUG

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "sliced.h"

/* Writer for old test/capture --sliced output.
   Attn: this code is not reentrant. */

#define N_ELEMENTS(array) (sizeof (array) / sizeof (*(array)))

static FILE *			write_file;
static double			write_last_time;

/* Hysterical compatibility. */
static const int
service_map [][2] = {
	{ VBI_SLICED_TELETEXT_B,			42 },
	{ VBI_SLICED_CAPTION_625,			2 },
	{ VBI_SLICED_VPS | VBI_SLICED_VPS_F2,		13 },
	{ VBI_SLICED_WSS_625,				2 },
	{ VBI_SLICED_WSS_CPR1204,			3 },
	{ 0, 0 },
	{ 0, 0 },
	{ VBI_SLICED_CAPTION_525,			2 }
};

vbi_bool
write_sliced			(vbi_sliced *		sliced,
				 unsigned int		n_lines,
				 double			timestamp)
{
	fprintf (write_file, "%f\n%c",
		 timestamp - write_last_time, n_lines);

	while (n_lines > 0) {
		unsigned int i;

		for (i = 0; i < N_ELEMENTS (service_map); ++i) {
			if (sliced->id & service_map[i][0]) {
				fprintf (write_file, "%c%c%c",
					 /* service number */ i,
					 /* line number low/high */
					 sliced->line & 0xFF,
					 sliced->line >> 8);

				fwrite (sliced->data, 1,
					/* n_bytes */ service_map[i][1],
					write_file);

				if (ferror (write_file))
					return FALSE;

				write_last_time = timestamp;

				break;
			}
		}

		++sliced;
		--n_lines;
	}

	return TRUE;
}

vbi_bool
open_sliced_write		(FILE *			fp,
				 double			timestamp)
{
	assert (NULL != fp);

	write_file = fp;

	write_last_time = timestamp;

	return TRUE;
}

/* Reader for old test/capture --sliced output.
   ATTN this code is not reentrant. */

static FILE *			read_file;
static double			read_elapsed;

int
read_sliced			(vbi_sliced *		sliced,
				 double *		timestamp,
				 unsigned int		max_lines)
{
	char buf[256];
	double dt;
	int n_lines;
	int n;
	vbi_sliced *s;

	assert (NULL != sliced);
	assert (NULL != timestamp);

	if (ferror (read_file))
		goto read_error;

	if (feof (read_file) || !fgets (buf, 255, read_file))
		goto eof;

	/* Time in seconds since last frame. */
	dt = strtod (buf, NULL);
	if (dt < 0.0) {
		dt = -dt;
	}

	*timestamp = read_elapsed;
	read_elapsed += dt;

	n_lines = fgetc (read_file);

	if (n_lines < 0)
		goto read_error;

	assert ((unsigned int) n_lines <= max_lines);

	s = sliced;

	for (n = n_lines; n > 0; --n) {
		int index;

		index = fgetc (read_file);
		if (index < 0)
			goto eof;

		s->line = (fgetc (read_file)
			   + 256 * fgetc (read_file)) & 0xFFF;

		if (feof (read_file) || ferror (read_file))
			goto read_error;

		switch (index) {
		case 0:
			s->id = VBI_SLICED_TELETEXT_B;
			fread (s->data, 1, 42, read_file);
			break;

		case 1:
			s->id = VBI_SLICED_CAPTION_625; 
			fread (s->data, 1, 2, read_file);
			break; 

		case 2:
			s->id = VBI_SLICED_VPS;
			fread (s->data, 1, 13, read_file);
			break;

		case 3:
			s->id = VBI_SLICED_WSS_625; 
			fread (s->data, 1, 2, read_file);
			break;

		case 4:
			s->id = VBI_SLICED_WSS_CPR1204; 
			fread (s->data, 1, 3, read_file);
			break;

		case 7:
			s->id = VBI_SLICED_CAPTION_525; 
			fread(s->data, 1, 2, read_file);
			break;

		default:
			fprintf (stderr,
				 "\nOops! Unknown data type %d "
				 "in sliced VBI file\n", index);
			exit (EXIT_FAILURE);
		}

		if (ferror (read_file))
			goto read_error;

		++s;
	}

	return n_lines;

 eof:
	return -1;

 read_error:
	perror ("Read error in sliced VBI file");
	exit (EXIT_FAILURE);
}

vbi_bool
open_sliced_read		(FILE *			fp)
{
	assert (NULL != fp);

	read_file = fp;

	read_elapsed = 0.0;

	return TRUE;
}
