/*
 *  libzvbi -- Table of Teletext page numbers
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

/* $Id: page_table.h,v 1.1 2007/08/27 06:44:31 mschimek Exp $ */

#ifndef __ZVBI_PAGE_TABLE_H__
#define __ZVBI_PAGE_TABLE_H__

#include "macros.h"
#include "bcd.h"

VBI_BEGIN_DECLS

typedef struct _vbi_page_table vbi_page_table;

extern vbi_bool
vbi_page_table_contains_all_subpages
				(const vbi_page_table *pt,
				 vbi_pgno		pgno)
  __attribute__ ((_vbi_nonnull (1)));
extern vbi_bool
vbi_page_table_contains_subpage	(const vbi_page_table *pt,
				 vbi_pgno		pgno,
				 vbi_subno		subno)
  __attribute__ ((_vbi_nonnull (1)));
static __inline__ vbi_bool
vbi_page_table_contains_page	(const vbi_page_table *pt,
				 vbi_pgno		pgno)
{
	return vbi_page_table_contains_subpage (pt, pgno, VBI_ANY_SUBNO);
}
extern vbi_bool
vbi_page_table_next_subpage	(const vbi_page_table *pt,
				 vbi_pgno *		pgno,
				 vbi_subno *		subno)
  __attribute__ ((_vbi_nonnull (1, 2, 3)));
extern vbi_bool
vbi_page_table_next_page	(const vbi_page_table *pt,
				 vbi_pgno *		pgno)
  __attribute__ ((_vbi_nonnull (1, 2)));
extern unsigned int
vbi_page_table_num_pages	(const vbi_page_table *pt)
  __attribute__ ((_vbi_nonnull (1)));
extern vbi_bool
vbi_page_table_remove_subpages	(vbi_page_table *	pt,
				 vbi_pgno		pgno,
				 vbi_subno		first_subno,
				 vbi_subno		last_subno)
  __attribute__ ((_vbi_nonnull (1)));
static __inline__ vbi_bool
vbi_page_table_remove_subpage	(vbi_page_table *	pt,
				 vbi_pgno		pgno,
				 vbi_subno		subno)
{
	return vbi_page_table_remove_subpages (pt, pgno, subno, subno);
}
extern vbi_bool
vbi_page_table_add_subpages	(vbi_page_table *	pt,
				 vbi_pgno		pgno,
				 vbi_subno		first_subno,
				 vbi_subno		last_subno)
  __attribute__ ((_vbi_nonnull (1)));
static __inline__ vbi_bool
vbi_page_table_add_subpage	(vbi_page_table *	pt,
				 vbi_pgno		pgno,
				 vbi_subno		subno)
{
	return vbi_page_table_add_subpages (pt, pgno, subno, subno);
}
extern vbi_bool
vbi_page_table_remove_pages	(vbi_page_table *	pt,
				 vbi_pgno		first_pgno,
				 vbi_pgno		last_pgno)
  __attribute__ ((_vbi_nonnull (1)));
static __inline__ vbi_bool
vbi_page_table_remove_page	(vbi_page_table *	pt,
				 vbi_pgno		pgno)
{
	return vbi_page_table_remove_pages (pt, pgno, pgno);
}
extern vbi_bool
vbi_page_table_add_pages	(vbi_page_table *	pt,
				 vbi_pgno		first_pgno,
				 vbi_pgno		last_pgno)
  __attribute__ ((_vbi_nonnull (1)));
static __inline__ vbi_bool
vbi_page_table_add_page		(vbi_page_table *	pt,
				 vbi_pgno		pgno)
{
	return vbi_page_table_add_pages (pt, pgno, pgno);
}
extern void
vbi_page_table_remove_all_pages	(vbi_page_table *	pt)
  __attribute__ ((_vbi_nonnull (1)));
extern void
vbi_page_table_add_all_displayable_pages
				(vbi_page_table *	pt)
  __attribute__ ((_vbi_nonnull (1)));
extern void
vbi_page_table_add_all_pages	(vbi_page_table *	pt)
  __attribute__ ((_vbi_nonnull (1)));
extern void
vbi_page_table_delete		(vbi_page_table *	pt);
extern vbi_page_table *
vbi_page_table_new		(void);

VBI_END_DECLS

#endif /* __ZVBI_PAGE_TABLE_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
