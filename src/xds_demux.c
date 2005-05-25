/*
 *  libzvbi - Extended Data Service demultiplexer
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

/* $Id: xds_demux.c,v 1.2 2005/05/25 02:26:10 mschimek Exp $ */

#include "../site_def.h"
#include "../config.h"

#include <assert.h>
#include <stdlib.h>		/* malloc() */
#include <string.h>		/* memcpy() */
#include "hamm.h"		/* vbi_ipar8() */
#include "misc.h"		/* vbi_log_printf */
#include "xds_demux.h"

/**
 * @addtogroup XDSDemux Extended Data Service Demultiplexer
 * @ingroup LowDec
 * @brief Separating XDS data from a Closed Caption stream
 *   (EIA 608).
 */

#ifndef XDS_DEMUX_LOG
#define XDS_DEMUX_LOG 0
#endif

#define log(format, args...)						\
do {									\
	if (XDS_DEMUX_LOG)						\
		fprintf (stderr, format , ##args);			\
} while (0)

/** @internal */
void
_vbi_xds_packet_dump		(const vbi_xds_packet *	xp,
				 FILE *			fp)
{
	unsigned int i;

	assert (NULL != xp);
	assert (NULL != fp);

	fprintf (fp, "XDS packet 0x%x/0x%02x ",
		 xp->xds_class, xp->xds_subclass);

	for (i = 0; i < xp->buffer_size; ++i)
		fprintf (fp, "%02x ", xp->buffer[i]);

	fputs (" '", fp);

	for (i = 0; i < xp->buffer_size; ++i)
		fputc (vbi_printable (xp->buffer[i]), fp);

	fputs ("'\n", fp);
}

/**
 * @param xd XDS demultiplexer context allocated with vbi_xds_demux_new().
 *
 * Resets the XDS demux, useful for example after a channel change.
 *
 * @since 0.2.16
 */
void
vbi_xds_demux_reset		(vbi_xds_demux *	xd)
{
	unsigned int n;
	unsigned int i;

	assert (NULL != xd);

	n = N_ELEMENTS (xd->subpacket) * N_ELEMENTS (xd->subpacket[0]);

	for (i = 0; i < n; ++i)
		xd->subpacket[0][i].count = 0;

	xd->curr_sp = NULL;
}

/**
 * @param xd XDS demultiplexer context allocated with vbi_xds_demux_new().
 * @param buffer Closed Caption character pair, as in struct vbi_sliced.
 *
 * This function takes two successive bytes of a raw Closed Caption
 * stream, filters out XDS data and calls the output function given to
 * vbi_xds_demux_new() when a new packet is complete.
 *
 * You should feed only data from NTSC line 284.
 *
 * @returns
 * FALSE if the buffer contained parity errors.
 *
 * @since 0.2.16
 */
vbi_bool
vbi_xds_demux_feed		(vbi_xds_demux *	xd,
				 const uint8_t		buffer[2])
{
	_vbi_xds_subpacket *sp;
	int c1, c2;

	assert (NULL != xd);
	assert (NULL != buffer);

	sp = xd->curr_sp;

	log ("XDS demux %02x %02x\n", buffer[0], buffer[1]);

	c1 = vbi_unpar8 (buffer[0]);
	c2 = vbi_unpar8 (buffer[1]);

	if ((c1 | c2) < 0) {
		log ("XDS tx error, discard current packet\n");
 
		if (sp) {
			sp->count = 0;
			sp->checksum = 0;
		}

		xd->curr_sp = NULL;

		return FALSE;
	}

	switch (c1) {
	case 0x00:
		/* Stuffing. */

		break;

	case 0x01 ... 0x0E:
	{
		vbi_xds_class xds_class;
		vbi_xds_subclass xds_subclass;

		/* Packet header. */

		xds_class = (c1 - 1) >> 1;
		xds_subclass = c2;

		if (xds_class > VBI_XDS_CLASS_MISC
		    || xds_subclass > N_ELEMENTS (xd->subpacket[0])) {
			log ("XDS ignore packet 0x%x/0x%02x, "
			     "unknown class or subclass\n",
			     xds_class, xds_subclass);
			goto discard;
		}

		sp = &xd->subpacket[xds_class][xds_subclass];

		xd->curr_sp = sp;
		xd->curr.xds_class = xds_class;
		xd->curr.xds_subclass = xds_subclass;

		if (c1 & 1) {
			/* Start packet. */
			sp->checksum = c1 + c2;
			sp->count = 2;
		} else {
			/* Continue packet. */
			if (0 == sp->count) {
				log ("XDS can't continue packet "
				     "0x%x/0x%02x, missed start\n",
				     xd->curr.xds_class,
				     xd->curr.xds_subclass);
				goto discard;
			}
		}

		break;
	}

	case 0x0F:
		/* Packet terminator. */

		if (!sp) {
			log ("XDS can't finish packet, missed start\n");
			break;
		}

		sp->checksum += c1 + c2;

		if (0 != (sp->checksum & 0x7F)) {
			log ("XDS ignore packet 0x%x/0x%02x, "
			     "checksum error\n",
			     xd->curr.xds_class, xd->curr.xds_subclass);
		} else if (sp->count <= 2) {
			log ("XDS ignore empty packet 0x%x/0x%02x\n",
			     xd->curr.xds_class, xd->curr.xds_subclass);
		} else {
			memcpy (xd->curr.buffer, sp->buffer, 32);

			xd->curr.buffer_size = sp->count - 2;
			xd->curr.buffer[sp->count - 2] = 0;

			if (XDS_DEMUX_LOG)
				_vbi_xds_packet_dump (&xd->curr, stderr);

			xd->callback (xd, &xd->curr, xd->user_data);
		}

		/* fall through */

	discard:
		if (sp) {
			sp->count = 0;
			sp->checksum = 0;
		}

		/* fall through */

	case 0x10 ... 0x1F:
		/* Closed Caption. */

		xd->curr_sp = NULL;

		break;

	case 0x20 ... 0x7F:
		/* Packet contents. */

		if (!sp) {
			log ("XDS can't store packet, missed start\n");
			break;
		}

		if (sp->count >= sizeof (sp->buffer) + 2) {
			log ("XDS discard packet 0x%x/0x%02x, "
			     "buffer overflow\n",
			     xd->curr.xds_class, xd->curr.xds_subclass);
			goto discard;
		}

		sp->buffer[sp->count - 2] = c1;
		sp->buffer[sp->count - 1] = c2;

		sp->checksum += c1 + c2;
		sp->count += 1 + (0 != c2);

		break;
	}

	return TRUE;
}

/** @internal */
void
_vbi_xds_demux_destroy		(vbi_xds_demux *	xd)
{
	assert (NULL != xd);

	CLEAR (*xd);
}

/** @internal */
vbi_bool
_vbi_xds_demux_init		(vbi_xds_demux *	xd,
				 vbi_xds_demux_cb *	callback,
				 void *			user_data)
{
	assert (NULL != xd);
	assert (NULL != callback);

	vbi_xds_demux_reset (xd);

	xd->callback = callback;
	xd->user_data = user_data;

	return TRUE;
}

/**
 * @param xd XDS demultiplexer context allocated with
 *   vbi_xds_demux_new(), can be @c NULL.
 *
 * Frees all resources associated with @a xd.
 *
 * @since 0.2.16
 */
void
vbi_xds_demux_delete		(vbi_xds_demux *	xd)
{
	if (NULL == xd)
		return;

	_vbi_xds_demux_destroy (xd);

	free (xd);		
}

/**
 * @param callback Function to be called by vbi_xds_demux_feed() when
 *   a new packet is available.
 * @param user_data User pointer passed through to @a callback function.
 *
 * Allocates a new Extended Data Service (EIA 608) demultiplexer.
 *
 * @returns
 * Pointer to newly allocated XDS demux context which must be
 * freed with vbi_xds_demux_delete() when done. @c NULL on failure
 * (out of memory).
 *
 * @since 0.2.16
 */
vbi_xds_demux *
vbi_xds_demux_new		(vbi_xds_demux_cb *	callback,
				 void *			user_data)
{
	vbi_xds_demux *xd;

	assert (NULL != callback);

	if (!(xd = malloc (sizeof (*xd)))) {
		return NULL;
	}

	_vbi_xds_demux_init (xd, callback, user_data);

	return xd;
}
