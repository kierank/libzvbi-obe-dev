/*
 *  libzvbi - Teletext decoder
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

/* $Id: packet.c,v 1.11 2003/02/16 21:11:28 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "hamm.h"
#include "lang.h"
#include "export.h"
#include "tables.h"
#include "vbi.h"

#ifndef FPC
#define FPC 0
#endif

#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))

static vbi_bool convert_drcs(vt_page *vtp, uint8_t *raw);

static inline void
dump_pagenum(pagenum page)
{
	printf("T%x %3x/%04x\n", page.type, page.pgno, page.subno);
}

static void
dump_raw(vt_page *vtp, vbi_bool unham)
{
	int i, j;

	printf("Page %03x.%04x\n", vtp->pgno, vtp->subno);

	for (j = 0; j < 25; j++) {
		if (unham)
			for (i = 0; i < 40; i++)
				printf("%01x ", vbi_hamm8(vtp->data.lop.raw[j][i]) & 0xF);
		else
			for (i = 0; i < 40; i++)
				printf("%02x ", vtp->data.lop.raw[j][i]);
		for (i = 0; i < 40; i++)
			putchar(printable(vtp->data.lop.raw[j][i]));
		putchar('\n');
	}
}

static void
dump_extension(vt_extension *ext)
{
	int i;

	printf("Extension:\ndesignations %08x\n", ext->designations);
	printf("char set primary %d secondary %d\n", ext->char_set[0], ext->char_set[1]);
	printf("default screen col %d row col %d\n", ext->def_screen_color, ext->def_row_color);
	printf("bbg subst %d color table remapping %d, %d\n",
		ext->fallback.black_bg_substitution, ext->foreground_clut, ext->background_clut);
	printf("panel left %d right %d left columns %d\n",
		ext->fallback.left_side_panel, ext->fallback.right_side_panel,
		ext->fallback.left_panel_columns);
	printf("color map (bgr):\n");
	for (i = 0; i < 40; i++) {
		printf("%08x, ", ext->color_map[i]);
		if ((i % 8) == 7) printf("\n");
	}
	printf("dclut4 global: ");
	for (i = 0; i <= 3; i++)
		printf("%2d ", ext->drcs_clut[i + 2]);
	printf("\ndclut4 normal: ");
	for (i = 0; i <= 3; i++)
		printf("%2d ", ext->drcs_clut[i + 6]);
	printf("\ndclut16 global: ");
	for (i = 0; i <= 15; i++)
		printf("%2d ", ext->drcs_clut[i + 10]);
	printf("\ndclut16 normal: ");
	for (i = 0; i <= 15; i++)
		printf("%2d ", ext->drcs_clut[i + 26]);
	printf("\n\n");
}

static void
dump_drcs(vt_page *vtp)
{
	int i, j, k;
	uint8_t *p = vtp->data.drcs.bits[0];

	printf("\nDRCS page %03x/%04x\n", vtp->pgno, vtp->subno);

	for (i = 0; i < 48; i++) {
		printf("DRC #%d mode %02x\n", i, vtp->data.drcs.mode[i]);

		for (j = 0; j < 10; p += 6, j++) {
			for (k = 0; k < 6; k++)
				printf("%x%x", p[k] & 15, p[k] >> 4);
			putchar('\n');
		}
	}
}

static void
dump_page_info(struct teletext *vt)
{
	int i, j;

	for (i = 0; i < 0x800; i += 16) {
		printf("%03x: ", i + 0x100);

		for (j = 0; j < 16; j++)
			printf("%02x:%02x:%04x ",
			       vt->page_info[i + j].code & 0xFF,
			       vt->page_info[i + j].language & 0xFF, 
			       vt->page_info[i + j].subcode & 0xFFFF);

		putchar('\n');
	}

	putchar('\n');
}

static inline vbi_bool
hamm8_page_number(pagenum *p, uint8_t *raw, int magazine)
{
	int b1, b2, b3, err, m;

	err = b1 = vbi_hamm16(raw + 0);
	err |= b2 = vbi_hamm16(raw + 2);
	err |= b3 = vbi_hamm16(raw + 4);

	if (err < 0)
		return FALSE;

	m = ((b3 >> 5) & 6) + (b2 >> 7);

	p->pgno = ((magazine ^ m) ? : 8) * 256 + b1;
	p->subno = (b3 * 256 + b2) & 0x3f7f;

	return TRUE;
}

static inline vbi_bool
parse_mot(vt_magazine *mag, uint8_t *raw, int packet)
{
	int err, i, j;

	switch (packet) {
	case 1 ... 8:
	{
		int index = (packet - 1) << 5;
		int n0, n1;

		for (i = 0; i < 20; index++, i++) {
			if (i == 10)
				index += 6;

			n0 = vbi_hamm8(*raw++);
			n1 = vbi_hamm8(*raw++);

			if ((n0 | n1) < 0)
				continue;

			mag->pop_lut[index] = n0 & 7;
			mag->drcs_lut[index] = n1 & 7;
		}

		return TRUE;
	}

	case 9 ... 14:
	{
		int index = (packet - 9) * 0x30 + 10;

		for (i = 0; i < 20; index++, i++) {
			int n0, n1;

			if (i == 6 || i == 12) {
				if (index == 0x100)
					break;
				else
					index += 10;
			}

			n0 = vbi_hamm8(*raw++);
			n1 = vbi_hamm8(*raw++);

			if ((n0 | n1) < 0)
				continue;

			mag->pop_lut[index] = n0 & 7;
			mag->drcs_lut[index] = n1 & 7;
		}

		return TRUE;
	}

	case 15 ... 18: /* not used */
		return TRUE;

	case 22 ... 23:	/* level 3.5 pops */
		packet--;

	case 19 ... 20: /* level 2.5 pops */
	{
		vt_pop_link *pop = mag->pop_link + (packet - 19) * 4;

		for (i = 0; i < 4; raw += 10, pop++, i++) {
			int n[10];

			for (err = j = 0; j < 10; j++)
				err |= n[j] = vbi_hamm8(raw[j]);

			if (err < 0) /* XXX unused bytes poss. not hammed (^ N3) */
				continue;

			pop->pgno = (((n[0] & 7) ? : 8) << 8) + (n[1] << 4) + n[2];

			/* n[3] number of subpages ignored */

			if (n[4] & 1)
				memset(&pop->fallback, 0, sizeof(pop->fallback));
			else {
				int x = (n[4] >> 1) & 3;

				pop->fallback.black_bg_substitution = n[4] >> 3;
				pop->fallback.left_side_panel = x & 1;
				pop->fallback.right_side_panel = x >> 1;
				pop->fallback.left_panel_columns = "\00\20\20\10"[x];
			}

			pop->default_obj[0].type = n[5] & 3;
			pop->default_obj[0].address = (n[7] << 4) + n[6];
			pop->default_obj[1].type = n[5] >> 2;
			pop->default_obj[1].address = (n[9] << 4) + n[8];
		}

		return TRUE;
	}

	case 21:	/* level 2.5 drcs */
	case 24:	/* level 3.5 drcs */
	    {
		int index = (packet == 21) ? 0 : 8;
		int n[4];

		for (i = 0; i < 8; raw += 4, index++, i++) {
			for (err = j = 0; j < 4; j++)
				err |= n[j] = vbi_hamm8(raw[j]);

			if (err < 0)
				continue;

			mag->drcs_link[index] = (((n[0] & 7) ? : 8) << 8) + (n[1] << 4) + n[2];

			/* n[3] number of subpages ignored */
		}

		return TRUE;
	    }
	}

	return TRUE;
}

static vbi_bool
parse_pop(vt_page *vtp, uint8_t *raw, int packet)
{
	int designation, triplet[13];
	vt_triplet *trip;
	int i;

	if ((designation = vbi_hamm8(raw[0])) < 0)
		return FALSE;

	for (raw++, i = 0; i < 13; raw += 3, i++)
		triplet[i] = vbi_hamm24(raw);

	if (packet == 26)
		packet += designation;

	switch (packet) {
	case 1 ... 2:
		if (!(designation & 1))
			return FALSE; /* fixed usage */

	case 3 ... 4:
		if (designation & 1) {
			int index = (packet - 1) * 26;

			for (index += 2, i = 1; i < 13; index += 2, i++)
				if (triplet[i] >= 0) {
					vtp->data.pop.pointer[index + 0] = triplet[i] & 0x1FF;
					vtp->data.pop.pointer[index + 1] = triplet[i] >> 9;
				}

			return TRUE;
		}

		/* fall through */

	case 5 ... 42:
		trip = vtp->data.pop.triplet + (packet - 3) * 13;

		for (i = 0; i < 13; trip++, i++)
			if (triplet[i] >= 0) {
				trip->address	= (triplet[i] >> 0) & 0x3F;
				trip->mode	= (triplet[i] >> 6) & 0x1F;
				trip->data	= (triplet[i] >> 11);
			}

		return TRUE;
	}

	return FALSE;
}

static unsigned int expand[64];

static void
init_expand(void)
{
	int i, j, n;

	for (i = 0; i < 64; i++) {
		for (n = j = 0; j < 6; j++)
			if (i & (0x20 >> j))
				n |= 1 << (j * 4);
		expand[i] = n;
	}
}

static vbi_bool
convert_drcs(vt_page *vtp, uint8_t *raw)
{
	uint8_t *p, *d;
	int i, j, q;

	p = raw;
	vtp->data.drcs.invalid = 0;

	for (i = 0; i < 24; p += 40, i++)
		if (vtp->lop_lines & (2 << i)) {
			for (j = 0; j < 20; j++)
				if (vbi_parity(p[j]) < 0x40) {
					vtp->data.drcs.invalid |= 1ULL << (i * 2);
					break;
				}
			for (j = 20; j < 40; j++)
				if (vbi_parity(p[j]) < 0x40) {
					vtp->data.drcs.invalid |= 1ULL << (i * 2 + 1);
					break;
				}
		} else {
			vtp->data.drcs.invalid |= 3ULL << (i * 2);
		}

	p = raw;
	d = vtp->data.drcs.bits[0];

	for (i = 0; i < 48; i++) {
		switch (vtp->data.drcs.mode[i]) {
		case DRCS_MODE_12_10_1:
			for (j = 0; j < 20; d += 3, j++) {
				d[0] = q = expand[p[j] & 0x3F];
				d[1] = q >> 8;
				d[2] = q >> 16;
			}
			p += 20;
			break;

		case DRCS_MODE_12_10_2:
			if (vtp->data.drcs.invalid & (3ULL << i)) {
				vtp->data.drcs.invalid |= (3ULL << i);
				d += 60;
			} else
				for (j = 0; j < 20; d += 3, j++) {
					q = expand[p[j +  0] & 0x3F]
					  + expand[p[j + 20] & 0x3F] * 2;
					d[0] = q;
					d[1] = q >> 8;
					d[2] = q >> 16;
				}
			p += 40;
			d += 60;
			i += 1;
			break;

		case DRCS_MODE_12_10_4:
			if (vtp->data.drcs.invalid & (15ULL << i)) {
				vtp->data.drcs.invalid |= (15ULL << i);
				d += 60;
			} else
				for (j = 0; j < 20; d += 3, j++) {
					q = expand[p[j +  0] & 0x3F]
					  + expand[p[j + 20] & 0x3F] * 2
					  + expand[p[j + 40] & 0x3F] * 4
					  + expand[p[j + 60] & 0x3F] * 8;
					d[0] = q;
					d[1] = q >> 8;
					d[2] = q >> 16;
				}
			p += 80;
			d += 180;
			i += 3;
			break;

		case DRCS_MODE_6_5_4:
			for (j = 0; j < 20; p += 4, d += 6, j++) {
				q = expand[p[0] & 0x3F]
				  + expand[p[1] & 0x3F] * 2
				  + expand[p[2] & 0x3F] * 4
				  + expand[p[3] & 0x3F] * 8;
				d[0] = (q & 15) * 0x11;
				d[1] = ((q >> 4) & 15) * 0x11;
				d[2] = ((q >> 8) & 15) * 0x11;
				d[3] = ((q >> 12) & 15) * 0x11;
				d[4] = ((q >> 16) & 15) * 0x11;
				d[5] = (q >> 20) * 0x11;
			}
			break;

		default:
			vtp->data.drcs.invalid |= (1ULL << i);
			p += 20;
			d += 60;
			break;
		}
	}

	if (0)
		dump_drcs(vtp);

	return TRUE;
}

static int
page_language(struct teletext *vt, vt_page *vtp, int pgno, int national)
{
	vt_magazine *mag;
	vt_extension *ext;
	int char_set;
	int lang = -1; /***/

	if (vtp) {
		if (vtp->function != PAGE_FUNCTION_LOP)
			return lang;

		pgno = vtp->pgno;
		national = vtp->national;
	}

	mag = (vt->max_level <= VBI_WST_LEVEL_1p5) ?
		vt->magazine : vt->magazine + (pgno >> 8);

	ext = (vtp && vtp->data.lop.ext) ?
		&vtp->data.ext_lop.ext : &mag->extension;

	char_set = ext->char_set[0];

	if (VALID_CHARACTER_SET(char_set))
		lang = char_set;

	char_set = (char_set & ~7) + national;

	if (VALID_CHARACTER_SET(char_set))
		lang = char_set;

	return lang;
}

static vbi_bool
parse_mip_page(vbi_decoder *vbi, vt_page *vtp,
	int pgno, int code, int *subp_index)
{
	uint8_t *raw;
	int subc, old_code, old_subc;

	if (code < 0)
		return FALSE;

	switch (code) {
	case 0x52 ... 0x6F: /* reserved */
	case 0xD2 ... 0xDF: /* reserved */
	case 0xFA ... 0xFC: /* reserved */
	case 0xFF: 	    /* reserved, we use it as 'unknown' flag */
		return TRUE;

	case 0x02 ... 0x4F:
	case 0x82 ... 0xCF:
		subc = code & 0x7F;
		code = (code >= 0x80) ? VBI_PROGR_SCHEDULE :
					VBI_NORMAL_PAGE;
		break;

	case 0x70 ... 0x77:
		code = VBI_SUBTITLE_PAGE;
		subc = 0;
		vbi->vt.page_info[pgno - 0x100].language =
			page_language(&vbi->vt,
				vbi_cache_get(vbi, pgno, 0, 0),
				pgno, code & 7);
		break;

	case 0x50 ... 0x51: /* normal */
	case 0xD0 ... 0xD1: /* program */
	case 0xE0 ... 0xE1: /* data */
	case 0x7B: /* current program */
	case 0xF8: /* keyword search list */
		if (*subp_index > 10 * 13)
			return FALSE;

		raw = &vtp->data.unknown.raw[*subp_index / 13 + 15]
				    [(*subp_index % 13) * 3 + 1];
		(*subp_index)++;

		if ((subc = vbi_hamm16(raw) | (vbi_hamm8(raw[2]) << 8)) < 0)
			return FALSE;

		if ((code & 15) == 1)
			subc += 1 << 12;
		else if (subc < 2)
			return FALSE;

		code =	(code == 0xF8) ? VBI_KEYWORD_SEARCH_LIST :
			(code == 0x7B) ? VBI_CURRENT_PROGR :
			(code >= 0xE0) ? VBI_CA_DATA_BROADCAST :
			(code >= 0xD0) ? VBI_PROGR_SCHEDULE :
					 VBI_NORMAL_PAGE;
		break;

	default:
		code = code;
		subc = 0;
		break;
	}

	old_code = vbi->vt.page_info[pgno - 0x100].code;
	old_subc = vbi->vt.page_info[pgno - 0x100].subcode;

	/*
	 *  When we got incorrect numbers and proved otherwise by
	 *  actually receiving the page...
	 */
	if (old_code == VBI_UNKNOWN_PAGE || old_code == VBI_SUBTITLE_PAGE
	    || code != VBI_NO_PAGE || code == VBI_SUBTITLE_PAGE)
		vbi->vt.page_info[pgno - 0x100].code = code;

	if (old_code == VBI_UNKNOWN_PAGE || subc > old_subc)
		vbi->vt.page_info[pgno - 0x100].subcode = subc;

	return TRUE;
}

static vbi_bool
parse_mip(vbi_decoder *vbi, vt_page *vtp)
{
	int packet, pgno, i, spi = 0;

	if (0)
		dump_raw(vtp, TRUE);

	for (packet = 1, pgno = vtp->pgno & 0xF00; packet <= 8; packet++, pgno += 0x20)
		if (vtp->lop_lines & (1 << packet)) {
			uint8_t *raw = vtp->data.unknown.raw[packet];

			for (i = 0x00; i <= 0x09; raw += 2, i++)
				if (!parse_mip_page(vbi, vtp, pgno + i, vbi_hamm16(raw), &spi))
					return FALSE;
			for (i = 0x10; i <= 0x19; raw += 2, i++)
				if (!parse_mip_page(vbi, vtp, pgno + i, vbi_hamm16(raw), &spi))
					return FALSE;
		}

	for (packet = 9, pgno = vtp->pgno & 0xF00; packet <= 14; packet++, pgno += 0x30)
		if (vtp->lop_lines & (1 << packet)) {
			uint8_t *raw = vtp->data.unknown.raw[packet];

			for (i = 0x0A; i <= 0x0F; raw += 2, i++)
				if (!parse_mip_page(vbi, vtp, pgno + i,
						    vbi_hamm16(raw), &spi))
					return FALSE;
			if (packet == 14) /* 0xFA ... 0xFF */
				break;
			for (i = 0x1A; i <= 0x1F; raw += 2, i++)
				if (!parse_mip_page(vbi, vtp, pgno + i,
						    vbi_hamm16(raw), &spi))
					return FALSE;
			for (i = 0x2A; i <= 0x2F; raw += 2, i++)
				if (!parse_mip_page(vbi, vtp, pgno + i,
						    vbi_hamm16(raw), &spi))
					return FALSE;
		}

	if (0 && packet == 1)
		dump_page_info(&vbi->vt);

	return TRUE;
}

static void
eacem_trigger(vbi_decoder *vbi, vt_page *vtp)
{
	vbi_page pg;
	uint8_t *s;
	int i, j;

	if (0)
		dump_raw(vtp, FALSE);

	if (!(vbi->event_mask & VBI_EVENT_TRIGGER))
		return;

	if (!vbi_format_vt_page(vbi, &pg, vtp, VBI_WST_LEVEL_1p5, 24, 0))
		return;

	s = (uint8_t *) pg.text;

	for (i = 1; i < 25; i++)
		for (j = 0; j < 40; j++) {
			int c = pg.text[i * 41 + j].unicode;
			*s++ = (c >= 0x20 && c <= 0xFF) ? c : 0x20;
		}
	*s = 0;

	vbi_eacem_trigger(vbi, (uint8_t *) pg.text);
}

/*
 *  Table Of Pages navigation
 */

static const int dec2bcdp[20] = {
	0x000, 0x040, 0x080, 0x120, 0x160, 0x200, 0x240, 0x280, 0x320, 0x360,
	0x400, 0x440, 0x480, 0x520, 0x560, 0x600, 0x640, 0x680, 0x720, 0x760
};

static vbi_bool
top_page_number(pagenum *p, uint8_t *raw)
{
	int n[8];
	int pgno, err, i;

	for (err = i = 0; i < 8; i++)
		err |= n[i] = vbi_hamm8(raw[i]);

	pgno = n[0] * 256 + n[1] * 16 + n[2];

	if (err < 0 || pgno > 0x8FF)
		return FALSE;

	p->pgno = pgno;
	p->subno = ((n[3] << 12) | (n[4] << 8) | (n[5] << 4) | n[6]) & 0x3f7f; // ?
	p->type = n[7]; // ?

	return TRUE;
}

static inline vbi_bool
parse_btt(vbi_decoder *vbi, uint8_t *raw, int packet)
{
	vt_page *vtp;

	switch (packet) {
	case 1 ... 20:
	{
		int i, j, code, index = dec2bcdp[packet - 1];

		for (i = 0; i < 4; i++) {
			for (j = 0; j < 10; index++, j++) {
				struct page_info *pi = vbi->vt.page_info + index;

				if ((code = vbi_hamm8(*raw++)) < 0)
					break;

				switch (code) {
				case BTT_SUBTITLE:
					pi->code = VBI_SUBTITLE_PAGE;
					if ((vtp = vbi_cache_get(vbi, index + 0x100, 0, 0)))
						pi->language = page_language(&vbi->vt, vtp, 0, 0);
					break;

				case BTT_PROGR_INDEX_S:
				case BTT_PROGR_INDEX_M:
					/* Usually schedule, not index (likely BTT_GROUP) */
					pi->code = VBI_PROGR_SCHEDULE;
					break;

				case BTT_BLOCK_S:
				case BTT_BLOCK_M:
					pi->code = VBI_TOP_BLOCK;
					break;

				case BTT_GROUP_S:
				case BTT_GROUP_M:
					pi->code = VBI_TOP_GROUP;
					break;

				case 8 ... 11:
					pi->code = VBI_NORMAL_PAGE;
					break;

				default:
					pi->code = VBI_NO_PAGE;
					continue;
				}

				switch (code) {
				case BTT_PROGR_INDEX_M:
				case BTT_BLOCK_M:
				case BTT_GROUP_M:
				case BTT_NORMAL_M:
					/* -> mpt, mpt_ex */
					break;

				default:
					pi->subcode = 0;
					break;
				}
			}

			index += ((index & 0xFF) == 0x9A) ? 0x66 : 0x06;
		}

		break;
	}

	case 21 ... 23:
	    {
		pagenum *p = vbi->vt.btt_link + (packet - 21) * 5;
		int i;

		vbi->vt.top = TRUE;

		for (i = 0; i < 5; raw += 8, p++, i++) {
			if (!top_page_number(p, raw))
				continue;

			if (0) {
				printf("BTT #%d: ", (packet - 21) * 5);
				dump_pagenum(*p);
			}

			switch (p->type) {
			case 1: /* MPT */
			case 2: /* AIT */
			case 3: /* MPT-EX */
				vbi->vt.page_info[p->pgno - 0x100].code = VBI_TOP_PAGE;
				vbi->vt.page_info[p->pgno - 0x100].subcode = 0;
				break;
			}
		}

		break;
	    }
	}

	if (0 && packet == 1)
		dump_page_info(&vbi->vt);

	return TRUE;
}

static vbi_bool
parse_ait(vt_page *vtp, uint8_t *raw, int packet)
{
	int i, n;
	ait_entry *ait;

	if (packet < 1 || packet > 23)
		return TRUE;

	ait = vtp->data.ait + (packet - 1) * 2;

	if (top_page_number(&ait[0].page, raw + 0)) {
		for (i = 0; i < 12; i++)
			if ((n = vbi_parity(raw[i + 8])) >= 0)
				ait[0].text[i] = n;
	}

	if (top_page_number(&ait[1].page, raw + 20)) {
		for (i = 0; i < 12; i++)
			if ((n = vbi_parity(raw[i + 28])) >= 0)
				ait[1].text[i] = n;
	}

	return TRUE;
}

static inline vbi_bool
parse_mpt(struct teletext *vt, uint8_t *raw, int packet)
{
	int i, j, index;
	int n;

	switch (packet) {
	case 1 ... 20:
		index = dec2bcdp[packet - 1];

		for (i = 0; i < 4; i++) {
			for (j = 0; j < 10; index++, j++)
				if ((n = vbi_hamm8(*raw++)) >= 0) {
					int code = vt->page_info[index].code;
					int subc = vt->page_info[index].subcode;

					if (n > 9)
						n = 0xFFFEL; /* mpt_ex? not transm?? */

					if (code != VBI_NO_PAGE && code != VBI_UNKNOWN_PAGE
					    && (subc >= 0xFFFF || n > subc))
						vt->page_info[index].subcode = n;
				}

			index += ((index & 0xFF) == 0x9A) ? 0x66 : 0x06;
		}
	}

	return TRUE;
}

static inline vbi_bool
parse_mpt_ex(struct teletext *vt, uint8_t *raw, int packet)
{
	int i, code, subc;
	pagenum p;

	switch (packet) {
	case 1 ... 23:
		for (i = 0; i < 5; raw += 8, i++) {
			if (!top_page_number(&p, raw))
				continue;

			if (0) {
				printf("MPT-EX #%d: ", (packet - 1) * 5);
				dump_pagenum(p);
			}

			if (p.pgno < 0x100)
				break;
			else if (p.pgno > 0x8FF || p.subno < 1)
				continue;

			code = vt->page_info[p.pgno - 0x100].code;
			subc = vt->page_info[p.pgno - 0x100].subcode;

			if (code != VBI_NO_PAGE && code != VBI_UNKNOWN_PAGE
			    && (p.subno > subc /* evidence */
				/* || subc >= 0xFFFF unknown */
				|| subc >= 0xFFFE /* mpt > 9 */))
				vt->page_info[p.pgno - 0x100].subcode = p.subno;
		}

		break;
	}

	return TRUE;
}

/**
 * @internal
 * @param vbi Initialized vbi decoding context.
 * @param vtp Raw teletext page to be converted.
 * @param cached The raw page is already cached, update the cache.
 * @param new_function The page function to convert to.
 * 
 * Since MOT, MIP and X/28 are optional, the function of a system page
 * may not be clear until we format a LOP and find a link of certain type,
 * so this function converts a page "after the fact".
 * 
 * @return
 * Pointer to the converted page, either @a vtp or the cached copy.
 **/
vt_page *
vbi_convert_page(vbi_decoder *vbi, vt_page *vtp,
		 vbi_bool cached, page_function new_function)
{
	vt_page page;
	int i;

	if (vtp->function != PAGE_FUNCTION_UNKNOWN)
		return NULL;

	memcpy(&page, vtp, sizeof(*vtp)	- sizeof(vtp->data) + sizeof(vtp->data.unknown));

	switch (new_function) {
	case PAGE_FUNCTION_LOP:
		vtp->function = new_function;
		return vtp;

	case PAGE_FUNCTION_GPOP:
	case PAGE_FUNCTION_POP:
		memset(page.data.pop.pointer, 0xFF, sizeof(page.data.pop.pointer));
		memset(page.data.pop.triplet, 0xFF, sizeof(page.data.pop.triplet));

		for (i = 1; i <= 25; i++)
			if (vtp->lop_lines & (1 << i))
				if (!parse_pop(&page, vtp->data.unknown.raw[i], i))
					return FALSE;

		if (vtp->enh_lines)
			memcpy(&page.data.pop.triplet[23 * 13],	vtp->data.enh_lop.enh,
				16 * 13 * sizeof(vt_triplet));
		break;

	case PAGE_FUNCTION_GDRCS:
	case PAGE_FUNCTION_DRCS:
		memmove(page.data.drcs.raw, vtp->data.unknown.raw, sizeof(page.data.drcs.raw));
		memset(page.data.drcs.mode, 0, sizeof(page.data.drcs.mode));
		page.lop_lines = vtp->lop_lines;

		if (!convert_drcs(&page, vtp->data.unknown.raw[1]))
			return FALSE;

		break;

	case PAGE_FUNCTION_AIT:
		memset(page.data.ait, 0, sizeof(page.data.ait));

		for (i = 1; i <= 23; i++)
			if (vtp->lop_lines & (1 << i))
				if (!parse_ait(&page, vtp->data.unknown.raw[i], i))
					return FALSE;
		break;

	case PAGE_FUNCTION_MPT:
		for (i = 1; i <= 20; i++)
			if (vtp->lop_lines & (1 << i))
				if (!parse_mpt(&vbi->vt, vtp->data.unknown.raw[i], i))
					return FALSE;
		break;

	case PAGE_FUNCTION_MPT_EX:
		for (i = 1; i <= 20; i++)
			if (vtp->lop_lines & (1 << i))
				if (!parse_mpt_ex(&vbi->vt, vtp->data.unknown.raw[i], i))
					return FALSE;
		break;

	default:
		return NULL;
	}

	page.function = new_function;

	if (cached) {
		return vbi_cache_put(vbi, &page);
	} else {
		memcpy(vtp, &page, vtp_size(&page));
		return vtp;
	}
}

typedef enum {
	CNI_NONE,
	CNI_VPS,	/* VPS format */
	CNI_8301,	/* Teletext packet 8/30 format 1 */
	CNI_8302,	/* Teletext packet 8/30 format 2 */
	CNI_X26		/* Teletext packet X/26 local enhancement */
} vbi_cni_type;

static unsigned int
station_lookup(vbi_cni_type type, int cni, const char **country, const char **name)
{
	const struct vbi_cni_entry *p;

	if (!cni)
		return 0;

	switch (type) {
	case CNI_8301:
		for (p = vbi_cni_table; p->name; p++)
			if (p->cni1 == cni) {
				*country = vbi_country_names_en[p->country];
				*name = p->name;
				return p->id;
			}
		break;

	case CNI_8302:
		for (p = vbi_cni_table; p->name; p++)
			if (p->cni2 == cni) {
				*country = vbi_country_names_en[p->country];
				*name = p->name;
				return p->id;
			}

		cni &= 0x0FFF;

		/* fall through */

	case CNI_VPS:
		/* if (cni == 0x0DC3) in decoder
			cni = mark ? 0x0DC2 : 0x0DC1; */

		for (p = vbi_cni_table; p->name; p++)
			if (p->cni4 == cni) {
				*country = vbi_country_names_en[p->country];
				*name = p->name;
				return p->id;
			}
		break;

	case CNI_X26:
		for (p = vbi_cni_table; p->name; p++)
			if (p->cni3 == cni) {
				*country = vbi_country_names_en[p->country];
				*name = p->name;
				return p->id;
			}

		/* try code | 0x0080 & 0x0FFF -> VPS ? */

		break;

	default:
		break;
	}

	return 0;
}

static void
unknown_cni(vbi_decoder *vbi, const char *dl, int cni)
{
	/* if (cni == 0) */
		return;

	fprintf(stderr,
"This network broadcasts an unknown CNI of 0x%04x using a %s data line.\n"
"If you see this message always when switching to this channel please\n"
"report network name, country, CNI and data line at http://zapping.sf.net\n"
"for inclusion in the Country and Network Identifier table. Thank you.\n",
		cni, dl);
}

#define BSDATA_TEST 0 /* Broadcaster Service Data */

#if BSDATA_TEST

static const char *month_names[] = {
	"0?", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
	"Sep", "Oct", "Nov", "Dec", "13?", "14?", "15?"
};

static const char *pcs_names[] = {
	"unknown", "mono", "stereo", "bilingual"
};

#define PIL(day, mon, hour, min) \
	(((day) << 15) + ((mon) << 11) + ((hour) << 6) + ((min) << 0))

static void
dump_pil(int pil)
{
	int day, mon, hour, min;

	day = pil >> 15;
	mon = (pil >> 11) & 0xF;
	hour = (pil >> 6) & 0x1F;
	min = pil & 0x3F;

	if (pil == PIL(0, 15, 31, 63))
		printf("... PDC: Timer-control (no PDC)\n");
	else if (pil == PIL(0, 15, 30, 63))
		printf("... PDC: Recording inhibit/terminate\n");
	else if (pil == PIL(0, 15, 29, 63))
		printf("... PDC: Interruption\n");
	else if (pil == PIL(0, 15, 28, 63))
		printf("... PDC: Continue\n");
	else if (pil == PIL(31, 15, 31, 63))
		printf("... PDC: No time\n");
	else
		printf("... PDC: %05x, %2d %s %02d:%02d\n",
			pil, day, month_names[mon], hour, min);
}

static void
dump_pty(int pty)
{
	extern const char *ets_program_class[16];
	extern const char *ets_program_type[8][16];

	if (pty == 0xFF)
		printf("... prog. type: %02x unused", pty);
	else
		printf("... prog. type: %02x class %s", pty, ets_program_class[pty >> 4]);

	if (pty < 0x80) {
		if (ets_program_type[pty >> 4][pty & 0xF])
			printf(", type %s", ets_program_type[pty >> 4][pty & 0xF]);
		else
			printf(", type undefined");
	}

	putchar('\n');
}

#endif /* BSDATA_TEST */

/**
 * @internal
 * @param vbi Initialized vbi decoding context.
 * @param buf 13 bytes.
 * 
 * Decode a VPS datagram (13 bytes) according to
 * ETS 300 231 and update decoder state. This may
 * send a @a VBI_EVENT_NETWORK.
 */
void
vbi_decode_vps(vbi_decoder *vbi, uint8_t *buf)
{
	vbi_network *n = &vbi->network.ev.network;
	const char *country, *name;
	int cni;

	cni = + ((buf[10] & 3) << 10)
	      + ((buf[11] & 0xC0) << 2)
	      + ((buf[8] & 0xC0) << 0)
	      + (buf[11] & 0x3F);

	if (cni == 0x0DC3)
		cni = (buf[2] & 0x10) ? 0x0DC2 : 0x0DC1;

	if (cni != n->cni_vps) {
		n->cni_vps = cni;
		n->cycle = 1;
	} else if (n->cycle == 1) {
		unsigned int id = station_lookup(CNI_VPS, cni, &country, &name);

		if (!id) {
			n->name[0] = 0;
			unknown_cni(vbi, "VPS", cni);
		} else {
			strncpy(n->name, name, sizeof(n->name) - 1);
		}

		if (id != n->nuid) {
			if (n->nuid != 0)
				vbi_chsw_reset(vbi, id);

			n->nuid = id;

			vbi->network.type = VBI_EVENT_NETWORK;
			vbi_send_event(vbi, &vbi->network);
		}

		n->cycle = 2;
	}

	if (BSDATA_TEST && 0) {
		static char pr_label[20];
		static char label[20];
		static int l = 0;
		int cni, pcs, pty, pil;
		int c, j;

		printf("\nVPS:\n");

		c = vbi_bit_reverse[buf[1]];

		if ((int8_t) c < 0) {
			label[l] = 0;
			memcpy(pr_label, label, sizeof(pr_label));
			l = 0;
		}

		c &= 0x7F;

		label[l] = printable(c);

		l = (l + 1) % 16;

		printf(" 3-10: %02x %02x %02x %02x %02x %02x %02x %02x (\"%s\")\n",
			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], pr_label);

		cni = + ((buf[10] & 3) << 10)
		      + ((buf[11] & 0xC0) << 2)
		      + ((buf[8] & 0xC0) << 0)
		      + (buf[11] & 0x3F);

		if (cni)
			for (j = 0; vbi_cni_table[j].name; j++)
				if (vbi_cni_table[j].cni4 == cni) {
					printf(" Country: %s\n Station: %s%s\n",
						vbi_country_names_en[vbi_cni_table[j].country],
						vbi_cni_table[j].name,
						(cni == 0x0DC3) ? ((buf[2] & 0x10) ? " (ZDF)" : " (ARD)") : "");
					break;
				}

		pcs = buf[2] >> 6;
		pil = ((buf[8] & 0x3F) << 14) + (buf[9] << 6) + (buf[10] >> 2);
		pty = buf[12];

		/* if (!cni || !vbi_cni_table[j].name) */
			printf(" CNI: %04x\n", cni);
#if BSDATA_TEST
		printf(" Analog audio: %s\n", pcs_names[pcs]);

		dump_pil(pil);
		dump_pty(pty);
#endif
	}
}

static vbi_bool
parse_bsd(vbi_decoder *vbi, uint8_t *raw, int packet, int designation)
{
	vbi_network *n = &vbi->network.ev.network;
	int err, i;

	switch (packet) {
	case 26:
		/* TODO, iff */
		break;

	case 30:
		if (designation >= 4)
			break;

		if (designation <= 1) {
			const char *country, *name;
			int cni;
#if BSDATA_TEST
			printf("\nPacket 8/30/%d:\n", designation);
#endif
			cni = vbi_bit_reverse[raw[7]] * 256
				+ vbi_bit_reverse[raw[8]];

			if (cni != n->cni_8301) {
				n->cni_8301 = cni;
				n->cycle = 1;
			} else if (n->cycle == 1) {
				unsigned int id;

				id = station_lookup(CNI_8301, cni, &country, &name);

				if (!id) {
					n->name[0] = 0;
					unknown_cni(vbi, "8/30/1", cni);
				} else {
					strncpy(n->name, name, sizeof(n->name) - 1);
				}

				if (id != n->nuid) {
					if (n->nuid != 0)
						vbi_chsw_reset(vbi, id);

					n->nuid = id;

					vbi->network.type = VBI_EVENT_NETWORK;
					vbi_send_event(vbi, &vbi->network);
				}

				n->cycle = 2;
			}
#if BSDATA_TEST
			if (1) { /* country and network identifier */
				if (station_lookup(CNI_8301, cni, &country, &name))
					printf("... country: %s\n... station: %s\n", country, name);
				else
					printf("... unknown CNI %04x\n", cni);
			}

			if (1) { /* local time */
				int lto, mjd, utc_h, utc_m, utc_s;
				struct tm tm;
				time_t ti;

				lto = (raw[9] & 0x7F) >> 1;

				mjd = + ((raw[10] & 15) - 1) * 10000
				      + ((raw[11] >> 4) - 1) * 1000
				      + ((raw[11] & 15) - 1) * 100
				      + ((raw[12] >> 4) - 1) * 10
				      + ((raw[12] & 15) - 1);

			    	utc_h = ((raw[13] >> 4) - 1) * 10 + ((raw[13] & 15) - 1);
				utc_m = ((raw[14] >> 4) - 1) * 10 + ((raw[14] & 15) - 1);
				utc_s = ((raw[15] >> 4) - 1) * 10 + ((raw[15] & 15) - 1);

				ti = (mjd - 40587) * 86400 + 43200;
				localtime_r(&ti, &tm);

				printf("... local time: MJD %d %02d %s %04d, UTC %02d:%02d:%02d %c%02d%02d\n",
					mjd, tm.tm_mday, month_names[tm.tm_mon + 1], tm.tm_year + 1900,
					utc_h, utc_m, utc_s, (raw[9] & 0x80) ? '-' : '+', lto >> 1, (lto & 1) * 30);
			}
#endif /* BSDATA_TEST */

		} else /* if (designation <= 3) */ {
			int t, b[7];
			const char *country, *name;
			int cni;
#if BSDATA_TEST
			printf("\nPacket 8/30/%d:\n", designation);
#endif
			for (err = i = 0; i < 7; i++) {
				err |= t = vbi_hamm16(raw + i * 2 + 6);
				b[i] = vbi_bit_reverse[t];
			}

			if (err < 0)
				return FALSE;

			cni = + ((b[4] & 0x03) << 10)
			      + ((b[5] & 0xC0) << 2)
			      + (b[2] & 0xC0)
			      + (b[5] & 0x3F)
			      + ((b[1] & 0x0F) << 12);

			if (cni == 0x0DC3)
				cni = (b[2] & 0x10) ? 0x0DC2 : 0x0DC1;

			if (cni != n->cni_8302) {
				n->cni_8302 = cni;
				n->cycle = 1;
			} else if (n->cycle == 1) {
				unsigned int id;

				id = station_lookup(CNI_8302, cni, &country, &name);

				if (!id) {
					n->name[0] = 0;
					unknown_cni(vbi, "8/30/2", cni);
				} else {
					strncpy(n->name, name, sizeof(n->name) - 1);
				}

				if (id != n->nuid) {
					if (n->nuid != 0)
						vbi_chsw_reset(vbi, id);

					n->nuid = id;

					vbi->network.type = VBI_EVENT_NETWORK;
					vbi_send_event(vbi, &vbi->network);
				}

				n->cycle = 2;
			}

#if BSDATA_TEST
			if (1) { /* country and network identifier */
				const char *country, *name;

				if (station_lookup(CNI_8302, cni, &country, &name))
					printf("... country: %s\n... station: %s\n", country, name);
				else
					printf("... unknown CNI %04x\n", cni);
			}

			if (1) { /* PDC data */
				int lci, luf, prf, mi, pil;

				lci = (b[0] >> 2) & 3;
				luf = !!(b[0] & 2);
				prf = b[0] & 1;
				mi = !!(b[1] & 0x20);
				pil = ((b[2] & 0x3F) << 14) + (b[3] << 6) + (b[4] >> 2);

				printf("... label channel %d: update %d,"
				       " prepare to record %d, mode %d\n",
					lci, luf, prf, mi);
				dump_pil(pil);
			}

			if (1) {
				int pty, pcs;

				pcs = b[1] >> 6;
				pty = b[6];

				printf("... analog audio: %s\n", pcs_names[pcs]);
				dump_pty(pty);
			}
#endif /* BSDATA_TEST */

		}

#if BSDATA_TEST
		/*
		 *  "transmission status message, e.g. the programme title",
		 *  "default G0 set". XXX add to program_info event.
		 */
		if (1) { 
			printf("... status: \"");

			for (i = 20; i < 40; i++) {
				int c = vbi_parity(raw[i]);

				c = (c < 0) ? '?' : printable(c);
				putchar(c);
			}

			printf("\"\n");
		}
#endif
		return TRUE;
	}

	return TRUE;
}

#define FPC_BLOCK_SEPARATOR	0xC
#define FPC_FILLER_BYTE		0x3

static inline void
vbi_reset_page_clear(struct page_clear *pc)
{
	pc->ci = 256;
	pc->packet = 256;
	pc->num_packets = 0;
	pc->bi = 0;
	pc->left = 0;
	pc->pfc.application_id = -1;
}

static void
parse_page_clear(struct page_clear *pc, uint8_t *p, int packet)
{
	int bp, col;

	if ((pc->packet + 1) != packet || packet > pc->num_packets)
		goto desync;

	pc->packet = packet;

	if ((bp = vbi_hamm8(p[0]) * 3) < 0 || bp > 39)
		goto desync;

	for (col = 1; col < 40;) {
		int bs;

		if (pc->left > 0) {
			int size = MIN(pc->left, 40 - col);

			memcpy(pc->pfc.block + pc->bi, p + col, size);

			pc->bi += size;
			pc->left -= size;

			if (pc->left > 0)
				return; /* packet done */

			col += size;

			if (pc->pfc.application_id < 0) {
				int sh = vbi_hamm16(pc->pfc.block)
					+ vbi_hamm16(pc->pfc.block + 2) * 256;

				pc->pfc.application_id = sh & 0x1F;
				pc->pfc.block_size =
					pc->left = sh >> 5;
				pc->bi = 0;

				continue;
			} else {
				int i;
					
				fprintf(stderr, "pfc %d %d\n",
					pc->pfc.application_id,
					pc->pfc.block_size);

				for (i = 0; i < pc->pfc.block_size; i++) {
					fputc(printable(pc->pfc.block[i]), stderr);

					if ((i % 75) == 75)
						fputc('\n', stderr);
				}

				fputc('\n', stderr);
			}
		}

		if (col <= 1) {
			if (bp >= 39)
				return; /* no SH in this packet */
			col = bp + 2;
			bs = vbi_hamm8(p[col - 1]);
		} else
			while ((bs = vbi_hamm8(p[col++])) == FPC_FILLER_BYTE) {
				if (col >= 40)
					return; /* packet done */
			}

		if (bs != FPC_BLOCK_SEPARATOR)
			goto desync;

		pc->pfc.application_id = -1;
		pc->left = 4; /* sizeof structure header */
		pc->bi = 0;
	}

	return;

 desync:
	// fprintf(stderr, "FPC reset\n");
	vbi_reset_page_clear(pc);
}

static int
same_header(int cur_pgno, uint8_t *cur,
	    int ref_pgno, uint8_t *ref,
	    int *page_num_offsetp)
{
	uint8_t buf[3];
	int i, j = 32 - 3, err = 0, neq = 0;

	/* Assumes vbi_is_bcd(cur_pgno) */
	buf[2] = (cur_pgno & 15) + '0';
	buf[1] = ((cur_pgno >> 4) & 15) + '0';
	buf[0] = (cur_pgno >> 8) + '0';

	vbi_set_parity(buf, 3);

	for (i = 8; i < 32; cur++, ref++, i++) {
		/* Skip page number */
		if (i < j
		    && cur[0] == buf[0]
		    && cur[1] == buf[1]
		    && cur[2] == buf[2]) {
			j = i; /* here, once */
			i += 3;
			cur += 3;
			ref += 3;
			continue;
		}

		err |= vbi_parity(*cur);
		err |= vbi_parity(*ref);

		neq |= *cur - *ref;
	}

	if (err < 0 || j >= 32 - 3) /* parity error, rare */
		return -2; /* inconclusive, useless */

	*page_num_offsetp = j;

	if (!neq)
		return TRUE;

	/* Test false negative due to date transition */

	if (((ref[32] * 256 + ref[33]) & 0x7F7F) == 0x3233
	    && ((cur[32] * 256 + cur[33]) & 0x7F7F) == 0x3030) {
		return -1; /* inconclusive */
	}

	/*
	 *  The problem here is that individual pages or
	 *  magazines from the same network can still differ.
	 */
	return FALSE;
}

static inline vbi_bool
same_clock(uint8_t *cur, uint8_t *ref)
{
	int i;

	for (i = 32; i < 40; cur++, ref++, i++)
	       	if (*cur != *ref
		    && (vbi_parity(*cur) | vbi_parity(*ref)) >= 0)
			return FALSE;
	return TRUE;
}

static inline vbi_bool
store_lop(vbi_decoder *vbi, vt_page *vtp)
{
	struct page_info *pi;
	vbi_event event;

	event.type = VBI_EVENT_TTX_PAGE;

	event.ev.ttx_page.pgno = vtp->pgno;
	event.ev.ttx_page.subno = vtp->subno;

	event.ev.ttx_page.roll_header =
		(((vtp->flags & (  C5_NEWSFLASH
				 | C6_SUBTITLE 
				 | C7_SUPPRESS_HEADER
				 | C9_INTERRUPTED
			         | C10_INHIBIT_DISPLAY)) == 0)
		 && (vtp->pgno <= 0x199
		     || (vtp->flags & C11_MAGAZINE_SERIAL))
		 && vbi_is_bcd(vtp->pgno) /* no hex numbers */);

	event.ev.ttx_page.header_update = FALSE;
	event.ev.ttx_page.raw_header = NULL;
	event.ev.ttx_page.pn_offset = -1;

	/*
	 *  We're not always notified about a channel switch,
	 *  this code prevents a terrible mess in the cache.
	 *
	 *  The roll_header thing shall reduce false negatives,
	 *  slows down detection of some stations, but does help.
	 *  A little. Maybe this should be optional.
	 */
	if (event.ev.ttx_page.roll_header) {
		int r;

		if (vbi->vt.header_page.pgno == 0) {
			/* First page after channel switch */
			r = same_header(vtp->pgno, vtp->data.lop.raw[0] + 8,
					vtp->pgno, vtp->data.lop.raw[0] + 8,
					&event.ev.ttx_page.pn_offset);
			event.ev.ttx_page.header_update = TRUE;
			event.ev.ttx_page.clock_update = TRUE;
		} else {
			r = same_header(vtp->pgno, vtp->data.lop.raw[0] + 8,
					vbi->vt.header_page.pgno, vbi->vt.header + 8,
					&event.ev.ttx_page.pn_offset);
			event.ev.ttx_page.clock_update =
				!same_clock(vtp->data.lop.raw[0], vbi->vt.header);
		}

		switch (r) {
		case TRUE:
			// fprintf(stderr, "+");

			pthread_mutex_lock(&vbi->chswcd_mutex);
			vbi->chswcd = 0;
			pthread_mutex_unlock(&vbi->chswcd_mutex);

			vbi->vt.header_page.pgno = vtp->pgno;
			memcpy(vbi->vt.header + 8,
			       vtp->data.lop.raw[0] + 8, 32);

			event.ev.ttx_page.raw_header = vbi->vt.header;

			break;

		case FALSE:
			/*
			 *  What can I do when every magazin has its own
			 *  header? Ouch. Let's hope p100 repeats frequently.
			 */
			if (((vtp->pgno ^ vbi->vt.header_page.pgno) & 0xF00) == 0) {
			     /* pthread_mutex_lock(&vbi->chswcd_mutex);
				if (vbi->chswcd == 0)
					vbi->chswcd = 40;
				pthread_mutex_unlock(&vbi->chswcd_mutex); */

				vbi_chsw_reset(vbi, 0);
				return TRUE;
			}

			/* fall through */

		default: /* inconclusive */
			pthread_mutex_lock(&vbi->chswcd_mutex);

			if (vbi->chswcd > 0) {
				pthread_mutex_unlock(&vbi->chswcd_mutex);
				return TRUE;
			}

			pthread_mutex_unlock(&vbi->chswcd_mutex);

			if (r == -1) {
				vbi->vt.header_page.pgno = vtp->pgno;
				memcpy(vbi->vt.header + 8,
				       vtp->data.lop.raw[0] + 8, 32);

				event.ev.ttx_page.raw_header = vbi->vt.header;

				// fprintf(stderr, "/");
			} else /* broken header */ {
				event.ev.ttx_page.roll_header = FALSE;
				event.ev.ttx_page.clock_update = FALSE;

				// fprintf(stderr, "X");
			}

			break;
		}

		if (0) {
			int i;

			for (i = 0; i < 40; i++)
				putchar(printable(vtp->data.unknown.raw[0][i]));
			putchar('\r');
			fflush(stdout);
		}
	} else {
		// fprintf(stderr, "-");
	}

	/*
	 *  Collect information about those pages
	 *  not listed in MIP etc.
	 */
	pi = vbi->vt.page_info + vtp->pgno - 0x100;

	if (pi->code == VBI_SUBTITLE_PAGE) {
		if (pi->language == 0xFF)
			pi->language = page_language(&vbi->vt, vtp, 0, 0);
	} else if (pi->code == VBI_NO_PAGE || pi->code == VBI_UNKNOWN_PAGE)
		pi->code = VBI_NORMAL_PAGE;

	if (pi->subcode >= 0xFFFE || vtp->subno > pi->subcode)
		pi->subcode = vtp->subno;

	/*
	 *  Store the page and send event.
	 */
	if (vbi_cache_put(vbi, vtp))
		vbi_send_event(vbi, &event);

	return TRUE;
}

#define TTX_EVENTS (VBI_EVENT_TTX_PAGE)
#define BSDATA_EVENTS (VBI_EVENT_NETWORK)

/*
 *  Teletext packet 27, page linking
 */
static inline vbi_bool
parse_27(vbi_decoder *vbi, uint8_t *p,
	 vt_page *cvtp, int mag0)
{
	int designation, control;
	int i;

	if (cvtp->function == PAGE_FUNCTION_DISCARD)
		return TRUE;

	if ((designation = vbi_hamm8(*p)) < 0)
		return FALSE;

//	printf("Packet X/27/%d page %x\n", designation, cvtp->pgno);

	switch (designation) {
	case 0:
		if ((control = vbi_hamm8(p[37])) < 0)
			return FALSE;

		/* printf("%x.%x X/27/%d %02x\n",
		       cvtp->pgno, cvtp->subno, designation, control); */
#if 0
/*
 *  CRC cannot be trusted, some stations transmit rubbish.
 *  Link Control Byte bits 1 ... 3 cannot be trusted, ETS 300 706 is
 *  inconclusive and not all stations follow the suggestions in ETR 287.
 */
		crc = p[38] + p[39] * 256;
		/* printf("CRC: %04x\n", crc); */

		if ((control & 7) == 0)
			return FALSE;
#endif
		cvtp->data.unknown.flof = control >> 3; /* display row 24 */

		/* fall through */
	case 1:
	case 2:
	case 3:
		for (p++, i = 0; i <= 5; p += 6, i++) {
			if (!hamm8_page_number(cvtp->data.unknown.link
					       + designation * 6 + i, p, mag0))
				; /* return TRUE; */

// printf("X/27/%d link[%d] page %03x/%03x\n", designation, i,
//	cvtp->data.unknown.link[designation * 6 + i].pgno, cvtp->data.unknown.link[designation * 6 + i].subno);
		}

		break;

	case 4:
	case 5:
		for (p++, i = 0; i <= 5; p += 6, i++) {
			int t1, t2;

			t1 = vbi_hamm24(p + 0);
			t2 = vbi_hamm24(p + 3);

			if ((t1 | t2) < 0)
				return FALSE;

			cvtp->data.unknown.link[designation * 6 + i].type = t1 & 3;
			cvtp->data.unknown.link[designation * 6 + i].pgno =
				((((t1 >> 12) & 0x7) ^ mag0) ? : 8) * 256
				+ ((t1 >> 11) & 0x0F0) + ((t1 >> 7) & 0x00F);
			cvtp->data.unknown.link[designation * 6 + i].subno =
				(t2 >> 3) & 0xFFFF;
if(0)
 printf("X/27/%d link[%d] type %d page %03x subno %04x\n", designation, i,
	cvtp->data.unknown.link[designation * 6 + i].type,
	cvtp->data.unknown.link[designation * 6 + i].pgno,
	cvtp->data.unknown.link[designation * 6 + i].subno);
		}

		break;
	}

	return TRUE;
}

/*
 *  Teletext packets 28 and 29, Level 2.5/3.5 enhancement
 */
static inline vbi_bool
parse_28_29(vbi_decoder *vbi, uint8_t *p,
	    vt_page *cvtp, int mag8, int packet)
{
	int designation, function;
	int triplets[13], *triplet = triplets, buf = 0, left = 0;
	vt_extension *ext;
	int i, j, err = 0;

	static int
	bits(int count)
	{
		int r, n;

		r = buf;

		if ((n = count - left) > 0) {
			r |= (buf = *triplet++) << left;
			left = 18;
		} else
			n = count;

		buf >>= n;
		left -= n;

		return r & ((1UL << count) - 1);
	}

	if ((designation = vbi_hamm8(*p)) < 0)
		return FALSE;

	if (0)
		fprintf(stderr, "Packet %d/%d/%d page %x\n",
			mag8, packet, designation, cvtp->pgno);

	for (p++, i = 0; i < 13; p += 3, i++)
		err |= triplet[i] = vbi_hamm24(p);

	switch (designation) {
	case 0: /* X/28/0, M/29/0 Level 2.5 */
	case 4: /* X/28/4, M/29/4 Level 3.5 */
		if (err < 0)
			return FALSE;

		function = bits(4);
		bits(3); /* page coding ignored */

//		printf("... function %d\n", function);

		/*
		 *  ZDF and BR3 transmit GPOP 1EE/.. with 1/28/0 function
		 *  0 = PAGE_FUNCTION_LOP, should be PAGE_FUNCTION_GPOP.
		 *  Makes no sense to me. Update: also encountered pages
		 *  mFE and mFF with function = 0. Strange. 
		 */
		if (function != PAGE_FUNCTION_LOP && packet == 28) {
			if (cvtp->function != PAGE_FUNCTION_UNKNOWN
			    && cvtp->function != function)
				return FALSE; /* XXX discard rpage? */

// XXX rethink		cvtp->function = function;
		}

		if (function != PAGE_FUNCTION_LOP)
			return FALSE;

		/* XXX X/28/0 Format 2, distinguish how? */

		ext = &vbi->vt.magazine[mag8].extension;

		if (packet == 28) {
			if (!cvtp->data.ext_lop.ext.designations) {
				cvtp->data.ext_lop.ext = *ext;
				cvtp->data.ext_lop.ext.designations <<= 16;
				cvtp->data.lop.ext = TRUE;
			}

			ext = &cvtp->data.ext_lop.ext;
		}

		if (designation == 4 && (ext->designations & (1 << 0)))
			bits(14 + 2 + 1 + 4);
		else {
			ext->char_set[0] = bits(7);
			ext->char_set[1] = bits(7);

			ext->fallback.left_side_panel = bits(1);
			ext->fallback.right_side_panel = bits(1);

			bits(1); /* panel status: level 2.5/3.5 */

			ext->fallback.left_panel_columns =
				vbi_bit_reverse[bits(4)] >> 4;

			if (ext->fallback.left_side_panel
			    | ext->fallback.right_side_panel)
				ext->fallback.left_panel_columns =
					ext->fallback.left_panel_columns ? : 16;
		}

		j = (designation == 4) ? 16 : 32;

		for (i = j - 16; i < j; i++) {
			vbi_rgba col = bits(12);

			if (i == 8) /* transparent */
				continue;

			col = VBI_RGBA((col >> 0) & 15,
				       (col >> 4) & 15,
				       (col >> 8) & 15);

			ext->color_map[i] = col | (col << 4);
		}

		if (designation == 4 && (ext->designations & (1 << 0)))
			bits(10 + 1 + 3);
		else {
			ext->def_screen_color = bits(5);
			ext->def_row_color = bits(5);

			ext->fallback.black_bg_substitution = bits(1);

			i = bits(3); /* color table remapping */

			ext->foreground_clut = "\00\00\00\10\10\20\20\20"[i];
			ext->background_clut = "\00\10\20\10\20\10\20\30"[i];
		}

		ext->designations |= 1 << designation;

		if (packet == 29) {
			if (0 && designation == 4)
				ext->designations &= ~(1 << 0);

			/*
			    XXX update
			    inherited_mag_desig =
			       page->extension.designations >> 16;
			    new_mag_desig = 1 << designation;
			    page_desig = page->extension.designations;
			    if (((inherited_mag_desig | page_desig)
			       & new_mag_desig) == 0)
			    shortcut: AND of (inherited_mag_desig | page_desig)
			      of all pages with extensions, no updates required
			      in round 2++
			    other option, all M/29/x should have been received
			      within the maximum repetition interval of 20 s.
			*/
		}

		return FALSE;

	case 1: /* X/28/1, M/29/1 Level 3.5 DRCS CLUT */
		ext = &vbi->vt.magazine[mag8].extension;

		if (packet == 28) {
			if (!cvtp->data.ext_lop.ext.designations) {
				cvtp->data.ext_lop.ext = *ext;
				cvtp->data.ext_lop.ext.designations <<= 16;
				cvtp->data.lop.ext = TRUE;
			}

			ext = &cvtp->data.ext_lop.ext;
			/* XXX TODO - lop? */
		}

		triplet++;

		for (i = 0; i < 8; i++)
			ext->drcs_clut[i + 2] = vbi_bit_reverse[bits(5)] >> 3;

		for (i = 0; i < 32; i++)
			ext->drcs_clut[i + 10] = vbi_bit_reverse[bits(5)] >> 3;

		ext->designations |= 1 << 1;

		if (0)
			dump_extension(ext);

		return FALSE;

	case 3: /* X/28/3 Level 2.5, 3.5 DRCS download page */
		if (packet == 29)
			break; /* M/29/3 undefined */

		if (err < 0)
			return FALSE;

		function = bits(4);
		bits(3); /* page coding ignored */

		if (function != PAGE_FUNCTION_GDRCS
		    || function != PAGE_FUNCTION_DRCS)
			return FALSE;

		if (cvtp->function == PAGE_FUNCTION_UNKNOWN) {
			memmove(cvtp->data.drcs.raw,
				cvtp->data.unknown.raw,
				sizeof(cvtp->data.drcs.raw));
			cvtp->function = function;
		} else if (cvtp->function != function) {
			cvtp->function = PAGE_FUNCTION_DISCARD;
			return 0;
		}

		bits(11);

		for (i = 0; i < 48; i++)
			cvtp->data.drcs.mode[i] = bits(4);

	default: /* ? */
		break;
	}

	return TRUE;
}

/*
 *  Teletext packet 8/30, broadcast service data
 */
static inline vbi_bool
parse_8_30(vbi_decoder *vbi, uint8_t *p, int packet)
{
	int designation;

	if ((designation = vbi_hamm8(*p)) < 0)
		return FALSE;

//	printf("Packet 8/30/%d\n", designation);

	if (designation > 4)
		return TRUE; /* ignored */

	if (vbi->event_mask & TTX_EVENTS) {
		if (!hamm8_page_number(&vbi->vt.initial_page, p + 1, 0))
			return FALSE;

		if ((vbi->vt.initial_page.pgno & 0xFF) == 0xFF) {
			vbi->vt.initial_page.pgno = 0x100;
			vbi->vt.initial_page.subno = VBI_ANY_SUBNO;
		}
	}

	if (vbi->event_mask & BSDATA_EVENTS)
		return parse_bsd(vbi, p, packet, designation);

	return TRUE;
}

/**
 * @internal
 * @param vbi Initialized vbi decoding context.
 * @param p Packet data.
 * 
 * Parse a teletext packet (42 bytes) and update the decoder
 * state accordingly. This function may send events.
 * 
 * Return value:
 * FALSE if the packet contained incorrectable errors. 
 */
vbi_bool
vbi_decode_teletext(vbi_decoder *vbi, uint8_t *p)
{
	vt_page *cvtp;
	struct raw_page *rvtp;
	int pmag, mag0, mag8, packet;
	vt_magazine *mag;

	if ((pmag = vbi_hamm16(p)) < 0)
		return FALSE;

	mag0 = pmag & 7;
	mag8 = mag0 ? : 8;
	packet = pmag >> 3;

	if (packet < 30
	    && !(vbi->event_mask & TTX_EVENTS))
		return TRUE;

	mag = vbi->vt.magazine + mag8;
	rvtp = vbi->vt.raw_page + mag0;
	cvtp = rvtp->page;

	p += 2;

	if (0) {
		unsigned int i;

		fprintf(stderr, "packet 0x%x %d >", mag8 * 0x100, packet);
		for (i = 0; i < 40; i++)
			fputc(printable(p[i]), stderr);
		fprintf(stderr, "<\n");
	}

	switch (packet) {
	case 0:
	{
		int pgno, page, subpage, flags;
		struct raw_page *curr;
		vt_page *vtp;
		int i;

		if ((page = vbi_hamm16(p)) < 0) {
			vbi_teletext_desync(vbi);
//			printf("Hamming error in packet 0 page number\n");
			return FALSE;
		}

		pgno = mag8 * 256 + page;

		/*
		 *  Store page terminated by new header.
		 */
		while ((curr = vbi->vt.current)) {
			vtp = curr->page;

			if (vtp->flags & C11_MAGAZINE_SERIAL) {
				if (vtp->pgno == pgno)
					break;
			} else {
				curr = rvtp;
				vtp = curr->page;

				if ((vtp->pgno & 0xFF) == page)
					break;
			}

			switch (vtp->function) {
			case PAGE_FUNCTION_DISCARD:
			case PAGE_FUNCTION_EPG:
				break;

			case PAGE_FUNCTION_LOP:
				if (!store_lop(vbi, vtp))
					return FALSE;
				break;

			case PAGE_FUNCTION_DRCS:
			case PAGE_FUNCTION_GDRCS:
				if (convert_drcs(vtp, vtp->data.drcs.raw[1]))
					vbi_cache_put(vbi, vtp);
				break;

			case PAGE_FUNCTION_MIP:
				parse_mip(vbi, vtp);
				break;

			case PAGE_FUNCTION_TRIGGER:
				eacem_trigger(vbi, vtp);
				break;

			default:
				vbi_cache_put(vbi, vtp);
				break;
			}

			vtp->function = PAGE_FUNCTION_DISCARD;
			break;
		}

		/*
		 *  Prepare for new page.
		 */

		cvtp->pgno = pgno;
		vbi->vt.current = rvtp;

		subpage = vbi_hamm16(p + 2) + vbi_hamm16(p + 4) * 256;
		flags = vbi_hamm16(p + 6);

		if (page == 0xFF || (subpage | flags) < 0) {
			cvtp->function = PAGE_FUNCTION_DISCARD;
			return FALSE;
		}

		cvtp->subno = subpage & 0x3F7F;
		cvtp->national = vbi_bit_reverse[flags] & 7;
		cvtp->flags = (flags << 16) + subpage;

		if (0 && ((page & 15) > 9 || page > 0x99))
			printf("data page %03x/%04x n%d\n",
			       cvtp->pgno, cvtp->subno, cvtp->national);

		if (1
		    && pgno != 0x1E7
		    && !(cvtp->flags & C4_ERASE_PAGE)
		    && (vtp = vbi_cache_get(vbi, cvtp->pgno, cvtp->subno, -1)))
		{
			memset(&cvtp->data, 0, sizeof(cvtp->data));
			memcpy(&cvtp->data, &vtp->data,
			       vtp_size(vtp) - sizeof(*vtp) + sizeof(vtp->data));

			/* XXX write cache directly | erc?*/
			/* XXX data page update */

			cvtp->function = vtp->function;

			switch (cvtp->function) {
			case PAGE_FUNCTION_UNKNOWN:
			case PAGE_FUNCTION_LOP:
				memcpy(cvtp->data.unknown.raw[0], p, 40);

			default:
				break;
			}

			cvtp->lop_lines = vtp->lop_lines;
			cvtp->enh_lines = vtp->enh_lines;
		} else {
			struct page_info *pi = vbi->vt.page_info + cvtp->pgno - 0x100;

			cvtp->flags |= C4_ERASE_PAGE;

			if (0)
				printf("rebuilding %3x/%04x from scratch\n", cvtp->pgno, cvtp->subno);

			if (cvtp->pgno == 0x1F0) {
				cvtp->function = PAGE_FUNCTION_BTT;
				pi->code = VBI_TOP_PAGE;
			} else if (cvtp->pgno == 0x1E7) {
				cvtp->function = PAGE_FUNCTION_TRIGGER;
				pi->code = VBI_DISP_SYSTEM_PAGE;
				pi->subcode = 0;
				memset(cvtp->data.unknown.raw[0], 0x20, sizeof(cvtp->data.unknown.raw));
				memset(cvtp->data.enh_lop.enh, 0xFF, sizeof(cvtp->data.enh_lop.enh));
				cvtp->data.unknown.ext = FALSE;
			} else if (page == 0xFD) {
				cvtp->function = PAGE_FUNCTION_MIP;
				pi->code = VBI_SYSTEM_PAGE;
			} else if (page == 0xFE) {
				cvtp->function = PAGE_FUNCTION_MOT;
				pi->code = VBI_SYSTEM_PAGE;
			} else if (FPC && pi->code == VBI_EPG_DATA) {
				int stream = (cvtp->subno >> 8) & 15;

				if (stream >= 2) {
					cvtp->function = PAGE_FUNCTION_DISCARD;
					// fprintf(stderr, "Discard FPC %d\n", stream);
				} else {
					struct page_clear *pc = vbi->epg_pc + stream;
					int ci = cvtp->subno & 15;

					cvtp->function = PAGE_FUNCTION_EPG;
					pc->pfc.pgno = cvtp->pgno;

					if (((pc->ci + 1) & 15) != ci)
						vbi_reset_page_clear(pc);

					pc->ci = ci;
					pc->packet = 0;
					pc->num_packets = ((cvtp->subno >> 4) & 7)
						+ ((cvtp->subno >> 9) & 0x18);
				}
			} else {
				cvtp->function = PAGE_FUNCTION_UNKNOWN;

				memcpy(cvtp->data.unknown.raw[0] + 0, p, 40);
				memset(cvtp->data.unknown.raw[0] + 40, 0x20, sizeof(cvtp->data.unknown.raw) - 40);
				memset(cvtp->data.unknown.link, 0xFF, sizeof(cvtp->data.unknown.link));
				memset(cvtp->data.enh_lop.enh, 0xFF, sizeof(cvtp->data.enh_lop.enh));

				cvtp->data.unknown.flof = FALSE;
				cvtp->data.unknown.ext = FALSE;
			}

			cvtp->lop_lines = 1;
			cvtp->enh_lines = 0;
		}

		if (cvtp->function == PAGE_FUNCTION_UNKNOWN) {
			page_function function = PAGE_FUNCTION_UNKNOWN;

			switch (vbi->vt.page_info[cvtp->pgno - 0x100].code) {
			case 0x01 ... 0x51:
			case 0x70 ... 0x7F:
			case 0x81 ... 0xD1:
			case 0xF4 ... 0xF7:
			case VBI_TOP_BLOCK:
			case VBI_TOP_GROUP:
				function = PAGE_FUNCTION_LOP;
				break;

			case VBI_SYSTEM_PAGE:	/* no MOT or MIP?? */
				/* remains function = PAGE_FUNCTION_UNKNOWN; */
				break;

			case VBI_TOP_PAGE:
				for (i = 0; i < 8; i++)
					if (cvtp->pgno == vbi->vt.btt_link[i].pgno)
						break;
				if (i < 8) {
					switch (vbi->vt.btt_link[i].type) {
					case 1:
						function = PAGE_FUNCTION_MPT;
						break;
					case 2:
						function = PAGE_FUNCTION_AIT;
						break;
					case 3:
						function = PAGE_FUNCTION_MPT_EX;
						break;
					default:
						if (0)
							printf("page is TOP, link %d, unknown type %d\n",
								i, vbi->vt.btt_link[i].type);
					}
				} else if (0)
					printf("page claims to be TOP, link not found\n");

				break;

			case 0xE5:
			case 0xE8 ... 0xEB:
				function = PAGE_FUNCTION_DRCS;
				break;

			case 0xE6:
			case 0xEC ... 0xEF:
				function = PAGE_FUNCTION_POP;
				break;

			case VBI_TRIGGER_DATA:
				function = PAGE_FUNCTION_TRIGGER;
				break;

			case VBI_EPG_DATA:	/* EPG/NexTView transport layer */
				if (FPC) {
					function = PAGE_FUNCTION_EPG;
					break;
				}

				/* fall through */

			case 0x52 ... 0x6F:	/* reserved */
			case VBI_ACI:		/* ACI page */
			case VBI_NOT_PUBLIC:
			case 0xD2 ... 0xDF:	/* reserved */
			case 0xE0 ... 0xE2:	/* data broadcasting */
			case 0xE4:		/* data broadcasting */
			case 0xF0 ... 0xF3:	/* broadcaster system page */
				function = PAGE_FUNCTION_DISCARD;
				break;

			default:
				if (page <= 0x99 && (page & 15) <= 9)
					function = PAGE_FUNCTION_LOP;
				/* else remains
					function = PAGE_FUNCTION_UNKNOWN; */
			}

			if (function != PAGE_FUNCTION_UNKNOWN) {
				vbi_convert_page(vbi, cvtp, FALSE, function);
			}
		}
//XXX?
		cvtp->data.ext_lop.ext.designations = 0;
		rvtp->num_triplets = 0;

		return TRUE;
	}

	case 1 ... 25:
	{
		int n;
		int i;

		switch (cvtp->function) {
		case PAGE_FUNCTION_DISCARD:
			return TRUE;

		case PAGE_FUNCTION_MOT:
			if (!parse_mot(vbi->vt.magazine + mag8, p, packet))
				return FALSE;
			break;

		case PAGE_FUNCTION_GPOP:
		case PAGE_FUNCTION_POP:
			if (!parse_pop(cvtp, p, packet))
				return FALSE;
			break;

		case PAGE_FUNCTION_GDRCS:
		case PAGE_FUNCTION_DRCS:
			memcpy(cvtp->data.drcs.raw[packet], p, 40);
			break;

		case PAGE_FUNCTION_BTT:
			if (!parse_btt(vbi, p, packet))
				return FALSE;
			break;

		case PAGE_FUNCTION_AIT:
			if (!(parse_ait(cvtp, p, packet)))
				return FALSE;
			break;

		case PAGE_FUNCTION_MPT:
			if (!(parse_mpt(&vbi->vt, p, packet)))
				return FALSE;
			break;

		case PAGE_FUNCTION_MPT_EX:
			if (!(parse_mpt_ex(&vbi->vt, p, packet)))
				return FALSE;
			break;

		case PAGE_FUNCTION_EPG:
			parse_page_clear(vbi->epg_pc + ((cvtp->subno >> 8) & 1), p, packet);
			break;

		case PAGE_FUNCTION_LOP:
		case PAGE_FUNCTION_TRIGGER:
			for (n = i = 0; i < 40; i++)
				n |= vbi_parity(p[i]);
			if (n < 0)
				return FALSE;

			/* fall through */

		case PAGE_FUNCTION_MIP:
		default:
			memcpy(cvtp->data.unknown.raw[packet], p, 40);
			break;
		}

		cvtp->lop_lines |= 1 << packet;

		break;
	}

	case 26:
	{
		int designation;
		vt_triplet triplet;
		int i;

		/*
		 *  Page enhancement packet
		 */

		switch (cvtp->function) {
		case PAGE_FUNCTION_DISCARD:
			return TRUE;

		case PAGE_FUNCTION_GPOP:
		case PAGE_FUNCTION_POP:
			return parse_pop(cvtp, p, packet);

		case PAGE_FUNCTION_GDRCS:
		case PAGE_FUNCTION_DRCS:
		case PAGE_FUNCTION_BTT:
		case PAGE_FUNCTION_AIT:
		case PAGE_FUNCTION_MPT:
		case PAGE_FUNCTION_MPT_EX:
			/* X/26 ? */
			vbi_teletext_desync(vbi);
			return TRUE;

		case PAGE_FUNCTION_TRIGGER:
		default:
			break;
		}

		if ((designation = vbi_hamm8(*p)) < 0)
			return FALSE;

		if (rvtp->num_triplets >= 16 * 13
		    || rvtp->num_triplets != designation * 13) {
			rvtp->num_triplets = -1;
			return FALSE;
		}

		for (p++, i = 0; i < 13; p += 3, i++) {
			int t = vbi_hamm24(p);

			if (t < 0)
				break; /* XXX */

			triplet.address = t & 0x3F;
			triplet.mode = (t >> 6) & 0x1F;
			triplet.data = t >> 11;

			cvtp->data.enh_lop.enh[rvtp->num_triplets++] = triplet;
		}

		cvtp->enh_lines |= 1 << designation;

		break;
	}

	case 27:
		if (!parse_27(vbi, p, cvtp, mag0))
			return FALSE;
		break;

	case 28:
		if (cvtp->function == PAGE_FUNCTION_DISCARD)
			break;

		/* fall through */
	case 29:
		if (!parse_28_29(vbi, p, cvtp, mag8, packet))
			return FALSE;
		break;

	case 30:
	case 31:
		/*
		 *  IDL packet (ETS 300 708)
		 */
		switch (/* Channel */ pmag & 15) {
		case 0: /* Packet 8/30 (ETS 300 706) */
			if (!parse_8_30(vbi, p, packet))
				return FALSE;
			break;

		default:
#if libzvbi_IDL_ALERT /* no 6.8 ??? */
			fprintf(stderr, "IDL: %d\n", pmag & 0x0F);
#endif
			break;
		}

		break;
	}

	return TRUE;
}

/*
 *  ETS 300 706 Table 30: Colour Map
 */
static const vbi_rgba
default_color_map[40] = {
	VBI_RGBA(0x00, 0x00, 0x00), VBI_RGBA(0xFF, 0x00, 0x00),
	VBI_RGBA(0x00, 0xFF, 0x00), VBI_RGBA(0xFF, 0xFF, 0x00),
	VBI_RGBA(0x00, 0x00, 0xFF), VBI_RGBA(0xFF, 0x00, 0xFF),
	VBI_RGBA(0x00, 0xFF, 0xFF), VBI_RGBA(0xFF, 0xFF, 0xFF),
	VBI_RGBA(0x00, 0x00, 0x00), VBI_RGBA(0x77, 0x00, 0x00),
	VBI_RGBA(0x00, 0x77, 0x00), VBI_RGBA(0x77, 0x77, 0x00),
	VBI_RGBA(0x00, 0x00, 0x77), VBI_RGBA(0x77, 0x00, 0x77),
	VBI_RGBA(0x00, 0x77, 0x77), VBI_RGBA(0x77, 0x77, 0x77),
	VBI_RGBA(0xFF, 0x00, 0x55), VBI_RGBA(0xFF, 0x77, 0x00),
	VBI_RGBA(0x00, 0xFF, 0x77), VBI_RGBA(0xFF, 0xFF, 0xBB),
	VBI_RGBA(0x00, 0xCC, 0xAA), VBI_RGBA(0x55, 0x00, 0x00),
	VBI_RGBA(0x66, 0x55, 0x22), VBI_RGBA(0xCC, 0x77, 0x77),
	VBI_RGBA(0x33, 0x33, 0x33), VBI_RGBA(0xFF, 0x77, 0x77),
	VBI_RGBA(0x77, 0xFF, 0x77), VBI_RGBA(0xFF, 0xFF, 0x77),
	VBI_RGBA(0x77, 0x77, 0xFF), VBI_RGBA(0xFF, 0x77, 0xFF),
	VBI_RGBA(0x77, 0xFF, 0xFF), VBI_RGBA(0xDD, 0xDD, 0xDD),

	/* Private colors */

	VBI_RGBA(0x00, 0x00, 0x00), VBI_RGBA(0xFF, 0xAA, 0x99),
	VBI_RGBA(0x44, 0xEE, 0x00), VBI_RGBA(0xFF, 0xDD, 0x00),
	VBI_RGBA(0xFF, 0xAA, 0x99), VBI_RGBA(0xFF, 0x00, 0xFF),
	VBI_RGBA(0x00, 0xFF, 0xFF), VBI_RGBA(0xEE, 0xEE, 0xEE)
};

/**
 * @param vbi Initialized vbi decoding context.
 * @param default_region A value between 0 ... 80, index into
 *   the Teletext character set table according to ETS 300 706,
 *   Section 15 (or libzvbi source file lang.c). The three last
 *   significant bits will be replaced.
 *
 * The original Teletext specification distinguished between
 * eight national character sets. When more countries started
 * to broadcast Teletext the three bit character set id was
 * locally redefined and later extended to seven bits grouping
 * the regional variants. Since some stations still transmit
 * only the legacy three bit id and we don't ship regional variants
 * of this decoder as TV manufacturers do, this function can be used to
 * set a default for the extended bits. The "factory default" is 16.
 */
void
vbi_teletext_set_default_region(vbi_decoder *vbi, int default_region)
{
	int i;

	if (default_region < 0 || default_region > 87)
		return;

	vbi->vt.region = default_region;

	for (i = 0; i < 9; i++) {
		vt_extension *ext = &vbi->vt.magazine[i].extension;

		ext->char_set[0] =
		ext->char_set[1] =
			default_region;
	}
}

/**
 * @param vbi Initialized vbi decoding context.
 * @param level 
 * 
 * @deprecated
 * This became a parameter of vbi_fetch_vt_page().
 */
void
vbi_teletext_set_level(vbi_decoder *vbi, int level)
{
	if (level < VBI_WST_LEVEL_1)
		level = VBI_WST_LEVEL_1;
	else if (level > VBI_WST_LEVEL_3p5)
		level = VBI_WST_LEVEL_3p5;

	vbi->vt.max_level = level;
}

/**
 * @internal
 * @param vbi Initialized vbi decoding context.
 * 
 * This function must be called after desynchronisation
 * has been detected (i. e. vbi data has been lost)
 * to reset the Teletext decoder.
 */
void
vbi_teletext_desync(vbi_decoder *vbi)
{
	int i;

	/* Discard all in progress pages */

	for (i = 0; i < 8; i++)
		vbi->vt.raw_page[i].page->function = PAGE_FUNCTION_DISCARD;

	vbi_reset_page_clear(vbi->epg_pc + 0);
	vbi_reset_page_clear(vbi->epg_pc + 1);

	vbi->epg_pc[0].pfc.stream = 1;
	vbi->epg_pc[1].pfc.stream = 2;
}

/**
 * @param vbi Initialized vbi decoding context.
 * 
 * This function must be called after a channel switch,
 * to reset the Teletext decoder.
 */
void
vbi_teletext_channel_switched(vbi_decoder *vbi)
{
	vt_magazine *mag;
	vt_extension *ext;
	int i, j;

	vbi->vt.initial_page.pgno = 0x100;
	vbi->vt.initial_page.subno = VBI_ANY_SUBNO;

	vbi->vt.top = FALSE;

	memset(vbi->vt.page_info, 0xFF, sizeof(vbi->vt.page_info));

	/* Magazine defaults */

	memset(vbi->vt.magazine, 0, sizeof(vbi->vt.magazine));

	for (i = 0; i < 9; i++) {
		mag = vbi->vt.magazine + i;

		for (j = 0; j < 16; j++) {
			mag->pop_link[j].pgno = 0x0FF;		/* unused */
			mag->drcs_link[j] = 0x0FF;		/* unused */
		}

		ext = &mag->extension;

		ext->def_screen_color		= VBI_BLACK;	/* A.5 */
		ext->def_row_color		= VBI_BLACK;	/* A.5 */
		ext->foreground_clut		= 0;
		ext->background_clut		= 0;

		for (j = 0; j < 8; j++)
			ext->drcs_clut[j + 2] = j & 3;

		for (j = 0; j < 32; j++)
			ext->drcs_clut[j + 10] = j & 15;

		memcpy(ext->color_map, default_color_map, sizeof(ext->color_map));
	}

	vbi_teletext_set_default_region(vbi, vbi->vt.region);

	vbi_teletext_desync(vbi);
}


/**
 * @internal
 * @param vbi VBI decoding context.
 * 
 * This function is called during @a vbi destruction
 * to destroy the Teletext subset of @a vbi object.
 */
void
vbi_teletext_destroy(vbi_decoder *vbi)
{
}

/**
 * @internal
 * @param vbi VBI decoding context.
 * 
 * This function is called during @a vbi initialization
 * to initialize the Teletext subset of @a vbi object.
 */
void
vbi_teletext_init(vbi_decoder *vbi)
{
	init_expand();

	vbi->vt.region = 16;
	vbi->vt.max_level = VBI_WST_LEVEL_2p5;

	vbi_teletext_channel_switched(vbi);     /* Reset */
}
