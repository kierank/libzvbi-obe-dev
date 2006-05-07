/*
 *  libzvbi - Video Programming System
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

/* $Id: vps.c,v 1.2 2006/05/07 20:54:00 mschimek Exp $ */

#include "../config.h"

#include <assert.h>
#include "misc.h"
#include "vps.h"

/**
 * @addtogroup VPS Video Programming System Decoder
 * @ingroup LowDec
 * @brief Functions to decode VPS packets (ETS 300 231).
 */

/**
 * @param cni CNI of type VBI_CNI_TYPE_VPS is stored here.
 * @param buffer VPS packet as defined for @c VBI_SLICED_VPS,
 *   i.e. 13 bytes without clock run-in and start code.
 *
 * Decodes a VPS packet according to ETS 300 231, returning the
 * 12 bit Country and Network Identifier in @a cni.
 *
 * The code 0xDC3 is translated according to TR 101 231: "As this
 * code is used for a time in two networks a distinction for automatic
 * tuning systems is given in data line 16 [VPS]: bit 3 of byte 5 = 1
 * for the ARD network / = 0 for the ZDF network."
 *
 * @returns
 * Always @c TRUE, no error checking possible. It may be prudent to
 * wait until two identical packets have been received.
 *
 * @since 0.2.20
 */
vbi_bool
vbi_decode_vps_cni		(unsigned int *		cni,
				 const uint8_t		buffer[13])
{
	unsigned int cni_value;

	assert (NULL != cni);
	assert (NULL != buffer);

	cni_value = (+ ((buffer[10] & 0x03) << 10)
		     + ((buffer[11] & 0xC0) << 2)
		     +  (buffer[ 8] & 0xC0)
		     +  (buffer[11] & 0x3F));

	if (0x0DC3 == cni_value)
		cni_value = (buffer[2] & 0x10) ?
			0x0DC2 /* ZDF */ : 0x0DC1 /* ARD */;

	*cni = cni_value;

	return TRUE;
}

#if 3 == VBI_VERSION_MINOR

/**
 * @param pid PDC data is stored here.
 * @param buffer VPS packet as defined for @c VBI_SLICED_VPS,
 *   i.e. 13 bytes without clock run-in and start code.
 * 
 * Decodes a VPS datagram according to ETS 300 231,
 * storing PDC recording-control data in @a pid.
 *
 * @returns
 * Always @c TRUE, no error checking possible.
 *
 * @since 9.9.9
 */
vbi_bool
vbi_decode_vps_pdc		(vbi_program_id *	pid,
				 const uint8_t		buffer[13])
{
	assert (NULL != pid);
	assert (NULL != buffer);

	pid->cni_type	= VBI_CNI_TYPE_VPS;

	pid->cni	= (+ ((buffer[10] & 0x03) << 10)
			   + ((buffer[11] & 0xC0) << 2)
			   +  (buffer[ 8] & 0xC0)
			   +  (buffer[11] & 0x3F));

	pid->channel	= VBI_PID_CHANNEL_VPS;

	pid->pil	= (+ ((buffer[ 8] & 0x3F) << 14)
			   +  (buffer[ 9] << 6)
			   +  (buffer[10] >> 2));

	pid->month	= VBI_PIL_MONTH (pid->pil) - 1; 
	pid->day	= VBI_PIL_DAY (pid->pil) - 1; 
	pid->hour	= VBI_PIL_HOUR (pid->pil); 
	pid->minute	= VBI_PIL_MINUTE (pid->pil); 

	pid->length	= 0; /* unknown */

	pid->luf	= FALSE; /* no update, just pil */
	pid->mi		= FALSE; /* label is not 30 s early */
	pid->prf	= FALSE; /* prepare to record unknown */

	pid->pcs_audio	= buffer[ 2] >> 6;
	pid->pty	= buffer[12];

	pid->tape_delayed = FALSE;

	return TRUE;
}

#endif /* 3 == VBI_VERSION_MINOR */

/**
 * @param buffer VPS packet as defined for @c VBI_SLICED_VPS,
 *   i.e. 13 bytes without clock run-in and start code.
 * @param cni CNI of type VBI_CNI_TYPE_VPS.
 *
 * Stores the 12 bit Country and Network Identifier @a cni in
 * a VPS packet according to ETS 300 231.
 *
 * @returns
 * @c FALSE if @a cni is invalid; in this case @a buffer remains
 * unmodified.
 *
 * @since 0.2.20
 */
vbi_bool
vbi_encode_vps_cni		(uint8_t		buffer[13],
				 unsigned int		cni)
{
	assert (NULL != buffer);

	if (unlikely (cni > 0x0FFF))
		return FALSE;

	buffer[8] = (buffer[8] & 0x3F) | (cni & 0xC0);
	buffer[10] = (buffer[10] & 0xFC) | (cni >> 10);
	buffer[11] = (cni & 0x3F) | ((cni >> 2) & 0xC0);

	return TRUE;
}

#if 3 == VBI_VERSION_MINOR

/**
 * @param buffer VPS packet as defined for @c VBI_SLICED_VPS,
 *   i.e. 13 bytes without clock run-in and start code.
 * @param pid PDC data.
 * 
 * Stores PDC recording-control data (CNI, PIL, PCS audio, PTY) in
 * a VPS datagram according to ETS 300 231. If non-zero the function
 * encodes @a pid->pil, otherwise it calculates the PIL from
 * @a pid->month, day, hour and minute.
 *
 * @returns
 * @c FALSE if any of the parameters to encode are invalid; in this
 * case @a buffer remains unmodified.
 *
 * @since 9.9.9
 */
vbi_bool
vbi_encode_vps_pdc		(uint8_t		buffer[13],
				 const vbi_program_id *pid)
{
	unsigned int month;
	unsigned int day;
	unsigned int hour;
	unsigned int minute;
	unsigned int pil;

	assert (NULL != buffer);
	assert (NULL != pid);

	if (unlikely ((unsigned int) pid->pty > 0xFF))
		return FALSE;

	if (unlikely ((unsigned int) pid->pcs_audio > 3))
		return FALSE;

	pil = pid->pil;

	switch (pil) {
	case VBI_PIL_TIMER_CONTROL:
	case VBI_PIL_INHIBIT_TERMINATE:
	case VBI_PIL_INTERRUPT:
	case VBI_PIL_CONTINUE:
		break;

	default:
		if (0 == pil) {
			month = pid->month;
			day = pid->day;
			hour = pid->hour;
			minute = pid->minute;

			pil = VBI_PIL (month, day, hour, minute);
		} else {
			month = VBI_PIL_MONTH (pil);
			day = VBI_PIL_DAY (pil);
			hour = VBI_PIL_HOUR (pil);
			minute = VBI_PIL_MINUTE (pil);
		}

		if (unlikely ((month - 1) > 11
			      || (day - 1) > 30
			      || hour > 23
			      || minute > 59))
			return FALSE;

		break;
	}

	if (!vbi_encode_vps_cni (buffer, pid->cni))
		return FALSE;

	buffer[2] = (buffer[2] & 0x3F) | (pid->pcs_audio << 6);
	buffer[8] = (buffer[8] & 0xC0) | ((pil >> 14) & 0x3F);
	buffer[9] = pil >> 6;
	buffer[10] = (buffer[10] & 0x03) | (pil << 2);
	buffer[12] = pid->pty;

	return TRUE;
}

#endif /* 3 == VBI_VERSION_MINOR */
