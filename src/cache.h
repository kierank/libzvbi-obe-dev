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

/* $Id: cache.h,v 1.4 2003/02/16 21:11:14 mschimek Exp $ */

#ifndef CACHE_H
#define CACHE_H

#include "vt.h" /* vt_page */

#ifndef VBI_DECODER
#define VBI_DECODER
typedef struct vbi_decoder vbi_decoder;
#endif

#define HASH_SIZE 113

typedef struct node node;
typedef struct list list;

struct node {
	node *			succ;
	node *			pred;
};

struct list {
	node *			head;
	node *			null;
	node *			tail;
	int			members;
};

struct cache {
	/* TODO: thread safe */
	list			hash[HASH_SIZE];

	int			npages;

	/* TODO */
	unsigned long		mem_used;
	unsigned long		mem_max;

	/* TODO: multi-station cache */
};

/* Public */

/**
 * @addtogroup Cache
 * @{
 */
extern void		vbi_unref_page(vbi_page *pg);

extern int		vbi_is_cached(vbi_decoder *, int pgno, int subno);
extern int		vbi_cache_hi_subno(vbi_decoder *vbi, int pgno);
/** @} */

/* Private */

typedef int foreach_callback(void *, vt_page *, vbi_bool); 

extern void		vbi_cache_init(vbi_decoder *);
extern void		vbi_cache_destroy(vbi_decoder *);
extern vt_page *	vbi_cache_put(vbi_decoder *, vt_page *vtp);
extern vt_page *	vbi_cache_get(vbi_decoder *, int pgno, int subno, int subno_mask);
extern int              vbi_cache_foreach(vbi_decoder *, int pgno, int subno, int dir, foreach_callback *func, void *data);
extern void             vbi_cache_flush(vbi_decoder *);

#endif /* CACHE_H */
