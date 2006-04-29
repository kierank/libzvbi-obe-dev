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

/* $Id: misc.c,v 1.3 2006/04/29 05:55:35 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

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

void
vbi_log_on_stderr		(vbi_log_level		level,
				 const char *		function,
				 const char *		message,
				 void *			user_data)
{
	vbi_log_level max_level;

	function = function; /* unused */

	if (NULL != user_data) {
		max_level = * (vbi_log_level *) user_data;
		if (level > max_level)
			return;
	}

	fprintf (stderr, "libzvbi: %s\n", message);
}

/** @internal */
void
vbi_log_printf			(vbi_log_fn		log_fn,
				 void *			user_data,
				 vbi_log_level		level,
				 const char *		function,
				 const char *		template,
				 ...)
{
	char *buffer;
	size_t buffer_size;
	int saved_errno;

	if (NULL == log_fn)
		return;

	assert (NULL != function);
	assert (NULL != template);

	saved_errno = errno;

	buffer = NULL;
	buffer_size = 256;

	for (;;) {
		char *new_buffer;
		va_list ap;
		int len;

		new_buffer = realloc (buffer, buffer_size);
		if (unlikely (NULL == new_buffer)) {
			break;
		}

		buffer = new_buffer;

		va_start (ap, template);
		len = vsnprintf (buffer, buffer_size, template, ap);
		va_end (ap);

		if (len < 0) {
			/* Not enough space. */
			buffer_size *= 2;
		} else if (len < buffer_size) {
			log_fn (level, function, buffer, user_data);
			break;
		} else {
			/* Size needed. */
			buffer_size = len + 1;
		}
	}

	free (buffer);

	errno = saved_errno;
}
