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

/* $Id: test-common.h,v 1.2 2007/10/14 14:54:17 mschimek Exp $ */

#include <string.h>
#include <inttypes.h>

#include "src/macros.h"

#define RAND(var) memset_rand (&(var), sizeof (var))

extern void *
memset_rand			(void *			d1,
				 size_t			n)
  __attribute__ ((_vbi_nonnull (1)));
extern void *
xmalloc				(size_t			n_bytes)
  __attribute__ ((_vbi_alloc));
extern void *
xralloc				(size_t			n_bytes)
  __attribute__ ((_vbi_alloc));
extern void *
xmemdup				(const void *		src,
				 size_t			n_bytes)
  __attribute__ ((_vbi_nonnull (1), _vbi_alloc));

extern void
test_malloc			(void			(* function)(void),
				 unsigned int		n_cycles = 1);

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
