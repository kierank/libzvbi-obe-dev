/*
 *  libzvbi
 *
 *  Copyright (C) 2002 Michael H. Schimek
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

/* $Id: misc.h,v 1.2 2003/01/03 06:18:05 mschimek Exp $ */

#ifndef MISC_H
#define MISC_H

#include <stddef.h>
#include <string.h>

#ifdef __GNUC__
# if __GNUC__ < 3
#  define __builtin_expect(exp, c) (exp)
# endif
#else
# define __inline__
# define __builtin_expect(exp, c) (exp)
#endif

/* Public */

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
/* doxygen omits static objects */
#define static_inline static __inline__
#endif

/**
 * @ingroup Basic
 * @name Boolean type
 * @{
 */
#undef TRUE
#undef FALSE
#define TRUE 1
#define FALSE 0

typedef int vbi_bool;
/** @} */

/**
 * @ingroup Event
 * @brief Unique network id (a libzvbi thing).
 *
 * 0 = unknown network, bit 31 reserved for preliminary nuids.
 * Other network codes are arbitrary.
 */
typedef unsigned int vbi_nuid;

/* preliminary */
#define NUID0 0

/* Private */

#define N_ELEMENTS(array) (sizeof (array) / sizeof (*(array)))

#define PARENT(ptr, type, member)					\
  ((type *)(((char *) ptr) - offsetof (type, member)))

#undef ABS
#define ABS(n) ({							\
	register int _n = n, _t = _n;					\
	_t >>= sizeof (_t) * 8 - 1;					\
	_n ^= _t;							\
	_n -= _t;							\
})

#undef MIN
#define MIN(x, y) ({							\
	__typeof__ (x) _x = x;						\
	__typeof__ (y) _y = y;						\
	(void)(&_x == &_y); /* alert when type mismatch */		\
	(_x < _y) ? _x : _y;						\
})

#undef MAX
#define MAX(x, y) ({							\
	__typeof__ (x) _x = x;						\
	__typeof__ (y) _y = y;						\
	(void)(&_x == &_y); /* alert when type mismatch */		\
	(_x > _y) ? _x : _y;						\
})

#define SWAP(x, y)							\
do {									\
	__typeof__ (x) _x = x;						\
	x = y;								\
	y = _x;								\
} while (0)

#undef SATURATE
#ifdef __i686__
#define SATURATE(n, min, max) ({					\
	__typeof__ (n) _n = n;						\
	__typeof__ (n) _min = min;					\
	__typeof__ (n) _max = max;					\
	if (_n < _min)							\
		_n = _min;						\
	if (_n > _max)							\
		_n = _max;						\
	_n;								\
})
#else
#define SATURATE(n, min, max) ({					\
	__typeof__ (n) _n = n;						\
	__typeof__ (n) _min = min;					\
	__typeof__ (n) _max = max;					\
	if (_n < _min)							\
		_n = _min;						\
	else if (_n > _max)						\
		_n = _max;						\
	_n;								\
})
#endif

#define SET(var) memset (&(var), ~0, sizeof (var))
#define CLEAR(var) memset (&(var), 0, sizeof (var))
#define MOVE(d, s) memmove (d, s, sizeof (d))

#endif /* MISC_H */
