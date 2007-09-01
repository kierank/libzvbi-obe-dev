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

/* $Id: sliced.c,v 1.9 2007/09/01 15:06:55 mschimek Exp $ */

/* For libzvbi version 0.2.x / 0.3.x. */

#undef NDEBUG

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "sliced.h"

/* Writer for old test/capture --sliced output.
   Attn: this code is not reentrant. */

#define N_ELEMENTS(array) (sizeof (array) / sizeof (*(array)))

static FILE *			write_file;
static double			write_last_time;

struct service {
	const char *		name;
	vbi_service_set		id;
	unsigned int		n_bytes;
};

static const struct service
service_map [] = {
	{ "TELETEXT_B",		VBI_SLICED_TELETEXT_B,			42 },
	{ "CAPTION_625",	VBI_SLICED_CAPTION_625,			2 },
	{ "VPS",		VBI_SLICED_VPS | VBI_SLICED_VPS_F2,	13 },
	{ "WSS_625",		VBI_SLICED_WSS_625,			2 },
	{ "WSS_CPR1204",	VBI_SLICED_WSS_CPR1204,			3 },
	{ NULL, 0, 0 },
	{ NULL, 0, 0 },
	{ "CAPTION_525",	VBI_SLICED_CAPTION_525,			2 },
};

static const uint8_t
base64 [] = ("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	     "abcdefghijklmnopqrstuvwxyz"
	     "0123456789+/");

static void
encode_base64			(uint8_t *		out,
				 const uint8_t *	in,
				 unsigned int		n_bytes)
{
	unsigned int block;

	for (; n_bytes >= 3; n_bytes -= 3) {
		block = in[0] * 65536 + in[1] * 256 + in[2];
		in += 3;
		out[0] = base64[block >> 18];
		out[1] = base64[(block >> 12) & 0x3F];
		out[2] = base64[(block >> 6) & 0x3F];
		out[3] = base64[block & 0x3F];
		out += 4;
	}

	switch (n_bytes) {
	case 2:
		block = in[0] * 256 + in[1];
		out[0] = base64[block >> 10];
		out[1] = base64[(block >> 4) & 0x3F];
		out[2] = base64[(block << 2) & 0x3F];
		out[3] = '=';
		out += 4;
		break;

	case 1:
		block = in[0];
		out[0] = base64[block >> 2];
		out[1] = base64[(block << 4) & 0x3F];
		out[2] = '=';
		out[3] = '=';
		out += 4;
		break;
	}

	*out = 0;
}

vbi_bool
write_sliced_xml		(vbi_sliced *		sliced,
				 unsigned int		n_lines,
				 unsigned int		n_frame_lines,
				 int64_t		stream_time,
				 struct timeval		capture_time)
{
	assert (NULL != sliced);
	assert (525 == n_frame_lines || 625 == n_frame_lines);
	assert (n_lines <= n_frame_lines);
	assert (stream_time >= 0);
	assert (capture_time.tv_sec >= 0);
	assert (capture_time.tv_usec >= 0);
	assert (capture_time.tv_usec < 1000000);

	fprintf (write_file,
		 "<frame system=\"%u\" stream_time=\"%" PRId64 "\" "
		 "capture_time=\"%" PRId64 ".%06u\">\n",
		 n_frame_lines, stream_time,
		 (int64_t) capture_time.tv_sec,
		 (unsigned int) capture_time.tv_usec);

	for (; n_lines > 0; ++sliced, --n_lines) {
		uint8_t data_base64[((sizeof (sliced->data) + 2) / 3)
				    * 4 + 1];
		unsigned int i;

		for (i = 0; i < N_ELEMENTS (service_map); ++i) {
			if (sliced->id & service_map[i].id)
				break;
		}

		if (i >= N_ELEMENTS (service_map))
			continue;

		assert (service_map[i].n_bytes <= sizeof (sliced->data));

		encode_base64 (data_base64, sliced->data,
			       service_map[i].n_bytes);

		fprintf (write_file,
			 "<vbi-data service=\"%s\" line=\"%u\">%s</line>\n",
			 service_map[i].name, sliced->line,
			 data_base64);

		write_last_time = capture_time.tv_sec
			+ capture_time.tv_usec * (1 / 1e6);
	}

	fputs ("</frame>\n", write_file);

	return !ferror (write_file);
}

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
			if (sliced->id & service_map[i].id) {
				fprintf (write_file, "%c%c%c",
					 /* service number */ i,
					 /* line number low/high */
					 sliced->line & 0xFF,
					 sliced->line >> 8);

				fwrite (sliced->data, 1,
					service_map[i].n_bytes,
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

/* Misc. helper functions. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

#include "src/dvb_demux.h"

#undef _
#define _(x) x /* later */

typedef vbi_bool
read_loop_fn			(struct stream *	st);

struct stream {
	uint8_t			buffer[4096];

	vbi_sliced		sliced[64];

	const uint8_t *		bp;
	const uint8_t *		end;

	stream_callback_fn *	callback;

	read_loop_fn *		read_loop;

	vbi_dvb_demux *		dx;

	double			sample_time;
	int64_t			stream_time;

	int			fd;
};

#ifndef HAVE_PROGRAM_INVOCATION_NAME
char *				program_invocation_name;
char *				program_invocation_short_name;
#endif

vbi_bool			option_quiet;
unsigned long			option_ts_pid;
unsigned int			option_log_mask;

void
vprint_error			(const char *		template,
				 va_list		ap)
{
	if (option_quiet)
		return;

	fprintf (stderr, "%s: ", program_invocation_short_name);

	vfprintf (stderr, template, ap);

	fputc ('\n', stderr);
}

void
error_msg			(const char *		template,
				 ...)
{
	va_list ap;

	va_start (ap, template);
	vprint_error (template, ap);
	va_end (ap);
}

void
error_exit			(const char *		template,
				 ...)
{
	va_list ap;

	va_start (ap, template);
	vprint_error (template, ap);
	va_end (ap);

	exit (EXIT_FAILURE);
}

void
write_error_exit		(const char *		msg)
{
	if (NULL == msg)
		msg = strerror (errno);

	error_exit (_("Write error: %s."), msg);
}

void
no_mem_exit			(void)
{
	error_exit (_("Out of memory."));
}

static void
premature_exit			(void)
{
	error_exit (_("Premature end of input file."));
}

static void
bad_format_exit			(void)
{
	error_exit (_("Invalid data in input file."));
}

static vbi_bool
read_more			(struct stream *	st)
{
	unsigned int retry;
	uint8_t *s;
	uint8_t *e;

	s = /* const cast */ st->end;
	e = st->buffer + sizeof (st->buffer);

	if (s >= e)
		s = st->buffer;

	retry = 100;

        do {
                ssize_t actual;
		int saved_errno;

                actual = read (st->fd, s, e - s);
                if (0 == actual)
			return FALSE; /* EOF */

		if (actual > 0) {
			st->bp = s;
			st->end = s + actual;
			return TRUE;
		}

		saved_errno = errno;

		if (EINTR != saved_errno) {
			error_exit (_("Read error: %s."),
				    strerror (errno));
		}
        } while (--retry > 0);

	error_exit (_("Read error."));

	return FALSE;
}

static vbi_bool
pes_ts_read_loop		(struct stream *	st)
{
	for (;;) {
		double sample_time;
		int64_t pts;
		unsigned int left;
		unsigned int n_lines;

		if (st->bp >= st->end) {
			if (!read_more (st))
				break; /* EOF */
		}

		left = st->end - st->bp;

		n_lines = vbi_dvb_demux_cor (st->dx,
					     st->sliced,
					     N_ELEMENTS (st->sliced),
					     &pts,
					     &st->bp,
					     &left);

		if (0 == n_lines)
			continue;

		if (pts < 0) {
			/* XXX WTF? */
			continue;
		}

		sample_time = pts * (1 / 90000.0);

		if (!st->callback (st->sliced, n_lines,
				   sample_time, pts))
			return FALSE;
	}

	return TRUE;
}

static vbi_bool
next_byte			(struct stream *	st,
				 int *			c)
{
	do {
		if (st->bp < st->end) {
			*c = *st->bp++;
			return TRUE;
		}
	} while (read_more (st));

	return FALSE; /* EOF */
}

static void
next_block			(struct stream *	st,
				 uint8_t *		buffer,
				 unsigned int		buffer_size)
{
	do {
		unsigned int available;

		available = st->end - st->bp;

		if (buffer_size <= available) {
			memcpy (buffer, st->bp, buffer_size);
			st->bp += buffer_size;
			return;
		}

		memcpy (buffer, st->bp, available);

		st->bp += available;

		buffer += available;
		buffer_size -= available;

	} while (read_more (st));

	premature_exit ();
}

static vbi_bool
next_time_delta			(struct stream *	st,
				 double *		dt)
{
	char buffer[32];
	unsigned int i;

	for (i = 0; i < N_ELEMENTS (buffer); ++i) {
		int c;

		if (!next_byte (st, &c)) {
			if (i > 0)
				premature_exit ();
			else
				return FALSE;
		}

		if ('\n' == c) {
			if (0 == i) {
				bad_format_exit ();
			} else {
				buffer[i] = 0;
				*dt = strtod (buffer, NULL);
				return TRUE;
			}
		}

		if ('-' != c && '.' != c && !isdigit (c))
			bad_format_exit ();

		buffer[i] = c;
	}

	return FALSE;
}

static vbi_bool
old_sliced_read_loop		(struct stream *	st)
{
	for (;;) {
		vbi_sliced *s;
		double sample_time;
		double dt;
		int64_t stream_time;
		int n_lines;
		int count;

		if (!next_time_delta (st, &dt))
			break; /* EOF */

		/* Time in seconds since last frame. */
		if (dt < 0.0)
			dt = -dt;

		sample_time = st->sample_time;
		st->sample_time += dt;

		if (!next_byte (st, &n_lines))
			bad_format_exit ();

		if ((unsigned int) n_lines > N_ELEMENTS (st->sliced))
			bad_format_exit ();

		s = st->sliced;

		for (count = n_lines; count > 0; --count) {
			int index;
			int line;

			if (!next_byte (st, &index))
				premature_exit ();

			if (!next_byte (st, &line))
				premature_exit ();
			s->line = line;

			if (!next_byte (st, &line))
				premature_exit ();
			s->line += (line & 15) * 256;

			switch (index) {
			case 0:
				s->id = VBI_SLICED_TELETEXT_B;
				next_block (st, s->data, 42);
				break;

			case 1:
				s->id = VBI_SLICED_CAPTION_625;
				next_block (st, s->data, 2);
				break; 

			case 2:
				s->id = VBI_SLICED_VPS;
				next_block (st, s->data, 13);
				break;

			case 3:
				s->id = VBI_SLICED_WSS_625;
				next_block (st, s->data, 2);
				break;

			case 4:
				s->id = VBI_SLICED_WSS_CPR1204;
				next_block (st, s->data, 3);
				break;

			case 7:
				s->id = VBI_SLICED_CAPTION_525;
				next_block (st, s->data, 2);
				break;

			default:
				bad_format_exit ();
				break;
			}

			++s;
		}

		stream_time = sample_time * 90000;

		if (!st->callback (st->sliced, n_lines,
				   sample_time, stream_time))
			return FALSE;
	}

	return TRUE;
}

vbi_bool
read_stream_loop		(struct stream *	st)
{
	return st->read_loop (st);
}

static vbi_bool
look_ahead			(struct stream *	st,
				 unsigned int		n_bytes)
{
	assert (n_bytes <= sizeof (st->buffer));

	do {
		unsigned int available;
		const uint8_t *end;

		available = st->end - st->bp;
		if (available >= n_bytes)
			return TRUE;

		end = st->buffer + sizeof (st->buffer);

		if (n_bytes > (unsigned int)(end - st->bp)) {
			memmove (st->buffer, st->bp, available);

			st->bp = st->buffer;
			st->end = st->buffer + available;
		}
	} while (read_more (st));

	return FALSE; /* EOF */
}

static vbi_bool
is_old_sliced_format		(const uint8_t		s[8])
{
	unsigned int i;

	if ('0' != s[0] || '.' != s[1])
		return FALSE;

	for (i = 2; i < 8; ++i) {
		if (!isdigit (s[i]))
			return FALSE;
	}

	return TRUE;
}

static vbi_bool
is_xml_format			(const uint8_t		s[6])
{
	unsigned int i;

	if ('<' != s[0])
		return FALSE;

	for (i = 1; i < 6; ++i) {
		if (!isalpha (s[i]))
			return FALSE;
	}

	return TRUE;
}

static vbi_bool
is_pes_format			(const uint8_t		s[4])
{
	return (0x00 == s[0] &&
		0x00 == s[1] &&
		0x01 == s[2] &&
		0xBD == s[3]);
}

static vbi_bool
is_ts_format			(const uint8_t		s[1])
{
	return (0x47 == s[0]);
}

static enum file_format
detect_file_format		(struct stream *	st)
{
	if (!look_ahead (st, 8))
		return 0; /* unknown format */

	if (is_old_sliced_format (st->bp))
		return FILE_FORMAT_SLICED;

	if (is_xml_format (st->buffer))
		return FILE_FORMAT_XML;

	/* Can/shall we guess a PID? */
	if (0) {
		/* Somewhat unreliable and works only if the
		   packets are aligned. */
		if (is_ts_format (st->buffer))
			return FILE_FORMAT_DVB_TS;
	}

	/* Works only if the packets are aligned. */
	if (is_pes_format (st->buffer))
		return FILE_FORMAT_DVB_PES;

	return 0; /* unknown format */
}

void
read_stream_delete		(struct stream *	st)
{
	if (NULL == st)
		return;

	CLEAR (*st);

	free (st);
}

struct stream *
read_stream_new			(enum file_format	file_format,
				 stream_callback_fn *	callback)
{
	struct stream *st;

	if (isatty (STDIN_FILENO))
		error_exit (_("No VBI data on standard input."));

	st = malloc (sizeof (*st));
	if (NULL == st)
		no_mem_exit ();

	st->fd = STDIN_FILENO;

	if (0 == file_format)
		file_format = detect_file_format (st);

	switch (file_format) {
	case FILE_FORMAT_SLICED:
		st->read_loop = old_sliced_read_loop;
		break;

	case FILE_FORMAT_XML:
		st->read_loop = NULL;
		error_exit ("XML read function "
			    "not implemented yet.");
		break;

	case FILE_FORMAT_DVB_PES:
		st->read_loop = pes_ts_read_loop;

		st->dx = vbi_dvb_pes_demux_new (/* callback */ NULL,
						/* user_data */ NULL);
		if (NULL == st->dx)
			no_mem_exit ();

		vbi_dvb_demux_set_log_fn (st->dx,
					  option_log_mask,
					  vbi_log_on_stderr,
					  /* user_data */ NULL);
		break;

	case FILE_FORMAT_DVB_TS:
		st->read_loop = pes_ts_read_loop;

		st->dx = _vbi_dvb_ts_demux_new (/* callback */ NULL,
						/* user_data */ NULL,
						option_ts_pid);
		if (NULL == st->dx)
			no_mem_exit ();

		vbi_dvb_demux_set_log_fn (st->dx,
					  option_log_mask,
					  vbi_log_on_stderr,
					  /* user_data */ NULL);
		break;

	default:
		error_exit (_("Unknown input file format."));
		break;
	}

	st->callback		= callback;

	st->sample_time		= 0.0;
	st->stream_time		= 0;

	st->bp			= st->buffer;
	st->end			= st->buffer;

	return st;
}

void
parse_option_ts			(void)
{
	const char *s = optarg;
	char *end;

	assert (NULL != optarg);

	option_ts_pid = strtoul (s, &end, 0);

	if (option_ts_pid <= 0x000F ||
	    option_ts_pid >= 0x1FFF) {
		error_exit (_("Invalid PID %u."),
			    option_ts_pid);
	}
}

void
init_helpers			(int			argc,
				 char **		argv)
{
	argc = argc;
	argv = argv;

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
}
