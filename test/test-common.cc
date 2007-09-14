/*
 *  libzvbi - Unit test helper functions
 *
 *  Copyright (C) 2007 Michael H. Schimek
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

/* $Id: test-common.cc,v 1.2 2007/09/14 14:20:50 mschimek Exp $ */

#include <stdlib.h>
#include "src/misc.h"
#include "test-common.h"
#include "src/version.h"

void *
memset_rand			(void *			dst,
				 size_t			n_bytes)
{
	uint8_t *p;
	uint8_t *p_end;
	unsigned int x;
	size_t todo;

	assert (n_bytes < (10 << 20));

	p = (uint8_t *) dst;

	todo = (size_t) dst & 3;
	if (0 != todo) {
		todo = MIN (4 - todo, n_bytes);
		p_end = p + todo;
		x = mrand48 ();

		while (p < p_end) {
			*p++ = x;
			x >>= 8;
		}

		n_bytes -= todo;
	}

	p_end = p + (n_bytes & ~3);

	for (; p < p_end; p += 4)
		* ((uint32_t *) p) = mrand48 ();

	todo = n_bytes & 3;
	if (0 != todo) {
		p_end = p + todo;
		x = mrand48 ();

		while (p < p_end) {
			*p++ = x;
			x >>= 8;
		}
	}

	return dst;
}

void *
xmalloc				(size_t			n_bytes)
{
	void *p;

	assert (n_bytes < (10 << 20));

	p = malloc (n_bytes);
	assert (NULL != p);

	return p;
}

void *
xralloc				(size_t			n_bytes)
{
	return memset_rand (xmalloc (n_bytes), n_bytes);
}

static unsigned int		malloc_count;
static unsigned int		malloc_fail_cycle;

static void *
my_malloc			(size_t			n_bytes)
{
	if (malloc_count++ == malloc_fail_cycle)
		return NULL;
	else
		return malloc (n_bytes);
}

void
test_malloc			(void			(* function)(void),
				 unsigned int		n_cycles)
{
#if 3 == VBI_VERSION_MINOR
	vbi_malloc = my_malloc;

	for (malloc_fail_cycle = 0; malloc_fail_cycle < n_cycles;
	     ++malloc_fail_cycle) {
		malloc_count = 0;
		function ();
	}

	vbi_malloc = malloc;
#endif
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
