/*
 *  libzvbi
 *
 *  Copyright (C) 2004 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: dvb_demux.c,v 1.3 2004/12/13 07:17:07 mschimek Exp $ */

#include <stdio.h>		/* fprintf() */
#include <stdlib.h>
#include <string.h>		/* memcpy(), memmove(), memset() */
#include <assert.h>
#include "dvb.h"
#include "dvb_demux.h"
#include "hamm.h"		/* vbi_rev8() */
#include "misc.h"		/* CLEAR() */

#ifndef DVB_DEMUX_LOG
#  define DVB_DEMUX_LOG 0
#endif

#define log(templ, args...)						\
do {									\
	if (DVB_DEMUX_LOG)						\
		fprintf (stderr, "%s: " templ, __FUNCTION__ , ##args);	\
} while (0)

#define vbi_inline static __inline__
#define vbi_rev8(n) vbi_bit_reverse[n]

#define printable(c) \
	((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))

struct wrap {
	/* Size must be >= maximum consume + maximum lookahead. */
	uint8_t	*		buffer;

	/* End of data in buffer. */
	uint8_t *		bp;

	/* See below. */
	unsigned int		skip;
     /* unsigned int		consume; */
	unsigned int		lookahead;

	/* Unconsumed data in buffer, starting at bp[-leftover]. */
	unsigned int		leftover;
};

/* Reads from buffer at *src + *src_left - src_size ... *src +
   *src_left, incrementing *src and decrementing *src_left by the
   number of bytes read. NOTE *src_left must be src_size when you
   change buffers.

   First removes w->skip bytes from the buffer and clears w->skip, then
   removes w->consume bytes, copying the data and the following
   w->lookahead bytes to an output buffer.

   Returns TRUE on success. *dst will point to the begin of the copied
   data. *scan_end will point to the end of the range you can scan
   with w->lookahead (can be > *dst + w->consume if *src_left permits).
   NOTE these pointers may point anywhere into the src_size input
   buffer, don't free / overwrite prematurely. *src_left will be >= 0.

   w->skip, w->consume and w->lookahead can change between successful
   calls.

   Returns FALSE if more data is needed, *src_left will be 0.
*/
vbi_inline vbi_bool
wrap_around			(struct wrap *		w,
				 const uint8_t **	dst,
				 const uint8_t **	scan_end,
				 const uint8_t **	src,
				 unsigned int *		src_left,
				 unsigned int		src_size)
{
	unsigned int available;
	unsigned int required;

	if (w->skip > 0) {
		/* Skip is not consume to save copying. */

		if (w->skip > w->leftover) {
			w->skip -= w->leftover;
			w->leftover = 0;

			if (w->skip > *src_left) {
				w->skip -= *src_left;

				*src += *src_left;
				*src_left = 0;

				return FALSE;
			}

			*src += w->skip;
			*src_left -= w->skip;
		} else {
			w->leftover -= w->skip;
		}

		w->skip = 0;
	}

	available = w->leftover + *src_left;
	required = /* w->consume + */ w->lookahead;

	if (required > available || available > src_size) {
		/* Not enough data at s, or we have bytes left
		   over from the previous buffer, must wrap. */

		if (required > w->leftover) {
			/* Need more data in the wrap_buffer. */

			memmove (w->buffer, w->bp - w->leftover, w->leftover);
			w->bp = w->buffer + w->leftover;

			required -= w->leftover;

			if (required > *src_left) {
				memcpy (w->bp, *src, *src_left);
				w->bp += *src_left;

				w->leftover += *src_left;

				*src += *src_left;
				*src_left = 0;

				return FALSE;
			}

			memcpy (w->bp, *src, required);
			w->bp += required;

			w->leftover = w->lookahead;

			*src += required;
			*src_left -= required;

			*dst = w->buffer;
			*scan_end = w->bp - w->lookahead;
		} else {
			*dst = w->bp - w->leftover;
			*scan_end = w->bp - w->lookahead;

			/* w->leftover -= w->consume; */
		}
	} else {
		/* All the required bytes are in this frame and
		   we have a complete copy of the w->buffer
		   leftover bytes before s. */

		*dst = *src - w->leftover;
		*scan_end = *src + *src_left - w->lookahead;

		/* if (w->consume > w->leftover) {
			unsigned int advance;

			advance = w->consume - w->leftover;

			*src += advance;
			*src_left -= advance;

			w->leftover = 0;
		} else {
			w->leftover -= w->consume;
		} */
	}

	return TRUE;
}

struct frame {
	/* Buffers for sliced and raw data of one frame. */

	vbi_sliced *		sliced_begin;
	vbi_sliced *		sliced_end;

	uint8_t *		raw;

	/* XXX replace by vbi_sampling_par. */
	unsigned int		raw_start[2];
	unsigned int		raw_count[2];

	/* Current position. */

	vbi_sliced *		sp;

	unsigned int		last_line;

	/* Number of lines extracted from current packet. */
	unsigned int		sliced_count;

	uint8_t *		rp;
	vbi_sliced *		raw_sp;
	unsigned int		raw_offset;
};

/* Converts the line_offset and field_parity byte. */
vbi_inline unsigned int
lofp_to_line			(unsigned int		lofp,
				 unsigned int		system)
{
	static const unsigned int start [2][2] = {
		{ 0, 263 },
		{ 0, 313 },
	};
	unsigned int line_offset;

	line_offset = lofp & 31;

	if (line_offset > 0) {
		unsigned int field_parity;

		field_parity = !(lofp & (1 << 5));

		return line_offset + start[system][field_parity];
	} else {
		/* Unknown line. */
		return 0;
	}
}

static vbi_sliced *
line_address			(struct frame *		f,
				 unsigned int		lofp,
				 unsigned int		system,
				 vbi_bool		raw)
{
	unsigned int line;
	vbi_sliced *s;

	if (f->sp >= f->sliced_end) {
		log ("Out of buffer space (%d lines)\n",
		     f->sliced_end - f->sliced_begin);

		return NULL;
	}

	line = lofp_to_line (lofp, system);

	if (line > 0) {
		if (raw) {
			unsigned int field;

			field = (line >= f->raw_start[1]);

			if (line < f->raw_start[0]
			    || line >= (f->raw_start[field]
					+ f->raw_count[field])) {
				log ("Raw line %u outside sampling range "
				     "%u ... %u, %u ... %u\n",
				     line,
				     f->raw_start[0],
				     f->raw_start[0] + f->raw_count[0],
				     f->raw_start[1],
				     f->raw_start[1] + f->raw_count[1]);

				return NULL;
			} else if (0 != field) {
				f->rp = f->raw
					+ (line + f->raw_count[0]) * 720;
			} else {
				f->rp = f->raw + line * 720;
			}
		}

		if (line > f->last_line) {
			f->last_line = line;

			s = f->sp++;
			s->line = line;
		} else {
			/* EN 301 775 section 4.1: ascending line
			   order, no line twice. */

			/* When the first line number in a packet is
			   smaller than the last line number in the
			   previous packet the frame is complete. */
			if (0 == f->sliced_count)
				return NULL;

			log ("Illegal line order %u >= %u\n",
			     line, f->last_line);

			return NULL;

			for (s = f->sliced_begin; s < f->sp; ++s)
				if (line <= s->line)
					break;

			if (line < s->line) {
				memmove (s, s + 1, (f->sp - s) * sizeof (*s));
				++f->sp;
				s->line = line;
			}

			if (raw) {
				memset (f->rp, 0, 720);
			}
		}
	} else {
		/* Unknown line. */

		if (0 == f->sliced_count
		    && f->last_line > 0)
			return NULL;

		++f->last_line;

		s = f->sp++;
		s->line = 0;

		f->rp += raw;
	}

	++f->sliced_count;

	return s;
}

static void
discard_raw			(struct frame *		f)
{
	memset (f->rp, 0, 720);

	memmove (f->raw_sp + 1, f->raw_sp,
		 (f->sp - f->raw_sp - 1) * sizeof (vbi_sliced));
	--f->sp;

	f->raw_sp = NULL;
	f->raw_offset = 0;
}

static int
demux_samples			(struct frame *		f,
				 uint8_t *		p,
				 unsigned int		system)
{
	vbi_sliced *s;
	unsigned int offset;
	unsigned int n_pixels;

	offset = p[3] * 256 + p[4];
	n_pixels = p[5];

	/* n_pixels <= 251 has been checked by caller. */
	if (0 == n_pixels || (offset + n_pixels) > 720) {
		log ("Illegal segment size %u ... %u (%u pixels)\n",
		     offset, offset + n_pixels, n_pixels);

		discard_raw (f);

		return 0;
	}

	if (p[2] & (1 << 7)) {
		/* First segment. */

		if (f->raw_offset > 0) {
			log ("Last segment missing, line %u, offset %u\n",
			     f->raw_sp->line, f->raw_offset);

			discard_raw (f);

			/* Recoverable error. */
		}

		if (!(f->raw_sp = line_address (f, p[2], system, TRUE))) {
			if (0 == f->sliced_count)
				return -1; /* is a new frame */
			else
				return 0; /* bad packet */
		}

		s->id = VBI_SLICED_NONE;
	} else {
		unsigned int line;

		line = lofp_to_line (p[2], system);

		if (0 == f->raw_offset) {
			log ("First segment missing of line %u, offset %u\n",
			     line, offset);

			/* Recoverable error. */
			return 1;
		} else if (line != f->raw_sp->line
			   || offset != f->raw_offset) {
			log ("Segment(s) missing or out of order, "
			     "expected line %u, offset %u, "
			     "got line %u, offset %u\n",
			     f->raw_sp->line, f->raw_offset,
			     line, offset);

			discard_raw (f);

			/* Recoverable error. */
			return 1;
		}
	}

	memcpy (f->rp + offset, p + 6, n_pixels);

	if (p[2] & (1 << 6)) {
		/* Last segment. */
		f->raw_offset = 0;
	} else {
		f->raw_offset = offset + n_pixels;
	}

	return TRUE;
}

static int
demux_data_units		(struct frame *		f,
				 const uint8_t **	src,
				 unsigned int *		src_left)
{
	const uint8_t *p;
	const uint8_t *end2;
	int r;

	assert (*src_left >= 2);

	r = 0; /* bad packet */

	p = *src;
	end2 = p + *src_left
		- 2 /* data_unit_id,
		       data_unit_length */;

	while (p < end2) {
		unsigned int data_unit_id;
		unsigned int data_unit_length;
		vbi_sliced *s;
		unsigned int i;

		data_unit_id = p[0];
		data_unit_length = p[1];

		/* EN 301 775 section 4.3.1: Data units
		   must not cross PES packet boundaries. */
		if (p + data_unit_length > end2)
			goto failure;

		switch (data_unit_id) {
		case DATA_UNIT_STUFFING:
			break;

		case DATA_UNIT_EBU_TELETEXT_NON_SUBTITLE:
		case DATA_UNIT_EBU_TELETEXT_SUBTITLE:
			if (data_unit_length < 1 + 1 + 42) {
			bad_length:
				log ("data_unit_length %u too small "
				     "for data_unit_id %u\n",
				     data_unit_length, data_unit_id);

				goto failure;
			}

			/* We cannot transcode custom framing codes. */
			if (0xE4 != p[3]) /* vbi_rev8 (0x27) */
				break;

			if (!(s = line_address (f, p[2], 1, FALSE)))
				goto no_line;

			s->id = VBI_SLICED_TELETEXT_B;

			for (i = 0; i < 42; ++i)
				s->data[i] = vbi_rev8 (p[4 + i]);

			if (0) {
				fprintf (stderr, "DU-TTX %u >", s->line);
				for (i = 0; i < 42; ++i)
					fputc (printable (s->data[i]), stderr);
				fprintf (stderr, "<\n");
			}

			break;

		case DATA_UNIT_VPS:
			if (data_unit_length < 1 + 13)
				goto bad_length;

			if (!(s = line_address (f, p[2], 1, FALSE)))
				goto no_line;

			s->id = (s->line >= 313) ?
				VBI_SLICED_VPS : VBI_SLICED_VPS;

			for (i = 0; i < 13; ++i)
				s->data[i] = p[3 + i];

			break;

		case DATA_UNIT_WSS:
			if (data_unit_length < 1 + 2)
				goto bad_length;

			if (!(s = line_address (f, p[2], 1, FALSE)))
				goto no_line;

			s->id = VBI_SLICED_WSS_625;

			s->data[0] = vbi_rev8 (p[3]);
			s->data[1] = vbi_rev8 (p[4]);

			break;

		case DATA_UNIT_ZVBI_WSS_CPR1204:
			if (data_unit_length < 1 + 3)
				goto bad_length;

			if (!(s = line_address (f, p[2], 0, FALSE)))
				goto no_line;

			s->id = VBI_SLICED_WSS_CPR1204;

			s->data[0] = p[3];
			s->data[1] = p[4];
			s->data[2] = p[5];

			break;

		case DATA_UNIT_ZVBI_CLOSED_CAPTION_525:
			if (data_unit_length < 1 + 2)
				goto bad_length;

			if (!(s = line_address (f, p[2], 0, FALSE)))
				goto no_line;

			s->id = VBI_SLICED_CAPTION_525;

			s->data[0] = vbi_rev8 (p[3]);
			s->data[1] = vbi_rev8 (p[4]);

			break;

		case DATA_UNIT_CLOSED_CAPTION:
			if (data_unit_length < 1 + 2)
				goto bad_length;

			if (!(s = line_address (f, p[2], 1, FALSE)))
				goto no_line;

			s->id = VBI_SLICED_CAPTION_625;

			s->data[0] = vbi_rev8 (p[3]);
			s->data[1] = vbi_rev8 (p[4]);

			break;

#if 0 /* later */
		case DATA_UNIT_MONOCHROME_SAMPLES:
			if (data_unit_length < 1 + 2 + 1 + p[5]) {
			bad_sample_length:
				log ("data_unit_length %u too small "
				     "for data_unit_id %u with %u samples\n",
				     data_unit_length, data_unit_id, p[5]);

				goto failure;
			}

			if ((r = demux_samples (f, p, 1)) < 1)
				goto failure;

			break;

		case DATA_UNIT_ZVBI_MONOCHROME_SAMPLES_525:
			if (data_unit_length < 1 + 2 + 1 + p[5])
				goto bad_sample_length;

			if ((r = demux_samples (f, p, 0)) < 1)
				goto failure;

			break;
#endif

		default:
			log ("Unknown data_unit_id %u\n", data_unit_id);
			break;
		}

		p += data_unit_length + 2;
	}

	*src = p;
	*src_left = 0;

	return 1; /* success */

 no_line:
	if (0 == f->sliced_count)
		r = -1; /* is a new frame */

 failure:
	*src_left = end2 + 2 - p;
	*src = p;

	return r;
}

vbi_inline void
reset_frame			(struct frame *		f)
{
	f->sp = f->sliced_begin;

	f->last_line = 0;
	f->sliced_count = 0;

	if (f->rp > f->raw) {
		unsigned int lines;

		lines = f->raw_count[0] + f->raw_count[1];
		memset (f->raw, 0, lines * 720);
	}

	f->rp = f->raw;
	f->raw_sp = NULL;
	f->raw_offset = 0;
}



/*
  	Add _vbi_dvb_demultiplex() here.
*/


#define HEADER_LOOKAHEAD 48

struct _vbi_dvb_demux {
	/* Must hold one PES packet, at most 6 + 65535 bytes. */
	uint8_t			buffer[65536 + 16];

	vbi_sliced		sliced[64];

	struct wrap		wrap;

	struct frame		frame;

	int64_t			frame_pts;
	int64_t			packet_pts;

	vbi_bool		new_frame;

	vbi_dvb_demux_cb *	callback;
	void *			user_data;
};

static vbi_bool
timestamp			(int64_t *		pts,
				 unsigned int		mark,
				 const uint8_t *	p)
{
	unsigned int t;

	if (mark != (p[0] & 0xF1u))
		return FALSE;

	t  = p[1] << 22;
	t |= (p[2] & ~1) << 14;
	t |= p[3] << 7;
	t |= p[4] >> 1;

	if (0) {
		int64_t old_pts;
		int64_t new_pts;

		old_pts = *pts;
		new_pts = t | (((int64_t) p[0] & 0x0E) << 29);

		fprintf (stderr, "TS%x 0x%llx %+lld\n",
			 mark, new_pts, new_pts - old_pts);
	}

	*pts = t | (((int64_t) p[0] & 0x0E) << 29);

	return TRUE;
}

static vbi_bool
demux_packet			(vbi_dvb_demux *	dx,
				 const uint8_t **	src,
				 unsigned int *		src_left)
{
	const uint8_t *s;
	unsigned int s_left;

	s = *src;
	s_left = *src_left;

	for (;;) {
		const uint8_t *p;
		const uint8_t *scan_begin;
		const uint8_t *scan_end;
		unsigned int packet_length;
		unsigned int header_length;

		if (!wrap_around (&dx->wrap,
				  &p, &scan_end,
				  &s, &s_left, *src_left))
			break; /* out of data */

		/* Data units */

		if (dx->wrap.lookahead > HEADER_LOOKAHEAD) {
			unsigned int left;

			if (dx->new_frame) {
				/* New frame commences in this packet. */

				reset_frame (&dx->frame);

				dx->frame_pts = dx->packet_pts;
				dx->new_frame = FALSE;
			}

			dx->frame.sliced_count = 0;

			left = dx->wrap.lookahead;

			for (;;) {
				unsigned int lines;
				int r;

				r = demux_data_units (&dx->frame, &p, &left);

				if (0 == r) {
					/* Bad packet, discard. */
					dx->new_frame = TRUE;
					break;
				}

				if (r > 0) {
					/* Data unit extraction successful.
					   Packet continues previous frame. */
					break;
				}

				/* A new frame commences in this packet.
				   We must flush dx->frame before we extract
				   data units from this packet. */

				/* Must not change dx->frame or dx->frame_pts
				   here to permit "pass by return". */
				dx->new_frame = TRUE;

				if (!dx->callback)
					goto failure;

				lines = dx->frame.sliced_begin - dx->frame.sp;

				if (!dx->callback (dx,
						   dx->user_data,
						   dx->frame.sliced_begin,
						   lines,
						   dx->frame_pts))
					goto failure;
			}

			dx->wrap.skip = dx->wrap.lookahead;
			dx->wrap.lookahead = HEADER_LOOKAHEAD;

			continue;
		}

		/* Start code scan */

		scan_begin = p;

		for (;;) {
			/* packet_start_code_prefix [24] == 0x000001,
			   stream_id [8] == PRIVATE_STREAM_1 */

			if (__builtin_expect (p[2] & ~1, TRUE)) {
				/* Not 000001 or xx0000 or xxxx00. */
				p += 3;
			} else if (0 != (p[0] | p[1]) || 1 != p[2]) {
				++p;
			} else if (PRIVATE_STREAM_1 == p[3]) {
				break;
			} else if (p[3] >= 0xBC) {
				packet_length = p[4] * 256 + p[5];
				dx->wrap.skip = (p - scan_begin)
					+ 6 + packet_length;
				goto outer_continue;
			}

			if (__builtin_expect (p >= scan_end, FALSE)) {
				dx->wrap.skip = p - scan_begin;
				goto outer_continue;
			}
		}

		/* Packet header */

		packet_length = p[4] * 256 + p[5];

		dx->wrap.skip = (p - scan_begin) + 6 + packet_length;

		/* EN 300 472 section 4.2: N x 184 - 6. (We'll read
		   46 bytes without further checks and need at least
		   one data unit to function properly.) */
		if (packet_length < 178)
			continue;

		/* PES_header_data_length [8] */
		header_length = p[8];

		/* EN 300 472 section 4.2: 0x24. */
		if (36 != header_length)
			continue;

		/* data_identifier (EN 301 775 section 4.3.2) */
		switch (p[9 + 36]) {
		case 0x10 ... 0x1F:
		case 0x99 ... 0x9B:
			break;

		default:
			continue;
		}

		/* '10', PES_scrambling_control [2] == 0 (not scrambled),
		   PES_priority, data_alignment_indicator == 1 (data unit
		   starts immediately after header),
		   copyright, original_or_copy */
		if (0x84 != (p[6] & 0xF4))
			continue;

		/* PTS_DTS_flags [2], ESCR_flag, ES_rate_flag,
		   DSM_trick_mode_flag, additional_copy_info_flag,
		   PES_CRC_flag, PES_extension_flag */
		switch (p[7] >> 6) {
		case 2:	/* PTS 0010 xxx 1 ... */
			if (!timestamp (&dx->packet_pts, 0x21, p + 9))
				continue;
			break;

		case 3:	/* PTS 0011 xxx 1 ... DTS ... */
			if (!timestamp (&dx->packet_pts, 0x31, p + 9))
				continue;
			break;

		default:
			/* EN 300 472 section 4.2: a VBI PES packet [...]
			   always carries a PTS. */
			/* But it doesn't matter if this packet continues
			   the previous frame. */
			if (dx->new_frame)
				continue;
			break;
		}

		/* Habemus packet. */

		dx->wrap.skip = (p - scan_begin) + 9 + 36 + 1;
		dx->wrap.lookahead = packet_length - 3 - 36 - 1;

 outer_continue:
		;
	}

	*src = s;
	*src_left = s_left;

	return TRUE;

 failure:
	*src = s;
	*src_left = s_left;

	return FALSE;
}

unsigned int
_vbi_dvb_demux_cor		(vbi_dvb_demux *	dx,
				 vbi_sliced *		sliced,
				 unsigned int 		sliced_lines,
				 int64_t *		pts,
				 const uint8_t **	buffer,
				 unsigned int *		buffer_left)
{
	assert (NULL != dx);
	assert (NULL != sliced);
	assert (NULL != pts);
	assert (NULL != buffer);
	assert (NULL != buffer_left);

	dx->frame.sliced_begin = sliced;
	dx->frame.sliced_end = sliced + sliced_lines;

	if (!demux_packet (dx, buffer, buffer_left)) {
		*pts = dx->frame_pts;

		return dx->frame.sp - dx->frame.sliced_begin;
	}

	return 0;
}

vbi_bool
_vbi_dvb_demux_demux		(vbi_dvb_demux *	dx,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size)
{
	assert (NULL != dx);
	assert (NULL != buffer);
	assert (NULL != dx->callback);

	return demux_packet (dx, &buffer, &buffer_size);
}

void
_vbi_dvb_demux_delete		(vbi_dvb_demux *	dx)
{
	if (NULL == dx)
		return;

	CLEAR (*dx);

	free (dx);		
}

vbi_dvb_demux *
_vbi_dvb_demux_pes_new		(vbi_dvb_demux_cb *	callback,
				 void *			user_data)
{
	vbi_dvb_demux *dx;

	if (!(dx = malloc (sizeof (*dx)))) {
		return NULL;
	}

	CLEAR (*dx);

	dx->wrap.buffer = dx->buffer;
	dx->wrap.bp = dx->buffer;

	dx->wrap.lookahead = HEADER_LOOKAHEAD;

	dx->frame.sliced_begin = dx->sliced;
	dx->frame.sliced_end = dx->sliced + N_ELEMENTS (dx->sliced);

	/* Raw data ignored for now. */

	dx->new_frame = TRUE;

	dx->callback = callback;
	dx->user_data = user_data;

	return dx;
}
