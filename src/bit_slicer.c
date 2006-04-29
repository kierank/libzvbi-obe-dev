/*
 *  libzvbi - Bit slicer
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
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

/* $Id: bit_slicer.c,v 1.6 2006/04/29 05:55:35 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include "misc.h"
#include "bit_slicer.h"

/* XXX not in 0.2.x */
#define VBI_PIXFMT_Y8 VBI_PIXFMT_YUV420
#define VBI_PIXFMT_RGB24_LE VBI_PIXFMT_RGB24
#define VBI_PIXFMT_BGR24_LE VBI_PIXFMT_BGR24
#define VBI_PIXFMT_RGB8 101
#define vbi_pixfmt_bytes_per_pixel VBI_PIXFMT_BPP

#ifndef BIT_SLICER_LOG
#  define BIT_SLICER_LOG 0
#endif

/*
 * $addtogroup BitSlicer Bit slicer
 * $ingroup Raw
 * $brief Converting a single scan line of raw VBI
 *   data to sliced VBI data.
 *
 * These are low level functions most useful if you want to decode
 * data services not covered by libzvbi. Usually you will want to
 * use the raw VBI decoder, converting several lines of different
 * data services at once.
 */

/* Read a green sample, e.g. rrrrrggg gggbbbbb. endian is const. */
#define GREEN2(raw, endian)						\
	(((raw)[0 + endian] + (raw)[1 - endian] * 256) & bs->green_mask)

/* Read a sample with pixfmt conversion. fmt is const. */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define GREEN(raw, fmt)							\
	((fmt == VBI_PIXFMT_RGB8) ?					\
	 *(const uint8_t *)(raw) & bs->green_mask :			\
	 ((fmt == VBI_PIXFMT_RGB16_LE) ?				\
	  *(const uint16_t *)(raw) & bs->green_mask :			\
	  ((fmt == VBI_PIXFMT_RGB16_BE) ?				\
	   GREEN2 (raw, 1) :						\
	   (raw)[0])))
#elif __BYTE_ORDER == __BIG_ENDIAN
#define GREEN(raw, fmt)							\
	((fmt == VBI_PIXFMT_RGB8) ?					\
	 *(const uint8_t *)(raw) & bs->green_mask :			\
	 ((fmt == VBI_PIXFMT_RGB16_LE) ?				\
	  GREEN2 (raw, 0) :						\
	  ((fmt == VBI_PIXFMT_RGB16_BE) ?				\
	   *(const uint16_t *)(raw) & bs->green_mask :			\
	   (raw)[0])))
#else
#define GREEN(raw, fmt)							\
	((fmt == VBI_PIXFMT_RGB8) ?					\
	 *(const uint8_t *)(raw) & bs->green_mask :			\
	 ((fmt == VBI_PIXFMT_RGB16_LE) ?				\
	  GREEN2 (raw, 0) :						\
	  ((fmt == VBI_PIXFMT_RGB16_BE) ?				\
	   GREEN2 (raw, 1) :						\
	   (raw)[0])))
#endif

/* raw0 = raw[index >> 8], linear interpolated.
   fmt is const, vbi_pixfmt_bytes_per_pixel(fmt) is const. */
#define SAMPLE(raw, index, fmt)						\
do {									\
	const uint8_t *r;						\
									\
	r = (raw) + ((index) >> 8) * vbi_pixfmt_bytes_per_pixel (fmt);	\
	raw0 = GREEN (r + 0, fmt);					\
	raw1 = GREEN (r + vbi_pixfmt_bytes_per_pixel (fmt), fmt);	\
	raw0 = (int)(raw1 - raw0) * ((index) & 255) + (raw0 << 8);	\
} while (0)

#define BIT_SLICER(fmt, oversampling, thresh_frac)			\
static vbi_bool								\
bit_slicer_##fmt		(vbi3_bit_slicer *	bs,		\
				 uint8_t *		buf,		\
				 const uint8_t *	raw)		\
{									\
	unsigned int i, j, k;						\
	unsigned int cl;	/* clock */				\
	unsigned int thresh0;	/* old 0/1 threshold (fpt) */		\
	unsigned int tr;	/* current threshold (integer) */	\
	unsigned int c;		/* current byte */			\
	unsigned int t;		/* oversampled sample */		\
	unsigned int raw0;	/* oversampling temporary */		\
	unsigned int raw1;						\
	unsigned char b1;	/* previous bit */			\
									\
	thresh0 = bs->thresh;						\
	raw += bs->skip;	/* e.g. red byte */			\
									\
	cl = 0;								\
	c = 0;								\
	b1 = 0;								\
						                        \
	for (i = bs->cri_bytes; i > 0; --i) {				\
		/* chg = abs (raw[1] - raw[0]) / 255; */		\
		/* thr = thr * (1 - chg) + raw[0] * chg; */		\
		tr = bs->thresh >> thresh_frac;				\
		raw0 = GREEN (raw, VBI_PIXFMT_##fmt);			\
		raw1 = GREEN (raw + vbi_pixfmt_bytes_per_pixel		\
			      (VBI_PIXFMT_##fmt), VBI_PIXFMT_##fmt);	\
		raw1 -= raw0;						\
		bs->thresh += (int)(raw0 - tr) * (int) ABS ((int) raw1); \
		/* for (j = 0; j < oversampling; ++j) */                \
		/*   t = (raw[0] * (oversampling - j) + raw[1] * j) */  \
		/*     / oversampling; */				\
		t = raw0 * oversampling;				\
									\
		for (j = oversampling; j > 0; --j) {			\
			unsigned char b; /* current bit */		\
									\
			b = ((t + (oversampling / 2))			\
				/ oversampling) >= tr;			\
									\
    			if (unlikely (b ^ b1)) {			\
				cl = bs->oversampling_rate >> 1;	\
			} else {					\
				cl += bs->cri_rate;			\
									\
				if (cl >= bs->oversampling_rate) {	\
					cl -= bs->oversampling_rate;	\
					c = c * 2 + b;			\
					if ((c & bs->cri_mask)		\
					    == bs->cri)			\
						goto payload;		\
				}					\
			}						\
									\
			b1 = b;						\
									\
			if (oversampling > 1)				\
				t += raw1;				\
		}							\
									\
		raw += vbi_pixfmt_bytes_per_pixel (VBI_PIXFMT_##fmt);	\
	}								\
									\
	bs->thresh = thresh0;						\
									\
	return FALSE;							\
									\
payload:								\
	i = bs->phase_shift; /* current bit position << 8 */		\
	tr *= 256;							\
	c = 0;								\
									\
	for (j = bs->frc_bits; j > 0; --j) {				\
		SAMPLE (raw, i, VBI_PIXFMT_##fmt);			\
		c = c * 2 + (raw0 >= tr);				\
		i += bs->step; /* next bit */				\
	}								\
									\
	if (c != bs->frc)						\
		return FALSE;						\
									\
	switch (bs->endian) {						\
	case 3: /* bitwise, lsb first */				\
		for (j = 0; j < bs->payload; ++j) {			\
			SAMPLE (raw, i, VBI_PIXFMT_##fmt);		\
			c = (c >> 1) + ((raw0 >= tr) << 7);		\
			i += bs->step;					\
			if ((j & 7) == 7)				\
				*buf++ = c;				\
		}							\
		*buf = c >> ((8 - bs->payload) & 7);			\
		break;							\
									\
	case 2: /* bitwise, msb first */				\
		for (j = 0; j < bs->payload; ++j) {			\
			SAMPLE (raw, i, VBI_PIXFMT_##fmt);		\
			c = c * 2 + (raw0 >= tr);			\
			i += bs->step;					\
			if ((j & 7) == 7)				\
				*buf++ = c;				\
		}							\
		*buf = c & ((1 << (bs->payload & 7)) - 1);		\
		break;							\
									\
	case 1: /* octets, lsb first */					\
		for (j = bs->payload; j > 0; --j) {			\
			for (k = 0, c = 0; k < 8; ++k) {		\
				SAMPLE (raw, i, VBI_PIXFMT_##fmt);	\
				c += (raw0 >= tr) << k;			\
				i += bs->step;				\
			}						\
			*buf++ = c;					\
		}							\
		break;							\
									\
	default: /* octets, msb first */				\
		for (j = bs->payload; j > 0; --j) {			\
			for (k = 0; k < 8; ++k) {			\
				SAMPLE (raw, i, VBI_PIXFMT_##fmt);	\
				c = c * 2 + (raw0 >= tr);		\
				i += bs->step;				\
			}						\
			*buf++ = c;					\
		}							\
		break;							\
	}								\
									\
	return TRUE;							\
}

BIT_SLICER (Y8, 4, 9)		/* any format with 0 bytes between Y or G */
BIT_SLICER (YUYV, 4, 9)		/* 1 byte */
BIT_SLICER (RGB24_LE, 4, 9)	/* 2 bytes */
BIT_SLICER (RGBA32_LE, 4, 9)	/* 3 bytes between Y or G */
BIT_SLICER (RGB16_LE, 4, bs->thresh_frac) /* any 16 bit RGB LE */
BIT_SLICER (RGB16_BE, 4, bs->thresh_frac) /* any 16 bit RGB BE */
#if 0 /* XXX not in 0.2.x */
BIT_SLICER (RGB8, 8, bs->thresh_frac) /* any 8 bit RGB */
#endif

/*
 * $param bs Pointer to vbi3_bit_slicer object allocated with
 *   vbi3_bit_slicer_new().
 * $param buffer Output data.
 * $param buffer_size Size of the output buffer. The buffer must be
 +   large enough to store the number of bits given as $a payload to
 *   vbi3_bit_slicer_new().
 * $param raw Input data. At least the number of pixels or samples
 *  given as $a samples_per_line to vbi3_bit_slicer_new().
 * 
 * Decodes one scan line of raw vbi data. Note the bit slicer tries
 * to adapt to the average signal amplitude, you should avoid
 * using the same vbi3_bit_slicer object for data from different
 * devices.
 *
 * $return
 * $c FALSE if the raw data does not contain the expected
 * information, i. e. the CRI/FRC has not been found. This may also
 * result from a too weak or noisy signal. Error correction must be
 * implemented at a higher layer.
 */
vbi_bool
vbi3_bit_slicer_slice		(vbi3_bit_slicer *	bs,
				 uint8_t *		buffer,
				 unsigned int		buffer_size,
				 const uint8_t *	raw)
{
	assert (NULL != bs);
	assert (NULL != buffer);
	assert (NULL != raw);

	if (bs->payload > buffer_size * 8) {
		vbi_log_printf (bs->log_fn, bs->log_user_data,
				VBI_LOG_ERR, __FUNCTION__,
				"buffer_size %u < %u bits of payload",
				buffer_size * 8, bs->payload);
		return FALSE;
	}

	return bs->func (bs, buffer, raw);
}

/*
 * $internal
 */
void
_vbi3_bit_slicer_destroy	(vbi3_bit_slicer *	bs)
{
	assert (NULL != bs);

	/* Make unusable. */
	CLEAR (*bs);
}

/*
 * $internal
 *
 * See vbi3_bit_slicer_new().
 *
 * $a sample_offset skips a number of samples at the start of the line
 * ($a samples_per_line includes $a sample_offset), this is used by
 * the vbi_raw_decoder.
 */
vbi_bool
_vbi3_bit_slicer_init		(vbi3_bit_slicer *	bs,
				 vbi_pixfmt		sample_format,
				 unsigned int		sampling_rate,
				 unsigned int		sample_offset,
				 unsigned int		samples_per_line,
				 unsigned int		cri,
				 unsigned int		cri_mask,
				 unsigned int		cri_bits,
				 unsigned int		cri_rate,
				 unsigned int		cri_end,
				 unsigned int		frc,
				 unsigned int		frc_bits,
				 unsigned int		payload_bits,
				 unsigned int		payload_rate,
				 vbi_modulation		modulation)
{
	unsigned int c_mask;
	unsigned int f_mask;
	unsigned int bytes_per_sample;
	unsigned int oversampling;
	unsigned int data_bits;
	unsigned int data_samples;
	unsigned int cri_samples;
	unsigned int skip;

	assert (NULL != bs);
	assert (cri_bits <= 32);
	assert (frc_bits <= 32);
	assert (payload_bits <= 32767);
	assert (samples_per_line <= 32767);

	if (BIT_SLICER_LOG) {
		bs->log_fn = vbi_log_on_stderr;
		bs->log_user_data = NULL;
	}

	if (cri_rate > sampling_rate) {
		vbi_log_printf (bs->log_fn, bs->log_user_data,
				VBI_LOG_ERR, __FUNCTION__,
				"cri_rate %u > sampling_rate %u.",
				cri_rate, sampling_rate);
		goto failure;
	}

	if (payload_rate > sampling_rate) {
		vbi_log_printf (bs->log_fn, bs->log_user_data,
				VBI_LOG_ERR, __FUNCTION__,
				"payload_rate %u > sampling_rate %u.",
				payload_rate, sampling_rate);
		goto failure;
	}

	c_mask = (cri_bits == 32) ? ~0U : (1U << cri_bits) - 1;
	f_mask = (frc_bits == 32) ? ~0U : (1U << frc_bits) - 1;

	oversampling = 4;
	skip = 0;

	/* 0-1 threshold, start value. */
	bs->thresh = 105 << 9; /* fixed point value */
	bs->thresh_frac = 9;

	switch (sample_format) {
#if 0 /* XXX not in 0.2.x */
	case VBI_PIXFMT_YUV444:
	case VBI_PIXFMT_YVU444:
	case VBI_PIXFMT_YUV422:
	case VBI_PIXFMT_YVU422:
	case VBI_PIXFMT_YUV411:
	case VBI_PIXFMT_YVU411:
#endif
	case VBI_PIXFMT_YUV420:
#if 0 /* XXX not in 0.2.x */
	case VBI_PIXFMT_YVU420:
	case VBI_PIXFMT_YUV410:
	case VBI_PIXFMT_YVU410:
	case VBI_PIXFMT_Y8:
#endif
		bs->func = bit_slicer_Y8;
		bytes_per_sample = 1;
		break;

#if 0 /* XXX not in 0.2.x */
	case VBI_PIXFMT_YUVA24_LE:
	case VBI_PIXFMT_YVUA24_LE:
		bs->func = bit_slicer_RGBA24_LE;
		bytes_per_sample = 4;
		break;

	case VBI_PIXFMT_YUVA24_BE:
	case VBI_PIXFMT_YVUA24_BE:
		bs->func = bit_slicer_RGBA24_LE;
		skip = 3;
		bytes_per_sample = 4;
		break;

	case VBI_PIXFMT_YUV24_LE:
	case VBI_PIXFMT_YVU24_LE:
	        bs->func = bit_slicer_RGB24_LE;
		bytes_per_sample = 3;
		break;

	case VBI_PIXFMT_YUV24_BE:
	case VBI_PIXFMT_YVU24_BE:
	        bs->func = bit_slicer_RGB24_LE;
		skip = 2;
		bytes_per_sample = 3;
		break;
#endif /* 0 */

	case VBI_PIXFMT_YUYV:
	case VBI_PIXFMT_YVYU:
		bs->func = bit_slicer_YUYV;
		bytes_per_sample = 2;
		break;

	case VBI_PIXFMT_UYVY:
	case VBI_PIXFMT_VYUY:
		bs->func = bit_slicer_YUYV;
		skip = 1;
		bytes_per_sample = 2;
		break;

	case VBI_PIXFMT_RGBA32_LE:
	case VBI_PIXFMT_BGRA32_LE:
		bs->func = bit_slicer_RGBA32_LE;
		skip = 1;
		bytes_per_sample = 4;
		break;

	case VBI_PIXFMT_RGBA32_BE:
	case VBI_PIXFMT_BGRA32_BE:
		bs->func = bit_slicer_RGBA32_LE;
		skip = 2;
		bytes_per_sample = 4;
		break;

	case VBI_PIXFMT_RGB24_LE:
	case VBI_PIXFMT_BGR24_LE:
	        bs->func = bit_slicer_RGB24_LE;
		skip = 1;
		bytes_per_sample = 3;
		break;

	case VBI_PIXFMT_RGB16_LE:
	case VBI_PIXFMT_BGR16_LE:
		bs->func = bit_slicer_RGB16_LE;
		bs->green_mask = 0x07E0;
		bs->thresh = 105 << (5 - 2 + 12);
		bs->thresh_frac = 12;
		bytes_per_sample = 2;
		break;

	case VBI_PIXFMT_RGB16_BE:
	case VBI_PIXFMT_BGR16_BE:
		bs->func = bit_slicer_RGB16_BE;
		bs->green_mask = 0x07E0;
		bs->thresh = 105 << (5 - 2 + 12);
		bs->thresh_frac = 12;
		bytes_per_sample = 2;
		break;

	case VBI_PIXFMT_RGBA15_LE:
	case VBI_PIXFMT_BGRA15_LE:
		bs->func = bit_slicer_RGB16_LE;
		bs->green_mask = 0x03E0;
		bs->thresh = 105 << (5 - 3 + 11);
		bs->thresh_frac = 11;
		bytes_per_sample = 2;
		break;

	case VBI_PIXFMT_RGBA15_BE:
	case VBI_PIXFMT_BGRA15_BE:
		bs->func = bit_slicer_RGB16_BE;
		bs->green_mask = 0x03E0;
		bs->thresh = 105 << (5 - 3 + 11);
		bs->thresh_frac = 11;
		bytes_per_sample = 2;
		break;

	case VBI_PIXFMT_ARGB15_LE:
	case VBI_PIXFMT_ABGR15_LE:
		bs->func = bit_slicer_RGB16_LE;
		bs->green_mask = 0x07C0;
		bs->thresh = 105 << (6 - 3 + 12);
		bs->thresh_frac = 12;
		bytes_per_sample = 2;
		break;

	case VBI_PIXFMT_ARGB15_BE:
	case VBI_PIXFMT_ABGR15_BE:
		bs->func = bit_slicer_RGB16_BE;
		bs->green_mask = 0x07C0;
		bs->thresh = 105 << (6 - 3 + 12);
		bs->thresh_frac = 12;
		bytes_per_sample = 2;
		break;

#if 0 /* XXX not in 0.2.x */
	case VBI_PIXFMT_RGBA12_LE:
	case VBI_PIXFMT_BGRA12_LE:
		bs->func = bit_slicer_RGB16_LE;
		bs->green_mask = 0x00F0;
		bs->thresh = 105 << (4 - 4 + 9);
		bs->thresh_frac = 9;
		bytes_per_sample = 2;
		break;

	case VBI_PIXFMT_RGBA12_BE:
	case VBI_PIXFMT_BGRA12_BE:
		bs->func = bit_slicer_RGB16_BE;
		bs->green_mask = 0x00F0;
		bs->thresh = 105 << (4 - 4 + 9);
		bs->thresh_frac = 9;
		bytes_per_sample = 2;
		break;

	case VBI_PIXFMT_ARGB12_LE:
	case VBI_PIXFMT_ABGR12_LE:
		bs->func = bit_slicer_RGB16_LE;
		bs->green_mask = 0x0F00;
		bs->thresh = 105 << (8 - 4 + 13);
		bs->thresh_frac = 13;
		bytes_per_sample = 2;
		break;

	case VBI_PIXFMT_ARGB12_BE:
	case VBI_PIXFMT_ABGR12_BE:
		bs->func = bit_slicer_RGB16_BE;
		bs->green_mask = 0x0F00;
		bs->thresh = 105 << (8 - 4 + 13);
		bs->thresh_frac = 13;
		bytes_per_sample = 2;
		break;

	case VBI_PIXFMT_RGB8:
	case VBI_PIXFMT_ARGB7:
	case VBI_PIXFMT_ABGR7:
		bs->func = bit_slicer_RGB8;
		bs->green_mask = 0x38;
		bs->thresh = 105 << (3 - 5 + 7);
		bs->thresh_frac = 7;
		bytes_per_sample = 1;
		oversampling = 8;
		break;

	case VBI_PIXFMT_BGR8:
	case VBI_PIXFMT_RGBA7:
	case VBI_PIXFMT_BGRA7:
		bs->func = bit_slicer_RGB8;
		bs->green_mask = 0x1C;
		bs->thresh = 105 << (2 - 5 + 6);
		bs->thresh_frac = 6;
		bytes_per_sample = 1;
		oversampling = 8;
		break;
#endif

	default:
		fprintf (stderr, "%s:%u: Unknown pixfmt 0x%x\n",
			 __FILE__, __LINE__,
			 (unsigned int) sample_format);
		exit (EXIT_FAILURE);
	}

	bs->skip = sample_offset * bytes_per_sample + skip;

	bs->cri_mask = cri_mask & c_mask;
	bs->cri = cri & bs->cri_mask;

	/* We stop searching for CRI when CRI, FRC and payload
	   cannot possibly fit anymore. Additionally this eliminates
	   a data end check in the payload loop. */
	cri_samples = (sampling_rate * (int64_t) cri_bits) / cri_rate;

	data_bits = payload_bits + frc_bits;
	data_samples = (sampling_rate * (int64_t) data_bits) / payload_rate;

	if (0)
		fprintf (stderr, "%s %u %u %u %u\n",
			 __FUNCTION__,
			 sample_offset, samples_per_line,
			 cri_samples, data_samples); 

	if ((sample_offset > samples_per_line)
	    || ((cri_samples + data_samples)
		> (samples_per_line - sample_offset))) {
		vbi_log_printf (bs->log_fn, bs->log_user_data,
				VBI_LOG_ERR, __FUNCTION__,
				"%u samples_per_line too small for "
				"sample_offset %u + %u cri_bits (%u samples) "
				"+ %u frc_bits and %u payload_bits "
				"(%u samples).",
				samples_per_line, sample_offset,
				cri_bits, cri_samples,
				frc_bits, payload_bits, data_samples);
		goto failure;
	}

	cri_end = MIN (cri_end, samples_per_line - data_samples);

	bs->cri_bytes = (cri_end - sample_offset) * bytes_per_sample;
	bs->cri_rate = cri_rate;

	bs->oversampling_rate = sampling_rate * oversampling;

	bs->frc = frc & f_mask;
	bs->frc_bits = frc_bits;

	/* Payload bit distance in 1/256 raw samples. */
	bs->step = (sampling_rate * (int64_t) 256) / payload_rate;

	if (payload_bits & 7) {
		/* Use bit routines. */
		bs->payload = payload_bits;
		bs->endian = 3;
	} else {
		/* Use faster octet routines. */
		bs->payload = payload_bits >> 3;
		bs->endian = 1;
	}

	switch (modulation) {
	case VBI_MODULATION_NRZ_MSB:
		--bs->endian;

		/* fall through */

	case VBI_MODULATION_NRZ_LSB:
		bs->phase_shift	= (int)
			(sampling_rate * 256.0 / cri_rate * .5
			 + bs->step * .5 + 128);
		break;

	case VBI_MODULATION_BIPHASE_MSB:
		--bs->endian;

		/* fall through */

	case VBI_MODULATION_BIPHASE_LSB:
		/* Phase shift between the NRZ modulated CRI and the
		   biphase modulated rest. */
		bs->phase_shift	= (int)
			(sampling_rate * 256.0 / cri_rate * .5
			 + bs->step * .25 + 128);
		break;
	}

	return TRUE;

 failure:
	_vbi3_bit_slicer_destroy (bs);

	return FALSE;
}

/*
 * $param bs Pointer to a vbi3_bit_slicer object allocated with
 *   vbi3_bit_slicer_new(), can be NULL.
 *
 * Deletes a vbi3_bit_slicer object.
 */
void
vbi3_bit_slicer_delete		(vbi3_bit_slicer *	bs)
{
	if (NULL == bs)
		return;

	_vbi3_bit_slicer_destroy (bs);

	free (bs);
}

/*
 * $param sample_format Format of the raw data, see vbi_pixfmt.
 * $param sampling_rate Raw vbi sampling rate in Hz, that is the number
 *   of samples or pixels sampled per second by the hardware.
 * $param samples_per_line Number of samples or pixels in one raw vbi
 *   line later passed to vbi3_bit_slicer_slice(). This limits the number of
 *   bytes read from the raw data buffer. Do not to confuse the value
 *   with bytes per line.
 * $param cri The Clock Run In is a NRZ modulated sequence of '1'
 *   and '0' bits prepending most data transmissions to synchronize data
 *   acquisition circuits. The bit slicer compares the bits in this
 *   word, lsb last transmitted, against the transmitted CRI. Decoding
 *   of FRC and payload starts with the next bit after a match, thus
 *   $a cri must contain a unique bit sequence. For example 0xAB to
 *   match '101010101011xxx'.
 * $param cri_mask Of the CRI bits in $a cri, only these bits are
 *   significant for a match. For instance it is wise not to rely on
 *   the very first CRI bits transmitted.
 * $param cri_bits Number of CRI bits, must not exceed 32.
 * $param cri_rate CRI bit rate in Hz, the number of CRI bits
 *   transmitted per second.
 * $param cri_end Number of samples between the start of the line and
 *   the latest possible end of the CRI. This is useful when
 *   the transmission is much shorter than samples_per_line, otherwise
 *   just pass $c ~0 and a limit will be calculated.
 * $param frc The FRaming Code usually following the CRI is a bit
 *   sequence identifying the data service. There is no mask parameter,
 *   all bits must match. We assume FRC has the same $a modulation as
 *   the payload and is transmitted at $a payload_rate.
 * $param frc_bits Number of FRC bits, must not exceed 32.
 * $param payload_bits Number of payload bits. Only this data
 *   will be stored in the vbi3_bit_slicer_slice() output. If this number
 *   is no multiple of eight, the most significant bits of the
 *   last byte are undefined.
 * $param payload_rate Payload bit rate in Hz, the number of payload
 *   bits transmitted per second.
 * $param modulation Modulation of the payload, see vbi_modulation.
 * 
 * Allocates and initializes a vbi3_bit_slicer object for use with
 * vbi3_bit_slicer_slice(). This is a low level function, see also
 * vbi3_raw_decoder_new().
 *
 * $returns
 * NULL when out of memory or parameters are invalid (e. g.
 * samples_per_line too small to contain CRI, FRC and payload).
 * Otherwise a pointer to an opaque vbi3_bit_slicer object, which
 * must be deleted with vbi3_bit_slicer_delete() when done.
 */
vbi3_bit_slicer *
vbi3_bit_slicer_new		(vbi_pixfmt		sample_format,
				 unsigned int		sampling_rate,
				 unsigned int		samples_per_line,
				 unsigned int		cri,
				 unsigned int		cri_mask,
				 unsigned int		cri_bits,
				 unsigned int		cri_rate,
				 unsigned int		cri_end,
				 unsigned int		frc,
				 unsigned int		frc_bits,
				 unsigned int		payload_bits,
				 unsigned int		payload_rate,
				 vbi_modulation		modulation)
{
	vbi3_bit_slicer *bs;

	bs = malloc (sizeof (*bs));
	if (NULL == bs)
		return NULL;

        if (!_vbi3_bit_slicer_init (bs,
				   sample_format, sampling_rate,
				   /* offset */ 0, samples_per_line,
				   cri, cri_mask, cri_bits, cri_rate, cri_end,
				   frc, frc_bits,
				   payload_bits, payload_rate, modulation)) {
		free (bs);
		bs = NULL;
	}

	return bs;
}
