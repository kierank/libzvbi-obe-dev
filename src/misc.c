/*
 *  libzvbi - Portability helper functions
 *
 *  Copyright (C) 2004 Michael H. Schimek
 *
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

/* $Id: misc.c,v 1.1 2004/10/25 16:55:22 mschimek Exp $ */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include "misc.h"

const char _zvbi_intl_domainname[] = PACKAGE;

#ifndef HAVE_STRLCPY

/**
 * @internal
 * strlcpy() is a BSD/GNU extension.
 */
size_t
_vbi_strlcpy			(char *			dst,
				 const char *		src,
				 size_t			len)
{
	char *dst1;
	char *end;
	char c;

	assert (NULL != dst);
	assert (NULL != src);
	assert (len > 0);

	dst1 = dst;

	end = dst + len - 1;

	while (dst < end && (c = *src++))
		*dst++ = c;

	*dst = 0;

	return dst - dst1;
}

#endif /* !HAVE_STRLCPY */

#ifndef HAVE_STRNDUP

/**
 * @internal
 * strndup() is a BSD/GNU extension.
 */
char *
_vbi_strndup			(const char *		s,
				 size_t			len)
{
	size_t n;
	char *r;

	if (NULL == s)
		return NULL;

	n = strlen (s);
	len = MIN (len, n);

	r = malloc (len + 1);

	if (r) {
		memcpy (r, s, len);
		r[len] = 0;
	}

	return r;
}

#endif /* !HAVE_STRNDUP */

#ifndef HAVE_ASPRINTF

/**
 * @internal
 * asprintf() is a GNU extension.
 */
int
vbi_asprintf			(char **		dstp,
				 const char *		templ,
				 ...)
{
	char *buf;
	int size;
	int temp;

	assert (NULL != dstp);
	assert (NULL != templ);

	temp = errno;

	buf = NULL;
	size = 64;

	for (;;) {
		va_list ap;
		char *buf2;
		int len;

		if (!(buf2 = realloc (buf, size)))
			break;

		buf = buf2;

		va_start (ap, templ);
		len = vsnprintf (buf, size, templ, ap);
		va_end (ap);

		if (len < 0) {
			/* Not enough. */
			size *= 2;
		} else if (len < size) {
			*dstp = buf;
			errno = temp;
			return len;
		} else {
			/* Size needed. */
			size = len + 1;
		}
	}

	free (buf);
	*dstp = NULL;
	errno = temp;

	return -1;
}

#endif /* !HAVE_ASPRINTF */
