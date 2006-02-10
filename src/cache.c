/*
 *  libzvbi - Teletext cache
 *
 *  Copyright (C) 2001, 2002 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
 *
 *  XXX this needs work
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#include "cache.h"
#include "vbi.h"

/*
  AleVT:
    There are some subtleties in this cache.

    - Simple hash is used.
    - All subpages of a page are in the same hash chain.
    - The newest subpage is at the front.

    Hmm... maybe a tree would be better...


  **NOTE**: This code is obsolete, see CVS branch-0-3 

*/

/**
 * @addtogroup Cache Page cache
 * @ingroup HiDec
 */

#define CACHED_MAXSUB1(pgno) vbi->vt.cached[((pgno) - 0x100) & 0x7FF]

static inline int
hash(int pgno)
{
    // very simple...
    return pgno % HASH_SIZE;
}

/*
 * list foo_list;
 * struct foo { int baz; node bar; } *foop;
 *
 * for_all_nodes(foop, &foo_list, bar)
 *   foop->baz = 0;
 *
 * Not useful to delete list members.
 */
#define for_all_nodes(p, l, _node_)					\
for ((p) = PARENT ((l)->_head._succ, __typeof__ (* (p)), _node_);	\
     (p) != &(l)->_head;						\
     (p) = PARENT ((p)->_node_._succ, __typeof__ (* (p)), _node_))

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
destroy_list(list *l)
{
	CLEAR (*l);
}

/**
 * @internal
 * @param l list * 
 * 
 * @return
 * The list pointer.
 */
static inline list *
init_list(list *l)
{
	l->_head._succ = &l->_head;
	l->_head._pred = &l->_head;

	l->_n_members = 0;

	return l;
}

/**
 * @internal
 * @param l list *
 * 
 * @return
 * Number of nodes linked in the list. You can read
 * l->members directly when the rwlock is unused.
 */
static inline unsigned int
list_members(list *l)
{
	return l->_n_members;
}

/**
 * @internal
 * @param l list *
 * 
 * @return
 * @c 1 if the list is empty, @c 0 otherwise. You can read
 * l->members directly when the rwlock is unused.
 */
static inline int
empty_list(list *l)
{
	return (0 == l->_n_members);
}

static inline node *
_remove_nodes			(node *			before,
				 node *			after,
				 node *			first,
				 node *			last)
{
	before->_succ = after;
	after->_pred = before;

	first->_pred = NULL;
	last->_succ = NULL;

	return first;
}

static inline node *
_insert_nodes			(node *			before,
				 node *			after,
				 node *			first,
				 node *			last)
{
	first->_pred = before;
        last->_succ = after;

	after->_pred = last;
	before->_succ = first;

	return first;
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
static inline node *
add_head(list *l, node *n)
{
	++l->_n_members;
	return _insert_nodes (&l->_head, l->_head._succ, n, n);
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
static inline node *
add_tail(list *l, node *n)
{
	++l->_n_members;
	return _insert_nodes (l->_head._pred, &l->_head, n, n);
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
static inline node *
rem_head(list *l)
{
	node *n = l->_head._succ;

	if (unlikely (n == &l->_head))
		return NULL;

	--l->_n_members;
	return _remove_nodes (&l->_head, n->_succ, n, n);
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
static inline node *
rem_tail(list *l)
{
	node *n = l->_head._pred;

	if (unlikely (n == &l->_head))
		return NULL;

	--l->_n_members;
	return _remove_nodes (n->_pred, &l->_head, n, n);
}

/**
 * @param l list *
 * @param n node *
 * 
 * Remove the node from its list. The node must
 * be a member of the list, not verified.
 * 
 * @return 
 * The node pointer.
 */
static inline node *
unlink_node(list *l, node *n)
{
	--l->_n_members;
	return _remove_nodes (n->_pred, n->_succ, n, n);
}

static inline vbi_bool
is_member			(const list *		l,
				 const node *		n)
{
	const node *q;

	for (q = l->_head._succ; q != &l->_head; q = q->_succ) {
		if (unlikely (q == n)) {
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * @param l list *
 * @param n node *
 * 
 * Remove the node if member of the list.
 * 
 * @return 
 * The node pointer or @c NULL if the node is not
 * member of the list.
 */
static inline node *
rem_node(list *l, node *n)
{
	if (!is_member (l, n))
		return NULL;

	return unlink_node (l, n);
}

typedef struct {
	node			node;		/* hash chain */
#if 0
	nuid			nuid;		/* network sending this page */
	int			priority;	/* cache purge priority */
	int                     refcount;       /* get */
#endif
	vt_page			page;

	/* dynamic size, no fields below */
} cache_page;

/*
    Get a page from the cache.
    If subno is SUB_ANY, the newest subpage of that page is returned
*/

vt_page *
vbi_cache_get(vbi_decoder *vbi, int pgno, int subno, int subno_mask)
{
	struct cache *ca = &vbi->cache;
	cache_page *cp;
	int h = hash(pgno);

	if (subno == VBI_ANY_SUBNO) {
		subno = 0;
		subno_mask = 0;
	}

	for_all_nodes (cp, ca->hash + h, node) {
		if (0)
			fprintf(stderr, "cache_get %x.%x\n",
				cp->page.pgno, cp->page.subno);
		if (cp->page.pgno == pgno
		    && (cp->page.subno & subno_mask) == subno) {
			/* found, move to front (make it 'new') */
			add_head(ca->hash + h, unlink_node(ca->hash + h, &cp->node));
			return &cp->page;
		}
	}

	return NULL;
}


/**
 * @param vbi 
 * @param pgno 
 * @param subno 
 * 
 * @deprecated At the moment pages can only be added to the
 * cache but not removed unless the decoder is reset. That
 * will change, making the result volatile in a multithreaded
 * environment.
 * 
 * @returns
 * @c TRUE if the given page is cached.
 */
int
vbi_is_cached(vbi_decoder *vbi, int pgno, int subno)
{
	return NULL != vbi_cache_get(vbi, pgno, subno, -1);
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
	if (pg && pg->vbi)
		pg->vbi->pageref--;
}

/*
    Put a page in the cache.
    If it's already there, it is updated.
*/

vt_page *
vbi_cache_put(vbi_decoder *vbi, vt_page *vtp)
{
	struct cache *ca = &vbi->cache;
	cache_page *cp;
	int h = hash(vtp->pgno);
	int size = vtp_size(vtp);
	vbi_bool already_cached;

	if (0)
		fprintf(stderr, "cache_put %x.%x\n",
			vtp->pgno, vtp->subno);

	already_cached = FALSE;
	for_all_nodes (cp, ca->hash + h, node)
		if (cp->page.pgno == vtp->pgno
		    && cp->page.subno == vtp->subno) {
			already_cached = TRUE;
			break;
		}

	if (already_cached) {
		if (vtp_size(&cp->page) == size) {
			// move to front.
			add_head(ca->hash + h, unlink_node(ca->hash + h, &cp->node));
		} else {
			cache_page *new_cp;

			if (!(new_cp = malloc(sizeof(*cp) - sizeof(cp->page) + size)))
				return 0;
			unlink_node(ca->hash + h, &cp->node);
			free(cp);
			cp = new_cp;
			add_head(ca->hash + h, &cp->node);
		}
	} else {
		if (!(cp = malloc(sizeof(*cp) - sizeof(cp->page) + size)))
			return 0;

		if (vtp->subno >= CACHED_MAXSUB1(vtp->pgno))
			CACHED_MAXSUB1(vtp->pgno) = vtp->subno + 1;

		ca->n_pages++;

		add_head(ca->hash + h, &cp->node);
	}

	memcpy(&cp->page, vtp, size);

	return &cp->page;
}


/*
    Same as cache_get but doesn't make the found entry new
*/

static vt_page *
cache_lookup(struct cache *ca, int pgno, int subno)
{
	cache_page *cp;
	int h = hash(pgno);

	for_all_nodes (cp, ca->hash + h, node)
		if (cp->page.pgno == pgno)
			if (subno == VBI_ANY_SUBNO || cp->page.subno == subno)
				return &cp->page;
	return NULL;
}

int
vbi_cache_foreach(vbi_decoder *vbi, int pgno, int subno,
		  int dir, foreach_callback *func, void *data)
{
	struct cache *ca = &vbi->cache;
	vt_page *vtp;
	int wrapped = 0;
	int r;

	if (ca->n_pages == 0)
		return 0;

	if ((vtp = cache_lookup(ca, pgno, subno)))
		subno = vtp->subno;
	else if (subno == VBI_ANY_SUBNO)
		subno = 0;

	for (;;) {
		if ((vtp = cache_lookup(ca, pgno, subno)))
			if ((r = func(data, vtp, wrapped)))
				return r;

		subno += dir;

		while (subno < 0 || subno >= CACHED_MAXSUB1(pgno)) {
			pgno += dir;

			if (pgno < 0x100) {
				pgno = 0x8FF;
				wrapped = 1;
			}

			if (pgno > 0x8FF) {
				pgno = 0x100;
				wrapped = 1;
			}

			subno = dir < 0 ? CACHED_MAXSUB1(pgno) - 1 : 0;
		}
	}
}

/**
 * @param vbi 
 * @param pgno
 * 
 * @deprecated Rationale same as vbi_is_cached().
 * 
 * @returns
 * Highest cached subpage of this page.
 */
int
vbi_cache_hi_subno(vbi_decoder *vbi, int pgno)
{
	return CACHED_MAXSUB1(pgno);
}

void
vbi_cache_flush(vbi_decoder *vbi)
{
	struct cache *ca = &vbi->cache;
	cache_page *cp;
	int h;

	for (h = 0; h < HASH_SIZE; h++)
		while ((cp = PARENT(rem_head(ca->hash + h),
				    cache_page, node))) {
			free(cp);
		}

	memset(vbi->vt.cached, 0, sizeof(vbi->vt.cached));
}

void
vbi_cache_destroy(vbi_decoder *vbi)
{
	struct cache *ca = &vbi->cache;
	int i;

	vbi_cache_flush(vbi);

	for (i = 0; i < HASH_SIZE; i++)
		destroy_list(ca->hash + i);
}

void
vbi_cache_init(vbi_decoder *vbi)
{
	struct cache *ca = &vbi->cache;
	int i;

	for (i = 0; i < HASH_SIZE; i++)
		init_list(ca->hash + i);

	ca->n_pages = 0;

	memset(vbi->vt.cached, 0, sizeof(vbi->vt.cached));
}
