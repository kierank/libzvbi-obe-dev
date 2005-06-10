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

/* $Id: sliced.c,v 1.1 2005/06/10 07:41:14 mschimek Exp $ */

#undef NDEBUG

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "src/libzvbi.h"
#include "sliced.h"

/* Reader for the old ad-hoc sliced VBI file format.
   ATTN this code is not reentrant. */

static FILE *			file;
static double			elapsed;

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

	if (ferror (file))
		goto read_error;

	if (feof (file) || !fgets (buf, 255, file))
		goto eof;

	/* Time in seconds since last frame. */
	dt = strtod (buf, NULL);

	*timestamp = elapsed;
	elapsed += dt;

	n_lines = fgetc (file);

	if (n_lines < 0)
		goto read_error;

	assert ((unsigned int) n_lines <= max_lines);

	s = sliced;

	for (n = n_lines; n > 0; --n) {
		int index;

		index = fgetc (file);
		if (index < 0)
			goto eof;

		s->line = (fgetc (file) + 256 * fgetc (file)) & 0xFFF;

		if (feof (file) || ferror (file))
			goto read_error;

		switch (index) {
		case 0:
			s->id = VBI_SLICED_TELETEXT_B;
			fread (s->data, 1, 42, file);
			break;

		case 1:
			s->id = VBI_SLICED_CAPTION_625; 
			fread (s->data, 1, 2, file);
			break; 

		case 2:
			s->id = VBI_SLICED_VPS;
			fread (s->data, 1, 13, file);
			break;

		case 3:
			s->id = VBI_SLICED_WSS_625; 
			fread (s->data, 1, 2, file);
			break;

		case 4:
			s->id = VBI_SLICED_WSS_CPR1204; 
			fread (s->data, 1, 3, file);
			break;

		case 7:
			s->id = VBI_SLICED_CAPTION_525; 
			fread(s->data, 1, 2, file);
			break;

		default:
			fprintf (stderr,
				 "\nOops! Unknown data type %d "
				 "in sliced VBI file\n", index);
			exit (EXIT_FAILURE);
		}

		if (ferror (file))
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
open_sliced			(FILE *			fp)
{
	assert (NULL != fp);

	file = fp;

	elapsed = 0.0;

	return TRUE;
}
