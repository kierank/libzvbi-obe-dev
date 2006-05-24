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

/* $Id: rawout.c,v 1.2 2006/05/24 04:47:21 mschimek Exp $ */

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
static int64_t			last_pts;

static vbi_bool
convert				(vbi_dvb_demux *	dx,
				 void *			user_data,
				 const vbi_sliced *	sliced,
				 unsigned int		n_lines,
				 int64_t		pts)
{
	vbi_bool success;
	ssize_t actual;

	dx = dx; /* unused, no warning */
	user_data = user_data;

	pts &= ((int64_t) 1 << 33) - 1;

	if (0 == last_pts) {
		last_pts = pts;
	} else if (pts < last_pts) {
		last_pts -= (int64_t) 1 << 33;
	}

	while (pts - last_pts > 90000 / 25) {
		/* No data for this frame. */

		success = vbi_raw_video_image (image, image_size, &sp,
					       0, 0, 0, pixel_mask, FALSE,
					       NULL, /* n_lines */ 0);
		assert (success);

		actual = write (STDOUT_FILENO, image, image_size);
		assert (actual == (ssize_t) image_size);

		last_pts += 90000 / 25;
	}

	success = vbi_raw_video_image (image, image_size, &sp,
				       /* blank_level: default */ 0,
				       /* black_level: default */ 0,
				       /* white_level: default */ 0,
				       pixel_mask,
				       /* swap_fields */ FALSE,
				       sliced, n_lines);
	assert (success);

	actual = write (STDOUT_FILENO, image, image_size);
	assert (actual == (ssize_t) image_size);

	last_pts = pts;

	return TRUE; /* success */
}

static void
pes_mainloop			(void)
{
	while (1 == fread (pes_buffer, sizeof (pes_buffer), 1, stdin)) {
		vbi_bool success;

		success = vbi_dvb_demux_feed (dvb,
					      pes_buffer,
					      sizeof (pes_buffer));
		assert (success);
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

	/* Helps debugging. */
	vbi_set_log_fn (/* mask */ VBI_LOG_WARNING * 2 - 1,
			vbi_log_on_stderr,
			/* user_data */ NULL);

	dvb = vbi_dvb_pes_demux_new (convert, /* user_data */ NULL);
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
