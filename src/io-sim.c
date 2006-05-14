/*
 *  libzvbi - VBI device simulation
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

/* $Id: io-sim.c,v 1.6 2006/05/14 14:22:48 mschimek Exp $ */

#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>

#include "misc.h"
#include "sliced.h"

#include "io-sim.h"

#define VBI_PIXFMT_RGB24_LE VBI_PIXFMT_RGB24
#define VBI_PIXFMT_RGB24_BE VBI_PIXFMT_BGR24

#if 1
#  define log(templ, args...)						\
	fprintf (stderr, "%s: " templ, __FUNCTION__ , ## args)
#else
#  define log(templ, args...)
#endif

#define PI 3.1415926535897932384626433832795029

#define PULSE(zero_level)						\
do {									\
	if (0 == seq) {							\
		raw[i] = zero_level;					\
	} else if (3 == seq) {						\
		raw[i] = zero_level + (int) signal_amp;			\
	} else if ((seq ^ bit) & 1) { /* down */			\
		double r = sin (q * tr - (PI / 2.0));			\
		raw[i] = zero_level + (int)(r * r * signal_amp);	\
	} else { /* up */						\
		double r = sin (q * tr);				\
		raw[i] = zero_level + (int)(r * r * signal_amp);	\
	}								\
} while (0)

#define PULSE_SEQ(zero_level)						\
do {									\
	double tr;							\
	unsigned int bit;						\
	unsigned int byte;						\
	unsigned int seq;						\
									\
	tr = t - t1;							\
	bit = tr * bit_rate;						\
	byte = bit >> 3;						\
	bit &= 7;							\
	seq = (buf[byte] >> 7) + buf[byte + 1] * 2;			\
	seq = (seq >> bit) & 3;						\
	PULSE (zero_level);						\
} while (0)

static void
signal_teletext			(const vbi_sampling_par *sp,
				 unsigned int		black_level,
				 double			signal_amp,
				 double			bit_rate,
				 unsigned int		frc,
				 unsigned int		payload,
				 uint8_t *		raw,
				 const vbi_sliced *	sliced)
{
	double bit_period = 1.0 / bit_rate;
	double t1 = 10.3e-6 - .5 * bit_period;
	double t2 = t1 + (payload * 8 + 24 + 1) * bit_period;
	double q = (PI / 2.0) * bit_rate;
	double sample_period = 1.0 / sp->sampling_rate;
	uint8_t buf[64];
	unsigned int i;
	double t;
	unsigned int samples_per_line;

	buf[0] = 0x00;
	buf[1] = 0x55; /* clock run-in */
	buf[2] = 0x55;
	buf[3] = frc;

	memcpy (buf + 4, sliced->data, payload);

	buf[payload + 4] = 0x00;

	t = sp->offset / (double) sp->sampling_rate;

	samples_per_line = sp->bytes_per_line
		/ VBI_PIXFMT_BPP (sp->sampling_format);

	for (i = 0; i < samples_per_line; ++i) {
		if (t >= t1 && t < t2)
			PULSE_SEQ (black_level);

		t += sample_period;
	}
}

static void
wss_biphase			(uint8_t *		buf,
				 const vbi_sliced *	sliced)
{
	unsigned int bit;
	unsigned int data;
	unsigned int i;

	/* 29 bit run-in and 24 bit start code, lsb first. */

	buf[0] = 0x00;
	buf[1] = 0x1F; /* 0001 1111 */
	buf[2] = 0xC7; /* 1100 0111 */
	buf[3] = 0x71; /* 0111 0001 */
	buf[4] = 0x1C; /* 000 | 1 1100 */
	buf[5] = 0x8F; /* 1000 1111 */
	buf[6] = 0x07; /* 0000 0111 */
	buf[7] = 0x1F; /*    1 1111 */

	bit = 8 + 29 + 24;
	data = sliced->data[0] + sliced->data[1] * 256;

	for (i = 0; i < 14; ++i) {
		static const unsigned int biphase [] = { 0x38, 0x07 };
		unsigned int byte;
		unsigned int shift;
		unsigned int seq;

		byte = bit >> 3;
		shift = bit & 7;
		bit += 6;

		seq = biphase[data & 1] << shift;
		data >>= 1;

		assert (byte < 31);

		buf[byte] |= seq;
		buf[byte + 1] = seq >> 8;
	}
}

static void
signal_wss_625			(const vbi_sampling_par *sp,
				 unsigned int		black_level,
				 unsigned int		white_level,
				 uint8_t *		raw,
				 const vbi_sliced *	sliced)
{
	double bit_rate = 15625 * 320;
	double t1 = 11.0e-6 - .5 / bit_rate;
	double t4 = t1 + (29 + 24 + 14 * 6) / bit_rate;
	double q = (PI / 2.0) * bit_rate;
	double sample_period = 1.0 / sp->sampling_rate;
	double signal_amp = white_level - black_level;
	uint8_t buf[32];
	unsigned int i;
	double t;
	unsigned int samples_per_line;

	wss_biphase (buf, sliced);

	t = sp->offset / (double) sp->sampling_rate;

	samples_per_line = sp->bytes_per_line
		/ VBI_PIXFMT_BPP (sp->sampling_format);

	for (i = 0; i < samples_per_line; ++i) {
		if (t >= t1 && t < t4)
			PULSE_SEQ (black_level);

		t += sample_period;
	}
}

static void
signal_closed_caption		(const vbi_sampling_par *sp,
				 unsigned int		blank_level,
				 unsigned int		white_level,
				 double			bit_rate,
				 uint8_t *		raw,
				 const vbi_sliced *	sliced)
{
	double bit_period = 1.0 / bit_rate;
	double t1 = 10.5e-6 - .25 * bit_period;
	double t2 = t1 + 7 * bit_period;	/* CRI 7 cycles */
	double t3 = t2 + 1.5 * bit_period;
	double t4 = t3 + 18 * bit_period;	/* 17 bits + raise and fall time */
	double q1 = PI * bit_rate;
	double q = q1 * .5;
	double sample_period = 1.0 / sp->sampling_rate;
	double signal_amp = (white_level - blank_level) * .5;
	unsigned int data;
	unsigned int i;
	double t;
	unsigned int samples_per_line;

	/* Twice 7 data + odd parity, start bit 0 -> 1 */

	data = (sliced->data[1] << 10) + (sliced->data[0] << 2) + 2;

	t = sp->offset / (double) sp->sampling_rate;

	samples_per_line = sp->bytes_per_line
		/ VBI_PIXFMT_BPP (sp->sampling_format);

	for (i = 0; i < samples_per_line; ++i) {
		if (t >= t1 && t < t2) {
			double r;

			r = sin (q1 * (t - t2));
			raw[i] = blank_level + (int)(r * r * signal_amp);
		} else if (t >= t3 && t < t4) {
			double tr;
			unsigned int bit;
			unsigned int seq;

			tr = t - t3;
			bit = tr * bit_rate;
			seq = (data >> bit) & 3;

			PULSE (blank_level);
		}

		t += sample_period;
	}
}

static void
clear_image			(uint8_t *		p,
				 unsigned int		value,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned int		bytes_per_line)
{
	if (width == bytes_per_line) {
		memset (p, value, height * bytes_per_line);
	} else {
		while (height-- > 0) {
			memset (p, value, width);
			p += bytes_per_line;
		}
	}
}

static vbi_bool
signal_u8			(const vbi_sampling_par *sp,
				 unsigned int		blank_level,
				 unsigned int		black_level,
				 unsigned int		white_level,
				 uint8_t *		raw,
				 const vbi_sliced *	sliced,
				 unsigned int		sliced_lines)
{
	unsigned int scan_lines;

	scan_lines = sp->count[0] + sp->count[1];

	clear_image (raw, blank_level,
		     sp->bytes_per_line / VBI_PIXFMT_BPP (sp->sampling_format),
		     scan_lines,
		     sp->bytes_per_line);

	for (; sliced_lines-- > 0; ++sliced) {
		int row;
		uint8_t *raw1;

		if (0 == sliced->line) {
			goto bounds;
		} else if (0 != sp->start[1]
			   && (int) sliced->line >= sp->start[1]) {
			row = sliced->line - sp->start[1];

			if (row >= sp->count[1])
				goto bounds;

			if (sp->interlaced)
				row = row * 2 + 1;
			else
				row += sp->count[0];
		} else if (0 != sp->start[0]
			   && (int) sliced->line >= sp->start[0]) {
			row = sliced->line - sp->start[0];

			if (row >= sp->count[0])
				goto bounds;

			if (sp->interlaced)
				row *= 2;
		} else {
		bounds:
			log ("Sliced line %u out of bounds\n",
			     sliced->line);
			return FALSE;
		}

		raw1 = raw + row * sp->bytes_per_line;

		switch (sliced->id) {
		case VBI_SLICED_TELETEXT_A:
			signal_teletext (sp, black_level,
					 .7 * (white_level - black_level), /* ? */
					 15625 * 397, 0xE7, 37,
					 raw1, sliced);
			break;

		case VBI_SLICED_TELETEXT_B_L10_625:
		case VBI_SLICED_TELETEXT_B_L25_625:
		case VBI_SLICED_TELETEXT_B:
			signal_teletext (sp, black_level,
					 .66 * (white_level - black_level),
					 15625 * 444, 0x27, 42,
					 raw1, sliced);
			break;

		case VBI_SLICED_TELETEXT_C_625:
			signal_teletext (sp, black_level,
					 .7 * (white_level - black_level),
					 15625 * 367, 0xE7, 33,
					 raw1, sliced);
			break;

		case VBI_SLICED_TELETEXT_D_625:
			signal_teletext (sp, black_level,
					 .7 * (white_level - black_level),
					 5642787, 0xA7, 34,
					 raw1, sliced);
			break;

		case VBI_SLICED_CAPTION_625_F1:
		case VBI_SLICED_CAPTION_625_F2:
		case VBI_SLICED_CAPTION_625:
			signal_closed_caption (sp, blank_level, white_level,
					       15625 * 32, raw1, sliced);
			break;

		case VBI_SLICED_WSS_625:
			signal_wss_625 (sp, black_level, white_level,
					raw1, sliced);
			break;

		case VBI_SLICED_TELETEXT_B_525:
			signal_teletext (sp, black_level,
					 .7 * (white_level - black_level),
					 5727272, 0x27, 34,
					 raw1, sliced);
			break;

		case VBI_SLICED_TELETEXT_C_525:
			signal_teletext (sp, black_level,
					 .7 * (white_level - black_level),
					 5727272, 0xE7, 33,
					 raw1, sliced);
			break;

		case VBI_SLICED_TELETEXT_D_525:
			signal_teletext (sp, black_level,
					 .7 * (white_level - black_level),
					 5727272, 0xA7, 34,
					 raw1, sliced);
			break;

		case VBI_SLICED_CAPTION_525_F1:
		case VBI_SLICED_CAPTION_525_F2:
		case VBI_SLICED_CAPTION_525:
			signal_closed_caption (sp, blank_level, white_level,
					       15734 * 32, raw1, sliced);
			break;

		default:
			log ("Service 0x%08x (%s) not supported\n",
			     sliced->id, vbi_sliced_name (sliced->id));
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * @internal
 * @param raw A raw VBI image will be stored here. The buffer
 *   must be large enough for @a sp->count[0] + count[1] lines
 *   of bytes_per_line each, with samples_per_line bytes
 *   actually written.
 * @param sp Describes the raw VBI data generated. sampling_format
 *   must be VBI_PIXFMT_Y8.
 * @param sliced Pointer to an array of vbi_sliced containing the
 *   VBI data to be encoded.
 * @param sliced_lines Number of elements in the vbi_sliced array.
 *
 * Generates a raw VBI image similar to those you get from VBI
 * sampling hardware. The following data services are
 * currently supported: All Teletext services, WSS 625, all Closed
 * Caption services except 2xCC. Sliced data is encoded as is,
 * without verification, except for buffer overflow checks.
 *
 * @return
 * @c FALSE on error.
 */
vbi_bool
_vbi_test_image_vbi		(uint8_t *		raw,
				 unsigned int		raw_size,
				 const vbi_sampling_par *sp,
				 const vbi_sliced *	sliced,
				 unsigned int		sliced_lines)
{
	unsigned int scan_lines;
	unsigned int blank_level;
	unsigned int black_level;
	unsigned int white_level;

	if (!_vbi_sampling_par_valid (sp, vbi_log_on_stderr,
				      /* log_user_data */ NULL))
		return FALSE;

	scan_lines = sp->count[0] + sp->count[1];
	if (scan_lines * sp->bytes_per_line > raw_size)
		return FALSE;

	if (525 == sp->scanning) {
		const double IRE = 200 / 140.0;

		blank_level = (int)(40   * IRE);
		black_level = (int)(47.5 * IRE);
		white_level = (int)(140  * IRE);
	} else {
		const double IRE = 200 / 143.0;

		blank_level = (int)(43   * IRE);
		black_level = blank_level;
		white_level = (int)(143  * IRE);
	}

	return signal_u8 (sp, blank_level, black_level, white_level,
			  raw, sliced, sliced_lines);
}

#define RGBA_TO_RGB16(value)						\
	(+(((value) & 0xF8) >> (3 - 0))					\
	 +(((value) & 0xFC00) >> (10 - 5))				\
	 +(((value) & 0xF80000) >> (19 - 11)))

#define RGBA_TO_RGBA15(value)						\
	(+(((value) & 0xF8) >> (3 - 0))					\
	 +(((value) & 0xF800) >> (11 - 5))				\
	 +(((value) & 0xF80000) >> (19 - 10))				\
	 +(((value) & 0x80000000) >> (31 - 15)))

#define RGBA_TO_ARGB15(value)						\
	(+(((value) & 0xF8) >> (3 - 1))					\
	 +(((value) & 0xF800) >> (11 - 6))				\
	 +(((value) & 0xF80000) >> (19 - 11))				\
	 +(((value) & 0x80000000) >> (31 - 0)))

#define RGBA_TO_RGBA12(value)						\
	(+(((value) & 0xF0) >> (4 - 0))					\
	 +(((value) & 0xF000) >> (12 - 4))				\
	 +(((value) & 0xF00000) >> (20 - 8))				\
	 +(((value) & 0xF0000000) >> (28 - 12)))

#define RGBA_TO_ARGB12(value)						\
	(+(((value) & 0xF0) << -(4 - 12))				\
	 +(((value) & 0xF000) >> (12 - 8))				\
	 +(((value) & 0xF00000) >> (20 - 4))				\
	 +(((value) & 0xF0000000) >> (28 - 0)))

#define RGBA_TO_RGB8(value)						\
	(+(((value) & 0xE0) >> (5 - 0))					\
	 +(((value) & 0xE000) >> (13 - 3))				\
	 +(((value) & 0xC00000) >> (22 - 6)))

#define RGBA_TO_BGR8(value)						\
	(+(((value) & 0xE0) >> (5 - 5))					\
	 +(((value) & 0xE000) >> (13 - 2))				\
	 +(((value) & 0xC00000) >> (22 - 0)))

#define RGBA_TO_RGBA7(value)						\
	(+(((value) & 0xC0) >> (6 - 0))					\
	 +(((value) & 0xE000) >> (13 - 2))				\
	 +(((value) & 0xC00000) >> (22 - 5))				\
	 +(((value) & 0x80000000) >> (31 - 7)))

#define RGBA_TO_ARGB7(value)						\
	(+(((value) & 0xC0) >> (6 - 6))					\
	 +(((value) & 0xE000) >> (13 - 3))				\
	 +(((value) & 0xC00000) >> (22 - 1))				\
	 +(((value) & 0x80000000) >> (31 - 0)))

#define MST1(d, val, mask) (d) = ((d) & ~(mask)) | ((val) & (mask))
#define MST2(d, val, mask) (d) = ((d) & (mask)) | (val)

#define SCAN_LINE_TO_N(conv, n)						\
do {									\
	for (i = 0; i < samples_per_line; ++i) {			\
		uint8_t *dd = d + i * (n);				\
		unsigned int value = s[i] * 0x01010101;			\
		unsigned int mask = ~pixel_mask;			\
									\
		value = conv (value) & pixel_mask;			\
		MST2 (dd[0], value >> 0, mask >> 0);			\
		if (n >= 2)						\
			MST2 (dd[1], value >> 8, mask >> 8);		\
		if (n >= 3)						\
			MST2 (dd[2], value >> 16, mask >> 16);		\
		if (n >= 4)						\
			MST2 (dd[3], value >> 24, mask >> 24);		\
	}								\
} while (0)

#define SCAN_LINE_TO_RGB2(conv, endian)					\
do {									\
	for (i = 0; i < samples_per_line; ++i) {			\
		uint8_t *dd = d + i * 2;				\
		unsigned int value = s[i] * 0x01010101;			\
		unsigned int mask;					\
									\
		value = conv (value) & pixel_mask;			\
		mask = ~pixel_mask;		       			\
		MST2 (dd[0 + endian], value >> 0, mask >> 0);		\
		MST2 (dd[1 - endian], value >> 8, mask >> 8);		\
	}								\
} while (0)

/**
 * @internal
 * @param raw A raw VBI image will be stored here. The buffer
 *   must be large enough for @a sp->count[0] + count[1] lines
 *   of bytes_per_line each, with samples_per_line actually written.
 * @param sp Describes the raw VBI data to generate. When the
 *   sampling_format is a planar YUV format the function writes
 *   the Y plane only.
 * @param pixel_mask This mask selects which color or alpha channel
 *   shall contain VBI data. Depending on @a sp->sampling_format it is
 *   interpreted as 0xAABBGGRR or 0xAAVVUUYY. A value of 0x000000FF
 *   for example writes data in "red bits", not changing other
 *   bits in the @a raw buffer.
 * @param sliced Pointer to an array of vbi_sliced containing the
 *   VBI data to be encoded.
 * @param sliced_lines Number of elements in the vbi_sliced array.
 *
 * Generates a raw VBI image similar to those you get from video
 * capture hardware. Otherwise identical to vbi_test_image_vbi().
 *
 * @return
 * TRUE on success.
 */
/* brightness, contrast parameter? */
vbi_bool
_vbi_test_image_video		(uint8_t *		raw,
				 unsigned int		raw_size,
				 const vbi_sampling_par *sp,
				 unsigned int		pixel_mask,
				 const vbi_sliced *	sliced,
				 unsigned int		sliced_lines)
{
	unsigned int blank_level;
	unsigned int black_level;
	unsigned int white_level;
	vbi_sampling_par sp8;
	unsigned int scan_lines;
	uint8_t *buf;
	uint8_t *s;
	uint8_t *d;

	if (!_vbi_sampling_par_valid (sp, /* log_fn */ NULL,
				      /* log_user_data */ NULL))
		return FALSE;

	scan_lines = sp->count[0] + sp->count[1];

	if (scan_lines * sp->bytes_per_line > raw_size) {
		log ("%u scan_lines * %u bpl > raw_size %u\n",
		     scan_lines, sp->bytes_per_line, raw_size);
		return FALSE;
	}

	switch (sp->sampling_format) {
#if 0
	case VBI_PIXFMT_YVUA24_LE:	/* 0xAAUUVVYY */
	case VBI_PIXFMT_YVU24_LE:	/* 0x00UUVVYY */
#endif
	case VBI_PIXFMT_YVYU:
	case VBI_PIXFMT_VYUY:		/* 0xAAUUVVYY */
		pixel_mask = (+ ((pixel_mask & 0xFF00) << 8)
			      + ((pixel_mask & 0xFF0000) >> 8)
			      + ((pixel_mask & 0xFF0000FF)));
		break;
#if 0
	case VBI_PIXFMT_YUVA24_BE:	/* 0xYYUUVVAA */
#endif
	case VBI_PIXFMT_RGBA32_BE:	/* 0xRRGGBBAA */
		pixel_mask = (+ ((pixel_mask & 0xFF) << 24)
			      + ((pixel_mask & 0xFF00) << 8)
			      + ((pixel_mask & 0xFF0000) >> 8)
			      + ((pixel_mask & 0xFF000000) >> 24));
		break;
#if 0
	case VBI_PIXFMT_YVUA24_BE:	/* 0xYYVVUUAA */
		pixel_mask = (+ ((pixel_mask & 0xFF) << 24)
			      + ((pixel_mask & 0xFFFF00))
			      + ((pixel_mask & 0xFF000000) >> 24));
		break;

	case VBI_PIXFMT_YUV24_BE:	/* 0xAAYYUUVV */
#endif
	case VBI_PIXFMT_ARGB32_BE:	/* 0xAARRGGBB */
	case VBI_PIXFMT_RGB24_BE:
	case VBI_PIXFMT_BGRA15_LE:
	case VBI_PIXFMT_BGRA15_BE:
	case VBI_PIXFMT_ABGR15_LE:
	case VBI_PIXFMT_ABGR15_BE:
#if 0
	case VBI_PIXFMT_BGRA12_LE:
	case VBI_PIXFMT_BGRA12_BE:
	case VBI_PIXFMT_ABGR12_LE:
	case VBI_PIXFMT_ABGR12_BE:
	case VBI_PIXFMT_BGRA7:
	case VBI_PIXFMT_ABGR7:
#endif
		pixel_mask = (+ ((pixel_mask & 0xFF) << 16)
			      + ((pixel_mask & 0xFF0000) >> 16)
			      + ((pixel_mask & 0xFF00FF00)));
		break;
#if 0
	case VBI_PIXFMT_YVU24_BE:	/* 0x00YYVVUU */
#endif
		pixel_mask = (+ ((pixel_mask & 0xFF) << 16)
			      + ((pixel_mask & 0xFFFF00) >> 8));
		break;


	case VBI_PIXFMT_BGRA32_BE:	/* 0xBBGGRRAA */
		pixel_mask = (+ ((pixel_mask & 0xFFFFFF) << 8)
			      + ((pixel_mask & 0xFF000000) >> 24));
		break;

	default:
		break;
	}

	switch (sp->sampling_format) {
	case VBI_PIXFMT_RGB16_LE:
	case VBI_PIXFMT_RGB16_BE:
	case VBI_PIXFMT_BGR16_LE:
	case VBI_PIXFMT_BGR16_BE:
		pixel_mask = RGBA_TO_RGB16 (pixel_mask);
		break;

	case VBI_PIXFMT_RGBA15_LE:
	case VBI_PIXFMT_RGBA15_BE:
	case VBI_PIXFMT_BGRA15_LE:
	case VBI_PIXFMT_BGRA15_BE:
		pixel_mask = RGBA_TO_RGBA15 (pixel_mask);
		break;

	case VBI_PIXFMT_ARGB15_LE:
	case VBI_PIXFMT_ARGB15_BE:
	case VBI_PIXFMT_ABGR15_LE:
	case VBI_PIXFMT_ABGR15_BE:
		pixel_mask = RGBA_TO_ARGB15 (pixel_mask);
		break;

#if 0
	case VBI_PIXFMT_RGBA12_LE:
	case VBI_PIXFMT_RGBA12_BE:
	case VBI_PIXFMT_BGRA12_LE:
	case VBI_PIXFMT_BGRA12_BE:
#endif
		pixel_mask = RGBA_TO_RGBA12 (pixel_mask);
		break;
#if 0
	case VBI_PIXFMT_ARGB12_LE:
	case VBI_PIXFMT_ARGB12_BE:
	case VBI_PIXFMT_ABGR12_LE:
	case VBI_PIXFMT_ABGR12_BE:
#endif
		pixel_mask = RGBA_TO_ARGB12 (pixel_mask);
		break;
#if 0
	case VBI_PIXFMT_RGB8:
		pixel_mask = RGBA_TO_RGB8 (pixel_mask);
		break;

	case VBI_PIXFMT_BGR8:
		pixel_mask = RGBA_TO_BGR8 (pixel_mask);
		break;

	case VBI_PIXFMT_RGBA7:
	case VBI_PIXFMT_BGRA7:
		pixel_mask = RGBA_TO_RGBA7 (pixel_mask);
		break;

	case VBI_PIXFMT_ARGB7:
	case VBI_PIXFMT_ABGR7:
		pixel_mask = RGBA_TO_ARGB7 (pixel_mask);
		break;
#endif
	default:
		break;
	}

	if (0 == pixel_mask) {
		/* Done! :-) */
		return TRUE;
	}

	/* ITU-R Rec BT.601 sampling assumed. */

	if (525 == sp->scanning) {
		blank_level = MAX (0, 16 - 40 * 220 / 100);
		black_level = 16;
		white_level = 16 + 219;
	} else {
		blank_level = MAX (0, 16 - 43 * 220 / 100);
		black_level = 16;
		white_level = 16 + 219;
	}

	sp8 = *sp;

	sp8.sampling_format = VBI_PIXFMT_YUV420; /* VBI_PIXFMT_Y8; */
	sp8.bytes_per_line = sp->bytes_per_line;
	/* sp8.bytes_per_line = sp->samples_per_line; */

	if (!(buf = malloc (scan_lines * sp->bytes_per_line))) {
		log ("Out of memory\n");
		errno = ENOMEM;
		return FALSE;
	}

	if (!signal_u8 (&sp8, blank_level, black_level, white_level,
			buf, sliced, sliced_lines)) {
		free (buf);
		return FALSE;
	}

	s = buf;
	d = raw;

	while (scan_lines-- > 0) {
		unsigned int i;
		unsigned int samples_per_line;

		samples_per_line = sp->bytes_per_line
			/ VBI_PIXFMT_BPP (sp->sampling_format);

		switch (sp->sampling_format) {
#if 0
		case VBI_PIXFMT_NONE:
		case VBI_PIXFMT_RESERVED0:
		case VBI_PIXFMT_RESERVED1:
		case VBI_PIXFMT_RESERVED2:
		case VBI_PIXFMT_RESERVED3:
			break;

		case VBI_PIXFMT_YUV444:
		case VBI_PIXFMT_YVU444:
		case VBI_PIXFMT_YUV422:
		case VBI_PIXFMT_YVU422:
		case VBI_PIXFMT_YUV411:
		case VBI_PIXFMT_YVU411:
#endif
		case VBI_PIXFMT_YUV420:
#if 0
		case VBI_PIXFMT_YVU420:
		case VBI_PIXFMT_YUV410:
		case VBI_PIXFMT_YVU410:
		case VBI_PIXFMT_Y8:
#endif
			for (i = 0; i < samples_per_line; ++i)
				MST1 (d[i], s[i], pixel_mask);
			break;

#if 0
		case VBI_PIXFMT_YUVA24_LE:
		case VBI_PIXFMT_YVUA24_LE:
		case VBI_PIXFMT_YUVA24_BE:
		case VBI_PIXFMT_YVUA24_BE:
#endif
		case VBI_PIXFMT_RGBA32_LE:
		case VBI_PIXFMT_RGBA32_BE:
		case VBI_PIXFMT_BGRA32_LE:
		case VBI_PIXFMT_BGRA32_BE:
			SCAN_LINE_TO_N (+, 4);
			break;
#if 0
		case VBI_PIXFMT_YUV24_LE:
		case VBI_PIXFMT_YUV24_BE:
		case VBI_PIXFMT_YVU24_LE:
		case VBI_PIXFMT_YVU24_BE:
#endif
		case VBI_PIXFMT_RGB24_LE:
		case VBI_PIXFMT_RGB24_BE:
			SCAN_LINE_TO_N (+, 3);
			break;

		case VBI_PIXFMT_YUYV:
		case VBI_PIXFMT_YVYU:
			for (i = 0; i < samples_per_line; i += 2) {
				uint8_t *dd = d + i * 2;
				unsigned int uv = (s[i] + s[i + 1] + 1) >> 1;

				MST1 (dd[0], s[i], pixel_mask);
				MST1 (dd[1], uv, pixel_mask >> 8);
				MST1 (dd[2], s[i + 1], pixel_mask);
				MST1 (dd[3], uv, pixel_mask >> 16);
			}
			break;

		case VBI_PIXFMT_UYVY:
		case VBI_PIXFMT_VYUY:
			for (i = 0; i < samples_per_line; i += 2) {
				uint8_t *dd = d + i * 2;
				unsigned int uv = (s[i] + s[i + 1] + 1) >> 1;

				MST1 (dd[0], uv, pixel_mask >> 8);
				MST1 (dd[1], s[i], pixel_mask);
				MST1 (dd[2], uv, pixel_mask >> 16);
				MST1 (dd[3], s[i + 1], pixel_mask);
			}
			break;

		case VBI_PIXFMT_RGB16_LE:
		case VBI_PIXFMT_BGR16_LE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_RGB16, 0);
			break;

		case VBI_PIXFMT_RGB16_BE:
		case VBI_PIXFMT_BGR16_BE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_RGB16, 1);
			break;

		case VBI_PIXFMT_RGBA15_LE:
		case VBI_PIXFMT_BGRA15_LE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_RGBA15, 0);
			break;

		case VBI_PIXFMT_RGBA15_BE:
		case VBI_PIXFMT_BGRA15_BE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_RGBA15, 1);
			break;

		case VBI_PIXFMT_ARGB15_LE:
		case VBI_PIXFMT_ABGR15_LE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_ARGB15, 0);
			break;

		case VBI_PIXFMT_ARGB15_BE:
		case VBI_PIXFMT_ABGR15_BE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_ARGB15, 1);
			break;
#if 0
		case VBI_PIXFMT_RGBA12_LE:
		case VBI_PIXFMT_BGRA12_LE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_RGBA12, 0);
			break;

		case VBI_PIXFMT_RGBA12_BE:
		case VBI_PIXFMT_BGRA12_BE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_RGBA12, 1);
			break;

		case VBI_PIXFMT_ARGB12_LE:
		case VBI_PIXFMT_ABGR12_LE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_ARGB12, 0);
			break;

		case VBI_PIXFMT_ARGB12_BE:
		case VBI_PIXFMT_ABGR12_BE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_ARGB12, 1);
			break;

		case VBI_PIXFMT_RGB8:
			SCAN_LINE_TO_N (RGBA_TO_RGB8, 1);
			break;

		case VBI_PIXFMT_BGR8:
			SCAN_LINE_TO_N (RGBA_TO_BGR8, 1);
			break;

		case VBI_PIXFMT_RGBA7:
		case VBI_PIXFMT_BGRA7:
			SCAN_LINE_TO_N (RGBA_TO_RGBA7, 1);
			break;

		case VBI_PIXFMT_ARGB7:
		case VBI_PIXFMT_ABGR7:
			SCAN_LINE_TO_N (RGBA_TO_ARGB7, 1);
			break;
#endif
		}

		s += sp8.bytes_per_line;
		d += sp->bytes_per_line;
	}

	free (buf);

	return TRUE;
}
