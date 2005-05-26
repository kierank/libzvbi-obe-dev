/*
 *  libzvbi - Teletext IDL packet demultiplexer
 *
 *  Copyright (C) 2005 Michael H. Schimek
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

/* $Id: idl_demux.h,v 1.3 2005/05/26 04:08:44 mschimek Exp $ */

#ifndef __ZVBI_IDL_DEMUX_H__
#define __ZVBI_IDL_DEMUX_H__

#include <inttypes.h>		/* uint8_t */
#include <stdio.h>		/* FILE */
/* #include "macros.h" */

/* VBI_BEGIN_DECLS */

/* Public */

/**
 * @addtogroup IDLDemux Teletext IDL packet demultiplexer
 * @ingroup LowDec
 * @brief Functions to decode data transmissions in Teletext
 *   Independent Data Line packets (EN 300 708 section 6).
 * @{
 */

/**
 * @brief IDL demultiplexer context.
 *
 * The contents of this structure are private.
 *
 * Call vbi_idl_demux_new() to allocate an IDL
 * demultiplexer context.
 */
typedef struct _vbi_idl_demux vbi_idl_demux;

/**
 * @name vbi_idl_demux_cb flags
 * @{
 */

/**
 * Data was lost (not received or uncorrectable) between the current and
 * previous call.
 */
#define VBI_IDL_DATA_LOST	(1 << 0)

/**
 * IDL Format A flag (EN 300 708 section 6.5.2): The data may require
 * the use of data in other channels or with other service packet
 * addresses as defined by the application.
 */
#define VBI_IDL_DEPENDENT	(1 << 3)

/** @} */

/**
 * @param dx IDL demultiplexer returned by
 *   vbi_idl_a_demux_new() and given to vbi_idl_demux_feed().
 * @param buffer Pointer to received user data.
 * @param n_bytes Number of bytes in the buffer. Can be @c 0 if
 *   the decoded packet did not contain user data.
 * @param flags @c VBI_IDL_DATA_LOST, @c VBI_IDL_DEPENDENT.
 * @param user_data User pointer passed to vbi_idl_demux_new().
 * 
 * The vbi_idl_demux_feed() function calls a function of this type
 * after successfully decoding an IDL packet.
 *
 * @returns
 * FALSE to abort vbi_idl_demux_feed() and return FALSE.
 */
typedef vbi_bool
vbi_idl_demux_cb		(vbi_idl_demux *	dx,
				 const uint8_t *	buffer,
				 unsigned int		n_bytes,
				 unsigned int		flags,
				 void *			user_data);

extern void
vbi_idl_demux_reset		(vbi_idl_demux *	dx);
extern vbi_bool
vbi_idl_demux_feed		(vbi_idl_demux *	dx,
				 const uint8_t		buffer[42]);
extern void
vbi_idl_demux_delete		(vbi_idl_demux *	dx);
extern vbi_idl_demux *
vbi_idl_a_demux_new		(unsigned int		channel,
				 unsigned int		address,
				 vbi_idl_demux_cb *	callback,
				 void *			user_data);

/** @} */

/* Private */

/** @internal */
#define _VBI_IDL_FORMAT_A		(1 << 0)
#define _VBI_IDL_FORMAT_B		(1 << 1)
#define _VBI_IDL_FORMAT_DATAVIDEO	(1 << 2)
#define	_VBI_IDL_FORMAT_AUDETEL		(1 << 3)
#define	_VBI_IDL_FORMAT_LBRA		(1 << 4)

/** @internal */
typedef unsigned int _vbi_idl_format;

/** @internal */
struct _vbi_idl_demux {
	_vbi_idl_format		format;

	/** Filter out packets of this channel, with this address. */
	int			channel;
	int			address;

	/** Expected next continuity indicator. */
	int			ci;

	/** Expected next repeat indicator. */
	int			ri;

	unsigned int		flags;

	vbi_idl_demux_cb *	callback;
	void *			user_data;
};

extern void
_vbi_idl_demux_destroy		(vbi_idl_demux *	dx);
extern vbi_bool
_vbi_idl_demux_init		(vbi_idl_demux *	dx,
				 _vbi_idl_format	format,
				 unsigned int		channel,
				 unsigned int		address,
				 vbi_idl_demux_cb *	callback,
				 void *			user_data);

/* VBI_END_DECLS */

#endif /* __ZVBI_IDL_DEMUX_H__ */
