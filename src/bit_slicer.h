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

/* $Id: bit_slicer.h,v 1.3 2005/01/19 04:23:52 mschimek Exp $ */

#ifndef __ZVBI_BIT_SLICER_H__
#define __ZVBI_BIT_SLICER_H__

#ifndef TEST
#include <inttypes.h>		/* uint8_t, uint16_t */
#include "decoder.h"
#endif

#if 0 /* in decoder.h */

/*
 * $brief Modulation used for VBI data transmission.
 */
typedef enum {
	/*
	 * The data is 'non-return to zero' coded, logical '1' bits
	 * are described by high sample values, logical '0' bits by
	 * low values. The data is last significant bit first transmitted.
	 */
	VBI_MODULATION_NRZ_LSB,
	/*
	 * 'Non-return to zero' coded, most significant bit first
	 * transmitted.
	 */
	VBI_MODULATION_NRZ_MSB,
	/*
	 * The data is 'bi-phase' coded. Each data bit is described
	 * by two complementary signalling elements, a logical '1'
	 * by a sequence of '10' elements, a logical '0' by a '01'
	 * sequence. The data is last significant bit first transmitted.
	 */
	VBI_MODULATION_BIPHASE_LSB,
	/* 'Bi-phase' coded, most significant bit first transmitted. */
	VBI_MODULATION_BIPHASE_MSB
} vbi_modulation;

#endif

/*
 * $brief Bit slicer context.
 *
 * The contents of this structure are private.
 * Call vbi_bit_slicer_new() to allocate a bit slicer context.
 */
typedef struct _vbi3_bit_slicer vbi3_bit_slicer;

extern vbi_bool
vbi3_bit_slicer_slice		(vbi3_bit_slicer *	bs,
				 uint8_t *		buffer,
				 unsigned int		buffer_size,
				 const uint8_t *	raw);
extern void
vbi3_bit_slicer_delete		(vbi3_bit_slicer *	bs);
extern vbi3_bit_slicer *
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
				 vbi_modulation		modulation) vbi_alloc;

/* Private */

typedef vbi_bool
_vbi3_bit_slicer_fn		(vbi3_bit_slicer *	bs,
				 uint8_t *		buffer,
				 const uint8_t *	raw);

/** @internal */
struct _vbi3_bit_slicer {
	_vbi3_bit_slicer_fn *	func;
	unsigned int		cri;
	unsigned int		cri_mask;
	unsigned int		thresh;
	unsigned int		thresh_frac;
	unsigned int		cri_bytes;
	unsigned int		cri_rate;
	unsigned int		oversampling_rate;
	unsigned int		phase_shift;
	unsigned int		step;
	unsigned int		frc;
	unsigned int		frc_bits;
	unsigned int		payload;
	unsigned int		endian;
	unsigned int		skip;
	unsigned int		green_mask;
};

extern void
_vbi3_bit_slicer_destroy	(vbi3_bit_slicer *	bs);
extern vbi_bool
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
				 vbi_modulation		modulation);
/* $} */

#endif /* __ZVBI_BIT_SLICER_H__ */
