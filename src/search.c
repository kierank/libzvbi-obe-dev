/*
 *  libzvbi - Teletext page cache search functions
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
 *  Copyright (C) 2000, 2001 Iñaki G. Etxebarria
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

/* $Id: search.c,v 1.4 2002/03/09 12:09:58 mschimek Exp $ */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lang.h"
#include "cache.h"
#include "search.h"
#include "ure.h"
#include "vbi.h"

#ifdef HAVE_LIBUNICODE

struct vbi_search {
	vbi_decoder *		vbi;

	int			start_pgno;
	int			start_subno;
	int			stop_pgno[2];
	int			stop_subno[2];
	int			row[2], col[2];

	int			dir;

	vbi_bool		(* progress)(vbi_page *pg);

	vbi_page		pg;

	ure_buffer_t		ub;
	ure_dfa_t		ud;
	ucs2_t			haystack[25 * (40 + 1) + 1];
};

#define SEPARATOR 0x000A

#define FIRST_ROW 1
#define LAST_ROW 24

static void
highlight(struct vbi_search *s, vt_page *vtp,
	  ucs2_t *first, long ms, long me)
{
	vbi_page *pg = &s->pg;
	ucs2_t *hp;
	int i, j;

	hp = s->haystack;

	s->start_pgno = vtp->pgno;
	s->start_subno = vtp->subno;
	s->row[0] = LAST_ROW + 1;
	s->col[0] = 0;

	for (i = FIRST_ROW; i < LAST_ROW; i++) {
		vbi_char *acp = &pg->text[i * pg->columns];

		for (j = 0; j < 40; acp++, j++) {
			int offset = hp - first;
 
			if (offset >= me) {
				s->row[0] = i;
				s->col[0] = j;
				return;
			}

			if (offset < ms) {
				if (j == 39) {
					s->row[1] = i + 1;
					s->col[1] = 0;
				} else {
					s->row[1] = i;
					s->col[1] = j + 1;
				}
			}

			switch (acp->size) {
			case VBI_DOUBLE_SIZE:
				if (offset >= ms) {
					acp[pg->columns].foreground = 32 + VBI_BLACK;
					acp[pg->columns].background = 32 + VBI_YELLOW;
					acp[pg->columns + 1].foreground = 32 + VBI_BLACK;
					acp[pg->columns + 1].background = 32 + VBI_YELLOW;
				}

				/* fall through */

			case VBI_DOUBLE_WIDTH:
				if (offset >= ms) {
					acp[0].foreground = 32 + VBI_BLACK;
					acp[0].background = 32 + VBI_YELLOW;
					acp[1].foreground = 32 + VBI_BLACK;
					acp[1].background = 32 + VBI_YELLOW;
				}

				hp++;
				acp++;
				j++;

				break;

			case VBI_DOUBLE_HEIGHT:
				if (offset >= ms) {
					acp[pg->columns].foreground = 32 + VBI_BLACK;
					acp[pg->columns].background = 32 + VBI_YELLOW;
				}

				/* fall through */

			case VBI_NORMAL_SIZE:
				if (offset >= ms) {
					acp[0].foreground = 32 + VBI_BLACK;
					acp[0].background = 32 + VBI_YELLOW;
				}

				hp++;
				break;

			default:
				/* skipped */
				/* hp++; */
				break;
			}
		}

		hp++;
	}
}

static int
search_page_fwd(void *p, vt_page *vtp, vbi_bool wrapped)
{
	vbi_search *s = p;
	vbi_char *acp;
	int row, this, start, stop;
	ucs2_t *hp, *first;
	unsigned long ms, me;
	int flags, i, j;

	this  = (vtp->pgno << 16) + vtp->subno;
	start = (s->start_pgno << 16) + s->start_subno;
	stop  = (s->stop_pgno[0] << 16) + s->stop_subno[0];

	if (start >= stop) {
		if (wrapped && this >= stop)
			return -1; /* all done, abort */
	} else if (this < start || this >= stop)
		return -1; /* all done, abort */

	if (vtp->function != PAGE_FUNCTION_LOP)
		return 0; /* try next */

	if (!vbi_format_vt_page(s->vbi, &s->pg, vtp, s->vbi->vt.max_level, 25, 1))
		return -3; /* formatting error, abort */

	if (s->progress)
		if (!s->progress(&s->pg)) {
			if (this != start) {
				s->start_pgno = vtp->pgno;
				s->start_subno = vtp->subno;
				s->row[0] = FIRST_ROW;
				s->row[1] = LAST_ROW + 1;
				s->col[0] = s->col[1] = 0;
			}

			return -2; /* canceled */
		}

	/* To Unicode */

	hp = s->haystack;
	first = hp;
	row = (this == start) ? s->row[0] : -1;
	flags = 0;

	if (row > LAST_ROW)
		return 0; /* try next page */

	for (i = FIRST_ROW; i < LAST_ROW; i++) {
		acp = &s->pg.text[i * s->pg.columns];

		for (j = 0; j < 40; acp++, j++) {
			if (i == row && j <= s->col[0])
				first = hp;

			if (acp->size == VBI_DOUBLE_WIDTH
			    || acp->size == VBI_DOUBLE_SIZE) {
				/* "ZZAAPPZILLA" -> "ZAPZILLA" */
				acp++; /* skip left half */
				j++;
			} else if (acp->size > VBI_DOUBLE_SIZE) {
				/* skip */
				/* *hp++ = 0x0020; */
				continue;
			}

			*hp++ = acp->unicode;
			flags = URE_NOTBOL;
		}

		*hp++ = SEPARATOR;
		flags = 0;
	}

	/* Search */

	if (first >= hp)
		return 0; /* try next page */
/*
#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))
fprintf(stderr, "exec: %x/%x; start %d,%d; %c%c%c...\n",
	vtp->pgno, vtp->subno,
	s->row[0], s->col[0],
	printable(first[0]),
	printable(first[1]),
	printable(first[2])
);
*/
	if (!ure_exec(s->ud, flags, first, hp - first, &ms, &me))
		return 0; /* try next page */

	highlight(s, vtp, first, ms, me);

	return 1; /* success, abort */
}

static int
search_page_rev(void *p, vt_page *vtp, vbi_bool wrapped)
{
	vbi_search *s = p;
	vbi_char *acp;
	int row, this, start, stop;
	unsigned long ms, me;
	ucs2_t *hp;
	int flags, i, j;

	this  = (vtp->pgno << 16) + vtp->subno;
	start = (s->start_pgno << 16) + s->start_subno;
	stop  = (s->stop_pgno[1] << 16) + s->stop_subno[1];

	if (start <= stop) {
		if (wrapped && this <= stop)
			return -1; /* all done, abort */
	} else if (this > start || this <= stop)
		return -1; /* all done, abort */

	if (vtp->function != PAGE_FUNCTION_LOP)
		return 0; /* try next page */

	if (!vbi_format_vt_page(s->vbi, &s->pg, vtp, s->vbi->vt.max_level, 25, 1))
		return -3; /* formatting error, abort */

	if (s->progress)
		if (!s->progress(&s->pg)) {
			if (this != start) {
				s->start_pgno = vtp->pgno;
				s->start_subno = vtp->subno;
				s->row[0] = FIRST_ROW;
				s->row[1] = LAST_ROW + 1;
				s->col[0] = s->col[1] = 0;
			}

			return -2; /* canceled */
		}

	/* To Unicode */

	hp = s->haystack;
	row = (this == start) ? s->row[1] : 100;
	flags = 0;

	if (row < FIRST_ROW)
		goto break2;

	for (i = FIRST_ROW; i < LAST_ROW; i++) {
		acp = &s->pg.text[i * s->pg.columns];

		for (j = 0; j < 40; acp++, j++) {
			if (i == row && j >= s->col[1])
				goto break2;

			if (acp->size == VBI_DOUBLE_WIDTH
			    || acp->size == VBI_DOUBLE_SIZE) {
				/* "ZZAAPPZILLA" -> "ZAPZILLA" */
				acp++; /* skip left half */
				j++;
			} else if (acp->size > VBI_DOUBLE_SIZE) {
				/* skip */
				/* *hp++ = 0x0020; */
				continue;
			}

			*hp++ = acp->unicode;
			flags = URE_NOTEOL;
		}

		*hp++ = SEPARATOR;
		flags = 0;
	}
break2:

	if (hp <= s->haystack)
		return 0; /* try next page */

	/* Search */

	ms = me = 0;

	for (i = 0; s->haystack + me < hp; i++) {
		unsigned long ms1, me1;
/*
#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))
fprintf(stderr, "exec: %x/%x; %d, %d; '%c%c%c...'\n",
	vtp->pgno, vtp->subno, i, me,
	printable(s->haystack[me + 0]),
	printable(s->haystack[me + 1]),
	printable(s->haystack[me + 2])
);
*/
		if (!ure_exec(s->ud, (me > 0) ? (flags | URE_NOTBOL) : flags,
		    s->haystack + me, hp - s->haystack - me, &ms1, &me1))
			break;

		ms = me + ms1;
		me = me + me1;
	}

	if (i == 0)
		return 0; /* try next page */

	highlight(s, vtp, s->haystack, ms, me);

	return 1; /* success, abort */
}

/**
 * vbi_search_delete:
 * @search: #vbi_search context.
 * 
 * Delete the search context created by vbi_search_new().
 **/
void
vbi_search_delete(vbi_search *search)
{
	if (!search)
		return;

	if (search->ud)
		ure_dfa_free(search->ud);

	if (search->ub)
		ure_buffer_free(search->ub);

	free(search);
}

static size_t
ucs2_strlen(const void *string)
{
	ucs2_t *p = (ucs2_t *) string;
	size_t i = 0;

	if (!string)
		return 0;

	for (i = 0; *p; i++)
		p++;

	return i;
}

/**
 * vbi_search_new:
 * @vbi: Initialized vbi decoding context.
 * @pgno: 
 * @subno: Page and subpage number of the first (forward) or
 *   last (backward) page to visit. Optional #VBI_ANY_SUBNO. 
 * @pattern: The Unicode (UCS-2, <emphasis>not</> UTF-16) search
 *   pattern, a 0-terminated string.
 * @casefold: Boolean, search case insensitive.
 * @regexp: Boolean, the search pattern is a regular expression.
 * @progress: A function called for each page scanned, can be
 *   %NULL. Shall return %FALSE to abort the search. @pg is valid
 *   for display (e. g. pg->pgno), do <emphasis>not</> call
 *   vbi_unref_page() or modify this page.
 * 
 * Allocate a #vbi_search context and prepare for searching
 * the Teletext page cache. The context must be freed with
 * vbi_search_delete().
 * 
 * Regular expression searching supports the standard set
 * of operators and constants, with these extensions:
 * <informaltable frame=none><tgroup cols=2><tbody>
 * <row><entry>\x....</><entry>hexadecimal number of up to 4 digits</></row>
 * <row><entry>\X....</><entry>hexadecimal number of up to 4 digits</></row>
 * <row><entry>\u....</><entry>hexadecimal number of up to 4 digits</></row>
 * <row><entry>\U....</><entry>hexadecimal number of up to 4 digits</></row>
 * <row><entry>:title:</><entry>Unicode specific character class</></row>
 * <row><entry>:gfx:</><entry>Teletext G1 or G3 graphics</></row>
 * <row><entry>:drcs:</><entry>Teletext DRCS</></row>
 * <row><entry>\pN1,N2,...,Nn</><entry>Character properties class</></row>
 * <row><entry>\PN1,N2,...,Nn</><entry>Negated character properties class</></row>
 * </tbody></tgroup></informaltable>
 * <informaltable frame=none><tgroup cols=2><thead>
 * <row><entry>N</><entry>Property</></row></thead><tbody>
 * <row><entry>1</><entry>alphanumeric</></row>
 * <row><entry>2</><entry>alpha</></row>
 * <row><entry>3</><entry>control</></row>
 * <row><entry>4</><entry>digit</></row>
 * <row><entry>5</><entry>graphical</></row>
 * <row><entry>6</><entry>lowercase</></row>
 * <row><entry>7</><entry>printable</></row>
 * <row><entry>8</><entry>punctuation</></row>
 * <row><entry>9</><entry>space</></row>
 * <row><entry>10</><entry>uppercase</></row>
 * <row><entry>11</><entry>hex digit</></row>
 * <row><entry>12</><entry>title</></row>
 * <row><entry>13</><entry>defined</></row>
 * <row><entry>14</><entry>wide</></row>
 * <row><entry>15</><entry>nonspacing</></row>
 * <row><entry>16</><entry>Teletext G1 or G3 graphics</></row>
 * <row><entry>17</><entry>Teletext DRCS</></row>
 * </tbody></tgroup></informaltable>
 * Character classes can contain literals, constants, and character
 * property classes. Example: [abc\U10A\p1,3,4]. Note double height
 * and size characters will match twice, on the upper and lower row,
 * and double width and size characters count as one (reducing the
 * line width) so one can find combinations of normal and enlarged
 * characters.
 *
 * Return value:
 * A #vbi_search context pointer or %NULL if some problem occured. 
 **/
vbi_search *
vbi_search_new(vbi_decoder *vbi,
	       vbi_pgno pgno, vbi_subno subno,
	       uint16_t *pattern,
	       vbi_bool casefold, vbi_bool regexp,
	       int (* progress)(vbi_page *pg))
{
	vbi_search *s;
	ucs2_t *esc_pat = NULL;
	int i, j, pat_len = ucs2_strlen(pattern);

	if (pat_len <= 0)
		return NULL;

	if (!(s = calloc(1, sizeof(*s))))
		return NULL;

	if (!regexp) {
		if (!(esc_pat = malloc(sizeof(ucs2_t) * pat_len * 2))) {
			free(s);
			return NULL;
		}

		for (i = j = 0; i < pat_len; i++) {
			if (strchr("!\"#$%&()*+,-./:;=?@[\\]^_{|}~", pattern[i]))
				esc_pat[j++] = '\\';
			esc_pat[j++] = pattern[i];
		}

		pattern = esc_pat;
		pat_len = j;
	}

	if (!(s->ub = ure_buffer_create()))
		goto abort;

	if (!(s->ud = ure_compile(pattern, pat_len, casefold, s->ub))) {
abort:
		vbi_search_delete(s);

		if (!regexp)
			free(esc_pat);

		return NULL;
	}

	if (!regexp)
		free(esc_pat);

	s->stop_pgno[0] = pgno;
	s->stop_subno[0] = (subno == VBI_ANY_SUBNO) ? 0 : subno;

	if (subno <= 0) {
		s->stop_pgno[1] = (pgno <= 0x100) ? 0x8FF : pgno - 1;
		s->stop_subno[1] = 0x3F7E;
	} else {
		s->stop_pgno[1] = pgno;

		if ((subno & 0x7F) == 0)
			s->stop_subno[1] = (subno - 0x100) | 0x7E;
		else
			s->stop_subno[1] = subno - 1;
	}

	s->vbi = vbi;
	s->progress = progress;

	return s;
}

/**
 * vbi_search_next:
 * @search: Initialized search context.
 * @pg: Place to store the formatted (as with vbi_fetch_vt_page())
 *   Teletext page containing the found pattern. Do <emphasis>not</>
 *   call vbi_unref_page() for this page, libzvbi take care. Also
 *   the page must not be modified.
 * @dir: Search direction +1 forward or -1 backward.
 *
 * Find the next occurence of the search pattern.
 *
 * Return value:
 * #vbi_search_status.
 * <informaltable frame=none><tgroup cols=2><thead>
 * <row><entry>Value</><entry>Meaning</></row></thead><tbody>
 * <row><entry></><entry></></row>
 * <row><entry>@VBI_SEARCH_SUCCESS</><entry>Pattern found.
 *   *@pg points to the page ready for display with the pattern
 *   highlighted, pg->pgno etc.</></row>
 * <row><entry>@VBI_SEARCH_NOT_FOUND</><entry>Pattern not found,
 *   *@pg is invalid. Another vbi_next_search() will restart
 *   from the original starting point.</></row>
 * <row><entry>@VBI_SEARCH_CANCELED</><entry>The search has been
 *   canceled by the progress function. *@pg points to the current
 *   page as in success case, except for the highlighting. Another
 *   vbi_next_search() continues from this page.</></row>
 * <row><entry>@VBI_SEARCH_CACHE_EMPTY</><entry>No pages in the
 *   cache, *@pg is invalid.</></row>
 * <row><entry>@VBI_SEARCH_ERROR</><entry>Some error occured,
 *   condition unclear. Call vbi_search_delete().</></row>
 * </tbody></tgroup></informaltable>
 **/
int
vbi_search_next(vbi_search *search, vbi_page **pg, int dir)
{
	*pg = NULL;
	dir = (dir > 0) ? +1 : -1;

	if (!search->dir) {
		search->dir = dir;

		if (dir > 0) {
			search->start_pgno = search->stop_pgno[0];
			search->start_subno = search->stop_subno[0];
		} else {
			search->start_pgno = search->stop_pgno[1];
			search->start_subno = search->stop_subno[1];
		}

		search->row[0] = FIRST_ROW;
		search->row[1] = LAST_ROW + 1;
		search->col[0] = search->col[1] = 0;
	}
#if 1 /* should switch to a 'two frontiers meet' model, but ok for now */
	else if (dir != search->dir) {
		search->dir = dir;

		search->stop_pgno[0] = search->start_pgno;
		search->stop_subno[0] = (search->start_subno == VBI_ANY_SUBNO) ?
			0 : search->start_subno;
		search->stop_pgno[1] = search->start_pgno;
		search->stop_subno[1] = search->start_subno;
	}
#endif
	switch (vbi_cache_foreach(search->vbi, search->start_pgno, search->start_subno, dir,
		 (dir > 0) ? search_page_fwd : search_page_rev, search)) {
	case 1:
		*pg = &search->pg;
		return VBI_SEARCH_SUCCESS;

	case 0:
		return VBI_SEARCH_CACHE_EMPTY;

	case -1:
		search->dir = 0;
		return VBI_SEARCH_NOT_FOUND;

	case -2:
		return VBI_SEARCH_CANCELED;

	default:
		break;
	}

	return VBI_SEARCH_ERROR;
}

#else /* !HAVE_LIBUNICODE */

vbi_search *
vbi_search_new(vbi_decoder *vbi,
	       vbi_pgno pgno, vbi_subno subno,
	       uint16_t *pattern,
	       vbi_bool casefold, vbi_bool regexp,
	       int (* progress)(vbi_page *pg))
{
	return NULL;
}

#endif /* !HAVE_LIBUNICODE */
