/*
 *  libzvbi - Error correction functions
 *
 *  Copyright (C) 2001 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
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

/* $Id: hamm.h,v 1.1 2002/01/12 16:18:44 mschimek Exp $ */

#ifndef HAMM_H
#define HAMM_H

#include <inttypes.h>

extern const unsigned char	vbi_bit_reverse[256];
extern const char               vbi_hamm24par[3][256];
extern const char		vbi_hamm8val[256];

/**
 * vbi_parity:
 * @c: Unsigned byte. 
 * 
 * Return value:
 * If the byte has odd parity (sum of bits mod 2 is 1) the
 * byte AND 127, otherwise -1.
 **/
static inline char
vbi_parity(unsigned char c)
{
#if 0 /* #cpu (i386) */
	char r;

	/* This saves cache business */
	asm (" test	%0,%0\n"
	     " setpe	%1\n"
	     " and	$127,%0\n"
	     " or	%1,%0n"
	     : "=r" (r) : "&r" (c));

	return r;
#else
	if (vbi_hamm24par[0][c] & 32)
		return c & 0x7F;
	else
		return -1;
#endif
}

static inline int
vbi_chk_parity(uint8_t *p, int n)
{
	unsigned int c;
	int err;

	for (err = 0; n--; p++)
		if (vbi_hamm24par[0][c = *p] & 32)
			*p = c & 0x7F;
		else
			*p = 0x20, err++;

	return err == 0;
}

static inline void
vbi_set_parity(uint8_t *p, int n)
{
	unsigned int c;

	for (; n--; p++)
		if (!(vbi_hamm24par[0][c = *p] & 32))
			*p = c | 0x80;
}

/**
 * vbi_hamm8:
 * @c: A Hamming 8/4 protected byte, lsb first
 *   transmitted.
 * 
 * This function decodes a Hamming 8/4 protected byte
 * as specified in ETS 300 706 8.2.
 * 
 * Return value: 
 * Data bits (D4 [msb] ... D1 [lsb])
 * or -1 if the byte contained incorrectable errors.
 **/
#define vbi_hamm8(c) vbi_hamm8val[(unsigned char)(c)]

/**
 * vbi_hamm16:
 * @p: Pointer to a Hamming 8/4 protected byte pair,
 *   bytes in transmission order, lsb first transmitted.
 * 
 * This function decodes a Hamming 8/4 protected byte pair
 * as specified in ETS 300 706 8.2.
 * 
 * Return value: 
 * Data bits D4 [msb] ... D1 of byte 1, D4 ... D1 [lsb] of byte 2
 * or a negative value if the pair contained incorrectable errors.
 **/
static inline int
vbi_hamm16(uint8_t *p)
{
	return vbi_hamm8val[p[0]] | (vbi_hamm8val[p[1]] << 4);
}

extern int vbi_hamm24(uint8_t *p);

#endif /* HAMM_H */
