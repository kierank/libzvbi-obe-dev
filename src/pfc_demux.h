/*
 *  libzvbi - Teletext packet page format clear demultiplexer
 *
 *  Copyright (C) 2003-2004 Michael H. Schimek
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

/* $Id: pfc_demux.h,v 1.5 2006/05/22 09:05:46 mschimek Exp $ */

#ifndef __ZVBI_PFC_DEMUX_H__
#define __ZVBI_PFC_DEMUX_H__

#include <inttypes.h>		/* uint8_t */
#include <stdio.h>		/* FILE */
#include "bcd.h"		/* vbi_pgno */

VBI_BEGIN_DECLS

/* Public */

/**
 * @addtogroup PFCDemux
 * @{
 */

/**
 * @brief One block of data returned by vbi_pfc_demux_cb().
 */
typedef struct {
	/** Source page as requested with vbi_pfc_demux_new(). */
	vbi_pgno		pgno;

	/** Source stream as requested with vbi_pfc_demux_new(). */
	unsigned int		stream;

	/** Application ID transmitted with this data block. */
	unsigned int		application_id;

	/** Size of the data block in bytes, 1 ... 2048. */
	unsigned int		block_size;

	/** Data block. */
	uint8_t			block[2048];
} vbi_pfc_block;

/**
 * @brief PFC demultiplexer context.
 *
 * The contents of this structure are private.
 *
 * Call vbi_pfc_demux_new() to allocate a PFC
 * demultiplexer context.
 */
typedef struct _vbi_pfc_demux vbi_pfc_demux;

/**
 * @param dx PFC demultiplexer context returned by
 *   vbi_pfx_demux_new() and given to vbi_pfc_demux_demux().
 * @param user_data User pointer given to vbi_pfc_demux_new().
 * @param block Structure describing the received data block.
 * 
 * Function called by vbi_pfc_demux_demux() when a
 * new data block is available.
 *
 * @returns
 * FALSE on error, will be returned by vbi_pfc_demux_feed().
 */
typedef vbi_bool
vbi_pfc_demux_cb		(vbi_pfc_demux *	dx,
				 const vbi_pfc_block *	block,
				 void *			user_data);

extern void
vbi_pfc_demux_reset		(vbi_pfc_demux *	dx);
extern vbi_bool
vbi_pfc_demux_feed		(vbi_pfc_demux *	dx,
				 const uint8_t		buffer[42]);
extern void
vbi_pfc_demux_delete		(vbi_pfc_demux *	dx);
extern vbi_pfc_demux *
vbi_pfc_demux_new		(vbi_pgno		pgno,
				 unsigned int		stream,
				 vbi_pfc_demux_cb *	callback,
				 void *			user_data)
  __attribute__ ((_vbi_alloc));

/* Private */

/** @internal */
struct _vbi_pfc_demux {
	/** Expected next continuity index. */
	unsigned int		ci;

	/** Expected next packet. */
	unsigned int		packet;

	/** Expected number of packets. */
	unsigned int		n_packets;

	/** Block index. */
	unsigned int		bi;

	/** Expected number of block bytes. */
	unsigned int		left;

	vbi_pfc_demux_cb *	callback;
	void *			user_data;

	vbi_pfc_block		block;
};

extern void
_vbi_pfc_block_dump		(const vbi_pfc_block *	pb,
				 FILE *			fp,
				 vbi_bool		binary);
extern vbi_bool
_vbi_pfc_demux_decode		(vbi_pfc_demux *	dx,
				 const uint8_t		buffer[42]);
extern void
_vbi_pfc_demux_destroy		(vbi_pfc_demux *	dx);
extern vbi_bool
_vbi_pfc_demux_init		(vbi_pfc_demux *	dx,
				 vbi_pgno		pgno,
				 unsigned int		stream,
				 vbi_pfc_demux_cb *	callback,
				 void *			user_data);
/** @} */

VBI_END_DECLS

#endif /* __ZVBI_PFC_DEMUX_H__ */
