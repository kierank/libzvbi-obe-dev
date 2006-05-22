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

/* $Id: io-sim.h,v 1.3 2006/05/22 09:07:17 mschimek Exp $ */

#ifndef __ZVBI_IO_SIM_H__
#define __ZVBI_IO_SIM_H__

#include "macros.h"
#include "version.h"
#include "sampling_par.h"
#include "io.h"

VBI_BEGIN_DECLS

/* Public */

extern vbi_bool
vbi_raw_video_image		(uint8_t *		raw,
				 unsigned long		raw_size,
				 const vbi_sampling_par *sp,
				 int			black_level,
				 int			white_level,
				 unsigned int		pixel_mask,
				 vbi_bool		swap_fields,
				 const vbi_sliced *	sliced,
				 unsigned int		n_sliced_lines);
extern vbi_bool
vbi_raw_vbi_image		(uint8_t *		raw,
				 unsigned long		raw_size,
				 const vbi_sampling_par *sp,
				 int			blank_level,
				 int			white_level,
				 vbi_bool		swap_fields,
				 const vbi_sliced *	sliced,
				 unsigned int		n_sliced_lines);
#if 3 == VBI_VERSION_MINOR
extern vbi_bool
vbi_capture_sim_load_vps	(vbi_capture *		cap,
				 const vbi_program_id *pid);
extern vbi_bool
vbi_capture_sim_load_wss_625	(vbi_capture *		cap,
				 const vbi_aspect_ratio *ar);
extern vbi_bool
vbi_capture_sim_load_caption	(vbi_capture *		cap,
				 const char *		stream,
				 vbi_bool		append);
#endif
extern void
vbi_capture_sim_decode_raw	(vbi_capture *		cap,
				 vbi_bool		enable);
extern vbi_capture *
vbi_capture_sim_new		(int			scanning,
				 unsigned int *		services,
				 vbi_bool		interlaced,
				 vbi_bool		synchronous);

/* Private */

VBI_END_DECLS

#endif /* __ZVBI_IO_SIM_H__ */
