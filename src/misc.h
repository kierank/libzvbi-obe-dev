/*
 *  libzvbi - Miscellaneous types and macros
 *
 *  Copyright (C) 2002-2003 Michael H. Schimek
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

/* $Id: misc.h,v 1.12 2006/05/18 16:52:48 mschimek Exp $ */

#ifndef MISC_H
#define MISC_H

/* Dox shall be system config independant. */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#ifndef TEST

/* Public */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#ifdef __cplusplus
#  define VBI_BEGIN_DECLS extern "C" {
#  define VBI_END_DECLS }
#else
#  define VBI_BEGIN_DECLS
#  define VBI_END_DECLS
#endif

#if __GNUC__ >= 2
   /* Inline this function at -O2 and higher. */
#  define vbi_inline static __inline__
#else
#  define vbi_inline static
#endif

#if __GNUC__ >= 3
   /* Function has no side effects and return value depends
      only on parameters and non-volatile globals or
      memory pointed to by parameters. */
#  define vbi_pure __attribute__ ((pure))
   /* Function has no side effects and return value depends
      only on parameters. */
#  define vbi_const __attribute__ ((const))
   /* Function returns pointer which does not alias anything. */
#  define vbi_alloc __attribute__ ((malloc))
#  define _vbi_alloc malloc
#else
#  define vbi_pure
#  define vbi_const
#  define vbi_alloc
#  define _vbi_alloc
#endif

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

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

/* Private */

#define N_ELEMENTS(array) (sizeof (array) / sizeof (*(array)))

#ifdef __GNUC__

#if __GNUC__ < 3
#  define likely(exp) (exp)
#  define unlikely(exp) (exp)
#else
#  define likely(exp) __builtin_expect (exp, 1)
#  define unlikely(exp) __builtin_expect (exp, 0)
#endif

#undef __i386__
#undef __i686__
#if #cpu (i386)
#define __i386__ 1
#endif
#if #cpu (i686)
#define __i686__ 1
#endif

#define PACKED __attribute__ ((packed))

/* &x == PARENT (&x.tm_min, struct tm, tm_min),
   safer than &x == (struct tm *) &x.tm_min */
#undef PARENT
#define PARENT(_ptr, _type, _member) ({					\
	__typeof__ (&((_type *) 0)->_member) _p = (_ptr);		\
	(_p != 0) ? (_type *)(((char *) _p) - offsetof (_type,		\
	  _member)) : (_type *) 0;					\
})

/* Like PARENT(), to be used with const _ptr. */
#undef CONST_PARENT
#define CONST_PARENT(_ptr, _type, _member) ({                           \
        __typeof__ (&((const _type *) 0)->_member) _p = (_ptr);         \
        (_p != 0) ? (const _type *)(((const char *) _p) - offsetof      \
         (const _type, _member)) : (const _type *) 0;                   \
})

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
#ifdef __i686__ /* conditional move */
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

#else /* !__GNUC__ */

#define likely(exp) (exp)
#define unlikely(exp) (exp)
#undef __i386__
#undef __i686__
#define __inline__
#define PACKED

static char *
PARENT_HELPER (char *p, unsigned int offset)
{ return (p == 0) ? 0 : p - offset; }

static const char *
CONST_PARENT_HELPER (const char *p, unsigned int offset)
{ return (p == 0) ? 0 : p - offset; }

#undef PARENT
#define PARENT(_ptr, _type, _member)                                    \
        ((offsetof (_type, _member) == 0) ? (_type *)(_ptr)             \
         : (_type *) PARENT_HELPER ((char *)(_ptr), offsetof (_type, _member)))

#undef CONST_PARENT
#define CONST_PARENT(_ptr, _type, _member)                              \
        ((offsetof (const _type, _member) == 0) ? (const _type *)(_ptr) \
         : (const _type *) CONST_PARENT_HELPER ((const char *)(_ptr),   \
          offsetof (const _type, _member)))

#undef ABS
#define ABS(n) (((n) < 0) ? -(n) : (n))

#undef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#undef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#define SWAP(x, y)							\
do {									\
	long _x = x;							\
	x = y;								\
	y = _x;								\
} while (0)

#undef SATURATE
#define SATURATE(n, min, max) MIN (MAX (n, min), max)

#endif /* !__GNUC__ */

/* NB gcc inlines and optimizes when size is const. */
#define SET(d) memset (&(d), ~0, sizeof (d))
#define CLEAR(d) memset (&(d), 0, sizeof (d))
#define MOVE(d, s) memmove (d, s, sizeof (d))

/* Use this instead of strncpy().
   strlcpy is a BSD/GNU extension.*/
#ifdef HAVE_STRLCPY
#  define _vbi_strlcpy strlcpy
#else
extern size_t
_vbi_strlcpy			(char *			dst,
				 const char *		src,
				 size_t			len);
#endif

/* strndup is a BSD/GNU extension. */
#ifdef HAVE_STRNDUP
#  define _vbi_strndup strndup
#else
extern char *
_vbi_strndup			(const char *		s,
				 size_t			len);
#endif

#ifdef HAVE_ASPRINTF
#  define vbi_asprintf asprintf
#else
extern int
vbi_asprintf			(char **		dstp,
				 const char *		templ,
				 ...);
#endif

#define STRCOPY(d, s) (_vbi_strlcpy (d, s, sizeof (d)) < sizeof (d))

/* For debugging. */
vbi_inline int
_vbi_to_ascii			(int			c)
{
	if (c < 0)
		return '?';

	c &= 0x7F;

	if (c < 0x20 || c >= 0x7F)
		return '.';

	return c;
}

/* Gettext i18n */

extern const char _zvbi_intl_domainname[];

#ifndef _
#  ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) dgettext (_zvbi_intl_domainname, String)
#    ifdef gettext_noop
#      define N_(String) gettext_noop (String)
#    else
#      define N_(String) (String)
#    endif
#  else /* Stubs that do something close enough.  */
#    define gettext(Msgid) ((const char *) (Msgid))
#    define dgettext(Domainname, Msgid) ((const char *) (Msgid))
#    define dcgettext(Domainname, Msgid, Category) ((const char *) (Msgid))
#    define ngettext(Msgid1, Msgid2, N) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define dngettext(Domainname, Msgid1, Msgid2, N) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define dcngettext(Domainname, Msgid1, Msgid2, N, Category) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define textdomain(Domainname) ((const char *) (Domainname))
#    define bindtextdomain(Domainname, Dirname) ((const char *) (Dirname))
#    define bind_textdomain_codeset(Domainname, Codeset) ((const char *) (Codeset))
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#endif

#define VBI_BEGIN_DECLS
#define VBI_END_DECLS

#endif /* !TEST */

typedef enum {
	VBI_LOG_ERR = 3, 
	VBI_LOG_WARNING,
	VBI_LOG_NOTICE,
	VBI_LOG_INFO,
	VBI_LOG_DEBUG,
} vbi_log_level;

typedef void
vbi_log_fn			(vbi_log_level		level,
				 const char *		function,
				 const char *		message,
				 void *			user_data);
extern void
vbi_log_on_stderr		(vbi_log_level		level,
				 const char *		function,
				 const char *		message,
				 void *			user_data);
extern void
vbi_log_printf			(vbi_log_fn		log_fn,
				 void *			user_data,
				 vbi_log_level		level,
				 const char *		function,
				 const char *		template,
				 ...);

#endif /* MISC_H */
