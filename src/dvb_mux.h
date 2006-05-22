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
 *
 *  WARNING this code is experimental, the API will change.
 */

/* $Id: dvb_mux.h,v 1.3 2006/05/22 09:08:46 mschimek Exp $ */

#ifndef __ZVBI_DVB_MUX_H__
#define __ZVBI_DVB_MUX_H__

#include <inttypes.h>		/* uint8_t */

#include "bcd.h"		/* vbi_bool */
#include "sliced.h"		/* vbi_sliced, vbi_service_set */

typedef enum {
	VBI_VIDEOSTD_SET_525_60 = 1,
	VBI_VIDEOSTD_SET_625_50 = 2,
} vbi_videostd_set;

extern void
_vbi_dvb_multiplex_sliced	(uint8_t **		packet,
				 unsigned int *		packet_left,
				 const vbi_sliced **	sliced,
				 unsigned int *		sliced_left,
				 unsigned int		data_identifier,
				 vbi_service_set	service_set);
extern void
_vbi_dvb_multiplex_samples	(uint8_t **		packet,
				 unsigned int *		packet_left,
				 const uint8_t **	samples,
				 unsigned int *		samples_left,
				 unsigned int		samples_size,
				 unsigned int		data_identifier,
				 vbi_videostd_set	videostd_set,
				 unsigned int		line,
				 unsigned int		offset);

typedef struct _vbi_dvb_mux vbi_dvb_mux;

typedef vbi_bool
vbi_dvb_mux_cb			(vbi_dvb_mux *		mx,
				 void *			user_data,
				 const uint8_t *	packet,
				 unsigned int		packet_size);

extern void
_vbi_dvb_mux_reset		(vbi_dvb_mux *		mx);
extern vbi_bool
_vbi_dvb_mux_feed		(vbi_dvb_mux *		mx,
				 int64_t		pts,
				 const vbi_sliced *	sliced,
				 unsigned int		sliced_size,
				 vbi_service_set	service_set);
extern void
_vbi_dvb_mux_delete		(vbi_dvb_mux *		mx);
extern vbi_dvb_mux *
_vbi_dvb_mux_pes_new		(unsigned int		data_identifier,
				 unsigned int		packet_size,
				 vbi_videostd_set	videostd_set,
				 vbi_dvb_mux_cb *	callback,
				 void *			user_data);
extern vbi_dvb_mux *
_vbi_dvb_mux_ts_new		(unsigned int		pid,
				 unsigned int		data_identifier,
				 unsigned int		packet_size,
				 vbi_videostd_set	videostd_set,
				 vbi_dvb_mux_cb *	callback,
				 void *			user_data);

#endif /* __ZVBI_DVB_MUX_H__ */
