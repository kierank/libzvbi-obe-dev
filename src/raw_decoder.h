/*
 *  libzvbi - Raw VBI decoder
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

/* $Id: raw_decoder.h,v 1.2 2004/12/13 07:15:26 mschimek Exp $ */

#ifndef RAW_DECODER_H
#define RAW_DECODER_H

#ifndef TEST
#include <stdio.h>
#include "decoder.h"
#endif

#include "bit_slicer.h"

/* XXX sampling_par.h */
typedef vbi_raw_decoder vbi_sampling_par;

#define VBI_VIDEOSTD_SET_EMPTY 0
#define VBI_VIDEOSTD_SET_PAL_BG 1
#define VBI_VIDEOSTD_SET_625_50 1
#define VBI_VIDEOSTD_SET_525_60 2
#define VBI_VIDEOSTD_SET_ALL 3
typedef uint64_t vbi_videostd_set;

/*
 * $ingroup RawDecoder
 * $brief Raw VBI decoder.
 *
 * The contents of this structure are private.
 * Call vbi3_raw_decoder_new() to allocate a raw VBI decoder.
 */
typedef struct _vbi3_raw_decoder vbi3_raw_decoder;

/*
 * $addtogroup RawDecoder
 * ${
 */
extern vbi3_raw_decoder *
vbi3_raw_decoder_new		(const vbi_sampling_par *sp);
extern void
vbi3_raw_decoder_delete		(vbi3_raw_decoder *	rd);
extern void
vbi3_raw_decoder_get_sampling_par
				(const vbi3_raw_decoder *rd,
				 vbi_sampling_par *	sp);
extern vbi_service_set
vbi3_raw_decoder_set_sampling_par
				(vbi3_raw_decoder *	rd,
				 const vbi_sampling_par *sp,
				 unsigned int		strict);
extern vbi_service_set
vbi3_raw_decoder_add_services	(vbi3_raw_decoder *	rd,
				 vbi_service_set	services,
				 unsigned int		strict);
extern vbi_service_set
vbi3_raw_decoder_remove_services
				(vbi3_raw_decoder *	rd,
				 vbi_service_set	services);
extern void
vbi3_raw_decoder_reset		(vbi3_raw_decoder *	rd);
extern unsigned int
vbi3_raw_decoder_decode		(vbi3_raw_decoder *	rd,
				 vbi_sliced *		sliced,
				 unsigned int		sliced_lines,
				 const uint8_t *	raw);
/* $} */

/* Private */

unsigned int
vbi_sliced_payload_bits		(vbi_service_set	service);
vbi_bool
_vbi_sampling_par_verify	(const vbi_sampling_par *sp);
vbi_service_set
vbi_sampling_par_check_services	(const vbi_sampling_par *sp,
				 vbi_service_set	services,
				 unsigned int		strict);
vbi_service_set
vbi_sampling_par_from_services	(vbi_sampling_par *	sp,
				 unsigned int *		max_rate,
				 vbi_videostd_set	videostd_set,
				 vbi_service_set	services);

/* $internal */
#define _VBI3_RAW_DECODER_MAX_JOBS 8
/* $internal */
#define _VBI3_RAW_DECODER_MAX_WAYS 8

/* $internal */
typedef struct {
	vbi_service_set		id;
	vbi3_bit_slicer		slicer;
} _vbi3_raw_decoder_job;

/* $internal */
struct _vbi3_raw_decoder {
	vbi_sampling_par	sampling;
	vbi_service_set		services;
	unsigned int		n_jobs;
	int			readjust;
	int8_t *		pattern;	/* n scan lines * MAX_WAYS */
	_vbi3_raw_decoder_job	jobs[_VBI3_RAW_DECODER_MAX_JOBS];
};

typedef struct _vbi_service_par _vbi_service_par;

/* $internal */
struct _vbi_service_par {
	vbi_service_set		id;
	const char *		label;

	/*
	 * Video standard
	 * - 525 lines, FV = 59.94 Hz, FH = 15734 Hz
	 * - 625 lines, FV = 50 Hz, FH = 15625 Hz
	 */
	vbi_videostd_set	videostd_set;

	/*
	 * Most scan lines used by the data service, first and last
	 * line of first and second field. ITU-R numbering scheme.
	 * Zero if no data from this field, requires field sync.
	 */
	unsigned int		first[2];
        unsigned int		last[2];

	/*
	 * Leading edge hsync to leading edge first CRI one bit,
	 * half amplitude points, in nanoseconds.
	 */
	unsigned int		offset;

	unsigned int		cri_rate;	/**< Hz */
	unsigned int		bit_rate;	/**< Hz */

	/* Clock Run In and FRaming Code, LSB last txed bit of FRC. */
	unsigned int		cri_frc;

	/* CRI and FRC bits significant for identification. */
	unsigned int		cri_frc_mask;

	/*
	 * Number of significat cri_bits (at cri_rate),
	 * frc_bits (at bit_rate).
	 */
	unsigned int		cri_bits;
	unsigned int		frc_bits;

	unsigned int		payload;	/**< bits */
	vbi_modulation		modulation;
};

extern const _vbi_service_par _vbi_service_table [];

extern vbi_bool
_vbi3_raw_decoder_init		(vbi3_raw_decoder *	rd,
				 const vbi_sampling_par *sp);
extern void
_vbi3_raw_decoder_destroy	(vbi3_raw_decoder *	rd);
extern void
_vbi3_raw_decoder_dump		(const vbi3_raw_decoder *rd,
				 FILE *			fp);

#endif /* RAW_DECODER_H */
