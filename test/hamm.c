/*
 *  libzvbi test
 *
 *  Copyright (C) 2003, 2005 Michael H. Schimek
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

/* $Id: hamm.c,v 1.1 2005/01/19 04:22:10 mschimek Exp $ */

/* Automated test of the odd parity and Hamming test/set routines. */

#include <stdlib.h>		/* mrand48() */
#include <limits.h>		/* CHAR_BIT */
#include <assert.h>

#include "src/hamm.h"

static unsigned int
parity				(unsigned int		n)
{
	unsigned int sh;

	for (sh = sizeof (n) * CHAR_BIT / 2; sh > 0; sh >>= 1)
		n ^= n >> sh;

	return n & 1;
}

#define BC(n) ((n) * (unsigned int) 0x0101010101010101ULL)

static unsigned int
population_count		(unsigned int		n)
{
	n -= (n >> 1) & BC (0x55);
	n = (n & BC (0x33)) + ((n >> 2) & BC (0x33));
	n = (n + (n >> 4)) & BC (0x0F);

	return (n * BC (0x01)) >> (sizeof (unsigned int) * 8 - 8);
}

static unsigned int
hamming_distance		(unsigned int		a,
				 unsigned int		b)
{
	return population_count (a ^ b);
}

int
main				(int			argc,
				 char **		argv)
{
	unsigned int i;

	for (i = 0; i < 10000; ++i) {
		unsigned int n = (i < 256) ? i : mrand48 ();
		uint8_t buf[4] = { n, n >> 8, n >> 16 };
		unsigned int r;
		unsigned int j;

		for (r = 0, j = 0; j < 8; ++j)
			if (n & (0x01 << j))
				r |= 0x80 >> j;

		assert (r == vbi_rev8 (n));

		if (parity (n & 0xFF))
			assert (vbi_unpar8 (n) == (int)(n & 127));
		else
			assert (-1 == vbi_unpar8 (n));

		assert (vbi_unpar8 (vbi_par8 (n)) >= 0);

		vbi_par (buf, sizeof (buf));
		assert (vbi_unpar (buf, sizeof (buf)) >= 0);
		assert (0 == ((buf[0] | buf[1] | buf[2]) & 0x80));

		buf[1] = vbi_par8 (buf[1]);
		buf[2] = buf[1] ^ 0x80;

		assert (vbi_unpar (buf, sizeof (buf)) < 0);
		assert (buf[2] == (buf[1] & 0x7F));
	}

	for (i = 0; i < 10000; ++i) {
		unsigned int n = (i < 256) ? i : mrand48 ();
		unsigned int A, B, C, D;
		int d;

		A = parity (n & 0xA3);
		B = parity (n & 0x8E);
		C = parity (n & 0x3A);
		D = parity (n & 0xFF);

		d = (+ ((n & 0x02) >> 1)
		     + ((n & 0x08) >> 2)
		     + ((n & 0x20) >> 3)
		     + ((n & 0x80) >> 4));

		if (A && B && C) {
			unsigned int nn;

			nn = D ? n : (n ^ 0x40);

			assert (vbi_ham8 (d) == (nn & 255));
			assert (vbi_unham8 (nn) == d);
		} else if (!D) {
			unsigned int nn;
			int dd;

			dd = vbi_unham8 (n);
			assert (dd >= 0 && dd <= 15);

			nn = vbi_ham8 (dd);
			assert (hamming_distance (n & 255, nn) == 1);
		} else {
			assert (vbi_unham8 (n) == -1);
		}

		/* vbi_ham16 (buf, n);
		   assert (vbi_unham16 (buf) == (int)(n & 255)); */
	}

	for (i = 0; i < (1 << 24); ++i) {
		uint8_t buf[4] = { i, i >> 8, i >> 16 };
		unsigned int A, B, C, D, E, F;
		int d;

		A = parity (i & 0x555555);
		B = parity (i & 0x666666);
		C = parity (i & 0x787878);
		D = parity (i & 0x007F80);
		E = parity (i & 0x7F8000);
		F = parity (i & 0xFFFFFF);

		d = (+ ((i & 0x000004) >> (3 - 1))
		     + ((i & 0x000070) >> (5 - 2))
		     + ((i & 0x007F00) >> (9 - 5))
		     + ((i & 0x7F0000) >> (17 - 12)));
		
		if (A && B && C && D && E) {
			assert (vbi_unham24p (buf) == d);
		} else if (F) {
			assert (vbi_unham24p (buf) < 0);
		} else {
			unsigned int err;
			unsigned int ii;

			err = ((E << 4) | (D << 3)
			       | (C << 2) | (B << 1) | A) ^ 0x1F;

			assert (err > 0);

			if (err >= 24) {
				assert (vbi_unham24p (buf) < 0);
				continue;
			}

			ii = i ^ (1 << (err - 1));

			A = parity (ii & 0x555555);
			B = parity (ii & 0x666666);
			C = parity (ii & 0x787878);
			D = parity (ii & 0x007F80);
			E = parity (ii & 0x7F8000);
			F = parity (ii & 0xFFFFFF);

			assert (A && B && C && D && E && F);

			d = (+ ((ii & 0x000004) >> (3 - 1))
			     + ((ii & 0x000070) >> (5 - 2))
			     + ((ii & 0x007F00) >> (9 - 5))
			     + ((ii & 0x7F0000) >> (17 - 12)));

			assert (vbi_unham24p (buf) == d);
		}
	}

	return 0;
}
