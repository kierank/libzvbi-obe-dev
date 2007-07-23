/*
 *  libzvbi -- Sliced VBI data filter
 *
 *  Copyright (C) 2006, 2007 Michael H. Schimek
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

/* $Id: sliced_filter.c,v 1.1 2007/07/23 19:59:52 mschimek Exp $ */

/* XXX UNTESTED */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <limits.h>
#include <errno.h>

#include "misc.h"
#include "version.h"
#include "hamm.h"		/* vbi_unham16p() */
#include "event.h"		/* VBI_SERIAL */
#include "sliced_filter.h"

#ifndef VBI_SERIAL
#  define VBI_SERIAL 0x100000
#endif

/* XXX Later. */
enum {
	VBI_ERR_INVALID_PGNO = 0,
	VBI_ERR_INVALID_SUBNO = 0,
	VBI_ERR_BUFFER_OVERFLOW = 0,
	VBI_ERR_PARITY = 0,
};

/* XXX Later. */
#undef _
#define _(x) (x)

#define MAX_SUBNO 0x3F7F

struct subpage_range {
	vbi_pgno		pgno;
	vbi_subno		first;
	vbi_subno		last;
};

struct _vbi_sliced_filter {
	/* One bit for each page. 0x100 -> keep_ttx_pages[0] & 1. */
	uint32_t		keep_ttx_pages[(0x900 - 0x100) / 32];

	/* A vector of subpages to keep, current size and capacity
	   (counting struct subpage_range). */
	struct subpage_range *	keep_ttx_subpages;
	unsigned int		keep_ttx_subpages_size;
	unsigned int		keep_ttx_subpages_capacity;

	/* Pages with non-BCD page numbers (page inventories, DRCS etc). */
	vbi_bool		keep_ttx_system_pages;

	/* See vbi_sliced_filter_feed(). */
	vbi_sliced *		output_buffer;
	unsigned int		output_max_lines;

	/* See decode_teletext_packet_0(). */
	unsigned int		keep_mag_set_next;

	/* VPS, WSS, all CC data, all TTX data. */
	vbi_service_set		keep_services;

	/* On error a description of the problem for users will be
	   stored here. Can be NULL if no error occurred yet or we're
	   out of memory. */
	char *			errstr;

	/* Callback for the vbi_sliced_filter_feed() function.*/
	vbi_sliced_filter_cb *	callback;
	void *			user_data;

	/* Log level and callback for debugging et al. */
	_vbi_log_hook		log;
};

static void
set_errstr			(vbi_sliced_filter *	sf,
				 const char *		templ,
				 ...)
{
	va_list ap;

	free (sf->errstr);
	sf->errstr = NULL;

	va_start (ap, templ);

	/* Log the error if that was requested. */
	_vbi_vlog (&sf->log, VBI_LOG_ERROR, templ, ap);

	/* Error ignored. */
	if (vasprintf (&sf->errstr, templ, ap) < 0)
		sf->errstr = NULL;

	va_end (ap);
}

static void
no_mem_error			(vbi_sliced_filter *	sf)
{
	set_errstr (sf, _("Out of memory."));

	errno = ENOMEM;
}

#if 0 /* to do */

vbi_bool
vbi_sliced_filter_drop_cc_channel
				(vbi_sliced_filter *	sf,
				 vbi_pgno		channel)
{
}

vbi_bool
vbi_sliced_filter_keep_cc_channel
				(vbi_sliced_filter *	sf,
				 vbi_pgno		channel)
{
}

/* Also to do: XDS. */

#endif

static vbi_bool
keeping_page			(vbi_sliced_filter *	sf,
				 vbi_pgno		pgno)
{
	uint32_t mask;
	unsigned int offset;

	mask = 1 << (pgno & 31);
	offset = (pgno - 0x100) >> 5;

	return (0 != (sf->keep_ttx_pages[offset] & mask));
}

void
vbi_sliced_filter_keep_ttx_system_pages
				(vbi_sliced_filter *	sf,
				 vbi_bool		keep)
{
	assert (NULL != sf);

	sf->keep_ttx_system_pages = !!keep;
}

static vbi_bool
extend_vector			(vbi_sliced_filter *	sf,
				 void **		vector,
				 unsigned int *		capacity,
				 unsigned int		min_capacity,
				 unsigned int		element_size)
{
	void *new_vec;
	unsigned int new_capacity;
	unsigned int max_capacity;

	assert (min_capacity > 0);
	assert (element_size > 0);

	/* This looks a bit odd to prevent overflows. */

	max_capacity = UINT_MAX / element_size;

	if (unlikely (min_capacity > max_capacity)) {
		no_mem_error (sf);
		return FALSE;
	}

	new_capacity = *capacity;

	if (unlikely (new_capacity > (max_capacity / 2))) {
		new_capacity = max_capacity;
	} else {
		new_capacity = MIN (min_capacity, new_capacity * 2);
	}

	new_vec = realloc (*vector, new_capacity * element_size);
	if (unlikely (NULL == new_vec)) {
		/* XXX we should try less new_capacity before giving up. */
		no_mem_error (sf);
		return FALSE;
	}

	*vector = new_vec;
	*capacity = new_capacity;

	return TRUE;
}

static vbi_bool
extend_ttx_subpages_vector	(vbi_sliced_filter *	sf,
				 unsigned int		min_capacity)
{
	if (min_capacity <= sf->keep_ttx_subpages_capacity)
		return TRUE;

	return extend_vector (sf,
			      (void **) &sf->keep_ttx_subpages,
			      &sf->keep_ttx_subpages_capacity,
			      min_capacity,
			      sizeof (*sf->keep_ttx_subpages));
}

static vbi_bool
valid_ttx_subpage_range		(vbi_sliced_filter *	sf,
				 vbi_pgno		pgno,
				 vbi_subno		first_subno,
				 vbi_subno		last_subno)
{
	if (unlikely ((unsigned int) pgno - 0x100 >= 0x800)) {
		set_errstr (sf, _("Invalid Teletext page number %x."),
			    pgno);
		errno = VBI_ERR_INVALID_PGNO;
		return FALSE;
	}

	if (likely ((unsigned int) first_subno <= MAX_SUBNO
		    && (unsigned int) last_subno <= MAX_SUBNO))
		return TRUE;

	if (first_subno == last_subno) {
		set_errstr (sf, _("Invalid Teletext subpage number %x."),
			    first_subno);
	} else {
		set_errstr (sf, _("Invalid Teletext subpage range %x-%x."),
			    first_subno, last_subno);
	}

	errno = VBI_ERR_INVALID_SUBNO;

	return FALSE;
}

vbi_bool
vbi_sliced_filter_drop_ttx_subpages
				(vbi_sliced_filter *	sf,
				 vbi_pgno		pgno,
				 vbi_subno		first_subno,
				 vbi_subno		last_subno)
{
	uint32_t mask;
	unsigned int offset;
	unsigned int i;

	assert (NULL != sf);

	errno = 0;

	if (unlikely (!valid_ttx_subpage_range (sf, pgno,
						first_subno,
						last_subno)))
		return FALSE;

	if (first_subno > last_subno)
		SWAP (first_subno, last_subno);

	if (sf->keep_services & VBI_SLICED_TELETEXT_B_625) {
		sf->keep_services &= ~VBI_SLICED_TELETEXT_B_625;
		memset (sf->keep_ttx_pages, 0xFF,
			sizeof (sf->keep_ttx_pages));
	}

	mask = 1 << (pgno & 31);
	offset = (pgno - 0x100) >> 5;

	if (0 != (sf->keep_ttx_pages[offset] & mask)) {
		i = sf->keep_ttx_subpages_size;

		if (!extend_ttx_subpages_vector (sf, i + 2))
			return FALSE;

		sf->keep_ttx_pages[offset] &= ~mask;

		if (first_subno > 0) {
			sf->keep_ttx_subpages[i].pgno = pgno;
			sf->keep_ttx_subpages[i].first = 0;
			sf->keep_ttx_subpages[i++].last = first_subno - 1;
		}

		if (last_subno < MAX_SUBNO) {
			sf->keep_ttx_subpages[i].pgno = pgno;
			sf->keep_ttx_subpages[i].first = last_subno + 1;
			sf->keep_ttx_subpages[i++].last = MAX_SUBNO;
		}

		sf->keep_ttx_subpages_size = i;

		return TRUE;
	}

	for (i = 0; i < sf->keep_ttx_subpages_size; ++i) {
		if (pgno != sf->keep_ttx_subpages[i].pgno)
			continue;

		if (first_subno > sf->keep_ttx_subpages[i].last)
			continue;

		if (last_subno < sf->keep_ttx_subpages[i].first)
			continue;

		if (first_subno > sf->keep_ttx_subpages[i].first)
			sf->keep_ttx_subpages[i].first = first_subno;

		if (last_subno < sf->keep_ttx_subpages[i].last)
			sf->keep_ttx_subpages[i].last = last_subno;

		if (sf->keep_ttx_subpages[i].first
		    > sf->keep_ttx_subpages[i].last) {
			memmove (&sf->keep_ttx_subpages[i],
				 &sf->keep_ttx_subpages[i + 1],
				 (sf->keep_ttx_subpages_size - i)
				 * sizeof (*sf->keep_ttx_subpages));
			--sf->keep_ttx_subpages_size;
			--i;
		}
	}

	return TRUE;
}

vbi_bool
vbi_sliced_filter_keep_ttx_subpages
				(vbi_sliced_filter *	sf,
				 vbi_pgno		pgno,
				 vbi_subno		first_subno,
				 vbi_subno		last_subno)
{
	unsigned int i;

	assert (NULL != sf);

	errno = 0;

	if (unlikely (!valid_ttx_subpage_range (sf, pgno,
						first_subno,
						last_subno)))
		return FALSE;

	if (sf->keep_services & VBI_SLICED_TELETEXT_B_625)
		return TRUE;

	if (keeping_page (sf, pgno))
		return TRUE;

	if (first_subno > last_subno)
		SWAP (first_subno, last_subno);

	for (i = 0; i < sf->keep_ttx_subpages_size; ++i) {
		if (pgno == sf->keep_ttx_subpages[i].pgno
		    && last_subno >= sf->keep_ttx_subpages[i].first
		    && first_subno <= sf->keep_ttx_subpages[i].last) {
			if (first_subno < sf->keep_ttx_subpages[i].first)
				sf->keep_ttx_subpages[i].first = first_subno;
			if (last_subno > sf->keep_ttx_subpages[i].last)
				sf->keep_ttx_subpages[i].last = last_subno;

			return TRUE;
		}
	}

	if (!extend_ttx_subpages_vector (sf, i + 1))
		return FALSE;

	sf->keep_ttx_subpages[i].pgno = pgno;
	sf->keep_ttx_subpages[i].first = first_subno;
	sf->keep_ttx_subpages[i].last = last_subno;

	sf->keep_ttx_subpages_size = i + 1;

	return TRUE;
}

static vbi_bool
valid_ttx_page_range		(vbi_sliced_filter *	sf,
				 vbi_pgno		first_pgno,
				 vbi_pgno		last_pgno)
{
	if (likely ((unsigned int) first_pgno - 0x100 < 0x800
		    && (unsigned int) last_pgno - 0x100 < 0x800))
		return TRUE;

	if (first_pgno == last_pgno) {
		set_errstr (sf, _("Invalid Teletext page number %x."),
			    first_pgno);
	} else {
		set_errstr (sf, _("Invalid Teletext page range %x-%x."),
			    first_pgno, last_pgno);
	}

	errno = VBI_ERR_INVALID_PGNO;

	return FALSE;
}

static void
drop_all_subpages		(vbi_sliced_filter *	sf,
				 vbi_pgno		first_pgno,
				 vbi_pgno		last_pgno)
{
	unsigned int i;

	for (i = 0; i < sf->keep_ttx_subpages_size; ++i) {
		if (sf->keep_ttx_subpages[i].pgno >= first_pgno
		    && sf->keep_ttx_subpages[i].pgno <= last_pgno) {
			memmove (&sf->keep_ttx_subpages[i],
				 &sf->keep_ttx_subpages[i + 1],
				 (sf->keep_ttx_subpages_size - i)
				 * sizeof (*sf->keep_ttx_subpages));
			--sf->keep_ttx_subpages_size;
			--i;
		}
	}
}

vbi_bool
vbi_sliced_filter_drop_ttx_pages
				(vbi_sliced_filter *	sf,
				 vbi_pgno		first_pgno,
				 vbi_pgno		last_pgno)
{
	uint32_t first_mask;
	uint32_t last_mask;
	unsigned int first_offset;
	unsigned int last_offset;

	assert (NULL != sf);

	errno = 0;

	if (unlikely (!valid_ttx_page_range (sf, first_pgno, last_pgno)))
		return FALSE;

	if (first_pgno > last_pgno)
		SWAP (first_pgno, last_pgno);

	if (sf->keep_services & VBI_SLICED_TELETEXT_B_625) {
		sf->keep_services &= ~VBI_SLICED_TELETEXT_B_625;
		memset (sf->keep_ttx_pages, 0xFF,
			sizeof (sf->keep_ttx_pages));
	} else {
		drop_all_subpages (sf, first_pgno, last_pgno);
	}

	/* 0 -> 0x00, 1 -> 0x01, 31 -> 0x7FFF FFFF. */
	first_mask = ~(-1 << (first_pgno & 31));
	first_offset = (first_pgno - 0x100) >> 5;

	/* 0 -> 0xFFFF FFFE, 1 -> 0xFFFF FFFC, 31 -> 0. */
	last_mask = -2 << (last_pgno & 31);
	last_offset = (last_pgno - 0x100) >> 5;

	if (first_offset == last_offset) {
		sf->keep_ttx_pages[first_offset] &= first_mask | last_mask;
	} else {
		sf->keep_ttx_pages[first_offset] &= first_mask;

		while (++first_offset < last_offset)
			sf->keep_ttx_pages[first_offset] = 0;

		sf->keep_ttx_pages[last_offset] &= last_mask;
	}

	return TRUE;
}

vbi_bool
vbi_sliced_filter_keep_ttx_pages
				(vbi_sliced_filter *	sf,
				 vbi_pgno		first_pgno,
				 vbi_pgno		last_pgno)
{
	uint32_t first_mask;
	uint32_t last_mask;
	unsigned int first_offset;
	unsigned int last_offset;

	assert (NULL != sf);

	errno = 0;

	if (unlikely (!valid_ttx_page_range (sf, first_pgno, last_pgno)))
		return FALSE;

	if (sf->keep_services & VBI_SLICED_TELETEXT_B_625)
		return TRUE;

	if (first_pgno > last_pgno)
		SWAP (first_pgno, last_pgno);

	/* Remove duplicates of keep_ttx_pages in keep_ttx_subpages. */
	drop_all_subpages (sf, first_pgno, last_pgno);

	/* 0 -> 0xFFFF FFFF, 1 -> 0xFFFF FFFE, 31 -> 0x8000 0000. */
	first_mask = -1 << (first_pgno & 31);
	first_offset = (first_pgno - 0x100) >> 5;

	/* 0 -> 0x01, 1 -> 0x03, 31 -> 0xFFFF FFFF. */
	last_mask = ~(-2 << (last_pgno & 31));
	last_offset = (last_pgno - 0x100) >> 5;

	if (first_offset == last_offset) {
		sf->keep_ttx_pages[first_offset] |= first_mask & last_mask;
	} else {
		sf->keep_ttx_pages[first_offset] |= first_mask;

		while (++first_offset < last_offset)
			sf->keep_ttx_pages[first_offset] = -1;

		sf->keep_ttx_pages[last_offset] |= last_mask;
	}

	return TRUE;
}

vbi_service_set
vbi_sliced_filter_drop_services
				(vbi_sliced_filter *	sf,
				 vbi_service_set	services)
{
	assert (NULL != sf);

	if (services & VBI_SLICED_TELETEXT_B_625) {
		memset (sf->keep_ttx_pages, 0,
			sizeof (sf->keep_ttx_pages));
		sf->keep_ttx_subpages_size = 0;
	}

	return sf->keep_services &= ~services;
}

vbi_service_set
vbi_sliced_filter_keep_services
				(vbi_sliced_filter *	sf,
				 vbi_service_set	services)
{
	assert (NULL != sf);

	if (services & VBI_SLICED_TELETEXT_B_625) {
		memset (sf->keep_ttx_pages, 0,
			sizeof (sf->keep_ttx_pages));
		sf->keep_ttx_subpages_size = 0;
	}

	return sf->keep_services |= services;
}

void
vbi_sliced_filter_reset	(vbi_sliced_filter *	sf)
{
	assert (NULL != sf);

	sf->keep_mag_set_next = 0;
}

static vbi_bool
decode_teletext_packet_0	(vbi_sliced_filter *	sf,
				 unsigned int *		keep_mag_set,
				 const uint8_t		buffer[42],
				 unsigned int		magazine)
{
	int page;
	int flags;
	vbi_pgno pgno;
	unsigned int mag_set;

	page = vbi_unham16p (buffer + 2);
	if (unlikely (page < 0)) {
		set_errstr (sf, _("Hamming error in Teletext "
				  "page number."));
		errno = VBI_ERR_PARITY;
		return FALSE;
	}

	if (0xFF == page) {
		debug2 (&sf->log, "Discard filler packet.");
		*keep_mag_set = 0;
		return TRUE;
	}

	pgno = magazine * 0x100 + page;

	flags = vbi_unham16p (buffer + 4)
		| (vbi_unham16p (buffer + 6) << 8)
		| (vbi_unham16p (buffer + 8) << 16);
	if (unlikely (flags < 0)) {
		set_errstr (sf, _("Hamming error in Teletext "
				  "packet flags."));
		errno = VBI_ERR_PARITY;
		return FALSE;
	}

	debug2 (&sf->log, "Teletext pgno=%03x flags/subno=0x%06x.",
		pgno, flags);

	/* Blank lines are not transmitted and there's no page end mark,
	   so Teletext decoders wait for another page before displaying
	   the previous one. In serial transmission mode that is any
	   page, in parallel mode a page of the same magazine. */
	if (flags & VBI_SERIAL) {
		mag_set = -1;
	} else {
		mag_set = 1 << magazine;
	}

	if (!vbi_is_bcd (pgno)) {
		/* Page inventories and TOP pages (e.g. to
		   find subtitles), DRCS and object pages, etc. */
		if (sf->keep_ttx_system_pages)
			goto match;
	} else {
		vbi_subno subno;
		unsigned int i;

		if (keeping_page (sf, pgno))
			goto match;

		subno = flags & 0x3F7F;

		for (i = 0; i < sf->keep_ttx_subpages_size; ++i) {
			if (pgno == sf->keep_ttx_subpages[i].pgno
			    && subno >= sf->keep_ttx_subpages[i].first
			    && subno <= sf->keep_ttx_subpages[i].last)
				goto match;
		}
	}

	if (*keep_mag_set & mag_set) {
		/* To terminate the previous page we keep the header
		   packet of this page (keep_mag_set) but discard all
		   following packets (keep_mag_set_next). */
		debug2 (&sf->log, "Keeping page header.");
		sf->keep_mag_set_next = *keep_mag_set & ~mag_set;
	} else {
		/* Discard this and following packets until we
		   find another header packet. */
		debug2 (&sf->log, "Dropping page.");
		*keep_mag_set &= ~mag_set;
		sf->keep_mag_set_next = *keep_mag_set;
	}

	return TRUE;

 match:
	/* Keep this and following packets. */
	debug2 (&sf->log, "Keeping page.");
	*keep_mag_set |= mag_set;
	sf->keep_mag_set_next = *keep_mag_set;

	return TRUE;
}

static vbi_bool
decode_teletext			(vbi_sliced_filter *	sf,
				 vbi_bool *		keep,
				 const uint8_t		buffer[42],
				 unsigned int		line)
{
	int pmag;
	unsigned int magazine;
	unsigned int packet;
	unsigned int keep_mag_set;

	line = line;

	keep_mag_set = sf->keep_mag_set_next;

	pmag = vbi_unham16p (buffer);
	if (unlikely (pmag < 0)) {
		set_errstr (sf, _("Hamming error in Teletext "
				  "packet/magazine number."));
		errno = VBI_ERR_PARITY;
		return FALSE;
	}

	magazine = pmag & 7;
	if (0 == magazine)
		magazine = 8;

	packet = pmag >> 3;

	switch (packet) {
	case 0: /* page header */
		if (!decode_teletext_packet_0 (sf, &keep_mag_set,
					       buffer, magazine))
			return FALSE; /* parity error */
		break;

	case 1 ... 25: /* page body */
		break;

	case 26: /* page enhancement packet */
	case 27: /* page linking */
	case 28:
	case 29: /* level 2.5/3.5 enhancement */
		break;

	case 30:
	case 31: /* IDL packet (ETS 300 708). */
		/* XXX make this optional. */
		debug3 (&sf->log, "Dropping Teletext IDL packet %u.",
			packet);

		*keep = FALSE;

		return TRUE;

	default:
		assert (0);
	}

	*keep = !!(keep_mag_set & (1 << magazine));

	debug3 (&sf->log, "%sing Teletext packet %u.",
		*keep ? "Keep" : "Dropp", packet);

	return TRUE;
}

/**
 * @brief Sliced VBI filter coroutine.
 * @param sf Sliced VBI filter context allocated with
 *   vbi_sliced_filter_new().
 * @param sliced_out Filtered sliced data will be stored here.
 *   @a sliced_out and @a sliced_in can be the same.
 * @param n_lines_out The number of sliced lines in the
 *   @a sliced_out buffer will be stored here.
 * @param max_lines_out The maximum number of sliced lines this
 *   function may store in the @a sliced_out buffer.
 * @param sliced_in The sliced data to be filtered.
 * @param n_lines_in Pointer to a variable which contains the
 *   number of sliced lines to be read from the @a sliced_in buffer.
 *   When the function fails, it stores here the number of sliced
 *   lines successfully read so far.
 *
 * This function takes one video frame worth of sliced VBI data and
 * filters out the lines which match the selected criteria.
 *
 * @returns
 * @c TRUE on success. @c FALSE if there is not enough room in the
 * output buffer to store the filtered data, or when the function
 * detects an error in the sliced input data. On failure the
 * @a sliced_out buffer will contain the data successfully filtered
 * so far, @a *n_lines_out will be valid, and @a *n_lines_in will
 * contain the number of lines read so far.
 *
 * @since 99.99.99
 */
vbi_bool
vbi_sliced_filter_cor		(vbi_sliced_filter *	sf,
				 vbi_sliced *		sliced_out,
				 unsigned int *		n_lines_out,
				 unsigned int		max_lines_out,
				 const vbi_sliced *	sliced_in,
				 unsigned int *		n_lines_in)
{
	unsigned int in;
	unsigned int out;

	assert (NULL != sf);
	assert (NULL != sliced_out);
	assert (NULL != n_lines_out);
	assert (NULL != sliced_in);

	errno = 0;

	out = 0;

	for (in = 0; in < *n_lines_in; ++in) {
		vbi_bool pass_through;

		pass_through = FALSE;

		debug2 (&sf->log, "sliced[%u]: line=%u service='%s'.",
			in,
			sliced_in[in].line,
			vbi_sliced_name (sliced_in[in].id));

		if (sliced_in[in].id & sf->keep_services) {
			debug2 (&sf->log, "Keeping service.");
			pass_through = TRUE;
		} else {
			switch (sliced_in[in].id) {
			case VBI_SLICED_TELETEXT_B_L10_625:
			case VBI_SLICED_TELETEXT_B_L25_625:
			case VBI_SLICED_TELETEXT_B_625:
				if (!decode_teletext (sf,
						      &pass_through,
						      sliced_in[in].data,
						      sliced_in[in].line))
					goto failed;
				break;

			default:
				debug2 (&sf->log, "Dropping service.");
				break;
			}
		}

		if (pass_through) {
			if (out >= max_lines_out) {
				set_errstr (sf, _("Output buffer overflow."));
				errno = VBI_ERR_BUFFER_OVERFLOW;
				goto failed;
			}

			memcpy (&sliced_out[out],
				&sliced_in[in],
				sizeof (*sliced_out));
			++out;
		}
	}

	*n_lines_out = out;

	return TRUE;

 failed:
	*n_lines_in = in + 1;
	*n_lines_out = out;

	return FALSE;
}

/**
 * @brief Feeds the sliced VBI filter with data.
 * @param sf Sliced VBI filter context allocated with
 *   vbi_sliced_filter_new().
 * @param sliced The sliced data to be filtered.
 * @param n_lines Pointer to a variable which contains the
 *   number of sliced lines to be read from the @a sliced buffer.
 *   When the function fails, it stores here the number of sliced
 *   lines successfully read so far.
 *
 * This function takes one video frame worth of sliced VBI data and
 * filters out the lines which match the selected criteria. Then if
 * no error occurred it calls the callback function passed to
 * vbi_sliced_filter_new() with a pointer to the filtered lines.
 *
 * @returns
 * @c TRUE on success. @c FALSE if the function detects an error in
 * the sliced input data, and @a *n_lines_in will contain the lines
 * successfully read so far.
 *
 * @since 99.99.99
 */
vbi_bool
vbi_sliced_filter_feed		(vbi_sliced_filter *	sf,
				 const vbi_sliced *	sliced,
				 unsigned int *		n_lines)
{
	unsigned int n_lines_out;

	assert (NULL != sf);
	assert (NULL != sliced);
	assert (NULL != n_lines);

	assert (*n_lines <= UINT_MAX / sizeof (vbi_sliced));

	if (unlikely (sf->output_max_lines < *n_lines)) {
		vbi_sliced *s;
		unsigned int n;

		n = MIN (*n_lines, 50U);
		s = realloc (sf->output_buffer,
			     n * sizeof (*sf->output_buffer));
		if (unlikely (NULL == s)) {
			no_mem_error (sf);
			return FALSE;
		}

		sf->output_buffer = s;
		sf->output_max_lines = n;
	}

	if (!vbi_sliced_filter_cor (sf,
				    sf->output_buffer,
				    &n_lines_out,
				    sf->output_max_lines,
				    sliced,
				    n_lines)) {
		return FALSE;
	}

	if (NULL != sf->callback) {
		return sf->callback (sf,
				     sf->output_buffer,
				     n_lines_out,
				     sf->user_data);
	}

	return TRUE;
}

const char *
vbi_sliced_filter_errstr	(vbi_sliced_filter *	sf)
{
	assert (NULL != sf);

	return sf->errstr;
}

void
vbi_sliced_filter_set_log_fn	(vbi_sliced_filter *	sf,
				 vbi_log_mask		mask,
				 vbi_log_fn *		log_fn,
				 void *			user_data)
{
	assert (NULL != sf);

	if (NULL == log_fn)
		mask = 0;

	sf->log.mask = mask;
	sf->log.fn = log_fn;
	sf->log.user_data = user_data;
}

void
vbi_sliced_filter_delete	(vbi_sliced_filter *	sf)
{
	if (NULL == sf)
		return;

	free (sf->keep_ttx_subpages);

	free (sf->output_buffer);

	free (sf->errstr);

	CLEAR (*sf);

	vbi_free (sf);		
}

vbi_sliced_filter *
vbi_sliced_filter_new		(vbi_sliced_filter_cb *	callback,
				 void *			user_data)
{
	vbi_sliced_filter *sf;

	sf = vbi_malloc (sizeof (*sf));
	if (NULL == sf) {
		return NULL;
	}

	CLEAR (*sf);

	sf->callback = callback;
	sf->user_data = user_data;

	return sf;
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
