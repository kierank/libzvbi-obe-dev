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

/* $Id: dvb_demux.h,v 1.1 2004/10/28 03:29:04 mschimek Exp $ */

#ifndef __ZVBI_DVB_DEMUX_H__
#define __ZVBI_DVB_DEMUX_H__

#ifndef TEST
#include <inttypes.h>		/* uintN_t */
#include "bcd.h"		/* vbi_bool */
#include "sliced.h"		/* vbi_sliced, vbi_service_set */
#endif

typedef struct _vbi_dvb_demux vbi_dvb_demux;

typedef vbi_bool
vbi_dvb_demux_cb		(vbi_dvb_demux *	dx,
				 void *			user_data,
				 const vbi_sliced *	sliced,
				 unsigned int		sliced_lines,
				 int64_t		pts);

extern void
_vbi_dvb_demux_reset		(vbi_dvb_demux *	dx);
extern unsigned int
_vbi_dvb_demux_cor		(vbi_dvb_demux *	dx,
				 vbi_sliced *		sliced,
				 unsigned int 		sliced_lines,
				 int64_t *		pts,
				 const uint8_t **	buffer,
				 unsigned int *		buffer_left);
extern vbi_bool
_vbi_dvb_demux_demux		(vbi_dvb_demux *	dx,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size);
extern void
_vbi_dvb_demux_delete		(vbi_dvb_demux *	dx);
extern vbi_dvb_demux *
_vbi_dvb_demux_pes_new		(vbi_dvb_demux_cb *	callback,
				 void *			user_data);

#endif /* __ZVBI_DVB_DEMUX_H__ */
