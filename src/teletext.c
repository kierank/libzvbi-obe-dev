/*
 *  libzvbi - Teletext formatter
 *
 *  Copyright (C) 2000, 2001 Michael H. Schimek
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

/* $Id: teletext.c,v 1.1 2002/01/12 16:19:02 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "site_def.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "bcd.h"
#include "vt.h"
#include "export.h"
#include "vbi.h"
#include "hamm.h"
#include "lang.h"

#ifndef _
#ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) gettext (String)
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#endif
#endif

#define DEBUG 0

#if DEBUG
#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? \
                      '.' : ((c) & 0x7F))
#define printv(templ, args...) fprintf(stderr, templ ,##args)
#else
#define printv(templ, args...)
#endif

#define ROWS			25
#define COLUMNS			40
#define EXT_COLUMNS		41
#define LAST_ROW		((ROWS - 1) * EXT_COLUMNS)

/*
 *  FLOF navigation
 */

static const vbi_color
flof_link_col[4] = { VBI_RED, VBI_GREEN, VBI_YELLOW, VBI_CYAN };

static inline void
flof_navigation_bar(vbi_page *pg, vt_page *vtp)
{
	vbi_char ac;
	int n, i, k, ii;

	memset(&ac, 0, sizeof(ac));

	ac.foreground	= VBI_WHITE;
	ac.background	= VBI_BLACK;
	ac.opacity	= pg->page_opacity[1];
	ac.unicode	= 0x0020;

	for (i = 0; i < EXT_COLUMNS; i++)
		pg->text[LAST_ROW + i] = ac;

	ac.link = TRUE;

	for (i = 0; i < 4; i++) {
		ii = i * 10 + 3;

		for (k = 0; k < 3; k++) {
			n = ((vtp->data.lop.link[i].pgno >> ((2 - k) * 4)) & 15) + '0';

			if (n > '9')
				n += 'A' - '9';

			ac.unicode = n;
			ac.foreground = flof_link_col[i];
			pg->text[LAST_ROW + ii + k] = ac;
			pg->nav_index[ii + k] = i;
		}

		pg->nav_link[i].pgno = vtp->data.lop.link[i].pgno;
		pg->nav_link[i].subno = vtp->data.lop.link[i].subno;
	}
}

static inline void
flof_links(vbi_page *pg, vt_page *vtp)
{
	vbi_char *acp = pg->text + LAST_ROW;
	int i, j, k, col = -1, start = 0;

	for (i = 0; i < COLUMNS + 1; i++) {
		if (i == COLUMNS || (acp[i].foreground & 7) != col) {
			for (k = 0; k < 4; k++)
				if (flof_link_col[k] == col)
					break;

			if (k < 4 && !NO_PAGE(vtp->data.lop.link[k].pgno)) {
				/* Leading and trailing spaces not sensitive */

				for (j = i - 1; j >= start && acp[j].unicode == 0x0020; j--);

				for (; j >= start; j--) {
					acp[j].link = TRUE;
					pg->nav_index[j] = k;
				}

		    		pg->nav_link[k].pgno = vtp->data.lop.link[k].pgno;
		    		pg->nav_link[k].subno = vtp->data.lop.link[k].subno;
			}

			if (i >= COLUMNS)
				break;

			col = acp[i].foreground & 7;
			start = i;
		}

		if (start == i && acp[i].unicode == 0x0020)
			start++;
	}
}

/*
 *  TOP navigation
 */

static void character_set_designation(struct vbi_font_descr **font,
				      extension *ext, vt_page *vtp);
static void screen_color(vbi_page *pg, int flags, int color);

static vbi_bool
top_label(vbi_decoder *vbi, vbi_page *pg, struct vbi_font_descr *font,
	  int index, int pgno, int foreground, int ff)
{
	int column = index * 13 + 1;
	vt_page *vtp;
	vbi_char *acp;
	ait_entry *ait;
	int i, j;

	acp = &pg->text[LAST_ROW + column];

	for (i = 0; i < 8; i++)
		if (vbi->vt.btt_link[i].type == 2) {
			vtp = vbi_cache_get(vbi,
					    vbi->vt.btt_link[i].pgno,
					    vbi->vt.btt_link[i].subno, 0x3f7f);
			if (!vtp) {
				printv("top ait page %x not cached\n", vbi->vt.btt_link[i].pgno);
				continue;
			} else if (vtp->function != PAGE_FUNCTION_AIT) {
				printv("no ait page %x\n", vtp->pgno);
				continue;
			}

			for (ait = vtp->data.ait, j = 0; j < 46; ait++, j++)
				if (ait->page.pgno == pgno) {
					pg->nav_link[index].pgno = pgno;
					pg->nav_link[index].subno = VBI_ANY_SUBNO;

					for (i = 11; i >= 0; i--)
						if (ait->text[i] > 0x20)
							break;

					if (ff && (i <= (11 - ff))) {
						acp += (11 - ff - i) >> 1;
						column += (11 - ff - i) >> 1;

						acp[i + 1].link = TRUE;
						pg->nav_index[column + i + 1] = index;

						acp[i + 2].unicode = 0x003E;
						acp[i + 2].foreground = foreground;
						acp[i + 2].link = TRUE;
						pg->nav_index[column + i + 2] = index;

						if (ff > 1) {
							acp[i + 3].unicode = 0x003E;
							acp[i + 3].foreground = foreground;
							acp[i + 3].link = TRUE;
							pg->nav_index[column + i + 3] = index;
						}
					} else {
						acp += (11 - i) >> 1;
						column += (11 - i) >> 1;
					}

					for (; i >= 0; i--) {
						acp[i].unicode = vbi_teletext_unicode(font->G0, font->subset,
							(ait->text[i] < 0x20) ? 0x20 : ait->text[i]);
						acp[i].foreground = foreground;
						acp[i].link = TRUE;
						pg->nav_index[column + i] = index;
					}

					return TRUE;
				}
		}

	return FALSE;
}

static inline void
top_navigation_bar(vbi_decoder *vbi, vbi_page *pg,
		   vt_page *vtp)
{
	vbi_char ac;
	int i, got;

	printv("PAGE MIP/BTT: %d\n", vbi->vt.page_info[vtp->pgno - 0x100].code);

	memset(&ac, 0, sizeof(ac));

	ac.foreground	= 32 + VBI_WHITE;
	ac.background	= 32 + VBI_BLACK;
	ac.opacity	= pg->page_opacity[1];
	ac.unicode	= 0x0020;

	for (i = 0; i < EXT_COLUMNS; i++)
		pg->text[LAST_ROW + i] = ac;

	if (pg->page_opacity[1] != VBI_OPAQUE)
		return;

	for (i = vtp->pgno; i != vtp->pgno + 1; i = (i == 0) ? 0x89a : i - 1)
		if (vbi->vt.page_info[i - 0x100].code == VBI_TOP_BLOCK ||
		    vbi->vt.page_info[i - 0x100].code == VBI_TOP_GROUP) {
			top_label(vbi, pg, pg->font[0], 0, i, 32 + VBI_WHITE, 0);
			break;
		}

	for (i = vtp->pgno + 1, got = FALSE; i != vtp->pgno; i = (i == 0x899) ? 0x100 : i + 1)
		switch (vbi->vt.page_info[i - 0x100].code) {
		case VBI_TOP_BLOCK:
			top_label(vbi, pg, pg->font[0], 2, i, 32 + VBI_YELLOW, 2);
			return;

		case VBI_TOP_GROUP:
			if (!got) {
				top_label(vbi, pg, pg->font[0], 1, i, 32 + VBI_GREEN, 1);
				got = TRUE;
			}

			break;
		}
}

static ait_entry *
next_ait(vbi_decoder *vbi, int pgno, int subno, vt_page **mvtp)
{
	vt_page *vtp;
	ait_entry *ait, *mait = NULL;
	int mpgno = 0xFFF, msubno = 0xFFFF;
	int i, j;

	*mvtp = NULL;

	for (i = 0; i < 8; i++) {
		if (vbi->vt.btt_link[i].type == 2) {
			vtp = vbi_cache_get(vbi,
					    vbi->vt.btt_link[i].pgno, 
					    vbi->vt.btt_link[i].subno, 0x3f7f);

			if (!vtp) {
				printv("top ait page %x not cached\n", vbi->vt.btt_link[i].pgno);
				continue;
			} else if (vtp->function != PAGE_FUNCTION_AIT) {
				printv("no ait page %x\n", vtp->pgno);
				continue;
			}

			for (ait = vtp->data.ait, j = 0; j < 46; ait++, j++) {
				if (!ait->page.pgno)
					break;

				if (ait->page.pgno < pgno
				    || (ait->page.pgno == pgno && ait->page.subno <= subno))
					continue;

				if (ait->page.pgno > mpgno
				    || (ait->page.pgno == mpgno && ait->page.subno > msubno))
					continue;

				mait = ait;
				mpgno = ait->page.pgno;
				msubno = ait->page.subno;
				*mvtp = vtp;
			}
		}
	}

	return mait;
}

static int
top_index(vbi_decoder *vbi, vbi_page *pg, int subno)
{
	vt_page *vtp;
	vbi_char ac, *acp;
	ait_entry *ait;
	int i, j, k, n, lines;
	int xpgno, xsubno;
	extension *ext;
	char *index_str;

	pg->vbi = vbi;

	subno = vbi_bcd2dec(subno);

	pg->rows = ROWS;
	pg->columns = EXT_COLUMNS;

	pg->dirty.y0 = 0;
	pg->dirty.y1 = ROWS - 1;
	pg->dirty.roll = 0;

	ext = &vbi->vt.magazine[0].extension;

	screen_color(pg, 0, 32 + VBI_BLUE);

	vbi_transp_colormap(vbi, pg->color_map, ext->color_map, 40);

	pg->drcs_clut = ext->drcs_clut;

	pg->page_opacity[0] = VBI_OPAQUE;
	pg->page_opacity[1] = VBI_OPAQUE;
	pg->boxed_opacity[0] = VBI_OPAQUE;
	pg->boxed_opacity[1] = VBI_OPAQUE;

	memset(pg->drcs, 0, sizeof(pg->drcs));

	memset(&ac, 0, sizeof(ac));

	ac.foreground	= VBI_BLACK; // 32 + VBI_BLACK;
	ac.background	= 32 + VBI_BLUE;
	ac.opacity	= VBI_OPAQUE;
	ac.unicode	= 0x0020;
	ac.size		= VBI_NORMAL_SIZE;

	for (i = 0; i < EXT_COLUMNS * ROWS; i++)
		pg->text[i] = ac;

	ac.size = VBI_DOUBLE_SIZE;

	/* NLS: Title of TOP Index page, Latin-1 only */
	index_str = _("TOP Index");

	for (i = 0; index_str[i]; i++) {
		ac.unicode = index_str[i];
		pg->text[1 * EXT_COLUMNS + 2 + i * 2] = ac;
	}

	ac.size = VBI_NORMAL_SIZE;

	acp = &pg->text[4 * EXT_COLUMNS];
	lines = 17;
	xpgno = 0;
	xsubno = 0;

	while ((ait = next_ait(vbi, xpgno, xsubno, &vtp))) {
		xpgno = ait->page.pgno;
		xsubno = ait->page.subno;

		/* No docs, correct? */
		character_set_designation(pg->font, ext, vtp);

		if (subno > 0) {
			if (lines-- == 0) {
				subno--;
				lines = 17;
			}

			continue;
		} else if (lines-- <= 0)
			continue;

		for (i = 11; i >= 0; i--)
			if (ait->text[i] > 0x20)
				break;

		switch (vbi->vt.page_info[ait->page.pgno - 0x100].code) {
		case VBI_TOP_GROUP:
			k = 3;
			break;

		default:
    			k = 1;
		}

		for (j = 0; j <= i; j++) {
			acp[k + j].unicode = vbi_teletext_unicode(pg->font[0]->G0,
				pg->font[0]->subset, (ait->text[j] < 0x20) ? 0x20 : ait->text[j]);
		}

		for (k += i + 2; k <= 33; k++)
			acp[k].unicode = '.';

		for (j = 0; j < 3; j++) {
			n = ((ait->page.pgno >> ((2 - j) * 4)) & 15) + '0';

			if (n > '9')
				n += 'A' - '9';

			acp[j + 35].unicode = n;
 		}

		acp += EXT_COLUMNS;
	}

	return 1;
}

/*
 *  Zapzilla navigation
 */

static int
keyword(vbi_link *ld, uint8_t *p, int column,
	int pgno, int subno, int *back)
{
	uint8_t *s = p + column;
	int i, j, k, l;

	ld->type = VBI_LINK_NONE;
	ld->name[0] = 0;
	ld->url[0] = 0;
	ld->pgno = 0;
	ld->subno = VBI_ANY_SUBNO;
	*back = 0;

	if (isdigit(*s)) {
		for (i = 0; isdigit(s[i]); i++)
			ld->pgno = ld->pgno * 16 + (s[i] & 15);

		if (isdigit(s[-1]) || i > 3)
			return i;

		if (i == 3) {
			if (ld->pgno >= 0x100 && ld->pgno <= 0x899)
				ld->type = VBI_LINK_PAGE;

			return i;
		}

		if (s[i] != '/' && s[i] != ':')
			return i;

		s += i += 1;

		for (ld->subno = j = 0; isdigit(s[j]); j++)
			ld->subno = ld->subno * 16 + (s[j] & 15);

		if (j > 1 || subno != ld->pgno || ld->subno > 0x99)
			return i + j;

		if (ld->pgno == ld->subno)
			ld->subno = 0x01;
		else
			ld->subno = vbi_add_bcd(ld->pgno, 0x01);

		ld->type = VBI_LINK_SUBPAGE;
		ld->pgno = pgno;

		return i + j;
	} else if (!strncasecmp(s, "https://", i = 8)) {
		ld->type = VBI_LINK_HTTP;
	} else if (!strncasecmp(s, "http://", i = 7)) {
		ld->type = VBI_LINK_HTTP;
	} else if (!strncasecmp(s, "www.", i = 4)) {
		ld->type = VBI_LINK_HTTP;
		strcpy(ld->url, "http://");
	} else if (!strncasecmp(s, "ftp://", i = 6)) {
		ld->type = VBI_LINK_FTP;
	} else if (*s == '@' || *s == 0xA7) {
		ld->type = VBI_LINK_EMAIL;
		strcpy(ld->url, "mailto:");
		i = 1;
	} else if (!strncasecmp(s, "(at)", i = 4)) {
		ld->type = VBI_LINK_EMAIL;
		strcpy(ld->url, "mailto:");
	} else if (!strncasecmp(s, "(a)", i = 3)) {
		ld->type = VBI_LINK_EMAIL;
		strcpy(ld->url, "mailto:");
	} else
		return 1;

	for (j = k = l = 0;;) {
		// RFC 1738
		while (isalnum(s[i + j]) || strchr("%&/=?+-~:;@", s[i + j])) {
			j++;
			l++;
		}

		if (s[i + j] == '.') {
			if (l < 1)
				return i;		
			l = 0;
			j++;
			k++;
		} else
			break;
	}

	if (k < 1 || l < 1) {
		ld->type = VBI_LINK_NONE;
		return i;
	}

	k = 0;

	if (ld->type == VBI_LINK_EMAIL) {
		for (; isalnum(s[k - 1]) || strchr("-~._", s[k - 1]); k--);

		if (k == 0) {
			ld->type = VBI_LINK_NONE;
			return i;
		}

		*back = k;

		strncat(ld->url, s + k, -k);
		strcat(ld->url, "@");
		strncat(ld->url, s + i, j);
	} else
		strncat(ld->url, s + k, i + j - k);

	return i + j;
}

static inline void
zap_links(vbi_page *pg, int row)
{
	unsigned char buffer[43]; /* One row, two spaces on the sides and NUL */
	vbi_link ld;
	vbi_char *acp;
	vbi_bool link[43];
	int i, j, n, b;

	acp = &pg->text[row * EXT_COLUMNS];

	for (i = j = 0; i < COLUMNS; i++) {
		if (acp[i].size == VBI_OVER_TOP || acp[i].size == VBI_OVER_BOTTOM)
			continue;
		buffer[j + 1] = (acp[i].unicode >= 0x20 && acp[i].unicode <= 0xFF) ?
			acp[i].unicode : 0x20;
		j++;
	}

	buffer[0] = ' '; 
	buffer[j + 1] = ' ';
	buffer[j + 2] = 0;

	for (i = 0; i < COLUMNS; i += n) { 
		n = keyword(&ld, buffer, i + 1,
			pg->pgno, pg->subno, &b);

		for (j = b; j < n; j++)
			link[i + j] = (ld.type != VBI_LINK_NONE);
	}

	for (i = j = 0; i < COLUMNS; i++) {
		acp[i].link = link[j];

		if (acp[i].size == VBI_OVER_TOP || acp[i].size == VBI_OVER_BOTTOM)
			continue;
		j++;
	}
}

/**
 * vbi_resolve_link:
 * @pg: With vbi_fetch_vt_page() obtained #vbi_page.
 * @column: Column 0 ... pg->columns - 1 of the character in question.
 * @row: Row 0 ... pg->rows - 1 of the character in question.
 * @ld: Place to store information about the link.
 * 
 * A #vbi_page (in practice only Teletext pages) may contain hyperlinks
 * such as HTTP URLs, e-mail addresses or links to other pages. Characters
 * being part of a hyperlink have a set #vbi_char.link flag, this function
 * returns a more verbose description of the respective link.
 **/
void
vbi_resolve_link(vbi_page *pg, int column, int row, vbi_link *ld)
{
	unsigned char buffer[43];
	vbi_char *acp;
	int i, j, b;

	assert(column >= 0 && column < EXT_COLUMNS);

	ld->nuid = pg->nuid;

	acp = &pg->text[row * EXT_COLUMNS];

	if (row == (ROWS - 1) && acp[column].link) {
		i = pg->nav_index[column];

		ld->type = VBI_LINK_PAGE;
		ld->pgno = pg->nav_link[i].pgno;
		ld->subno = pg->nav_link[i].subno;

		return;
	}

	if (row < 1 || row > 23 || column >= COLUMNS || pg->pgno < 0x100) {
		ld->type = VBI_LINK_NONE;
		return;
	}

	for (i = j = b = 0; i < COLUMNS; i++) {
		if (acp[i].size == VBI_OVER_TOP || acp[i].size == VBI_OVER_BOTTOM)
			continue;
		if (i < column && !acp[i].link)
			j = b = -1;

		buffer[j + 1] = (acp[i].unicode >= 0x20 && acp[i].unicode <= 0xFF) ?
			acp[i].unicode : 0x20;

		if (b <= 0) {
			if (buffer[j + 1] == ')' && j > 2) {
				if (!strncasecmp(buffer + j + 1 - 3, "(at", 3))
					b = j - 3;
				else if (!strncasecmp(buffer + j + 1 - 2, "(a", 2))
					b = j - 2;
			} else if (buffer[j + 1] == '@' || buffer[j + 1] == 167)
				b = j;
		}

		j++;
	}

	buffer[0] = ' ';
	buffer[j + 1] = ' ';
	buffer[j + 2] = 0;

	keyword(ld, buffer, 1, pg->pgno, pg->subno, &i);

	if (ld->type == VBI_LINK_NONE)
		keyword(ld, buffer, b + 1, pg->pgno, pg->subno, &i);
}

/**
 * vbi_resolve_home:
 * @pg: With vbi_fetch_vt_page() obtained #vbi_page.
 * @ld: Place to store information about the link.
 * 
 * All Teletext pages have a built-in home link, by default
 * page 100, but also the magazine intro page or another page
 * selected by the editor.
 **/
void
vbi_resolve_home(vbi_page *pg, vbi_link *ld)
{
	if (pg->pgno < 0x100) {
		ld->type = VBI_LINK_NONE;
		return;
	}

	ld->type = VBI_LINK_PAGE;
	ld->pgno = pg->nav_link[5].pgno;
	ld->subno = pg->nav_link[5].subno;
}

static inline void
ait_title(vbi_decoder *vbi, vt_page *vtp, ait_entry *ait, char *buf)
{
	struct vbi_font_descr *font[2];
	int i;

	character_set_designation(font, &vbi->vt.magazine[0].extension, vtp);

	for (i = 11; i >= 0; i--)
		if (ait->text[i] > 0x20)
			break;
	buf[i + 1] = 0;

	for (; i >= 0; i--) {
		unsigned int unicode = vbi_teletext_unicode(
			font[0]->G0, font[0]->subset,
			(ait->text[i] < 0x20) ?	0x20 : ait->text[i]);

		buf[i] = (unicode >= 0x20 && unicode <= 0xFF) ? unicode : 0x20;
	}
}

/**
 * vbi_page_title:
 * @vbi: Initialized vbi decoding context.
 * @pgno: Page number, see #vbi_pgno.
 * @subno: Subpage number.
 * @buf: Place to store the titel, Latin-1 format, at least
 *   41 characters including the terminating zero.
 * 
 * Given a Teletext page number this function tries to deduce a
 * page title for bookmarks or other purposes, mainly from navigation
 * data. (XXX TODO: FLOF)
 * 
 * Return value: 
 * TRUE if a title has been found.
 **/
vbi_bool
vbi_page_title(vbi_decoder *vbi, int pgno, int subno, char *buf)
{
	vt_page *vtp;
	ait_entry *ait;
	int i, j;

	if (vbi->vt.top) {
		for (i = 0; i < 8; i++)
			if (vbi->vt.btt_link[i].type == 2) {
				vtp = vbi_cache_get(vbi,
						    vbi->vt.btt_link[i].pgno, 
						    vbi->vt.btt_link[i].subno, 0x3f7f);

				if (!vtp) {
					printv("p/t top ait page %x not cached\n", vbi->vt.btt_link[i].pgno);
					continue;
				} else if (vtp->function != PAGE_FUNCTION_AIT) {
					printv("p/t no ait page %x\n", vtp->pgno);
					continue;
				}

				for (ait = vtp->data.ait, j = 0; j < 46; ait++, j++)
					if (ait->page.pgno == pgno) {
						ait_title(vbi, vtp, ait, buf);
						return TRUE;
					}
			}
	} else {
		/* find a FLOF link and the corresponding label */
	}

	return FALSE;
}

/*
 *  Teletext page formatting
 */

static void
character_set_designation(struct vbi_font_descr **font,
			  extension *ext, vt_page *vtp)
{
	int i;

#ifdef libzvbi_TTX_OVERRIDE_CHAR_SET

	font[0] = vbi_font_descriptors + libzvbi_TTX_OVERRIDE_CHAR_SET;
	font[1] = vbi_font_descriptors + libzvbi_TTX_OVERRIDE_CHAR_SET;

	fprintf(stderr, "override char set with %d\n",
		libzvbi_TTX_OVERRIDE_CHAR_SET);
#else

	font[0] = vbi_font_descriptors + 0;
	font[1] = vbi_font_descriptors + 0;

	for (i = 0; i < 2; i++) {
		int char_set = ext->char_set[i];

		if (VALID_CHARACTER_SET(char_set))
			font[i] = vbi_font_descriptors + char_set;

		char_set = (char_set & ~7) + vtp->national;

		if (VALID_CHARACTER_SET(char_set))
			font[i] = vbi_font_descriptors + char_set;
	}
#endif
}

static void
screen_color(vbi_page *pg, int flags, int color)
{ 
	pg->screen_color = color;

	if (color == VBI_TRANSPARENT_BLACK
	    || (flags & (C5_NEWSFLASH | C6_SUBTITLE)))
		pg->screen_opacity = VBI_TRANSPARENT_SPACE;
	else
		pg->screen_opacity = VBI_OPAQUE;
}

#define elements(array) (sizeof(array) / sizeof(array[0]))

static vt_triplet *
resolve_obj_address(vbi_decoder *vbi, object_type type,
	int pgno, object_address address, page_function function,
	int *remaining)
{
	int s1, packet, pointer;
	vt_page *vtp;
	vt_triplet *trip;
	int i;

	s1 = address & 15;
	packet = ((address >> 7) & 3);
	i = ((address >> 5) & 3) * 3 + type;

	printv("obj invocation, source page %03x/%04x, "
		"pointer packet %d triplet %d\n", pgno, s1, packet + 1, i);

	vtp = vbi_cache_get(vbi, pgno, s1, 0x000F);

	if (!vtp) {
		printv("... page not cached\n");
		return 0;
	}

	if (vtp->function == PAGE_FUNCTION_UNKNOWN) {
		if (!(vtp = vbi_convert_page(vbi, vtp, TRUE, function))) {
			printv("... no g/pop page or hamming error\n");
			return 0;
		}
	} else if (vtp->function == PAGE_FUNCTION_POP)
		vtp->function = function;
	else if (vtp->function != function) {
		printv("... source page wrong function %d, expected %d\n",
			vtp->function, function);
		return 0;
	}

	pointer = vtp->data.pop.pointer[packet * 24 + i * 2 + ((address >> 4) & 1)];

	printv("... triplet pointer %d\n", pointer);

	if (pointer > 506) {
		printv("... triplet pointer out of bounds (%d)\n", pointer);
		return 0;
	}

	if (DEBUG) {
		packet = (pointer / 13) + 3;

		if (packet <= 25)
			printv("... object start in packet %d, triplet %d (pointer %d)\n",
				packet, pointer % 13, pointer);
		else
			printv("... object start in packet 26/%d, triplet %d (pointer %d)\n",
				packet - 26, pointer % 13, pointer);	
	}

	trip = vtp->data.pop.triplet + pointer;
	*remaining = elements(vtp->data.pop.triplet) - (pointer+1);

	printv("... obj def: ad 0x%02x mo 0x%04x dat %d=0x%x\n",
		trip->address, trip->mode, trip->data, trip->data);

	address ^= trip->address << 7;
	address ^= trip->data;

	if (trip->mode != (type + 0x14) || (address & 0x1FF)) {
		printv("... no object definition\n");
		return 0;
	}

	return trip + 1;
}

/* FIXME: panels */

static vbi_bool
enhance(vbi_decoder *vbi, magazine *mag,	extension *ext,
	vbi_page *pg, vt_page *vtp,
	object_type type, vt_triplet *p,
	int max_triplets,
	int inv_row, int inv_column,
	vbi_wst_level max_level, vbi_bool header_only)
{
	vbi_char ac, mac, *acp;
	int active_column, active_row;
	int offset_column, offset_row;
	int row_color, next_row_color;
	struct vbi_font_descr *font;
	int invert;
	int drcs_s1[2];

	static void
	flush(int column)
	{
		int row = inv_row + active_row;
		int i;

		if (row >= ROWS)
			return;

		if (type == OBJ_TYPE_PASSIVE && !mac.unicode) {
			active_column = column;
			return;
		}

		printv("flush [%04x%c,F%d%c,B%d%c,S%d%c,O%d%c,H%d%c] %d ... %d\n",
			ac.unicode, mac.unicode ? '*' : ' ',
			ac.foreground, mac.foreground ? '*' : ' ',
			ac.background, mac.background ? '*' : ' ',
			ac.size, mac.size ? '*' : ' ',
			ac.opacity, mac.opacity ? '*' : ' ',
			ac.flash, mac.flash ? '*' : ' ',
			active_column, column - 1);

		for (i = inv_column + active_column; i < inv_column + column;) {
			vbi_char c;

			if (i > 39)
				break;

			c = acp[i];

			if (mac.underline) {
				int u = ac.underline;

				if (!mac.unicode)
					ac.unicode = c.unicode;

				if (vbi_is_gfx(ac.unicode)) {
					if (u)
						ac.unicode &= ~0x20; /* separated */
					else
						ac.unicode |= 0x20; /* contiguous */
					mac.unicode = ~0;
					u = 0;
				}

				c.underline = u;
			}
			if (mac.foreground)
				c.foreground = (ac.foreground == VBI_TRANSPARENT_BLACK) ?
					row_color : ac.foreground;
			if (mac.background)
				c.background = (ac.background == VBI_TRANSPARENT_BLACK) ?
					row_color : ac.background;
			if (invert) {
				int t = c.foreground;

				c.foreground = c.background;
				c.background = t;
			}
			if (mac.opacity)
				c.opacity = ac.opacity;
			if (mac.flash)
				c.flash = ac.flash;
			if (mac.conceal)
				c.conceal = ac.conceal;
			if (mac.unicode) {
				c.unicode = ac.unicode;
				mac.unicode = 0;

				if (mac.size)
					c.size = ac.size;
				else if (c.size > VBI_DOUBLE_SIZE)
					c.size = VBI_NORMAL_SIZE;
			}

			acp[i] = c;

			if (type == OBJ_TYPE_PASSIVE)
				break;

			i++;

			if (type != OBJ_TYPE_PASSIVE
			    && type != OBJ_TYPE_ADAPTIVE) {
				int raw;

				raw = (row == 0 && i < 9) ?
					0x20 : vbi_parity(vtp->data.lop.raw[row][i - 1]);

				/* set-after spacing attributes cancelling non-spacing */

				switch (raw) {
				case 0x00 ... 0x07:	/* alpha + foreground color */
				case 0x10 ... 0x17:	/* mosaic + foreground color */
					printv("... fg term %d %02x\n", i, raw);
					mac.foreground = 0;
					mac.conceal = 0;
					break;

				case 0x08:		/* flash */
					mac.flash = 0;
					break;

				case 0x0A:		/* end box */
				case 0x0B:		/* start box */
					if (i < COLUMNS && vbi_parity(vtp->data.lop.raw[row][i]) == raw) {
						printv("... boxed term %d %02x\n", i, raw);
						mac.opacity = 0;
					}

					break;

				case 0x0D:		/* double height */
				case 0x0E:		/* double width */
				case 0x0F:		/* double size */
					printv("... size term %d %02x\n", i, raw);
					mac.size = 0;
					break;
				}

				if (i > 39)
					break;

				raw = (row == 0 && i < 8) ?
					0x20 : vbi_parity(vtp->data.lop.raw[row][i]);

				/* set-at spacing attributes cancelling non-spacing */

				switch (raw) {
				case 0x09:		/* steady */
					mac.flash = 0;
					break;

				case 0x0C:		/* normal size */
					printv("... size term %d %02x\n", i, raw);
					mac.size = 0;
					break;

				case 0x18:		/* conceal */
					mac.conceal = 0;
					break;

					/*
					 *  Non-spacing underlined/separated display attribute
					 *  cannot be cancelled by a subsequent spacing attribute.
					 */

				case 0x1C:		/* black background */
				case 0x1D:		/* new background */
					printv("... bg term %d %02x\n", i, raw);
					mac.background = 0;
					break;
				}
			}
		}

		active_column = column;
	}

	static void
	flush_row(void)
	{
		if (type == OBJ_TYPE_PASSIVE || type == OBJ_TYPE_ADAPTIVE)
			flush(active_column + 1);
		else
			flush(COLUMNS);

		if (type != OBJ_TYPE_PASSIVE)
			memset(&mac, 0, sizeof(mac));
	}

	active_column = 0;
	active_row = 0;

	acp = &pg->text[(inv_row + 0) * EXT_COLUMNS];

	offset_column = 0;
	offset_row = 0;

	row_color =
	next_row_color = ext->def_row_color;

	drcs_s1[0] = 0; /* global */
	drcs_s1[1] = 0; /* normal */

	memset(&ac, 0, sizeof(ac));
	memset(&mac, 0, sizeof(mac));

	invert = 0;

	if (type == OBJ_TYPE_PASSIVE) {
		ac.foreground = VBI_WHITE;
		ac.background = VBI_BLACK;
		ac.opacity = pg->page_opacity[1];

		mac.foreground = ~0;
		mac.background = ~0;
		mac.opacity = ~0;
		mac.size = ~0;
		mac.underline = ~0;
		mac.conceal = ~0;
		mac.flash = ~0;
	}

	font = pg->font[0];

	for (;max_triplets>0; p++, max_triplets--) {
		if (p->address >= COLUMNS) {
			/*
			 *  Row address triplets
			 */
			int s = p->data >> 5;
			int row = (p->address - COLUMNS) ? : (ROWS - 1);
			int column = 0;


			switch (p->mode) {
			case 0x00:		/* full screen color */
				if (max_level >= VBI_WST_LEVEL_2p5
				    && s == 0 && type <= OBJ_TYPE_ACTIVE)
					screen_color(pg, vtp->flags, p->data & 0x1F);

				break;

			case 0x07:		/* address display row 0 */
				if (p->address != 0x3F)
					break; /* reserved, no position */

				row = 0;

				/* fall through */

			case 0x01:		/* full row color */
				row_color = next_row_color;

				if (s == 0) {
					row_color = p->data & 0x1F;
					next_row_color = ext->def_row_color;
				} else if (s == 3) {
					row_color =
					next_row_color = p->data & 0x1F;
				}

				goto set_active;

			case 0x02:		/* reserved */
			case 0x03:		/* reserved */
				break;

			case 0x04:		/* set active position */
				if (max_level >= VBI_WST_LEVEL_2p5) {
					if (p->data >= COLUMNS)
						break; /* reserved */

					column = p->data;
				}

				if (row > active_row)
					row_color = next_row_color;

			set_active:
				if (header_only && row > 0) {
					for (;max_triplets>1; p++, max_triplets--)
						if (p[1].address >= COLUMNS) {
							if (p[1].mode == 0x07)
								break;
							else if ((unsigned int) p[1].mode >= 0x1F)
								goto terminate;
						}
					break;
				}

				printv("enh set_active row %d col %d\n", row, column);

				if (row > active_row)
					flush_row();

				active_row = row;
				active_column = column;

				acp = &pg->text[(inv_row + active_row) * EXT_COLUMNS];

				break;

			case 0x05:		/* reserved */
			case 0x06:		/* reserved */
			case 0x08 ... 0x0F:	/* PDC data */
				break;

			case 0x10:		/* origin modifier */
				if (max_level < VBI_WST_LEVEL_2p5)
					break;

				if (p->data >= 72)
					break; /* invalid */

				offset_column = p->data;
				offset_row = p->address - COLUMNS;

				printv("enh origin modifier col %+d row %+d\n",
					offset_column, offset_row);

				break;

			case 0x11 ... 0x13:	/* object invocation */
			{
				int source = (p->address >> 3) & 3;
				object_type new_type = p->mode & 3;
				vt_triplet *trip;
				int remaining_max_triplets = 0;

				if (max_level < VBI_WST_LEVEL_2p5)
					break;

				printv("enh obj invocation source %d type %d\n", source, new_type);

				if (new_type <= type) { /* 13.2++ */
					printv("... priority violation\n");
					break;
				}

				if (source == 0) /* illegal */
					break;
				else if (source == 1) { /* local */
					int designation = (p->data >> 4) + ((p->address & 1) << 4);
					int triplet = p->data & 15;

					if (type != LOCAL_ENHANCEMENT_DATA || triplet > 12)
						break; /* invalid */

					printv("... local obj %d/%d\n", designation, triplet);

					if (!(vtp->enh_lines & 1)) {
						printv("... no packet %d\n", designation);
						return FALSE;
					}

					trip = vtp->data.enh_lop.enh + designation * 13 + triplet;
					remaining_max_triplets = elements(vtp->data.enh_lop.enh) - (designation* 13 + triplet);
				}
				else /* global / public */
				{
					page_function function;
					int pgno, i = 0;

					if (source == 3) {
						function = PAGE_FUNCTION_GPOP;
						pgno = vtp->data.lop.link[24].pgno;

						if (NO_PAGE(pgno)) {
							if (max_level < VBI_WST_LEVEL_3p5
							    || NO_PAGE(pgno = mag->pop_link[8].pgno))
								pgno = mag->pop_link[0].pgno;
						} else
							printv("... X/27/4 GPOP overrides MOT\n");
					} else {
						function = PAGE_FUNCTION_POP;
						pgno = vtp->data.lop.link[25].pgno;

						if (NO_PAGE(pgno)) {
							if ((i = mag->pop_lut[vtp->pgno & 0xFF]) == 0) {
								printv("... MOT pop_lut empty\n");
								return FALSE; /* has no link (yet) */
							}

							if (max_level < VBI_WST_LEVEL_3p5
							    || NO_PAGE(pgno = mag->pop_link[i + 8].pgno))
								pgno = mag->pop_link[i + 0].pgno;
						} else
							printv("... X/27/4 POP overrides MOT\n");
					}

					if (NO_PAGE(pgno)) {
						printv("... dead MOT link %d\n", i);
						return FALSE; /* has no link (yet) */
					}

					printv("... %s obj\n", (source == 3) ? "global" : "public");

					trip = resolve_obj_address(vbi, new_type, pgno,
						(p->address << 7) + p->data, function,
						&remaining_max_triplets);

					if (!trip)
						return FALSE;
				}

				row = inv_row + active_row;
				column = inv_column + active_column;

				if (!enhance(vbi, mag, ext, pg, vtp, new_type, trip,
					     remaining_max_triplets,
					     row + offset_row, column + offset_column,
					     max_level, header_only))
					return FALSE;

				printv("... object done\n");

				offset_row = 0;
				offset_column = 0;

				break;
			}

			case 0x14:		/* reserved */
				break;

			case 0x15 ... 0x17:	/* object definition */
				flush_row();
				printv("enh obj definition 0x%02x 0x%02x\n", p->mode, p->data);
				printv("enh terminated\n");
				goto swedish;

			case 0x18:		/* drcs mode */
				printv("enh DRCS mode 0x%02x\n", p->data);
				drcs_s1[p->data >> 6] = p->data & 15;
				break;

			case 0x19 ... 0x1E:	/* reserved */
				break;

			case 0x1F:		/* termination marker */
			default:
	                terminate:
				flush_row();
				printv("enh terminated %02x\n", p->mode);
				goto swedish;
			}
		} else {
			/*
			 *  Column address triplets
			 */
			int s = p->data >> 5;
			int column = p->address;
			int unicode;

			switch (p->mode) {
			case 0x00:		/* foreground color */
				if (max_level >= VBI_WST_LEVEL_2p5 && s == 0) {
					if (column > active_column)
						flush(column);

					ac.foreground = p->data & 0x1F;
					mac.foreground = ~0;

					printv("enh col %d foreground %d\n", active_column, ac.foreground);
				}

				break;

			case 0x01:		/* G1 block mosaic character */
				if (max_level >= VBI_WST_LEVEL_2p5) {
					if (column > active_column)
						flush(column);

					if (p->data & 0x20) {
						unicode = 0xEE00 + p->data; /* G1 contiguous */
						goto store;
					} else if (p->data >= 0x40) {
						unicode = vbi_teletext_unicode(
							font->G0, NO_SUBSET, p->data);
						goto store;
					}
				}

				break;

			case 0x0B:		/* G3 smooth mosaic or line drawing character */
				if (max_level < VBI_WST_LEVEL_2p5)
					break;

				/* fall through */

			case 0x02:		/* G3 smooth mosaic or line drawing character */
				if (p->data >= 0x20) {
					if (column > active_column)
						flush(column);

					unicode = 0xEF00 + p->data;
					goto store;
				}

				break;

			case 0x03:		/* background color */
				if (max_level >= VBI_WST_LEVEL_2p5 && s == 0) {
					if (column > active_column)
						flush(column);

					ac.background = p->data & 0x1F;
					mac.background = ~0;

					printv("enh col %d background %d\n", active_column, ac.background);
				}

				break;

			case 0x04:		/* reserved */
			case 0x05:		/* reserved */

			case 0x06:		/* PDC data */
				break;

			case 0x07:		/* additional flash functions */
				if (max_level >= VBI_WST_LEVEL_2p5) {
					if (column > active_column)
						flush(column);

					/*
					 *  Only one flash function (if any) implemented:
					 *  Mode 1 - Normal flash to background color
					 *  Rate 0 - Slow rate (1 Hz)
					 */
					ac.flash = !!(p->data & 3);
					mac.flash = ~0;

					printv("enh col %d flash 0x%02x\n", active_column, p->data);
				}

				break;

			case 0x08:		/* modified G0 and G2 character set designation */
				if (max_level >= VBI_WST_LEVEL_2p5) {
					if (column > active_column)
						flush(column);

					if (VALID_CHARACTER_SET(p->data))
						font = vbi_font_descriptors + p->data;
					else
						font = pg->font[0];

					printv("enh col %d modify character set %d\n", active_column, p->data);
				}

				break;

			case 0x09:		/* G0 character */
				if (max_level >= VBI_WST_LEVEL_2p5 && p->data >= 0x20) {
					if (column > active_column)
						flush(column);

					unicode = vbi_teletext_unicode(font->G0, NO_SUBSET, p->data);
					goto store;
				}

				break;

			case 0x0A:		/* reserved */
				break;

			case 0x0C:		/* display attributes */
				if (max_level < VBI_WST_LEVEL_2p5)
					break;

				if (column > active_column)
					flush(column);

				ac.size = ((p->data & 0x40) ? VBI_DOUBLE_WIDTH : 0)
					+ ((p->data & 1) ? VBI_DOUBLE_HEIGHT : 0);
				mac.size = ~0;

				if (p->data & 2) {
					if (vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE))
						ac.opacity = VBI_SEMI_TRANSPARENT;
					else
						ac.opacity = VBI_TRANSPARENT_SPACE;
				} else
					ac.opacity = pg->page_opacity[1];
				mac.opacity = ~0;

				ac.conceal = !!(p->data & 4);
				mac.conceal = ~0;

				/* (p->data & 8) reserved */

				invert = p->data & 0x10;

				ac.underline = !!(p->data & 0x20);
				mac.underline = ~0;

				printv("enh col %d display attr 0x%02x\n", active_column, p->data);

				break;

			case 0x0D:		/* drcs character invocation */
			{
				int normal = p->data >> 6;
				int offset = p->data & 0x3F;
				vt_page *dvtp;
				page_function function;
				int pgno, page, i = 0;

				if (max_level < VBI_WST_LEVEL_2p5)
					break;

				if (offset >= 48)
					break; /* invalid */

				if (column > active_column)
					flush(column);

				page = normal * 16 + drcs_s1[normal];

				printv("enh col %d DRCS %d/0x%02x\n", active_column, page, p->data);

				/* if (!pg->drcs[page]) */ {
					if (!normal) {
						function = PAGE_FUNCTION_GDRCS;
						pgno = vtp->data.lop.link[26].pgno;

						if (NO_PAGE(pgno)) {
							if (max_level < VBI_WST_LEVEL_3p5
							    || NO_PAGE(pgno = mag->drcs_link[8]))
								pgno = mag->drcs_link[0];
						} else
							printv("... X/27/4 GDRCS overrides MOT\n");
					} else {
						function = PAGE_FUNCTION_DRCS;
						pgno = vtp->data.lop.link[25].pgno;

						if (NO_PAGE(pgno)) {
							if ((i = mag->drcs_lut[vtp->pgno & 0xFF]) == 0) {
								printv("... MOT drcs_lut empty\n");
								return FALSE; /* has no link (yet) */
							}

							if (max_level < VBI_WST_LEVEL_3p5
							    || NO_PAGE(pgno = mag->drcs_link[i + 8]))
								pgno = mag->drcs_link[i + 0];
						} else
							printv("... X/27/4 DRCS overrides MOT\n");
					}

					if (NO_PAGE(pgno)) {
						printv("... dead MOT link %d\n", i);
						return FALSE; /* has no link (yet) */
					}

					printv("... %s drcs from page %03x/%04x\n",
						normal ? "normal" : "global", pgno, drcs_s1[normal]);

					dvtp = vbi_cache_get(vbi,
						pgno, drcs_s1[normal], 0x000F);

					if (!dvtp) {
						printv("... page not cached\n");
						return FALSE;
					}

					if (dvtp->function == PAGE_FUNCTION_UNKNOWN) {
						if (!(dvtp = vbi_convert_page(vbi, dvtp, TRUE, function))) {
							printv("... no g/drcs page or hamming error\n");
							return FALSE;
						}
					} else if (dvtp->function == PAGE_FUNCTION_DRCS) {
						dvtp->function = function;
					} else if (dvtp->function != function) {
						printv("... source page wrong function %d, expected %d\n",
							dvtp->function, function);
						return FALSE;
					}

					if (dvtp->data.drcs.invalid & (1ULL << offset)) {
						printv("... invalid drcs, prob. tx error\n");
						return FALSE;
					}

					pg->drcs[page] = dvtp->data.drcs.bits[0];
				}

				unicode = 0xF000 + (page << 6) + offset;
				goto store;
			}

			case 0x0E:		/* font style */
			{
				int italic, bold, proportional;
				int col, row, count;
				vbi_char *acp;

				if (max_level < VBI_WST_LEVEL_3p5)
					break;

				row = inv_row + active_row;
				count = (p->data >> 4) + 1;
				acp = &pg->text[row * EXT_COLUMNS];

				proportional = (p->data >> 0) & 1;
				bold = (p->data >> 1) & 1;
				italic = (p->data >> 2) & 1;

				while (row < ROWS && count > 0) {
					for (col = inv_column + column; col < COLUMNS; col++) {
						acp[col].italic = italic;
		    				acp[col].bold = bold;
						acp[col].proportional = proportional;
					}

					acp += EXT_COLUMNS;
					row++;
					count--;
				}

				printv("enh col %d font style 0x%02x\n", active_column, p->data);

				break;
			}

			case 0x0F:		/* G2 character */
				if (p->data >= 0x20) {
					if (column > active_column)
						flush(column);

					unicode = vbi_teletext_unicode(font->G2, NO_SUBSET, p->data);
					goto store;
				}

				break;

			case 0x10 ... 0x1F:	/* characters including diacritical marks */
				if (p->data >= 0x20) {
					if (column > active_column)
						flush(column);

					unicode = vbi_teletext_composed_unicode(
						p->mode - 0x10, p->data);
			store:
					printv("enh row %d col %d print 0x%02x/0x%02x -> 0x%04x %c\n",
						active_row, active_column, p->mode, p->data,
						gl, unicode & 0xFF);

					ac.unicode = unicode;
					mac.unicode = ~0;
				}

				break;
			}
		}
	}

swedish:

#if 0
	acp = pg->data[0];

	for (active_row = 0; active_row < ROWS; active_row++) {
		printv("%2d: ", active_row);

		for (active_column = 0; active_column < COLUMNS; acp++, active_column++) {
			printv("%04x ", acp->unicode);
		}

		printv("\n");

		acp += EXT_COLUMNS - COLUMNS;
	}
#endif

	return TRUE;
}

static void
post_enhance(vbi_page *pg, int display_rows)
{
	int last_row = MIN(display_rows, ROWS) - 2;
	vbi_char ac, *acp;
	int column, row;

	acp = pg->text;

	for (row = 0; row <= last_row; row++) {
		for (column = 0; column < COLUMNS; acp++, column++) {
			if (1)
				printv("%c", printable(acp->unicode));
			else
				printv("%04xF%dB%dS%dO%d ", acp->unicode,
				       acp->foreground, acp->background,
				       acp->size, acp->opacity);

			if (acp->opacity == VBI_TRANSPARENT_SPACE
			    || (acp->foreground == VBI_TRANSPARENT_BLACK
				&& acp->background == VBI_TRANSPARENT_BLACK)) {
				acp->opacity = VBI_TRANSPARENT_SPACE;
				acp->unicode = 0x0020;
			} else if (acp->background == VBI_TRANSPARENT_BLACK) {
				acp->opacity = VBI_SEMI_TRANSPARENT;
			}
			/* transparent foreground not implemented */

			switch (acp->size) {
			case VBI_NORMAL_SIZE:
				if (row < last_row
				    && (acp[EXT_COLUMNS].size == VBI_DOUBLE_HEIGHT2
					|| acp[EXT_COLUMNS].size == VBI_DOUBLE_SIZE2)) {
					acp[EXT_COLUMNS].unicode = 0x0020;
					acp[EXT_COLUMNS].size = VBI_NORMAL_SIZE;
				}

				if (column < 39
				    && (acp[1].size == VBI_OVER_TOP
					|| acp[1].size == VBI_OVER_BOTTOM)) {
					acp[1].unicode = 0x0020;
					acp[1].size = VBI_NORMAL_SIZE;
				}

				break;

			case VBI_DOUBLE_HEIGHT:
				if (row < last_row) {
					ac = acp[0];
					ac.size = VBI_DOUBLE_HEIGHT2;
					acp[EXT_COLUMNS] = ac;
				}
				break;

			case VBI_DOUBLE_SIZE:
				if (row < last_row) {
					ac = acp[0];
					ac.size = VBI_DOUBLE_SIZE2;
					acp[EXT_COLUMNS] = ac;
					ac.size = VBI_OVER_BOTTOM;
					acp[EXT_COLUMNS + 1] = ac;
				}

				/* fall through */

			case VBI_DOUBLE_WIDTH:
				if (column < 39) {
					ac = acp[0];
					ac.size = VBI_OVER_TOP;
					acp[1] = ac;
				}
				break;

			default:
				break;
			}
		}

		printv("\n");

		acp += EXT_COLUMNS - COLUMNS;
	}
}

static inline vbi_bool
default_object_invocation(vbi_decoder *vbi, magazine *mag,
	extension *ext, vbi_page *pg, vt_page *vtp,
	vbi_wst_level max_level, vbi_bool header_only)
{
	pop_link *pop;
	int i, order;

	if (!(i = mag->pop_lut[vtp->pgno & 0xFF]))
		return FALSE; /* has no link (yet) */

	pop = mag->pop_link + i + 8;

	if (max_level < VBI_WST_LEVEL_3p5 || NO_PAGE(pop->pgno)) {
		pop = mag->pop_link + i;

		if (NO_PAGE(pop->pgno)) {
			printv("default object has dead MOT pop link %d\n", i);
			return FALSE;
		}
	}

	order = pop->default_obj[0].type > pop->default_obj[1].type;

	for (i = 0; i < 2; i++) {
		object_type type = pop->default_obj[i ^ order].type;
		vt_triplet *trip;
		int remaining_max_triplets;

		if (type == OBJ_TYPE_NONE)
			continue;

		printv("default object #%d invocation, type %d\n", i ^ order, type);

		trip = resolve_obj_address(vbi, type, pop->pgno,
			pop->default_obj[i ^ order].address, PAGE_FUNCTION_POP,
			&remaining_max_triplets);

		if (!trip)
			return FALSE;

		if (!enhance(vbi, mag, ext, pg, vtp, type, trip,
			     remaining_max_triplets, 0, 0, max_level,
			     header_only))
			return FALSE;
	}

	return TRUE;
}

/**
 * vbi_format_vt_page:
 * @vbi: Initialized vbi decoding context.
 * @pg: Place to store the formatted page.
 * @vtp: Raw Teletext page. 
 * @max_level: Format the page at this Teletext implementation level.
 * @display_rows: Number of rows to format, between 1 ... 25.
 * @navigation: Analyse the page and add navigation links,
 *   including TOP and FLOF.
 * 
 * Format a page @pg from a raw Teletext page @vtp. This function is
 * used internally by libzvbi only.
 * 
 * Return value: 
 * TRUE if the page could be formatted.
 **/
int
vbi_format_vt_page(vbi_decoder *vbi,
		   vbi_page *pg, vt_page *vtp,
		   vbi_wst_level max_level,
		   int display_rows, vbi_bool navigation)
{
	char buf[16];
	magazine *mag;
	extension *ext;
	int column, row, i;

	if (vtp->function != PAGE_FUNCTION_LOP &&
	    vtp->function != PAGE_FUNCTION_TRIGGER)
		return FALSE;

	printv("\nFormatting page %03x/%04x pg=%p lev=%d rows=%d nav=%d\n",
	       vtp->pgno, vtp->subno, pg, max_level, display_rows, navigation);

	display_rows = SATURATE(display_rows, 1, ROWS);

	pg->vbi = vbi;

	pg->nuid = vbi->network.ev.network.nuid;

	pg->pgno = vtp->pgno;
	pg->subno = vtp->subno;

	pg->rows = display_rows;
	pg->columns = EXT_COLUMNS;

	pg->dirty.y0 = 0;
	pg->dirty.y1 = ROWS - 1;
	pg->dirty.roll = 0;

	mag = (max_level <= VBI_WST_LEVEL_1p5) ?
		vbi->vt.magazine : vbi->vt.magazine + (vtp->pgno >> 8);

	if (vtp->data.lop.ext)
		ext = &vtp->data.ext_lop.ext;
	else
		ext = &mag->extension;

	/* Character set designation */

	character_set_designation(pg->font, ext, vtp);

	/* Colors */

	screen_color(pg, vtp->flags, ext->def_screen_color);

	vbi_transp_colormap(vbi, pg->color_map, ext->color_map, 40);

	pg->drcs_clut = ext->drcs_clut;

	/* Opacity */

	pg->page_opacity[1] =
		(vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE | C10_INHIBIT_DISPLAY)) ?
			VBI_TRANSPARENT_SPACE : VBI_OPAQUE;
	pg->boxed_opacity[1] =
		(vtp->flags & C10_INHIBIT_DISPLAY) ?
			VBI_TRANSPARENT_SPACE : VBI_SEMI_TRANSPARENT;

	if (vtp->flags & C7_SUPPRESS_HEADER) {
		pg->page_opacity[0] = VBI_TRANSPARENT_SPACE;
		pg->boxed_opacity[0] = VBI_TRANSPARENT_SPACE;
	} else {
		pg->page_opacity[0] = pg->page_opacity[1];
		pg->boxed_opacity[0] = pg->boxed_opacity[1];
	}

	/* DRCS */

	memset(pg->drcs, 0, sizeof(pg->drcs));

	/* Current page number in header */

	sprintf(buf, "\2%x.%02x\7", vtp->pgno, vtp->subno & 0xff);

	/* Level 1 formatting */

	i = 0;
	pg->double_height_lower = 0;

	for (row = 0; row < display_rows; row++) {
		struct vbi_font_descr *font;
		int mosaic_unicodes; /* 0xEE00 separate, 0xEE20 contiguous */
		int held_mosaic_unicode;
		vbi_bool hold, mosaic;
		vbi_bool double_height, wide_char;
		vbi_char ac, *acp = &pg->text[row * EXT_COLUMNS];

		held_mosaic_unicode = 0xEE20; /* G1 block mosaic, blank, contiguous */

		memset(&ac, 0, sizeof(ac));

		ac.unicode      = 0x0020;
		ac.foreground	= ext->foreground_clut + VBI_WHITE;
		ac.background	= ext->background_clut + VBI_BLACK;
		mosaic_unicodes	= 0xEE20; /* contiguous */
		ac.opacity	= pg->page_opacity[row > 0];
		font		= pg->font[0];
		hold		= FALSE;
		mosaic		= FALSE;

		double_height	= FALSE;
		wide_char	= FALSE;

		acp[COLUMNS] = ac; /* artificial column 41 */

		for (column = 0; column < COLUMNS; ++column) {
			int raw;

			if (row == 0 && column < 8) {
				raw = buf[column];
				i++;
			} else if ((raw = vbi_parity(vtp->data.lop.raw[0][i++])) < 0)
				raw = ' ';

			/* set-at spacing attributes */

			switch (raw) {
			case 0x09:		/* steady */
				ac.flash = FALSE;
				break;

			case 0x0C:		/* normal size */
				ac.size = VBI_NORMAL_SIZE;
				break;

			case 0x18:		/* conceal */
				ac.conceal = TRUE;
				break;

			case 0x19:		/* contiguous mosaics */
				mosaic_unicodes = 0xEE20;
				break;

			case 0x1A:		/* separated mosaics */
				mosaic_unicodes = 0xEE00;
				break;

			case 0x1C:		/* black background */
				ac.background = ext->background_clut + VBI_BLACK;
				break;

			case 0x1D:		/* new background */
				ac.background = ext->background_clut + (ac.foreground & 7);
				break;

			case 0x1E:		/* hold mosaic */
				hold = TRUE;
				break;
			}

			if (raw <= 0x1F)
				ac.unicode = (hold & mosaic) ? held_mosaic_unicode : 0x0020;
			else
				if (mosaic && (raw & 0x20)) {
					held_mosaic_unicode = mosaic_unicodes + raw - 0x20;
					ac.unicode = held_mosaic_unicode;
				} else
					ac.unicode = vbi_teletext_unicode(font->G0,
									  font->subset, raw);

			if (!wide_char) {
				acp[column] = ac;

				wide_char = /*!!*/(ac.size & VBI_DOUBLE_WIDTH);

				if (wide_char && column < (COLUMNS - 1)) {
					acp[column + 1] = ac;
					acp[column + 1].size = VBI_OVER_TOP;
				}
			} else
				wide_char = FALSE;

			/* set-after spacing attributes */

			switch (raw) {
			case 0x00 ... 0x07:	/* alpha + foreground color */
				ac.foreground = ext->foreground_clut + (raw & 7);
				ac.conceal = FALSE;
				mosaic = FALSE;
				break;

			case 0x08:		/* flash */
				ac.flash = TRUE;
				break;

			case 0x0A:		/* end box */
				if (column < (COLUMNS - 1)
				    && vbi_parity(vtp->data.lop.raw[0][i]) == 0x0a)
					ac.opacity = pg->page_opacity[row > 0];
				break;

			case 0x0B:		/* start box */
				if (column < (COLUMNS - 1)
				    && vbi_parity(vtp->data.lop.raw[0][i]) == 0x0b)
					ac.opacity = pg->boxed_opacity[row > 0];
				break;

			case 0x0D:		/* double height */
				if (row <= 0 || row >= 23)
					break;
				ac.size = VBI_DOUBLE_HEIGHT;
				double_height = TRUE;
				break;

			case 0x0E:		/* double width */
				printv("spacing col %d row %d double width\n", column, row);
				if (column < (COLUMNS - 1))
					ac.size = VBI_DOUBLE_WIDTH;
				break;

			case 0x0F:		/* double size */
				printv("spacing col %d row %d double size\n", column, row);
				if (column >= (COLUMNS - 1) || row <= 0 || row >= 23)
					break;
				ac.size = VBI_DOUBLE_SIZE;
				double_height = TRUE;

				break;

			case 0x10 ... 0x17:	/* mosaic + foreground color */
				ac.foreground = ext->foreground_clut + (raw & 7);
				ac.conceal = FALSE;
				mosaic = TRUE;
				break;

			case 0x1F:		/* release mosaic */
				hold = FALSE;
				break;

			case 0x1B:		/* ESC */
				font = (font == pg->font[0]) ? pg->font[1] : pg->font[0];
				break;
			}
		}

		if (double_height) {
			for (column = 0; column < EXT_COLUMNS; column++) {
				ac = acp[column];

				switch (ac.size) {
				case VBI_DOUBLE_HEIGHT:
					ac.size = VBI_DOUBLE_HEIGHT2;
					acp[EXT_COLUMNS + column] = ac;
					break;
		
				case VBI_DOUBLE_SIZE:
					ac.size = VBI_DOUBLE_SIZE2;
					acp[EXT_COLUMNS + column] = ac;
					ac.size = VBI_OVER_BOTTOM;
					acp[EXT_COLUMNS + (++column)] = ac;
					break;

				default: /* NORMAL, DOUBLE_WIDTH, OVER_TOP */
					ac.size = VBI_NORMAL_SIZE;
					ac.unicode = 0x0020;
					acp[EXT_COLUMNS + column] = ac;
					break;
				}
			}

			i += COLUMNS;
			row++;

			pg->double_height_lower |= 1 << row;
		}
	}
#if 0
	if (row < ROWS) {
		vbi_char ac;

		memset(&ac, 0, sizeof(ac));

		ac.foreground	= ext->foreground_clut + WHITE;
		ac.background	= ext->background_clut + BLACK;
		ac.opacity	= pg->page_opacity[1];
		ac.unicode	= 0x0020;

		for (i = row * EXT_COLUMNS; i < ROWS * EXT_COLUMNS; i++)
			pg->text[i] = ac;
	}
#endif
	/* Local enhancement data and objects */

	if (max_level >= VBI_WST_LEVEL_1p5 && display_rows > 0) {
		vbi_page page;
		vbi_bool success;

		memcpy(&page, pg, sizeof(page));

		if (!(vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE))) {
			pg->boxed_opacity[0] = VBI_TRANSPARENT_SPACE;
			pg->boxed_opacity[1] = VBI_TRANSPARENT_SPACE;
		}

		if (vtp->enh_lines & 1) {
			printv("enhancement packets %08x\n", vtp->enh_lines);
			success = enhance(vbi, mag, ext, pg, vtp, LOCAL_ENHANCEMENT_DATA,
				vtp->data.enh_lop.enh, elements(vtp->data.enh_lop.enh),
				0, 0, max_level, display_rows == 1);
		} else
			success = default_object_invocation(vbi, mag, ext, pg, vtp,
							    max_level, display_rows == 1);

		if (success) {
			if (max_level >= VBI_WST_LEVEL_2p5)
				post_enhance(pg, display_rows);
		} else
			memcpy(pg, &page, sizeof(*pg));
	}

	/* Navigation */

	if (navigation) {
		pg->nav_link[5].pgno = vbi->vt.initial_page.pgno;
		pg->nav_link[5].subno = vbi->vt.initial_page.subno;

		for (row = 1; row < MIN(ROWS - 1, display_rows); row++)
			zap_links(pg, row);

		if (display_rows >= ROWS) {
			if (vtp->data.lop.flof) {
				if (vtp->data.lop.link[5].pgno >= 0x100
				    && vtp->data.lop.link[5].pgno <= 0x899
				    && (vtp->data.lop.link[5].pgno & 0xFF) != 0xFF) {
					pg->nav_link[5].pgno = vtp->data.lop.link[5].pgno;
					pg->nav_link[5].subno = vtp->data.lop.link[5].subno;
				}

				if (vtp->lop_lines & (1 << 24))
					flof_links(pg, vtp);
				else
					flof_navigation_bar(pg, vtp);
			} else if (vbi->vt.top)
				top_navigation_bar(vbi, pg, vtp);
		}
	}

	return TRUE;
}

/**
 * vbi_fetch_vt_page:
 * @vbi: Initialized VBI decoding context.
 * @pg: Place to store the formatted page.
 * @pgno: Page number of the page to fetch, see #vbi_pgno.
 * @subno: Subpage number to fetch (optional #VBI_ANY_SUBNO).
 * @max_level: Format the page at this Teletext implementation level.
 * @display_rows: Number of rows to format, between 1 ... 25.
 * @navigation: Analyse the page and add navigation links,
 *   including TOP and FLOF.
 * 
 * Fetches a Teletext page designated by @pgno and @subno from the
 * cache, formats and stores it in @pg. Formatting is limited to row
 * 0 ... @display_rows - 1 inclusive. The really useful values
 * are 1 (format header only) or 25 (everything). Likewise
 * @navigation can be used to save unnecessary formatting time.
 * 
 * Return value:
 * FALSE if the page is not cached or could not be formatted
 * for other reasons, for instance a data page not intended for
 * display. Level 2.5/3.5 pages which could not be formatted e. g.
 * due to referencing data pages not in cache are formatted at a
 * lower level.
 **/
vbi_bool
vbi_fetch_vt_page(vbi_decoder *vbi, vbi_page *pg,
		  vbi_pgno pgno, vbi_subno subno,
		  vbi_wst_level max_level,
		  int display_rows, vbi_bool navigation)
{
	vt_page *vtp;
	int row;

	switch (pgno) {
	case 0x900:
		if (subno == VBI_ANY_SUBNO)
			subno = 0;

		if (!vbi->vt.top || !top_index(vbi, pg, subno))
			return FALSE;

		pg->nuid = vbi->network.ev.network.nuid;
		pg->pgno = 0x900;
		pg->subno = subno;

		post_enhance(pg, ROWS);

		for (row = 1; row < ROWS; row++)
			zap_links(pg, row);

		return TRUE;

	default:
		vtp = vbi_cache_get(vbi, pgno, subno, -1);

		if (!vtp)
			return FALSE;

		return vbi_format_vt_page(vbi, pg, vtp,
					  max_level, display_rows, navigation);
	}
}

