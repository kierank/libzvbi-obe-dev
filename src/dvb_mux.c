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

/* $Id: dvb_mux.c,v 1.1 2004/10/25 16:54:11 mschimek Exp $ */

#include <stdio.h>		/* fprintf() */
#include <stdlib.h>		/* abort() */
#include <string.h>		/* memcpy(), memset() */
#include <assert.h>
#include "dvb.h"
#include "dvb_mux.h"
#include "hamm.h"		/* vbi_rev8() */
#include "misc.h"		/* MIN(), CLEAR() */

#define vbi_rev8(n) vbi_bit_reverse[n]

/**
 * @param packet *packet is the output buffer pointer and will be
 *   advanced by the number of bytes stored.
 * @param packet_left *packet_left is the number of bytes left
 *   in the output buffer, and will be decremented by the number of
 *   bytes stored.
 * @param sliced *sliced is the vbi_sliced data to be multiplexed,
 *   and will be advanced by the number of sliced lines read.
 * @param sliced_left *sliced_left is the number of sliced lines
 *   left in the *sliced array, and will be decremented by the
 *   number of lines read.
 * @param data_identifier Compliant to EN 301 775 section
 *   4.3.2, when the data_indentifier lies in range 0x10 ... 0x1F
 *   data units will be split or padded to data_unit_length 0x2C.
 * @param service_set Only data services in this set will be
 *   encoded. Create a set by or-ing @c VBI_SLICED_ values.
 *
 * Stores an array of vbi_sliced data in a MPEG-2 PES packet
 * as defined in EN 301 775. When *sliced_left is zero, or when
 * *packet_left becomes too small the function fills the remaining
 * space with stuffing bytes.
 *
 * The following data services can be encoded per EN 300 472:
 * @c VBI_SLICED_TELETEXT_B_625_L10 with data_unit_id 0x02. Additionally
 * EN 301 775 permits @c VBI_SLICED_VPS, @c _VPS_F2, @c _WSS_625
 * and @c _CAPTION_625 (any field).
 *
 * For completeness the function also encodes
 * @c VBI_SLICED_TELETEXT_B_625_L25, @c _CAPTION_525 (any field) and
 * @c _WSS_CPR1204 with data_unit_id 0x02, 0xB5 and 0xB4 respectively.
 * You can modify this behaviour with the service_set parameter.
 *
 * The lines in the sliced array must be sorted by ascending line
 * number and belong to the same frame. If unknown all lines can
 * have line number zero. Sliced data outside lines 1 ... 31 and 313 ... 344
 * (1 ... 31 and 263 ... 294 for NTSC data services) and data services
 * not covered will be ignored.
 */
void
_vbi_dvb_multiplex_sliced	(uint8_t **		packet,
				 unsigned int *		packet_left,
				 const vbi_sliced **	sliced,
				 unsigned int *		sliced_left,
				 unsigned int		data_identifier,
				 vbi_service_set	service_set)
{
	uint8_t *p;
	const vbi_sliced *s;
	unsigned int p_left;
	unsigned int s_left;
	unsigned int last_line;
	vbi_bool fixed_length;

	assert (NULL != packet);
	assert (NULL != sliced);
	assert (NULL != packet_left);
	assert (NULL != sliced_left);

	p = *packet;
	p_left = *packet_left;

	if (NULL == p || 0 == p_left) {
		return;
	}

	s = *sliced;
	s_left = *sliced_left;

	if (NULL == s || 0 == s_left) {
		/* data_unit_id: DATA_UNIT_STUFFING (0xFF),
		   stuffing byte: 0xFF */
		memset (p, 0xFF, p_left);

		p += p_left;
		p_left = 0;

		goto finish;
	}

	last_line = 0;

	fixed_length = (data_identifier >= DATA_ID_EBU_TELETEXT_BEGIN
			&& data_identifier < DATA_ID_EBU_TELETEXT_END);

	while (s_left > 0) {
		unsigned int length;
		unsigned int f2start;
		unsigned int i;

		if (s->line > 0) {
			if (s->line < last_line) {
				fprintf (stderr,
					 "%s: Sliced lines not sorted.\n",
					 __FUNCTION__);
				abort ();
			}

			last_line = s->line;
		}

		if (!(s->id & service_set))
			goto skip;

		f2start = 313;

		if (s->id & (VBI_SLICED_CAPTION_525 |
			     VBI_SLICED_WSS_CPR1204))
			f2start = 263;

		if (fixed_length) {
			length = 2 + 1 + 1 + 42;
		} else if (s->id & VBI_SLICED_TELETEXT_B) {
			length = 2 + 1 + 1 + 42;
		} else if (s->id & (VBI_SLICED_VPS)) {
			length = 2 + 1 + 13;
		} else if (s->id & (VBI_SLICED_WSS_625 |
				    VBI_SLICED_CAPTION_525 |
				    VBI_SLICED_CAPTION_625)) {
			length = 2 + 1 + 2;
		} else if (s->id & (VBI_SLICED_WSS_CPR1204)) {
			length = 2 + 1 + 3;
		} else {
			if (0)
				fprintf (stderr, "Skipping sliced id "
					 "0x%08x\n", s->id);
			goto skip;
		}

		if (length > p_left) {
			/* EN 301 775 section 4.3.1: Data units
			   cannot cross PES packet boundaries. */
			/* data_unit_id: DATA_UNIT_STUFFING (0xFF),
			   stuffing byte: 0xFF */
			memset (p, 0xFF, p_left);

			p += p_left;
			p_left = 0;

			break;
		}

		if (s->line < 32) {
			/* Unknown line (0) or first field. */
			p[2] = (3 << 6) + (1 << 5) + s->line;
		} else if (s->line < f2start) {
			if (0)
				fprintf (stderr, "Sliced line %u exceeds "
					 "limit %u ... %u, %u ... %u\n",
					 s->line, 0, 31,
					 f2start, f2start + 31);
			goto skip;
		} else if (s->line < f2start + 32) {
			/* Second field. */
			p[2] = (3 << 6) + (0 << 5)
				+ s->line - f2start;
		} else {
			if (0)
				fprintf (stderr, "Sliced line %u exceeds "
					 "limit %u ... %u, %u ... %u\n",
					 s->line, 0, 31,
					 f2start, f2start + 31);
			goto skip;
		}

		if (s->id & VBI_SLICED_TELETEXT_B) {
			/* data_unit_id [8], data_unit_length [8],
			   reserved [2], field_parity, line_offset [5],
			   framing_code [8], magazine_and_packet_address [16],
			   data_block [320] (msb first transmitted) */
			p[0] = DATA_UNIT_EBU_TELETEXT_NON_SUBTITLE;
			p[1] = 44;
			p[3] = 0xE4; /* vbi_rev8 (0x27); */

			for (i = 0; i < 42; ++i)
				p[4 + i] = vbi_rev8 (s->data[i]);
		} else if (s->id & VBI_SLICED_CAPTION_525) {
			/* data_unit_id [8], data_unit_length [8],
			   reserved [2], field_parity, line_offset [5],
			   data_block [16] (msb first) */
			p[0] = DATA_UNIT_ZVBI_CLOSED_CAPTION_525;
			p[1] = 3;
			p[3] = vbi_rev8 (s->data[0]);
			p[4] = vbi_rev8 (s->data[1]);
		} else if (s->id & (VBI_SLICED_VPS)) {
			/* data_unit_id [8], data_unit_length [8],
			   reserved [2], field_parity, line_offset [5],
			   vps_data_block [104] (msb first) */
			p[0] = DATA_UNIT_VPS;
			p[1] = 14;

			for (i = 0; i < 13; ++i)
				p[3 + i] = vbi_rev8 (s->data[i]);
		} else if (s->id & VBI_SLICED_WSS_625) {
			/* data_unit_id [8], data_unit_length [8],
			   reserved[2], field_parity, line_offset [5],
			   wss_data_block[14] (msb first), reserved[2] */
			p[0] = DATA_UNIT_WSS;
			p[1] = 3;
			p[3] = vbi_rev8 (s->data[0]);
			p[4] = vbi_rev8 (s->data[1]) | 3;
		} else if (s->id & VBI_SLICED_CAPTION_625) {
			/* data_unit_id [8], data_unit_length [8],
			   reserved[2], field_parity, line_offset [5],
			   data_block[16] (msb first) */
			p[0] = DATA_UNIT_CLOSED_CAPTION;
			p[1] = 3;
			p[3] = vbi_rev8 (s->data[0]);
			p[4] = vbi_rev8 (s->data[1]);
		} else if (s->id & VBI_SLICED_WSS_CPR1204) {
			/* data_unit_id [8], data_unit_length [8],
			   reserved[2], field_parity, line_offset [5],
			   wss_data_block[20] (msb first), reserved[4] */
			p[0] = DATA_UNIT_ZVBI_WSS_CPR1204;
			p[1] = 4;
			p[3] = vbi_rev8 (s->data[0]);
			p[4] = vbi_rev8 (s->data[1]);
			p[5] = vbi_rev8 (s->data[2]) | 0xF;
		} else {
			if (0)
				fprintf (stderr, "Skipping sliced id "
					 "0x%08x\n", s->id);
			goto skip;
		}

		i = p[1] + 2;
		p += i;

		/* Pad to data_unit_length 0x2C if necessary. */
		while (i++ < length)
			*p++ = 0xFF;

		p_left -= length;

	skip:
		++s;
		--s_left;
	}

 finish:
	*packet = p;
	*packet_left = p_left;
	*sliced = s;
	*sliced_left = s_left;
}

/**
 * @param packet *packet is the output buffer pointer and will be
 *   advanced by the number of bytes stored.
 * @param packet_left *packet_left is the number of bytes left
 *   in the output buffer, and will be decremented by the number of
 *   bytes stored.
 * @param samples *samples is the raw VBI data line to be
 *   multiplexed, in ITU-R BT.601 format, Y values only.
 *   *samples will be advanced by the number of bytes read.
 * @param samples_left *samples_left is the number of bytes left
 *   in the *samples buffer, and will be decremented by the
 *   number of bytes read.
 * @param samples_size Number of bytes in the *samples buffer.
 *   offset + samples_size must not exceed 720.
 * @param data_identifier Compliant to EN 301 775 section
 *   4.3.2, when the data_indentifier lies in range 0x10 ... 0x1F
 *   data units will be restricted to data_unit_length 0x2C.
 * @param videostd_set Video standard.
 * @param line ITU-R line number to be encoded (see vbi_sliced).
 *   Valid line numbers are 0 (unknown), 1 ... 31 and 313 ... 344
 *   for @c VBI_VIDEOSTD_SET_625, 1 ... 31 and 263 ... 294 for
 *   @c VBI_VIDEOSTD_SET_525.
 * @param offset Offset of the first sample in the *samples buffer.
 *
 * Stores one line of raw VBI data in a MPEG-2 PES packet
 * as defined in EN 301 775. When *packet_left becomes too small
 * the function fills up the remaining space with stuffing bytes.
 *
 * EN 301 775 permits all video standards in the
 * @c VBI_VIDEOSTD_SET_625. Additionally the function accepts
 * @c VBI_VIDEOSTD_SET_525, these samples will be encoded
 * with data_unit_id 0xB6.
 */
void
_vbi_dvb_multiplex_samples	(uint8_t **		packet,
				 unsigned int *		packet_left,
				 const uint8_t **	samples,
				 unsigned int *		samples_left,
				 unsigned int		samples_size,
				 unsigned int		data_identifier,
				 vbi_videostd_set	videostd_set,
				 unsigned int		line,
				 unsigned int		offset)
{
	uint8_t *p;
	const uint8_t *s;
	unsigned int p_left;
	unsigned int s_left;
	unsigned int id;
	unsigned int f2_start;
	unsigned int min_space;

	assert (NULL != packet);
	assert (NULL != packet_left);
	assert (NULL != samples);
	assert (NULL != samples_left);

	p = *packet;
	p_left = *packet_left;

	if (NULL == p || 0 == p_left) {
		return;
	}

	if (videostd_set & VBI_VIDEOSTD_SET_525_60) {
		if (videostd_set & VBI_VIDEOSTD_SET_625_50) {
			fprintf (stderr,
				 "%s: Ambiguous videostd_set 0x%x\n",
				 __FUNCTION__,
				 (unsigned int) videostd_set);
			abort ();
		}

		id = DATA_UNIT_ZVBI_MONOCHROME_SAMPLES_525;
		f2_start = 263;
	} else {
		id = DATA_UNIT_MONOCHROME_SAMPLES;
		f2_start = 313;
	}

	if (line < 32) {
		/* Unknown line (0) or first field. */
		line = (1 << 5) + line;
	} else if (line >= f2_start && line < f2_start + 32) {
		line = (0 << 5) + line - f2_start;
	} else {
		fprintf (stderr,
			 "%s: Line number %u exceeds limits "
			 "%u ... %u, %u ... %u",
			 __FUNCTION__, line, 0, 31, f2_start, f2_start + 31);
		abort ();
	}

	s = *samples;
	s_left = *samples_left;

	if (offset + samples_size > 720) {
		fprintf (stderr,
			 "%s: offset %u + samples_size %u > 720\n",
			 __FUNCTION__, offset, samples_size);
		abort ();
	}

	if (s_left > samples_size) {
		fprintf (stderr,
			 "%s: samples_left %u > samples_size %u",
			  __FUNCTION__, s_left, samples_size);
		abort ();
	}

	if (data_identifier >= DATA_ID_EBU_TELETEXT_BEGIN
	    && data_identifier < DATA_ID_EBU_TELETEXT_END)
		min_space = 7;
	else
		min_space = 2 + 0x2C;

	offset += samples_size - s_left;

	while (s_left > 0) {
		unsigned int n_pixels;

		if (min_space > p_left) {
			/* EN 301 775 section 4.3.1: Data units
			   cannot cross PES packet boundaries. */
			/* data_unit_id: DATA_UNIT_STUFFING (0xFF),
			   stuffing byte: 0xFF */
			memset (p, 0xFF, p_left);

			p += p_left;
			p_left = 0;

			goto finish;
		}

		if (min_space > 7) {
			uint8_t *end;

			n_pixels = MIN (s_left, 2u + 0x2Cu - 6u);
			n_pixels = MIN (n_pixels, p_left - 6);

			/* data_unit_id [8], data_unit_length [8],
			   first_segment_flag [1], last_segment_flag [1],
			   field_parity [1], line_offset [5],
			   first_pixel_position [16], n_pixels [8] */
			p[0] = id;
			p[1] = 0x2C;
			p[2] = line
				+ ((s_left == samples_size) << 7)
				+ ((s_left == n_pixels) << 6);
			p[3] = offset >> 8;
			p[4] = offset;
			p[5] = n_pixels;

			memcpy (p + 6, s + offset, n_pixels);

			end = p + 2 + 0x2C;

			offset += n_pixels;

			n_pixels += 6;

			p += n_pixels;
			p_left -= n_pixels;

			/* Pad to data_unit_length 0x2C if necessary. */
			while (p < end)
				*p++ = 0xFF;
		} else {
			n_pixels = MIN (s_left, 251u);
			n_pixels = MIN (n_pixels, p_left - 6);

			/* data_unit_id [8], data_unit_length [8],
			   first_segment_flag [1], last_segment_flag [1],
			   field_parity [1], line_offset [5],
			   first_pixel_position [16], n_pixels [8] */
			p[0] = id;
			p[1] = n_pixels + 4;
			p[2] = line
				+ ((s_left == samples_size) << 7)
				+ ((s_left == n_pixels) << 6);
			p[3] = offset >> 8;
			p[4] = offset;
			p[5] = n_pixels;

			memcpy (p + 6, s + offset, n_pixels);

			offset += n_pixels;

			n_pixels += 6;

			p += n_pixels;
			p_left -= n_pixels;
		}

		s += n_pixels;
		s_left -= n_pixels;
	}

 finish:
	*packet = p;
	*packet_left = p_left;
	*samples = s;
	*samples_left = s_left;
}

struct _vbi_dvb_mux {
	/* Must hold one PES packet, at most 356 * 184 = 6 + 65498 bytes,
	   in TS mode additionally one TS header of 4 bytes. */
	uint8_t			packet[65536];

	unsigned int		pid;
	unsigned int		continuity_counter;
	unsigned int		data_identifier;
	unsigned int		payload_size;

	vbi_videostd_set	videostd_set;

	vbi_dvb_mux_cb *	callback;
	void *			user_data;
};

void
_vbi_dvb_mux_reset		(vbi_dvb_mux *		mx)
{
	/* Nothing to do at this time. */

	mx = mx;
}

static void
timestamp			(uint8_t *		p,
				 int64_t		pts,
				 unsigned int		mark)
{
	unsigned int t;

	p[0] = mark + (unsigned int)((pts >> 29) & 0xE);

	t = (unsigned int) pts;

	p[1] = t >> 22;
	p[2] = (t >> 14) | 1;
	p[3] = t >> 7;
	p[4] = t * 2 + 1;
}

static vbi_bool
ts_packet_output		(vbi_dvb_mux *		mx,
				 const uint8_t *	end)
{
	uint8_t *p;
	unsigned int header1;

	/* ISO 13818-1 section 2.4.3.3: payload_unit_start_indicator is
	   set if exactly one PES packet commences in this TS packet
	   immediately after the header. */
	header1 = (1 << 6) | (mx->pid >> 8);

	for (p = mx->packet; p < end; p += 184) {
		/* NOTE this overwrites the end of the previous TS packet. */

		/* sync_byte [8] = 0x47 */
		p[0] = 0x47;

		/* transport_error_indicator = 0 (no error),
		   payload_unit_start_indicator,
		   transport_priority = 0 (normal), PID[13] */
		p[1] = header1;
		p[2] = mx->pid;

		header1 = (0 << 6) | (mx->pid >> 8);

		/* transport_scrambling_control [2] = 0 (not scrambled),
		   adaptation_field_control [2] = 1 (payload only),
		   continuity_counter [4] */
		p[3] = (1 << 4) + (mx->continuity_counter++ & 15);

		mx->callback (mx, mx->user_data, p, 188);
	}

	return TRUE;
}

/* XXX must be able to encode raw data. Would be nice if we could
   write into a client supplied buffer, that might save a copy. */

vbi_bool
_vbi_dvb_mux_mux		(vbi_dvb_mux *		mx,
				 int64_t		pts,
				 const vbi_sliced *	sliced,
				 unsigned int		sliced_size,
				 vbi_service_set	service_set)
{
	uint8_t *p;
	unsigned int left;

	while (sliced_size > 0) {
		if (pts >= 0) {
			/* PTS_DTS_flags [2] = 2 (PTS only),
			   ESCR_flag, ES_rate_flag, DSM_trick_mode_flag,
			   additional_copy_info_flag,
			   PES_CRC_flag, PES_extension_flag - no extensions. */
			mx->packet[11] = (2 << 6);

			timestamp (&mx->packet[13], pts, 0x21);
		} else {
			mx->packet[11] = 0;

			/* Stuffing bytes. */
			memset (&mx->packet[13], 0xFF, 36);
		}

		p = &mx->packet[4 + 45 + 1];
		left = mx->payload_size;

		while (left > 0) {
			_vbi_dvb_multiplex_sliced (&p, &left,
						   &sliced, &sliced_size,
						   mx->data_identifier,
						   service_set);
		}

		if (mx->pid) {
			ts_packet_output (mx, p);
		} else {
			mx->callback (mx, mx->user_data,
				      &mx->packet[4],
				      p - &mx->packet[4]);
		}
	}

	return TRUE;
}

void
_vbi_dvb_mux_delete		(vbi_dvb_mux *		mx)
{
	CLEAR (mx);
	free (mx);
}

/* XXX we're wasting a lot of space here on stuffing bytes.
   packet_size == 0 "minimum" would be nice. */

vbi_dvb_mux *
_vbi_dvb_mux_pes_new		(unsigned int		data_identifier,
				 unsigned int		packet_size,
				 vbi_videostd_set	videostd_set,
				 vbi_dvb_mux_cb *	callback,
				 void *			user_data)
{
	vbi_dvb_mux *mx;
	unsigned int packet_length;

	assert (NULL != callback);
	assert (packet_size > 0);
	assert (packet_size < 65535 + 6);

	/* EN 300 472 section 4.2: packet_length must be N x 184 - 6
	   (4 start_code + 2 packet_length). */
	assert (0 == (packet_size % 184));

	if (!(mx = malloc (sizeof (*mx)))) {
		return NULL;
	}

	/* Bytes 0 ... 3 reserved for TS header. */

	/* packet_start_code_prefix [24] = 0x000001,
	   stream_id [8] = PRIVATE_STREAM_1 */
	mx->packet[4] = 0x00;
	mx->packet[5] = 0x00;
	mx->packet[6] = 0x01;
	mx->packet[7] = PRIVATE_STREAM_1;

	packet_length = packet_size - 6;

	/* packet_length[16] */
	mx->packet[8] = packet_length >> 8;
	mx->packet[9] = packet_length;

	/* '10', PES_scrambling_control [2] == 0 (not scrambled), PES_priority,
	   data_alignment_indicator = 1 (EN 300 472 section 4.2),
	   copyright = 0 (undefined), original_or_copy = 0 (copy) */
	mx->packet[10] = (2 << 6) + (1 << 2);

	/* PTS_DTS_flags [2] = 0 (neither), ESCR_flag, ES_rate_flag,
	   DSM_trick_mode_flag, additional_copy_info_flag,
	   PES_CRC_flag, PES_extension_flag */
	mx->packet[11] = 0;

	/* PES_header_data_length [8] = 36 (EN 300 472 section 4.2) */
	mx->packet[12] = 36;

	/* Stuffing bytes. */
	memset (&mx->packet[13], 0xFF, 36);

	mx->packet[45] = data_identifier;

	mx->pid = 0;
	mx->data_identifier = data_identifier;
	mx->payload_size = packet_size - 46;

	mx->videostd_set = videostd_set;

	mx->callback = callback;
	mx->user_data = user_data;

	return mx;
}

vbi_dvb_mux *
_vbi_dvb_mux_ts_new		(unsigned int		pid,
				 unsigned int		data_identifier,
				 unsigned int		packet_size,
				 vbi_videostd_set	videostd_set,
				 vbi_dvb_mux_cb *	callback,
				 void *			user_data)
{
	vbi_dvb_mux *mx;

	assert (0 != pid);

	mx = _vbi_dvb_mux_pes_new (data_identifier,
				   packet_size,
				   videostd_set,
				   callback,
				   user_data);

	if (mx) {
		mx->pid = pid & 0x1FFF;
		mx->continuity_counter = 0;
	}

	return mx;
}
