/*
 *  libzvbi raw VBI output example.
 *
 *  Copyright (C) 2006 Michael H. Schimek
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

/* $Id: rawout.c,v 1.1 2006/05/22 09:06:38 mschimek Exp $ */

/* This example shows how to convert VBI data in a DVB PES to raw
   VBI data.

   gcc -o rawout rawout.c `pkg-config zvbi-0.2 --cflags --libs`

   ./rawout <pes | mplayer - -rawvideo on:w=720:h=34:format=0x32595559 */

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libzvbi.h>

static vbi_dvb_demux *		dvb;
static uint8_t			pes_buffer[2048];
static vbi_sampling_par		sp;
static uint8_t *		image;
static unsigned int		image_size;
static unsigned int		pixel_mask;

static void
convert				(const vbi_sliced *	sliced,
				 unsigned int		n_lines)
{
	vbi_bool success;
	ssize_t actual;

	success = vbi_raw_video_image (image, image_size, &sp,
				       /* black_level: default */ 0,
				       /* white_level: default */ 0,
				       pixel_mask,
				       /* swap_fields */ FALSE,
				       sliced, n_lines);
	assert (success);

	actual = write (STDOUT_FILENO, image, image_size);
	assert (actual == (ssize_t) image_size);
}

static void
pes_mainloop			(void)
{
	while (1 == fread (pes_buffer, sizeof (pes_buffer), 1, stdin)) {
		const uint8_t *bp;
		unsigned int left;

		bp = pes_buffer;
		left = sizeof (pes_buffer);

		while (left > 0) {
			vbi_sliced sliced[64];
			unsigned int n_lines;
			int64_t pts;

			n_lines = vbi_dvb_demux_cor (dvb, sliced, 64,
						     &pts, &bp, &left);
			convert (sliced, n_lines);
		}
	}

	fprintf (stderr, "End of stream.\n");
}

int
main				(void)
{
	if (isatty (STDIN_FILENO)) {
		fprintf (stderr, "No DVB PES on standard input.\n");
		exit (EXIT_FAILURE);
	}

	if (isatty (STDOUT_FILENO)) {
		fprintf (stderr, "Output is binary image data. Pipe to "
			 "another tool or redirect to a file.\n");
		exit (EXIT_FAILURE);
	}

	dvb = vbi_dvb_pes_demux_new (/* callback */ NULL,
				     /* user_data */ NULL);
	assert (NULL != dvb);

	memset (&sp, 0, sizeof (sp));

#if 1
	/* ITU BT.601 YUYV. */

	sp.scanning		= 625; /* PAL/SECAM */
	sp.sampling_format	= VBI_PIXFMT_YUYV;
	sp.sampling_rate	= 13.5e6;
	sp.bytes_per_line	= 720 * 2; /* 2 bpp */
	sp.offset		= 9.5e-6 * 13.5e6;
	sp.start[0]		= 6;
	sp.count[0]		= 17;
	sp.start[1]		= 319;
	sp.count[1]		= 17;
	sp.interlaced		= TRUE;
	sp.synchronous		= TRUE;

	pixel_mask		= 0x000000FF; /* 0xAAVVUUYY */
#else
	/* PAL square pixels BGRA32. */

	sp.scanning		= 625; /* PAL/SECAM */
	sp.sampling_format	= VBI_PIXFMT_BGRA32_LE;
	sp.sampling_rate	= 14.75e6;
	sp.bytes_per_line	= 768 * 4; /* 4 bpp */
	sp.offset		= 10.2e-6 * 14.75e6;
	sp.start[0]		= 6;
	sp.count[0]		= 17;
	sp.start[1]		= 319;
	sp.count[1]		= 17;
	sp.interlaced		= TRUE;
	sp.synchronous		= TRUE;

	pixel_mask		= 0x00FFFFFF; /* 0xAABBGGRR */
#endif

	image_size = (sp.count[0] + sp.count[1]) * sp.bytes_per_line;
	image = malloc (image_size);
	assert (NULL != image);

	if (VBI_PIXFMT_YUYV == sp.sampling_format) {
		/* Reset U/V bytes. */
		memset (image, 0x80, image_size);
	} else {
		memset (image, 0x00, image_size);
	}

	pes_mainloop ();

	vbi_dvb_demux_delete (dvb);

	exit (EXIT_SUCCESS);

	return 0;
}
