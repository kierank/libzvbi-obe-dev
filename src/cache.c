/*
 *  libzvbi - Teletext cache
 *
 *  Copyright (C) 2001, 2002 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
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

#include "../site_def.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>

#include "cache.h"
#include "vbi.h"
#include "misc.h"

#warning untested, check esp level 3.5

#ifndef CACHE_DEBUG
#define CACHE_DEBUG 0
#endif

#ifndef CACHE_STATUS
#define CACHE_STATUS 0
#endif

/**
 * @addtogroup Cache Page cache
 * @ingroup Service
 */

/*
 * list foo_list;
 * struct foo { int baz; node bar; }, *foop;
 *
 * for_all_nodes(foop, &foo_list, bar)
 *   foop->baz = 0;
 *
 * Not useful to delete list members.
 */
#define for_all_nodes(p, l, _node_)					\
for ((p) = PARENT((l)->head, typeof(*(p)), _node_);			\
     (p)->_node_.succ;							\
     (p) = PARENT((p)->_node_.succ, typeof(*(p)), _node_))

/**
 * @internal
 * @param l list *
 *
 * Free all resources associated with the list,
 * you must pair this with an init_list() call.
 *
 * Does not free the list object or any nodes.
 */
static __inline__ void
list_destroy			(list *			l)
{
}

/**
 * @internal
 * @param l list * 
 * 
 * @return
 * The list pointer.
 */
static __inline__ list *
list_init			(list *			l)
{
	if (l) {
		l->head = (node *) &l->null;
		l->null = (node *) 0;
		l->tail = (node *) &l->head;
	}

	return l;
}

/**
 * @internal
 * @param l list *
 * 
 * @return
 * @c 1 if the list is empty, @c 0 otherwise. You can read
 * l->members directly when the rwlock is unused.
 */
static __inline__ int
empty_list			(list *			l)
{
	return l->head == (node *) &l->null;
}

/**
 * @internal
 * @param l list *
 * @param n node *
 * 
 * Add node at the head of the list.
 *
 * @return
 * The node pointer.
 */
static __inline__ node *
add_head			(list *			l,
				 node *			n)
{
	n->pred = (node *) &l->head;
	n->succ = l->head;
	l->head->pred = n;
	l->head = n;

	return n;
}

/**
 * @internal
 * @param l list *
 * @param n node *
 * 
 * Add node at the end of the list.
 * 
 * @return 
 * The node pointer.
 */
static __inline__ node *
add_tail			(list *			l,
				 node *			n)
{
	n->succ = (node *) &l->null;
	n->pred = l->tail;
	l->tail->succ = n;
	l->tail = n;

	return n;
}

/**
 * @internal
 * @param l1 list *
 * @param l2 list *
 * 
 * Add all nodes on l2 to the end of l1.
 * 
 * @return 
 * Head node of l2.
 */
static __inline__ node *
add_tail_list			(list *			l1,
				 list *			l2)
{
	node *n = l2->head;

	n->succ = (node *) &l1->null;
	n->pred = l1->tail;
	l1->tail->succ = n;
	l1->tail = l2->tail;

	l2->head = (node *) &l2->null;
	l2->tail = (node *) &l2->head;

	return n;
}

/**
 * @internal
 * @param l list *
 * 
 * Remove first node of the list.
 * 
 * @return
 * Node pointer, or @c NULL if the list is empty.
 */
static __inline__ node *
rem_head			(list *			l)
{
	node *n = l->head, *s = n->succ;

	if (__builtin_expect (s != NULL, 1)) {
		s->pred = (node *) &l->head;
		l->head = s;
	} else {
		n = NULL;
	}

	return n;
}

/**
 * @internal
 * @param l list *
 * 
 * Remove last node of the list.
 * 
 * @return 
 * Node pointer, or @c NULL if the list is empty.
 */
static __inline__ node *
rem_tail			(list *			l)
{
	node *n = l->tail, *p = n->pred;

	if (__builtin_expect (p != NULL, 1)) {
		p->succ = (node *) &l->null;
		l->tail = p;
	} else {
		n = NULL;
	}

	return n;
}

/**
 * @internal
 * @param n node *
 * 
 * Remove the node from its list. The node must
 * be a member of the list, not verified.
 * 
 * @return 
 * The node pointer.
 */
static __inline__ node *
unlink_node			(node *			n)
{
	n->pred->succ = n->succ;
	n->succ->pred = n->pred;

	return n;
}

/**
 * @internal
 * @param l list *
 * @param n node *
 * 
 * Remove the node if member of the list.
 * 
 * @return 
 * The node pointer or @c NULL if the node is not
 * member of the list.
 */
static __inline__ node *
rem_node			(list *			l,
				 node *			n)
{
	node *q;

	for (q = l->head; q->succ; q = q->succ)
		if (n == q) {
			n->pred->succ = n->succ;
			n->succ->pred = n->pred;

			return n;
		}

	return NULL;
}

typedef enum cache_priority cache_priority;

enum cache_priority {
	/* Locked page to be deleted when unlocked. */
	CACHE_PRI_ZOMBIE,

	/* Ordinary pages of other channels, oldest at head of list.
	 * These are deleted first when we run out of memory.
	 */
	CACHE_PRI_ATTIC_NORMAL,

	/* Special pages of other channels, oldest at head of list. */
	CACHE_PRI_ATTIC_SPECIAL,

	/* Ordinary pages of the current channel. */
	CACHE_PRI_NORMAL,

	/* Pages we expect to use frequently, or which take long to reload:
	 * - pgno x00 and xyz with x=y=z
	 * - shared pages (objs, drcs, navigation)
	 * - subpages
	 * - visited pages (most recent at end of list)
	 */
	CACHE_PRI_SPECIAL,

	CACHE_PRI_NUM
};

struct cache_page {
	node			hash_node;	/* hash chain */
	node			pri_node;	/* priority chain */

	cache_stat *		stat;		/* network sending this page */

	unsigned int		ref_count;
	cache_priority		priority;

	vt_page			page;

	/* Dynamic size, don't add fields below */
};

static const unsigned int cache_page_overhead =
	sizeof (cache_page) - sizeof (((cache_page *) 0)->page);

static void
cache_page_dump			(const cache_page *	cp,
				 char			lf)
{
	const cache_stat *cs;
	static const char *cache_pri_str[CACHE_PRI_NUM] = {
		"ZOMBIE",
		"ATTIC_NORMAL",
		"ATTIC_SPECIAL",
		"NORMAL",
		"SPECIAL",
	};

	fprintf (stderr, "page %x.%x ", cp->page.pgno, cp->page.subno);

	if ((cs = cp->stat)) {
		const page_stat *ps = cs->pages + cp->page.pgno - 0x100;

		fprintf (stderr, "nuid=%08x/%08x C%u/L%u/S%04x sub %u/%u (%u-%u) ",
			 cs->temp_nuid, cs->real_nuid,
			 ps->code, ps->language, ps->subcode,
			 ps->num_pages, ps->max_pages,
			 ps->subno_min, ps->subno_max);
	} else {
		fprintf (stderr, "nuid=n/a ");
	}

	fprintf (stderr, "ref=%u CACHE_PRI_%s%c",
		 cp->ref_count,
		 cache_pri_str[cp->priority],
		 lf);
}

static void
cache_stat_dump			(const cache_stat *	cs,
				 char			lf)
{
	if (cs)
		fprintf (stderr, "cache_stat nuid=%08x/%08x "
			 "pages=%u/%u locked=%u ref=%u%c",
			 cs->temp_nuid, cs->real_nuid,
			 cs->num_pages, cs->max_pages,
			 cs->locked_pages,
			 cs->ref_count,
			 lf);
	else
		fprintf (stderr, "no cache_stat%c", lf);
}

static __inline__ void
cache_stat_remove_page		(cache_page *		cp)
{
	cache_stat *cs;
	page_stat *ps;

	if (!(cs = cp->stat))
		return;

	cp->stat = NULL;

	cs->num_pages--;

	if (CACHE_DEBUG)
		cs->locked_pages -= (cp->ref_count > 0);

	ps = cs->pages + cp->page.pgno - 0x100;

	ps->num_pages--;
}

static __inline__ void
cache_stat_add_page		(cache_stat *		cs,
				 cache_page *		cp)
{
	page_stat *ps;
	vbi_subno subno;

	cp->stat = cs;

	cs->num_pages++;

	if (cs->num_pages > cs->max_pages)
		cs->max_pages = cs->num_pages;

	ps = cs->pages + cp->page.pgno - 0x100;

	ps->num_pages++;

	if (ps->num_pages > ps->max_pages)
		ps->max_pages = ps->num_pages;

	subno = cp->page.subno;

	if (__builtin_expect (subno > 0x00 && subno <= 0x99, 0)) {
		if (ps->subno_min == 0 || subno < ps->subno_min)
			ps->subno_min = subno;
		if (subno > ps->subno_max)
			ps->subno_max = subno;
	}
}

#define HASH_SIZE 113

static __inline__ unsigned int
hash				(vbi_pgno		pgno)
{
	return pgno % HASH_SIZE;
}

struct cache {
	pthread_mutex_t		mutex;

	/* Pages by pgno (normal access) */

	list			hash[HASH_SIZE];

	unsigned int		num_pages;

	/* Pages by priority (to speed up replacement) */

	list			priority[CACHE_PRI_NUM];

	unsigned long		mem_used;
	unsigned long		mem_max;

	/* Locked pages (ref_count > 0), pri_node */

	list			locked;

	/* Page statistics */

	list			stations;

	unsigned int		num_stations;
	unsigned int		max_stations;
};

static void
cache_dump			(const cache *		ca,
				 char			lf)
{
	fprintf (stderr, "cache pages=%u mem=%lu/%lu KB stations=%u/%u%c",
		 ca->num_pages,
		 (ca->mem_used + 1023) >> 10,
		 (ca->mem_max + 1023) >> 10,
		 ca->num_stations, ca->max_stations,
		 lf);
}

static __inline__ void
cache_lock			(cache *		ca)
{
	int r = pthread_mutex_lock (&ca->mutex);

	assert (r == 0);
}

static __inline__ void
cache_unlock			(cache *		ca)
{
	int r = pthread_mutex_unlock (&ca->mutex);

	assert (r == 0);
}

static __inline__ cache_stat *
cache_stat_from_nuid		(cache *		ca,
				 vbi_nuid		nuid)
{
	cache_stat *cs;

	for_all_nodes (cs, &ca->stations, node)
		if (cs->temp_nuid == nuid || cs->real_nuid == nuid) {
			if (__builtin_expect (ca->stations.head != &cs->node, 0)) {
				/* Probably needed again soon */

				unlink_node (&cs->node);
				add_head (&ca->stations, &cs->node);
			}

			return cs;
		}

	return NULL;
}

static __inline__ void
delete_page			(cache *		ca,
				 cache_page *		cp)
{
	if (CACHE_DEBUG) {
		fprintf (stderr, "delete "); 
		cache_page_dump (cp, ' ');
		cache_stat_dump (cp->stat, '\n');
	}

	if (__builtin_expect (cp->priority != CACHE_PRI_ZOMBIE, 1))
		ca->mem_used -= vtp_size (&cp->page) + cache_page_overhead;

	unlink_node (&cp->pri_node);
	unlink_node (&cp->hash_node);

	cache_stat_remove_page (cp);

	free (cp);

	ca->num_pages--;
}

static void
delete_all_by_priority		(cache *		ca,
				 cache_priority		pri)
{
	list *pri_list = ca->priority + pri;

	while (!empty_list (pri_list)) {
		cache_page *cp = PARENT (pri_list->head, cache_page, pri_node);

		delete_page (ca, cp);
	}
}

static void
delete_all_by_nuid		(cache *		ca,
				 cache_stat *		cs)
{
	cache_page *cp;
	cache_priority pri;

	pri = CACHE_PRI_ATTIC_NORMAL;
	cp = PARENT (ca->priority[pri].head, cache_page, pri_node);

	for (;;) {
		cache_page *next_cp;

		while (__builtin_expect (!cp->pri_node.succ, 0)) {
			/* No page at this priority */

			if (++pri >= CACHE_PRI_NUM)
				return;

			cp = PARENT (ca->priority[pri].head,
				     cache_page, pri_node);
		}

		next_cp = PARENT (cp->pri_node.succ, cache_page, pri_node);

		if (cp->stat == cs)
			delete_page (ca, cp);

		cp = next_cp;
	}
}

static void
cache_flush			(cache *		ca)
{
	cache_page *cp;
	cache_priority pri;

	pri = CACHE_PRI_ATTIC_NORMAL;
	cp = PARENT (ca->priority[pri].head, cache_page, pri_node);

	while (ca->mem_used > ca->mem_max) {
		cache_page *next_cp;

		while (__builtin_expect (!cp->pri_node.succ, 0)) {
			/* No page at this priority */

			if (++pri >= CACHE_PRI_NUM)
				return;

			cp = PARENT (ca->priority[pri].head,
				     cache_page, pri_node);
		}

		next_cp = PARENT (cp->pri_node.succ, cache_page, pri_node);

		delete_page (ca, cp);

		cp = next_cp;
	}
}

static void
change_priority			(cache *		ca,
				 cache_priority		pri_to,
				 cache_priority		pri_from)
{
	list *pri_to_list;
	list *pri_from_list;
	cache_page *cp;

	pri_to_list = ca->priority + pri_to;
	pri_from_list = ca->priority + pri_from;

	for_all_nodes (cp, pri_from_list, pri_node)
		cp->priority = pri_to;

	add_tail_list (pri_to_list, pri_from_list);
}

/**
 * @internal
 * @param vbi
 * @param vtp
 * 
 * Unref a page returned by vbi_cache_get ().
 */
void
vbi_cache_unref			(vbi_decoder *		vbi,
				 const vt_page *	vtp)
{
	cache *ca = vbi->cache;
	cache_page *cp;

	if (!vtp)
		return;

	cp = PARENT ((vt_page *) vtp, cache_page, page);

	assert (cp->ref_count > 0);

	cache_lock (ca);

	if (CACHE_DEBUG) {
		fprintf (stderr, "vbi_cache_unref ");
		cache_dump (ca, ' ');
		cache_page_dump (cp, ' ');
	}

	if (__builtin_expect (--cp->ref_count == 0, 1)) {
		if (__builtin_expect (cp->priority == CACHE_PRI_ZOMBIE, 0)) {
			delete_page (ca, cp);

			if (CACHE_DEBUG)
				cp->stat->locked_pages--;
		} else {
			if (CACHE_DEBUG) {
				cache_stat_dump (cp->stat, ' ');
				cp->stat->locked_pages--;
			}

			add_tail (&ca->priority[cp->priority],
				  unlink_node (&cp->pri_node));

			ca->mem_used += vtp_size (&cp->page) + cache_page_overhead;
		}

		if (__builtin_expect (ca->mem_used > ca->mem_max, 0))
			cache_flush (ca);
	}

	if (CACHE_DEBUG)
		fputc ('\n', stderr);

	cache_unlock (ca);
}

static cache_page *
page_lookup			(cache *		ca,
				 cache_stat *		cs,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 vbi_subno		subno_mask,
				 list **		hlist)
{
	list *hash_list;
	cache_page *cp;

	if (subno == VBI_ANY_SUBNO) {
		subno = 0;
		subno_mask = 0;
	}

	hash_list = ca->hash + hash (pgno);

	for_all_nodes (cp, hash_list, hash_node) {
		if (CACHE_DEBUG > 1) {
			fprintf (stderr, "trying ");
			cache_page_dump (cp, '\n');
		}

		if (cp->page.pgno == pgno
		    && (cp->page.subno & subno_mask) == subno
		    && cp->stat == cs
		    && cp->priority != CACHE_PRI_ZOMBIE)
			goto found;
	}

	return NULL;

 found:
	*hlist = hash_list;

	if (CACHE_DEBUG) {
		fprintf (stderr, "found ");
		cache_page_dump (cp, ' ');
	}

	if (__builtin_expect (cp->ref_count == 0, 1)) {
		if (CACHE_DEBUG) {
			cache_stat_dump (cs, ' ');
			cs->locked_pages++;
		}

		ca->mem_used -= vtp_size (&cp->page) + cache_page_overhead;

		add_tail (&ca->locked, unlink_node (&cp->pri_node));
	}

	if (CACHE_DEBUG)
		fputc ('\n', stderr);

	cp->ref_count++;

	return cp;
}

/**
 * @internal
 * @param vbi vbi_decoder *
 * @param nuid
 * @param pgno
 * @param subno
 * @param subno_mask
 * @param user_access Raise the page priority.
 * 
 * Get a page from the cache. When @param subno is SUB_ANY, the most
 * recently received subpage of that page is returned.
 * 
 * The reference counter of the page is incremented, you must call
 * vbi_cache_unref() to unref the page. When @param user_access is set,
 * the page gets higher priority to stay in cache and is marked as
 * most recently received subpage.
 * 
 * @return 
 * const vt_page pointer, NULL when the requested page is not cached.
 */
const vt_page *
vbi_cache_get			(vbi_decoder *		vbi,
				 vbi_nuid		nuid,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 vbi_subno		subno_mask,
				 vbi_bool		user_access)
{
	cache *ca = vbi->cache;
	cache_stat *cs;
	cache_page *cp;
	list *hash_list;

	cache_lock (ca);

	if (CACHE_DEBUG) {
		fprintf (stderr, "vbi_cache_get %s%x.%x/%x ",
			 user_access ? "user " : "",
			 pgno, subno, subno_mask);
		cache_dump (ca, '\n');
	}

	if (!(cs = cache_stat_from_nuid (ca, nuid)))
		goto failure;

	if (!(cp = page_lookup (ca, cs, pgno, subno, subno_mask, &hash_list)))
		goto failure;

	if (user_access) {
		switch (cp->priority) {
		case CACHE_PRI_ATTIC_NORMAL:
			cp->priority = CACHE_PRI_ATTIC_SPECIAL;
			break;

		case CACHE_PRI_NORMAL:
			cp->priority = CACHE_PRI_SPECIAL;
			break;

		default:
			break;
		}

		add_head (hash_list, unlink_node (&cp->hash_node));
	}

	cache_unlock (ca);

	return &cp->page;

 failure:
	cache_unlock (ca);

	return NULL;
}

/* XXX rethink */
int
vbi_cache_foreach		(vbi_decoder *		vbi,
				 vbi_nuid		nuid,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 int			dir,
				 foreach_callback *	func,
				 void *			data)
{
	cache *ca = vbi->cache;
	cache_stat *cs;
	cache_page *cp;
	page_stat *ps;
	vbi_bool wrapped = FALSE;
	list *hash_list;

	cache_lock (ca);

	cs = cache_stat_from_nuid (ca, nuid);

	if (!cs || cs->num_pages == 0)
		return 0;

	if ((cp = page_lookup (ca, cs, pgno, subno, ~0, &hash_list))) {
		subno = cp->page.subno;
	} else if (subno == VBI_ANY_SUBNO) {
		cp = NULL;
		subno = 0;
	}

	ps = cs->pages + pgno - 0x100;

	for (;;) {
		if (cp) {
			int r;

			cache_unlock (ca); /* XXX */

			if ((r = func (data, &cp->page, wrapped)))
				return r;

			cache_lock (ca);
		}

		subno += dir;

		while (ps->num_pages == 0
		       || subno < ps->subno_min
		       || subno > ps->subno_max) {
			if (dir < 0) {
				pgno--;
				ps--;

				if (pgno < 0x100) {
					pgno = 0x8FF;
					ps = cs->pages + 0x7FF;
					wrapped = 1;
				}

				subno = ps->subno_max;
			} else {
				pgno++;
				ps++;

				if (pgno > 0x8FF) {
					pgno = 0x100;
					ps = cs->pages + 0x000;
					wrapped = 1;
				}

				subno = ps->subno_min;
			}
		}

		cp = page_lookup (ca, cs, pgno, subno, ~0, &hash_list);
	}
}

static void
cache_stations_flush		(cache *		ca)
{
	cache_stat *cs = PARENT (ca->stations.tail, cache_stat, node);

	while (ca->num_stations > ca->max_stations) {
		cache_stat *prev_cs = PARENT (cs->node.pred, cache_stat, node);

		/* Flush last recently used */

		if (!prev_cs) /* no more stations */
			break;

		if (cs->ref_count == 0) {
			if (CACHE_DEBUG) {
				fprintf (stderr, "flushing ");
				cache_stat_dump (cs, '\n');
			}

			if (cs->num_pages > 0)
				delete_all_by_nuid (ca, cs);

			unlink_node (&cs->node);

			free (cs);

			ca->num_stations--;
		}

		cs = prev_cs;
	}
}

static cache_stat *
cache_stat_create		(cache *		ca,
				 vbi_nuid		nuid)
{
	cache_stat *cs = cache_stat_from_nuid (ca, nuid);

	if (cs) /* already present */
		return cs;

	if (ca->num_stations >= ca->max_stations) {
		/* We absorb the last recently used cache_stat */

		cs = PARENT (ca->stations.tail, cache_stat, node);

		while (cs->ref_count > 0) {
			cs = PARENT (cs->node.pred, cache_stat, node);

			if (!cs->node.pred) {
				if (CACHE_DEBUG)
					fprintf (stderr, "all stations locked in "
						 "cache_stat_create\n");
				return NULL;
			}
		}

		if (cs->num_pages > 0)
			delete_all_by_nuid (ca, cs);

		cs->temp_nuid = nuid; /* FIXME */
		cs->real_nuid = nuid;

		cs->num_pages = 0;
		cs->max_pages = 0;
		cs->locked_pages = 0;
		cs->ref_count = 0;

		memset (cs->pages, 0, sizeof (cs->pages));

		unlink_node (&cs->node);
	} else {
		if (!(cs = calloc (1, sizeof (*cs))))
			return NULL;

		cs->temp_nuid = nuid; /* FIXME */
		cs->real_nuid = nuid;

		ca->num_stations++;
	}

	add_head (&ca->stations, &cs->node);

	return cs;
}

/**
 * @internal
 * @param vbi
 * @param cs
 */
void
vbi_cache_stat_unref		(vbi_decoder *		vbi,
				 const cache_stat *	cs)
{
	cache *ca = vbi->cache;

	if (!cs)
		return;

	assert (cs->ref_count > 0);

	cache_lock (ca);

	((cache_stat *) cs)->ref_count--;

	if (ca->num_stations > ca->max_stations)
		cache_stations_flush (ca);

	cache_unlock (ca);
}

/**
 * @internal
 * @param vbi
 * @param nuid
 */
const cache_stat *
vbi_cache_stat			(vbi_decoder *		vbi,
				 vbi_nuid		nuid)
{
	cache *ca = vbi->cache;
	cache_stat *cs;

	cache_lock (ca);

	if ((cs = cache_stat_create (ca, nuid)))
		cs->ref_count++;

	cache_unlock (ca);

	return cs;
}

/**
 * @internal
 * @param vbi
 * @param nuid
 * @param vtp
 * @param new_ref
 *
 * When the function succeeded and @param new_ref is true, it calls
 * vbi_cache_get (vbi, nuid, vtp->pgno, vtp->subno, ~0, FALSE) and
 * returns a pointer to the stored page. Otherwise it returns the
 * @param vtp parameter, or @c NULL on failure.
 */
const vt_page *
vbi_cache_put			(vbi_decoder *		vbi,
				 vbi_nuid		nuid,
				 const vt_page *	vtp,
				 vbi_bool		new_ref)
{
	cache *ca = vbi->cache;
	cache_page *dead_row[20];
	unsigned int dead_count;
	cache_page *old_cp, *cp;
	cache_priority pri;
	list *hash_list;
	cache_stat *cs;
	long int free_size;
	long int new_size;
	unsigned int i;

	dead_count = 0;

	free_size = (long) ca->mem_max - (long) ca->mem_used; /* can be < 0 */
	new_size = vtp_size (vtp) + cache_page_overhead;

	hash_list = ca->hash + hash (vtp->pgno);

	cache_lock (ca);

	if (CACHE_DEBUG) {
		fprintf (stderr, "vbi_cache_put %x.%x ", vtp->pgno, vtp->subno);
		cache_dump (ca, ' ');
	}

	if (!(cs = cache_stat_create (ca, nuid)))
		return FALSE;

	if (CACHE_DEBUG)
		cache_stat_dump (cs, '\n');

	for_all_nodes (old_cp, hash_list, hash_node)
		if (old_cp->stat == cs
		    && old_cp->page.pgno == vtp->pgno
		    && old_cp->page.subno == vtp->subno)
			break;

	if (__builtin_expect (old_cp->hash_node.succ != NULL, 1)) {
		if (CACHE_DEBUG) {
			fprintf (stderr, "is cached ");
			cache_page_dump (old_cp, '\n');
		}

		if (__builtin_expect (old_cp->ref_count > 0, 0)) {
			/* Not reusable */

			unlink_node (&old_cp->hash_node);
			cache_stat_remove_page (old_cp);

			old_cp->priority = CACHE_PRI_ZOMBIE;
			old_cp = NULL;
		} else {
			dead_row[dead_count++] = old_cp;
			free_size += vtp_size (&old_cp->page) + cache_page_overhead;
		}
	} else {
		old_cp = NULL;
	}

	pri = CACHE_PRI_ATTIC_NORMAL;
	cp = PARENT (ca->priority[pri].head, cache_page, pri_node);

	while (free_size < new_size) {
		while (__builtin_expect (!cp->pri_node.succ, 0)) {
			/* No page at this priority */

			if (++pri >= CACHE_PRI_NUM) {
				if (CACHE_DEBUG)
					fprintf (stderr, "not enough old "
						 "unlocked pages to replace\n");
				goto failure;
			}

			cp = PARENT (ca->priority[pri].head, cache_page, pri_node);
		}

		if (__builtin_expect (cp != old_cp, 1)) {
			assert (dead_count < N_ELEMENTS (dead_row));
			dead_row[dead_count++] = cp;
			free_size += vtp_size (&cp->page) + cache_page_overhead;
		}

		cp = PARENT (cp->pri_node.succ, cache_page, pri_node);
	}

	i = 0;

	if (__builtin_expect (free_size == new_size && dead_count == 1, 1)) {
		cp = dead_row[i++];

		if (CACHE_DEBUG) {
			fprintf (stderr, "reusing ");
			cache_page_dump (cp, ' ');
		}

		unlink_node (&cp->pri_node);
		unlink_node (&cp->hash_node);

		cache_stat_remove_page (cp);
	} else {
		if (!(cp = malloc (new_size))) {
			if (CACHE_DEBUG)
				fprintf (stderr, "out of memory\n");
			goto failure;
		}

		ca->num_pages++;
	}

	while (i < dead_count)
		delete_page (ca, dead_row[i++]);

	add_head (hash_list, &cp->hash_node);

	if (__builtin_expect
	    ((vtp->subno > 0 && vtp->subno < 0x99)		/* Rolling page */
	    || (vtp->pgno & 0xFF) == 0x00			/* Magazine start page */
	     || (vtp->pgno >> 4) == (vtp->pgno & 0xFF)		/* Magic pgno Mmm */
	    || vtp->function != PAGE_FUNCTION_LOP, 0)) {	/* Shared page (objs, drcs) */
		cp->priority = CACHE_PRI_SPECIAL;
	} else {
		cp->priority = CACHE_PRI_NORMAL;
	}

	memcpy (&cp->page, vtp, new_size - cache_page_overhead);

	if (new_ref) {
		cp->ref_count = 1;
		ca->mem_used += 0;

		if (CACHE_DEBUG)
			cs->locked_pages++;

		add_tail (&ca->locked, &cp->pri_node);

		vtp = &cp->page;
	} else {
		cp->ref_count = 0;
		ca->mem_used += new_size;

		add_tail (ca->priority + cp->priority, &cp->pri_node);
	}

	cache_stat_add_page (cs, cp);

	if (CACHE_STATUS) {
		fprintf (stderr, "cache status:\n");
		cache_dump (ca, '\n');
		cache_page_dump (cp, '\n');
		cache_stat_dump (cp->stat, '\n');
	}

	cache_unlock (ca);

	return vtp;

 failure:
	cache_unlock (ca);

	return NULL;
}

/**
 * @internal
 * @param vbi vbi_decoder *
 * @param all Free cached pages regardless of priority.
 * 
 * Flush the cached pages of the current station. When the cache stores
 * multiple stations this just lowers the priority of the pages to stay
 * in the cache when we run out of memory. Otherwise the pages are freed.
 * 
 * Referenced pages are marked for according flushing when unref'ed.
 */
void
vbi_cache_flush			(vbi_decoder *		vbi,
				 vbi_bool		all)
{
	cache *ca = vbi->cache;
	cache_priority pri;
	cache_page *cp;

	cache_lock (ca);

	if (ca->max_stations > 1 && !all) {
		if (CACHE_DEBUG) {
			fprintf (stderr, "vbi_cache_flush pri shift "); 
			cache_dump (ca, '\n');
		}

		change_priority (ca, CACHE_PRI_ATTIC_NORMAL,
				 CACHE_PRI_NORMAL);

		change_priority (ca, CACHE_PRI_ATTIC_SPECIAL,
				 CACHE_PRI_SPECIAL);

		for_all_nodes (cp, &ca->locked, pri_node) {
			if (cp->priority == CACHE_PRI_NORMAL)
				cp->priority = CACHE_PRI_ATTIC_NORMAL;
			else if (cp->priority == CACHE_PRI_SPECIAL)
				cp->priority = CACHE_PRI_ATTIC_SPECIAL;
		}
    	} else {
		if (CACHE_DEBUG) {
			fprintf (stderr, "vbi_cache_flush all=%u ", all); 
			cache_dump (ca, '\n');
		}

		if (all) {
			for_all_nodes (cp, &ca->locked, pri_node) {
				cache_stat_remove_page (cp);
				cp->priority = CACHE_PRI_ZOMBIE;
			}

			pri = CACHE_PRI_ATTIC_NORMAL;
		} else {
			pri = CACHE_PRI_NORMAL;
		}

		for (; pri < CACHE_PRI_NUM; pri++)
			delete_all_by_priority (ca, pri);
	}

	cache_unlock (ca);
}

void
vbi_cache_destroy		(vbi_decoder *		vbi)
{
	cache *ca = vbi->cache;
	unsigned int i;
	int r;

	if (!empty_list (&ca->locked)) {
		fprintf (stderr, "Bug warning: not all cache pages "
			 "have been unreferenced, memory leaks.\n");
		return;
	}

	vbi_cache_flush (vbi, TRUE);

	ca->num_stations = 0;

	cache_stations_flush (ca);

	if (!empty_list (&ca->stations)) {
		fprintf (stderr, "Bug warning: not all cache_stat "
			 "have been unreferenced, memory leaks.\n");
		return;
	}

	list_destroy (&ca->stations);

	list_destroy (&ca->locked);

	for (i = 0; i < sizeof (ca->priority) / sizeof (*ca->priority); i++)
		list_destroy (ca->priority + i);

	for (i = 0; i < sizeof (ca->hash) / sizeof (*ca->hash); i++)
		list_destroy (ca->hash + i);

	r = pthread_mutex_destroy (&ca->mutex);

	assert (r == 0);

	free (ca);

	vbi->cache = NULL;
}

/**
 * @param vbi vbi_decoder *
 * @param size Maximum size of the cache in bytes.
 * 
 * Determine the memory occupancy of the Teletext page cache. Reasonable
 * values range from 16 KB to 1 GB, default is 1 GB. (Together with the
 * default of caching only one station this mimics the libzvbi 0.2
 * behaviour of never deleting cached pages until the station is
 * switched.) The number of pages transmitted by stations varies,
 * expect on the order of one megabyte for a complete set.
 * 
 * When the cache is too small to contain all pages of a station,
 * newly received pages will replace older pages. Priority to keep them
 * is given pages of the current station over other stations, to pages
 * which take longer to reload, which have been recently requested or
 * are likely to be requested in the future. Referenced pages have
 * infinite priority but add to the cache size, to prevent a deadlock
 * after all pages have been referenced.
 * 
 * When @param size is smaller than the amount of memory currently
 * used this function attempts to delete an appropriate number of
 * low priority pages.
 */
void
vbi_cache_max_size		(vbi_decoder *		vbi,
				 unsigned long		size)
{
	cache *ca = vbi->cache;

	cache_lock (ca);

	ca->mem_max = SATURATE (size, 16UL << 10, 1UL << 30);

	cache_flush (ca);

	cache_unlock (ca);
}

/**
 * @param vbi vbi_decoder *
 * @param count Maximum number of stations.
 * 
 * Determine the maximum number of stations cached in the Teletext
 * page cache. Useful numbers will be in range 1 to 300, the
 * default is 1 (mimicking libzvbi 0.2). Each station takes about
 * 16 KB additional to the vbi_cache_max_size().
 *
 * When the number is smaller than the current number of stations
 * cached this function attempts to delete the data of the last
 * recently requested stations, including all its pages. If any of
 * those pages are still in use, they are marked for later removal.
 */
void
vbi_cache_max_stations		(vbi_decoder *		vbi,
				 unsigned int		count)
{
	cache *ca = vbi->cache;

	cache_lock (ca);

	ca->max_stations = SATURATE (count, 1, 300);

	cache_stations_flush (ca);

	cache_unlock (ca);
}

void
vbi_cache_init			(vbi_decoder *		vbi)
{
	cache *ca;
	unsigned int i;
	int r;

	ca = malloc (sizeof (*ca));

	vbi->cache = ca;

	r = pthread_mutex_init (&ca->mutex, NULL);

	assert (r == 0);

	for (i = 0; i < sizeof (ca->hash) / sizeof (*ca->hash); i++)
		list_init (ca->hash + i);

	ca->num_pages = 0;

	for (i = 0; i < sizeof (ca->priority) / sizeof (*ca->priority); i++)
		list_init (ca->priority + i);

	ca->mem_used = 0;
	ca->mem_max = 1 << 30;

	list_init (&ca->locked);

	list_init (&ca->stations);

        ca->num_stations = 0;
	ca->max_stations = 1;
}

/**
 * @param pg Previously fetched vbi_page.
 *
 * A vbi_page fetched from cache with vbi_fetch_vt_page() or
 * vbi_fetch_cc_page() may reference other resource in cache which
 * are locked after fetching. When done processing the page, you
 * must call this function to unlock all the resources associated
 * with this vbi_page.
 */
void
vbi_unref_page(vbi_page *pg)
{
#warning XXX
	if (pg && pg->vbi)
		pg->vbi->pageref--;
}
