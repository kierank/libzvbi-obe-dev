/*
 *  Copyright (C) 2001-2006 Michael H. Schimek
 *  Copyright (C) 2000-2003 Iñaki García Etxebarria
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

/* $Id: misc.c,v 1.7 2007/07/23 20:00:11 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <ctype.h>
#include <errno.h>

#include "misc.h"
#include "version.h"

#if 2 == VBI_VERSION_MINOR
const char _zvbi_intl_domainname[] = PACKAGE;
#endif

_vbi_log_hook		_vbi_global_log;

/**
 * @internal
 * strlcpy() is a BSD/GNU extension.
 * Don't call this function directly, we #define strlcpy if necessary.
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

/**
 * @internal
 * strndup() is a BSD/GNU extension.
 * Don't call this function directly, we #define strndup if necessary.
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

/**
 * @internal
 * vasprintf() is a GNU extension.
 * Don't call this function directly, we #define vasprintf if necessary.
 */
int
_vbi_vasprintf			(char **		dstp,
				 const char *		templ,
				 va_list		ap)
{
	char *buf;
	unsigned long size;
	int temp;

	assert (NULL != dstp);
	assert (NULL != templ);

	temp = errno;

	buf = NULL;
	size = 64;

	for (;;) {

		char *buf2;
		long len;

		if (!(buf2 = realloc (buf, size)))
			break;

		buf = buf2;

		len = vsnprintf (buf, size, templ, ap);

		if (len < 0) {
			/* Not enough. */
			size *= 2;
		} else if ((unsigned long) len < size) {
			*dstp = buf;
			errno = temp;
			return len;
		} else {
			/* Size needed. */
			size = len + 1;
		}
	}

	free (buf);
	buf = NULL;

	/* According to man 3 asprintf GNU's version leaves *dstp
	   undefined on error, so don't count on it. FreeBSD's
	   asprintf NULLs *dstp, which is safer. */
	*dstp = NULL;

	errno = temp;

	return -1;
}

/**
 * @internal
 * asprintf() is a GNU extension.
 * Don't call this function directly, we #define asprintf if necessary.
 */
int
_vbi_asprintf			(char **		dstp,
				 const char *		templ,
				 ...)
{
	va_list ap;
	int len;

	va_start (ap, templ);

	/* May fail, returning -1. */
	len = vasprintf (dstp, templ, ap);

	va_end (ap);

	return len;
}

/** @internal */
vbi_bool
_vbi_keyword_lookup		(int *			value,
				 const char **		inout_s,
				 const _vbi_key_value_pair *table,
				 unsigned int		n_pairs)
{
	const char *s;
	unsigned int i;

	assert (NULL != value);
	assert (NULL != inout_s);
	assert (NULL != *inout_s);
	assert (NULL != table);

	s = *inout_s;

	while (isspace (*s))
		++s;

	if (isdigit (*s)) {
		long val;
		char *end;

		val = strtol (s, &end, 10);

		for (i = 0; NULL != table[i].key; ++i) {
			if (val == table[i].value) {
				*value = val;
				*inout_s = end;
				return TRUE;
			}
		}
	} else {
		for (i = 0; i < n_pairs; ++i) {
			size_t len = strlen (table[i].key);

			if (0 == strncasecmp (s, table[i].key, len)
			    && !isalnum (s[len])) {
				*value = table[i].value;
				*inout_s = s + len;
				return TRUE;
			}
		}
	}

	return FALSE;
}

/**
 * @ingroup Basic
 *
 * Log function printing messages on standard output.
 *
 * @since 0.2.22
 */
void
vbi_log_on_stderr		(vbi_log_mask		level,
				 const char *		context,
				 const char *		message,
				 void *			user_data)
{
	vbi_log_mask max_level;

	/* This function exists in libzvbi 0.2 with vbi_ prefix and
	   in libzvbi 0.3 and Zapping with vbi3_ prefix (so I can
	   use both versions in Zapping until 0.3 is finished). */
	if (0 == strncmp (context, "vbi_", 4)) {
		context += 4;
	/* Not "vbi3_" to prevent an accidental s/vbi3_/vbi_. */
	} else if (0 == strncmp (context, "vbi" "3_", 5)) {
		context += 5;
	}

	if (NULL != user_data) {
		max_level = * (vbi_log_mask *) user_data;
		if (level > max_level)
			return;
	}

	fprintf (stderr, "libzvbi:%s: %s\n", context, message);
}

/** @internal */
void
_vbi_log_vprintf		(vbi_log_fn		log_fn,
				 void *			user_data,
				 vbi_log_mask		mask,
				 const char *		source_file,
				 const char *		context,
				 const char *		templ,
				 va_list		ap)
{
	char ctx_buffer[160];
	char *msg_buffer;
	int saved_errno;
	unsigned int i;
	int r;

	assert (NULL != source_file);
	assert (NULL != context);
	assert (NULL != templ);

	if (NULL == log_fn)
		return;

	saved_errno = errno;

	for (i = 0; i < N_ELEMENTS (ctx_buffer) - 2; ++i) {
		int c = source_file[i];

		if ('.' == c)
			break;

		ctx_buffer[i] = c;
	}

	ctx_buffer[i++] = ':';

	strlcpy (ctx_buffer + i, context,
		 N_ELEMENTS (ctx_buffer) - i);

	r = vasprintf (&msg_buffer, templ, ap);
	if (r > 1 && NULL != msg_buffer) {
		log_fn (mask, ctx_buffer, msg_buffer, user_data);

		free (msg_buffer);
		msg_buffer = NULL;
	}

	errno = saved_errno;
}

/** @internal */
void
_vbi_log_printf			(vbi_log_fn		log_fn,
				 void *			user_data,
				 vbi_log_mask		mask,
				 const char *		source_file,
				 const char *		context,
				 const char *		templ,
				 ...)
{
	va_list ap;

	va_start (ap, templ);

	_vbi_log_vprintf (log_fn, user_data, mask,
			  source_file, context, templ, ap);

	va_end (ap);
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
