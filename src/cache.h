/*
 *  libzvbi - Teletext page cache
 *
 *  Copyright (C) 2001, 2002 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig
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

/* $Id: cache.h,v 1.3 2002/12/24 15:44:31 mschimek Exp $ */

#ifndef CACHE_H
#define CACHE_H

#include "vt.h" /* vt_page */

#ifndef VBI_DECODER
#define VBI_DECODER
typedef struct vbi_decoder vbi_decoder;
#endif

/* Wheel, reinvented */

typedef struct node node;
typedef struct list list;

/* private, for cache_stat */
struct node {
	node *			succ;
	node *			pred;
};

/* private, for cache_stat */
struct list {
	node *			head;
	node *			null;
	node *			tail;
};

typedef struct page_stat page_stat;
typedef struct cache_page cache_page;
typedef struct cache_stat cache_stat;
typedef struct cache cache;

struct page_stat {
	/* Information gathered from Magazine Inventory Pages (MIP) */

	unsigned 		code		: 8;
	unsigned		language	: 8;
	unsigned 		subcode		: 16;

	/* Cache statistics (read-only outside cache.c) */

	unsigned		num_pages	: 8;
	unsigned		max_pages	: 8;

	unsigned		subno_min	: 8;
	unsigned		subno_max	: 8;
};

/* private, for vbi_page_stat */
struct cache_stat {
	node			node;		/* station chain */

	vbi_nuid		temp_nuid;
	vbi_nuid		real_nuid;

	unsigned int		num_pages;	/* how many cached */
	unsigned int		max_pages;	/* cached and deleted */

	unsigned int		ref_count;
	unsigned int		locked_pages;

	page_stat		pages[0x800];
};

static __inline__ page_stat *
vbi_page_stat			(cache_stat *		cs,
				 vbi_pgno		pgno)
{
	return cs->pages + pgno - 0x100;
}

/* Public */

/**
 * @addtogroup Cache
 * @{
 */
extern void		vbi_unref_page(vbi_page *pg);

extern void		vbi_cache_max_size		(vbi_decoder *		vbi,
							 unsigned long		size);
extern void		vbi_cache_max_stations		(vbi_decoder *		vbi,
							 unsigned int		count);
/** @} */

/* Private */

typedef int		foreach_callback (void *, const vt_page *, vbi_bool);

extern void		vbi_cache_init			(vbi_decoder *		vbi);
extern void		vbi_cache_destroy		(vbi_decoder *		vbi);
extern void		vbi_cache_flush			(vbi_decoder *		vbi,
							 vbi_bool		all);
extern const vt_page *	vbi_cache_put			(vbi_decoder *		vbi,
							 vbi_nuid		nuid,
							 const vt_page *	vtp,
							 vbi_bool		user_access);
extern const vt_page *	vbi_cache_get			(vbi_decoder *		vbi,
							 vbi_nuid		nuid,
							 vbi_pgno		pgno,
							 vbi_subno		subno,
							 vbi_subno		subno_mask,
							 vbi_bool		new_ref);
extern void		vbi_cache_unref			(vbi_decoder *		vbi,
							 const vt_page *	vtp);
extern int		vbi_cache_foreach		(vbi_decoder *		vbi,
							 vbi_nuid		nuid,
							 vbi_pgno		pgno,
							 vbi_subno		subno,
							 int			dir,
							 foreach_callback *	func,
							 void *			data);
extern const cache_stat *vbi_cache_stat			(vbi_decoder *		vbi,
							 vbi_nuid		nuid);
extern void		vbi_cache_stat_unref		(vbi_decoder *		vbi,
							 const cache_stat *	cs);

#endif /* CACHE_H */
