/*
 *  libzvbi - Closed Caption and Teletext rendering
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
 *  Copyright (C) 1999 Paul Ortyl <ortylp@from.pl>
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

/* $Id: exp-gfx.c,v 1.3 2002/07/16 00:11:36 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lang.h"
#include "export.h"
#include "exp-gfx.h"
#include "vt.h" /* VBI_TRANSPARENT_BLACK */

#include "wstfont2.xbm"
#include "ccfont2.xbm"

/* Teletext character cell dimensions - hardcoded (DRCS) */

#define TCW 12
#define TCH 10

#define TCPL (wstfont2_width / TCW * wstfont2_height / TCH)

/* Closed Caption character cell dimensions */

#define CCW 16
#define CCH 26 /* line doubled */

#define CCPL (ccfont2_width / CCW * ccfont2_height / CCH)

static void init_gfx(void) __attribute__ ((constructor));

static void
init_gfx(void)
{
	uint8_t *t, *p;
	int i, j;

	/* de-interleave font image (puts all chars in row 0) */

	if (!(t = malloc(wstfont2_width * wstfont2_height / 8)))
		exit(EXIT_FAILURE);

	for (p = t, i = 0; i < TCH; i++)
		for (j = 0; j < wstfont2_height; p += wstfont2_width / 8, j += TCH)
			memcpy(p, wstfont2_bits + (j + i) * wstfont2_width / 8,
			       wstfont2_width / 8);

	memcpy(wstfont2_bits, t, wstfont2_width * wstfont2_height / 8);
	free (t);

	if (!(t = malloc(ccfont2_width * ccfont2_height / 8)))
		exit(EXIT_FAILURE);

	for (p = t, i = 0; i < CCH; i++)
		for (j = 0; j < ccfont2_height; p += ccfont2_width / 8, j += CCH)
			memcpy(p, ccfont2_bits + (j + i) * ccfont2_width / 8,
			       ccfont2_width / 8);

	memcpy(ccfont2_bits, t, ccfont2_width * ccfont2_height / 8);
	free(t);
}

/**
 * @internal
 * @param c Unicode.
 * @param italic @c TRUE to switch to slanted character set (doesn't affect
 *          Hebrew and Arabic). If this is a G1 block graphic character
 *          switch to separated block mosaic set.
 * 
 * Translate Unicode character to glyph number in wstfont2 image. 
 * 
 * @return
 * Glyph number.
 */
static int
unicode_wstfont2(unsigned int c, int italic)
{
	static const unsigned short specials[] = {
		0x01B5, 0x2016, 0x01CD, 0x01CE, 0x0229, 0x0251, 0x02DD, 0x02C6,
		0x02C7, 0x02C9, 0x02CA, 0x02CB, 0x02CD, 0x02CF, 0x02D8, 0x02D9,
		0x02DA, 0x02DB, 0x02DC, 0x2014, 0x2018, 0x2019, 0x201C,	0x201D,
		0x20A0, 0x2030, 0x20AA, 0x2122, 0x2126, 0x215B, 0x215C, 0x215D,
		0x215E, 0x2190, 0x2191, 0x2192, 0x2193, 0x25A0, 0x266A, 0xE800,
		0xE75F };
	int i;

	if (c < 0x0180) {
		if (c < 0x0080) {
			if (c < 0x0020)
				return 357; /* invalid */
			else /* %3 Basic Latin (ASCII) 0x0020 ... 0x007F */
				c = c - 0x0020 + 0 * 32;
		} else if (c < 0x00A0)
			return 357; /* invalid */
		else /* %3 Latin-1 Supplement, Latin Extended-A 0x00A0 ... 0x017F */
			c = c - 0x00A0 + 3 * 32;
	} else if (c < 0xEE00) {
		if (c < 0x0460) {
			if (c < 0x03D0) {
				if (c < 0x0370)
					goto special;
				else /* %5 Greek 0x0370 ... 0x03CF */
					c = c - 0x0370 + 12 * 32;
			} else if (c < 0x0400)
				return 357; /* invalid */
			else /* %5 Cyrillic 0x0400 ... 0x045F */
				c = c - 0x0400 + 15 * 32;
		} else if (c < 0x0620) {
			if (c < 0x05F0) {
				if (c < 0x05D0)
					return 357; /* invalid */
				else /* %6 Hebrew 0x05D0 ... 0x05EF */
					return c - 0x05D0 + 18 * 32;
			} else if (c < 0x0600)
				return 357; /* invalid */
			else /* %6 Arabic 0x0600 ... 0x061F */
				return c - 0x0600 + 19 * 32;
		} else if (c >= 0xE600 && c < 0xE740)
			return c - 0xE600 + 19 * 32; /* %6 Arabic (TTX) */
		else
			goto special;
	} else if (c < 0xEF00) { /* %3 G1 Graphics */
		return (c ^ 0x20) - 0xEE00 + 23 * 32;
	} else if (c < 0xF000) { /* %4 G3 Graphics */
		return c - 0xEF20 + 27 * 32;
	} else /* 0xF000 ... 0xF7FF reserved for DRCS */
		return 357; /* invalid */

	if (italic)
		return c + 31 * 32;
	else
		return c;
special:
	for (i = 0; i < sizeof(specials) / sizeof(specials[0]); i++)
		if (specials[i] == c) {
			if (italic)
				return i + 41 * 32;
			else
				return i + 10 * 32;
		}

	return 357; /* invalid */
}

/**
 * @internal
 * @param c Unicode.
 * @param italic @c TRUE to switch to slanted character set.
 * 
 * Translate Unicode character to glyph number in ccfont2 image. 
 * 
 * @return
 * Glyph number.
 */
static int
unicode_ccfont2(unsigned int c, int italic)
{
	static const unsigned short specials[] = {
								0x00E1, 0x00E9,
		0x00ED, 0x00F3, 0x00FA, 0x00E7, 0x00F7, 0x00D1, 0x00F1, 0x25A0,
		0x00AE, 0x00B0, 0x00BD, 0x00BF, 0x2122, 0x00A2, 0x00A3, 0x266A,
		0x00E0, 0x0020, 0x00E8, 0x00E2, 0x00EA, 0x00EE, 0x00F4, 0x00FB };
	int i;

	if (c < 0x0020)
		c = 15; /* invalid */
	else if (c < 0x0080)
		c = c;
	else {
		for (i = 0; i < sizeof(specials) / sizeof(specials[0]); i++)
			if (specials[i] == c) {
				c = i + 6;
				goto slant;
			}

		c = 15; /* invalid */
	}

slant:
	if (italic)
		c += 4 * 32;

	return c;
}

/**
 * @internal
 * @param p Plane of @a canvas_type char, short, int.
 * @param i Index.
 *
 * @return
 * Pixel @a i in plane @a p.
 */
#define peek(p, i)							\
((canvas_type == sizeof(uint8_t)) ? ((uint8_t *)(p))[i] :		\
    ((canvas_type == sizeof(uint16_t)) ? ((uint16_t *)(p))[i] :		\
	((uint32_t *)(p))[i]))

/**
 * @internal
 * @param p Plane of @a canvas_type char, short, int.
 * @param i Index.
 * @param v Value.
 * 
 * Set pixel @a i in plane @a p to value @a v.
 */
#define poke(p, i, v)							\
((canvas_type == sizeof(uint8_t)) ? (((uint8_t *)(p))[i] = (v)) :	\
    ((canvas_type == sizeof(uint16_t)) ? (((uint16_t *)(p))[i] = (v)) :	\
	(((uint32_t *)(p))[i] = (v))))

/**
 * @internal
 * @param canvas_type sizeof(char, short, int).
 * @param canvas Pointer to image plane where the character is to be drawed.
 * @param rowstride @a canvas <em>byte</em> distance from line to line.
 * @param pen Pointer to color palette of @a canvas_type (index 0 background
 *   pixels, index 1 foreground pixels).
 * @param font Pointer to font image with width @a cpl x @a cw pixels, height
 *   @a ch pixels, depth one bit, bit '1' is foreground.
 * @param cpl Chars per line (number of characters in @a font image).
 * @param cw Character cell width in pixels.
 * @param ch Character cell height in pixels.
 * @param glyph Glyph number in font image, 0 ... @a cpl - 1.
 * @param bold Draw character bold (font image | font image << 1).
 * @param underline Bit mask of character rows. For each bit
 *   1 << (n = 0 ... @a ch - 1) set all of character row n to
 *   foreground color.
 * @param size Size of character, either NORMAL, DOUBLE_WIDTH (draws left
 *   and right half), DOUBLE_HEIGHT (draws upper half only),
 *   DOUBLE_SIZE (left and right upper half), DOUBLE_HEIGHT2
 *   (lower half), DOUBLE_SIZE2 (left and right lower half).
 * 
 * Draw one character (function template - define a static version with
 * constant @a canvas_type, @a font, @a cpl, @a cw, @a ch).
 */
static inline void
draw_char(int canvas_type, uint8_t *canvas, int rowstride,
	  uint8_t *pen, uint8_t *font, int cpl, int cw, int ch,
	  int glyph, int bold, unsigned int underline, vbi_size size)
{
	uint8_t *src;
	int shift, x, y;

	bold = !!bold;
	assert(cw >= 8 && cw <= 16);
	assert(ch >= 1 && cw <= 31);

	x = glyph * cw;
	shift = x & 7;
	src = font + (x >> 3);

	switch (size) {
	case VBI_DOUBLE_HEIGHT2:
	case VBI_DOUBLE_SIZE2:
		src += cpl * cw / 8 * ch / 2;
		underline >>= ch / 2;

	case VBI_DOUBLE_HEIGHT:
	case VBI_DOUBLE_SIZE: 
		ch >>= 1;

	default:
		break;
	}

	for (y = 0; y < ch; underline >>= 1, y++) {
		int bits = ~0;

		if (!(underline & 1)) {
#if #cpu (i386)
			bits = (*((uint16_t *) src) >> shift);
#else
                        /* unaligned/little endian */
			bits = ((src[1] * 256 + src[0]) >> shift);
#endif
			bits |= bits << bold;
		}

		switch (size) {
		case VBI_NORMAL_SIZE:
			for (x = 0; x < cw; bits >>= 1, x++)
				poke(canvas, x, peek(pen, bits & 1));

			canvas += rowstride;

			break;

		case VBI_DOUBLE_HEIGHT:
		case VBI_DOUBLE_HEIGHT2:
			for (x = 0; x < cw; bits >>= 1, x++) {
				unsigned int col = peek(pen, bits & 1);

				poke(canvas, x, col);
				poke(canvas, x + rowstride / canvas_type, col);
			}

			canvas += rowstride * 2;

			break;

		case VBI_DOUBLE_WIDTH:
			for (x = 0; x < cw * 2; bits >>= 1, x += 2) {
				unsigned int col = peek(pen, bits & 1);

				poke(canvas, x + 0, col);
				poke(canvas, x + 1, col);
			}

			canvas += rowstride;

			break;

		case VBI_DOUBLE_SIZE:
		case VBI_DOUBLE_SIZE2:
			for (x = 0; x < cw * 2; bits >>= 1, x += 2) {
				unsigned int col = peek(pen, bits & 1);

				poke(canvas, x + 0, col);
				poke(canvas, x + 1, col);
				poke(canvas, x + rowstride / canvas_type + 0, col);
				poke(canvas, x + rowstride / canvas_type + 1, col);
			}

			canvas += rowstride * 2;

			break;

		default:
			break;
		}

		src += cpl * cw / 8;
	}
}

/**
 * @internal
 * @param canvas_type sizeof(char, short, int).
 * @param canvas Pointer to image plane where the character is to be drawed.
 * @param rowstride @a canvas <em>byte</em> distance from line to line.
 * @param pen Pointer to color palette of @a canvas_type (index 0 ... 1 for
 *   depth 1 DRCS, 0 ... 3 for depth 2, 0 ... 15 for depth 4).
 * @param color Offset into color palette.
 * @param font Pointer to DRCS image. Each pixel is coded in four bits, an
 *   index into the color palette, and stored in LE order (i. e. first
 *   pixel 0x0F, second pixel 0xF0). Character size is 12 x 10 pixels,
 *   60 bytes, without padding.
 * @param glyph Glyph number in font image, 0x00 ... 0x3F.
 * @param size Size of character, either NORMAL, DOUBLE_WIDTH (draws left
 *   and right half), DOUBLE_HEIGHT (draws upper half only),
 *   DOUBLE_SIZE (left and right upper half), DOUBLE_HEIGHT2
 *   (lower half), DOUBLE_SIZE2 (left and right lower half).
 * 
 * Draw one Teletext Dynamically Redefinable Character (function template -
 * define a static version with constant @a canvas_type, @a font).
 */
static inline void
draw_drcs(int canvas_type, uint8_t *canvas, unsigned int rowstride,
	  uint8_t *pen, int color, uint8_t *font, int glyph, vbi_size size)
{
	uint8_t *src;
	unsigned int col;
	int x, y;

	src = font + glyph * 60;
	pen = pen + color * canvas_type;

	switch (size) {
	case VBI_NORMAL_SIZE:
		for (y = 0; y < TCH; canvas += rowstride, y++)
			for (x = 0; x < 12; src++, x += 2) {
				poke(canvas, x + 0, peek(pen, *src & 15));
				poke(canvas, x + 1, peek(pen, *src >> 4));
			}
		break;

	case VBI_DOUBLE_HEIGHT2:
		src += 30;

	case VBI_DOUBLE_HEIGHT:
		for (y = 0; y < TCH / 2; canvas += rowstride * 2, y++)
			for (x = 0; x < 12; src++, x += 2) {
				col = peek(pen, *src & 15);
				poke(canvas, x + 0, col);
				poke(canvas, x + rowstride / canvas_type + 0, col);

				col = peek(pen, *src >> 4);
				poke(canvas, x + 1, col);
				poke(canvas, x + rowstride / canvas_type + 1, col);
			}
		break;

	case VBI_DOUBLE_WIDTH:
		for (y = 0; y < TCH; canvas += rowstride, y++)
			for (x = 0; x < 12 * 2; src++, x += 4) {
				col = peek(pen, *src & 15);
				poke(canvas, x + 0, col);
				poke(canvas, x + 1, col);

				col = peek(pen, *src >> 4);
				poke(canvas, x + 2, col);
				poke(canvas, x + 3, col);
			}
		break;

	case VBI_DOUBLE_SIZE2:
		src += 30;

	case VBI_DOUBLE_SIZE:
		for (y = 0; y < TCH / 2; canvas += rowstride * 2, y++)
			for (x = 0; x < 12 * 2; src++, x += 4) {
				col = peek(pen, *src & 15);
				poke(canvas, x + 0, col);
				poke(canvas, x + 1, col);
				poke(canvas, x + rowstride / canvas_type + 0, col);
				poke(canvas, x + rowstride / canvas_type + 1, col);

				col = peek(pen, *src >> 4);
				poke(canvas, x + 2, col);
				poke(canvas, x + 3, col);
				poke(canvas, x + rowstride / canvas_type + 2, col);
				poke(canvas, x + rowstride / canvas_type + 3, col);
			}
		break;

	default:
		break;
	}
}

/**
 * @internal
 * @param canvas_type sizeof(char, short, int).
 * @param canvas Pointer to image plane where the character is to be drawed.
 * @param rowstride @a canvas <em>byte</em> distance from line to line.
 * @param color Color value of @a canvas_type.
 * @param cw Character width in pixels.
 * @param ch Character height in pixels.
 * 
 * Draw blank character.
 */
static inline void
draw_blank(int canvas_type, uint8_t *canvas, unsigned int rowstride,
	   unsigned int color, int cw, int ch)
{
	int x, y;

	for (y = 0; y < ch; y++) {
		for (x = 0; x < cw; x++)
			poke(canvas, x, color);

		canvas += rowstride;
	}
}

/**
 * @param fmt Target format. For now only VBI_PIXFMT_RGBA32_LE (vbi_rgba) permitted.
 * @param canvas Pointer to destination image (currently an array of vbi_rgba), this
 *   must be at least @a rowstride * @a height * 26 bytes large.
 * @param rowstride @a canvas <em>byte</em> distance from line to line.
 *   If this is -1, pg->columns * 16 * sizeof(vbi_rgba) bytes will be assumed.
 * @param column First source column, 0 ... pg->columns - 1.
 * @param row First source row, 0 ... pg->rows - 1.
 * @param width Number of columns to draw, 1 ... pg->columns.
 * @param height Number of rows to draw, 1 ... pg->rows.
 * 
 * Draw a subsection of a Closed Caption vbi_page. In this mode one
 * character occupies 16 x 26 pixels.
 */
void
vbi_draw_cc_page_region(vbi_page *pg,
			vbi_pixfmt fmt, void *canvas, int rowstride,
			int column, int row, int width, int height)
{
	vbi_rgba pen[2], *canvast = canvas;
	int count, row_adv;
	vbi_char *ac;

	if (fmt != VBI_PIXFMT_RGBA32_LE)
		return;

	if (0) {
		int i, j;

		for (i = 0; i < pg->rows; i++) {
			fprintf(stderr, "%2d: ", i);
			ac = &pg->text[i * pg->columns];
			for (j = 0; j < pg->columns; j++)
				fprintf(stderr, "%d%d%02x ",
					ac[j].foreground,
					ac[j].background,
					ac[j].unicode & 0xFF);
			fprintf(stderr, "\n");
		}
	}

	if (rowstride == -1)
		rowstride = pg->columns * CCW * sizeof(*canvast);

	row_adv = rowstride * CCH - width * CCW * sizeof(*canvast);

	for (; height > 0; height--, row++) {
		ac = &pg->text[row * pg->columns + column];

		for (count = width; count > 0; count--, ac++) {
			pen[0] = pg->color_map[ac->background];
			pen[1] = pg->color_map[ac->foreground];

			draw_char(sizeof(*canvast), (uint8_t *) canvast, rowstride,
				  (uint8_t *) pen, ccfont2_bits, CCPL, CCW, CCH,
				  unicode_ccfont2(ac->unicode, ac->italic), 0 /* bold */,
				  ac->underline * (3 << 24) /* cell row 24, 25 */,
				  VBI_NORMAL_SIZE);

			canvast += CCW;
		}

		canvast += row_adv / sizeof(*canvast);
	}
}

/**
 * @param pg Source page.
 * @param fmt Target format. For now only VBI_PIXFMT_RGBA32_LE (vbi_rgba) permitted.
 * @param canvas Pointer to destination image (currently an array of vbi_rgba), this
 *   must be at least @a rowstride * @a height * 10 bytes large.
 * @param rowstride @a canvas <em>byte</em> distance from line to line.
 *   If this is -1, pg->columns * 12 * sizeof(vbi_rgba) bytes will be assumed.
 * @param column First source column, 0 ... pg->columns - 1.
 * @param row First source row, 0 ... pg->rows - 1.
 * @param width Number of columns to draw, 1 ... pg->columns.
 * @param height Number of rows to draw, 1 ... pg->rows.
 * @param reveal If FALSE, draw characters flagged 'concealed' (see vbi_char) as
 *   space (U+0020).
 * @param flash_on If FALSE, draw characters flagged 'blink' (see vbi_char) as
 *   space (U+0020).
 * 
 * Draw a subsection of a Teletext vbi_page. In this mode one
 * character occupies 12 x 10 pixels.
 */
void
vbi_draw_vt_page_region(vbi_page *pg,
			vbi_pixfmt fmt, void *canvas, int rowstride,
			int column, int row, int width, int height,
			int reveal, int flash_on)
{
	vbi_rgba pen[64], *canvast = canvas;
	int count, row_adv;
	int conceal, unicode;
	vbi_char *ac;
	int i;

	if (fmt != VBI_PIXFMT_RGBA32_LE)
		return;

	if (0) {
		int i, j;

		for (i = 0; i < pg->rows; i++) {
			fprintf(stderr, "%2d: ", i);
			ac = &pg->text[i * pg->columns];
			for (j = 0; j < pg->columns; j++)
				fprintf(stderr, "%04x ", ac[j].unicode);
			fprintf(stderr, "\n");
		}
	}

	if (rowstride == -1)
		rowstride = pg->columns * 12 * sizeof(*canvast);

	row_adv = rowstride * 10 - width * 12 * sizeof(*canvast);

	conceal = !reveal;

	if (pg->drcs_clut)
		for (i = 2; i < 2 + 8 + 32; i++)
			pen[i] = pg->color_map[pg->drcs_clut[i]];

	for (; height > 0; height--, row++) {
		ac = &pg->text[row * pg->columns + column];

		for (count = width; count > 0; count--, ac++) {
			if ((ac->conceal & conceal) || !flash_on)
				unicode = 0x0020;
			else
				unicode = ac->unicode;

			pen[0] = pg->color_map[ac->background];
			pen[1] = pg->color_map[ac->foreground];

			switch (ac->size) {
			case VBI_OVER_TOP:
			case VBI_OVER_BOTTOM:
				break;

			default:
				if (vbi_is_drcs(unicode)) {
					uint8_t *font = pg->drcs[(unicode >> 6) & 0x1F];

					if (font)
						draw_drcs(sizeof(*canvast), (uint8_t *) canvast, rowstride,
							  (uint8_t *) pen, ac->drcs_clut_offs,
							  font, unicode & 0x3F, ac->size);
					else /* shouldn't happen */
						draw_blank(sizeof(*canvast), (uint8_t *) canvast, rowstride,
							   pen[0], TCW, TCH);
				} else {
					draw_char(sizeof(*canvast), (uint8_t *) canvast, rowstride,
						(uint8_t *) pen, wstfont2_bits, TCPL, TCW, TCH,
						unicode_wstfont2(unicode, ac->italic), ac->bold,
						ac->underline << 9 /* cell row 9 */, ac->size);
				}
			}

			canvast += TCW;
		}

		canvast += row_adv / sizeof(*canvast);
	}
}

/*
 *  This won't scale with proportional spacing or custom fonts,
 *  to be removed.
 */

/**
 * @param w
 * @param h
 *
 * @deprecated
 * Character cells are 12 x 10 for Teletext and 16 x 26 for Caption.
 * Page size is in vbi_page.
 */
void
vbi_get_max_rendered_size(int *w, int *h)
{
  if (w) *w = 41 * TCW;
  if (h) *h = 25 * TCH;
}

/**
 * @param w
 * @param h
 *
 * @deprecated
 * Character cells are 12 x 10 for Teletext and 16 x 26 for Caption.
 */
void
vbi_get_vt_cell_size(int *w, int *h)
{
  if (w) *w = TCW;
  if (h) *h = TCH;
}

/*
 *  Shared export options
 */

typedef struct gfx_instance
{
	vbi_export		export;

	/* Options */
	unsigned		double_height : 1;
	/*
	 *  The raw image contains the same information a real TV
	 *  would show, however a TV overlays the image on both fields.
	 *  So raw pixel aspect is 2:1, and this option will double
	 *  lines adding redundant information. The resulting images
	 *  with pixel aspect 2:2 are still too narrow compared to a
	 *  real TV closer to 4:3 (11 MHz TXT pixel clock), but I
	 *  think one should export raw, not scaled data (which is
	 *  still possible in Zapping using the screenshot plugin).
	 */
} gfx_instance;

static vbi_export *
gfx_new(void)
{
	gfx_instance *gfx;

	if (!(gfx = calloc(1, sizeof(*gfx))))
		return NULL;

	return &gfx->export;
}

static void
gfx_delete(vbi_export *e)
{
	free(PARENT(e, gfx_instance, export));
}


static vbi_option_info
gfx_options[] = {
	VBI_OPTION_BOOL_INITIALIZER
	  ("aspect", N_("Correct aspect ratio"),
	   TRUE, N_("Approach an image aspect ratio similar to "
		    "a real TV. This will double the image size."))
};

#define elements(array) (sizeof(array) / sizeof(array[0]))

static vbi_option_info *
option_enum(vbi_export *e, int index)
{
	if (index < 0 || index >= elements(gfx_options))
		return NULL;
	else
		return gfx_options + index;
}

static vbi_bool
option_get(vbi_export *e, const char *keyword, vbi_option_value *value)
{
	gfx_instance *gfx = PARENT(e, gfx_instance, export);

	if (strcmp(keyword, "aspect") == 0) {
		value->num = gfx->double_height;
	} else {
		vbi_export_unknown_option(e, keyword);
		return FALSE;
	}

	return TRUE;
}

static vbi_bool
option_set(vbi_export *e, const char *keyword, va_list args)
{
	gfx_instance *gfx = PARENT(e, gfx_instance, export);

	if (strcmp(keyword, "aspect") == 0) {
		gfx->double_height = !!va_arg(args, int);
	} else {
		vbi_export_unknown_option(e, keyword);
		return FALSE;
	}

	return TRUE;
}

/*
 *  PPM - Portable Pixmap File (raw)
 */

static vbi_bool
ppm_export(vbi_export *e, FILE *fp, vbi_page *pg)
{
	gfx_instance *gfx = PARENT(e, gfx_instance, export);
	int cw, ch, ww, size, scale, row;
	vbi_rgba *image;
	uint8_t *body;
	int i;

	if (pg->columns < 40) /* caption */ {
		cw = CCW;
		ch = CCH;
		/* characters already line-doubled */
		scale = !!gfx->double_height;
	} else {
		cw = TCW;
		ch = TCH;
		scale = 1 + !!gfx->double_height;
	}

	ww = cw * pg->columns;
	size = ww * ch * 1; /* one character row */

	if (!(image = malloc(size * sizeof(*image)))) {
		vbi_export_error_printf(e, _("Unable to allocate %d KB image buffer."),
					size * sizeof(*image) / 1024);
		return FALSE;
	}

	fprintf(fp, "P6 %d %d 255\n",
		cw * pg->columns, (ch * pg->rows / 2) << scale);

	if (ferror(fp))
		goto write_error;

	for (row = 0; row < pg->rows; row++) {
		if (pg->columns < 40)
			vbi_draw_cc_page_region(pg, VBI_PIXFMT_RGBA32_LE, image, -1,
						0, row, pg->columns, 1 /* rows */);
		else
			vbi_draw_vt_page_region(pg, VBI_PIXFMT_RGBA32_LE, image, -1,
						0, row, pg->columns, 1 /* rows */,
						!e->reveal, 1 /* flash_on */);
		body = (uint8_t *) image;

		if (scale == 0)
			for (i = 0; i < size; body += 3, i++) {
				body[0] = ((image[i] & 0xFF) + (image[i + ww] & 0xFF)
					   + 0x01) >> 1;
				body[1] = ((image[i] & 0xFF00) + (image[i + ww] & 0xFF00)
					   + 0x0100) >> 9;
				body[2] = ((image[i] & 0xFF0000) + (image[i + ww] & 0xFF0000)
					   + 0x010000) >> 17;
			}
		else
			for (i = 0; i < size; body += 3, i++) {
				unsigned int n = image[i];

				body[0] = n;
				body[1] = n >> 8;
				body[2] = n >> 16;
			}

		switch (scale) {
			int rows, stride;

		case 0:
			body = (uint8_t *) image;
			rows = ch / 2;
			stride = ww * 3;

			for (i = 0; i < rows; i++, body += stride * 2)
				if (!fwrite(body, stride, 1, fp))
					goto write_error;
			break;

		case 1:
			if (!fwrite(image, size * 3, 1, fp))
				goto write_error;
			break;

		case 2:
			body = (uint8_t *) image;
			stride = cw * pg->columns * 3;

			for (i = 0; i < ch; body += stride, i++) {
				if (!fwrite(body, stride, 1, fp))
					goto write_error;
				if (!fwrite(body, stride, 1, fp))
					goto write_error;
			}

			break;
		}
	}

	free(image);
	image = NULL;

	return TRUE;

write_error:
	vbi_export_write_error(e);

	if (image)
		free(image);

	return FALSE;
}

vbi_export_class
vbi_export_class_ppm = {
	._public = {
		.keyword	= "ppm",
		.label		= N_("PPM"),
		.tooltip	= N_("Export this page as raw PPM image"),

		.mime_type	= "image/x-portable-pixmap",
		.extension	= "ppm",
	},

	._new			= gfx_new,
	._delete		= gfx_delete,
	.option_enum		= option_enum,
	.option_get		= option_get,
	.option_set		= option_set,
	.export			= ppm_export
};

VBI_AUTOREG_EXPORT_MODULE(vbi_export_class_ppm)

/*
 *  PNG - Portable Network Graphics File
 */

#ifdef HAVE_LIBPNG

#include "png.h"
#include "setjmp.h"

static void
draw_char_cc_indexed(png_bytep canvas, int rowstride, png_bytep pen,
		     int unicode, vbi_char *ac)
{
	draw_char(sizeof(png_byte), (uint8_t *) canvas, rowstride,
		  (uint8_t *) pen, ccfont2_bits, CCPL, CCW, CCH,
		  unicode_ccfont2(unicode, ac->italic), 0 /* bold */,
		  ac->underline * (3 << 24) /* cell row 24, 25 */,
		  VBI_NORMAL_SIZE);
}

static void
draw_char_vt_indexed(png_bytep canvas, int rowstride, png_bytep pen,
		     int unicode, vbi_char *ac)
{
	draw_char(sizeof(png_byte), (uint8_t *) canvas, rowstride,
		  (uint8_t *) pen, wstfont2_bits, TCPL, TCW, TCH,
		  unicode_wstfont2(unicode, ac->italic), ac->bold,
		  ac->underline << 9 /* cell row 9 */, ac->size);
}

static void
draw_drcs_indexed(png_bytep canvas, int rowstride, png_bytep pen,
		  uint8_t *font, int glyph, vbi_size size)
{
	draw_drcs(sizeof(png_byte), (uint8_t *) canvas, rowstride,
		  (uint8_t *) pen, 0, font, glyph, size);
}

static vbi_bool
png_export(vbi_export *e, FILE *fp, vbi_page *pg)
{
	gfx_instance *gfx = PARENT(e, gfx_instance, export);
	png_structp png_ptr;
	png_infop info_ptr;
	png_color palette[80];
	png_byte alpha[80];
	png_text text[4];
	char title[80];
	png_bytep *row_pointer;
	png_bytep image;
	int cw, ch, ww, wh, rowstride, scale;
	void (* draw_char_indexed)(png_bytep, int, png_bytep, int, vbi_char *);
	int i;

	if (pg->columns < 40) /* caption */ {
		draw_char_indexed = draw_char_cc_indexed;
		cw = CCW;
		ch = CCH;
		/* characters are already line-doubled */
		scale = !!gfx->double_height;
	} else {
		draw_char_indexed = draw_char_vt_indexed;
		cw = TCW;
		ch = TCH;
		scale = 1 + !!gfx->double_height;
	}

	ww = cw * pg->columns;
	wh = ch * pg->rows;
	rowstride = ww * sizeof(*image);

	if (!(row_pointer = malloc(sizeof(*row_pointer) * wh * 2))) {
		vbi_export_error_printf(e, _("Unable to allocate %d byte buffer."),
					sizeof(*row_pointer) * wh * 2);
		return FALSE;
	}

	if ((image = malloc(wh * ww * sizeof(*image)))) {
		png_bytep canvas = image;
		png_byte pen[128];
		int row, column;
		vbi_char *ac;
		int unicode, conceal = !e->reveal;
		int row_adv;

		row_adv = pg->columns * cw * (ch - 1);

		if (pg->drcs_clut)
			for (i = 2; i < 2 + 8 + 32; i++) {
				pen[i]      = pg->drcs_clut[i]; /* opaque */
				pen[i + 64] = pg->drcs_clut[i] + 40; /* translucent */
			}

		for (row = 0; row < pg->rows; canvas += row_adv, row++) {
			for (column = 0; column < pg->columns ; canvas += cw, column++) {
				ac = &pg->text[row * pg->columns + column];

				if (ac->size == VBI_OVER_TOP
				    || ac->size == VBI_OVER_BOTTOM)
					continue;

				unicode = (ac->conceal & conceal) ? 0x0020u : ac->unicode;

				switch (ac->opacity) {
				case VBI_TRANSPARENT_SPACE:
					/*
					 *  Transparent foreground and background.
					 */
					draw_blank(sizeof(*canvas), (uint8_t *) canvas,
						   rowstride, VBI_TRANSPARENT_BLACK, cw, ch);
					break;

				case VBI_TRANSPARENT_FULL:
					/*
					 *  Transparent background, opaque foreground. Currently not used.
					 *  Mind Teletext level 2.5 foreground and background transparency
					 *  by referencing colormap entry 8, VBI_TRANSPARENT_BLACK.
					 *  The background of multicolor DRCS is ambiguous, so we make
					 *  them opaque.
					 */
					if (vbi_is_drcs(unicode)) {
						uint8_t *font = pg->drcs[(unicode >> 6) & 0x1F];

						pen[0] = VBI_TRANSPARENT_BLACK;
						pen[1] = ac->foreground;

						if (font && (draw_char_indexed == draw_char_vt_indexed))
							draw_drcs_indexed(canvas, rowstride, pen,
									  font, unicode & 0x3F, ac->size);
						else /* shouldn't happen */
							draw_blank(sizeof(*canvas), (uint8_t *) canvas,
								   rowstride, VBI_TRANSPARENT_BLACK, cw, ch);
					} else {
						pen[0] = VBI_TRANSPARENT_BLACK;
						pen[1] = ac->foreground;

						draw_char_indexed(canvas, rowstride, pen, unicode, ac);
					}

					break;

				case VBI_SEMI_TRANSPARENT:
					/*
					 *  Translucent background (for 'boxed' text), opaque foreground.
					 *  The background of multicolor DRCS is ambiguous, so we make
					 *  them completely translucent. 
					 */
					if (vbi_is_drcs(unicode)) {
						uint8_t *font = pg->drcs[(unicode >> 6) & 0x1F];

						pen[64] = ac->background + 40;
						pen[65] = ac->foreground;

						if (font && (draw_char_indexed == draw_char_vt_indexed))
							draw_drcs_indexed(canvas, rowstride,
									  (uint8_t *)(pen + 64),
									  font, unicode & 0x3F, ac->size);
						else /* shouldn't happen */
							draw_blank(sizeof(*canvas), (uint8_t *) canvas,
								   rowstride, VBI_TRANSPARENT_BLACK, cw, ch);
					} else {
						pen[0] = ac->background + 40; /* translucent */
						pen[1] = ac->foreground;

						draw_char_indexed(canvas, rowstride, pen, unicode, ac);
					}

					break;

				case VBI_OPAQUE:
					pen[0] = ac->background;
					pen[1] = ac->foreground;

					if (vbi_is_drcs(unicode)) {
						uint8_t *font = pg->drcs[(unicode >> 6) & 0x1F];

						if (font && (draw_char_indexed == draw_char_vt_indexed))
							draw_drcs_indexed(canvas, rowstride, pen,
									  font, unicode & 0x3F, ac->size);
						else /* shouldn't happen */
							draw_blank(sizeof(*canvas), (uint8_t *) canvas,
								   rowstride, pen[0], cw, ch);
					} else
						draw_char_indexed(canvas, rowstride, pen, unicode, ac);
					break;
				}
			}
		}
	} else {
		vbi_export_error_printf(e, _("Unable to allocate %d KB image buffer."),
					wh * ww * sizeof(*image) / 1024);
		free(row_pointer);
		return FALSE;
	}

	/* Now save the image */

	if (!(png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)))
		goto unknown_error;

	if (!(info_ptr = png_create_info_struct(png_ptr))) {
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		goto unknown_error;
	}

	/* avoid possible longjmp breakage due to libpng ugliness */
	{ static int do_write() {
	if (setjmp(png_ptr->jmpbuf))
		return 1;

	png_init_io(png_ptr, fp);

	png_set_IHDR(png_ptr, info_ptr, ww, (wh << scale) >> 1,
		8 /* bit_depth */,
		PNG_COLOR_TYPE_PALETTE,
		(gfx->double_height) ?
			PNG_INTERLACE_ADAM7 : PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);

	/* Could be optimized (or does libpng?) */
	for (i = 0; i < 40; i++) {
		/* opaque */
		palette[i].red   = pg->color_map[i] & 0xFF;
		palette[i].green = (pg->color_map[i] >> 8) & 0xFF;
		palette[i].blue	 = (pg->color_map[i] >> 16) & 0xFF;
		alpha[i]	 = 255;

		/* translucent */
		palette[i + 40]  = palette[i];
		alpha[i + 40]	 = 128;
	}

	alpha[VBI_TRANSPARENT_BLACK] = 0;
	alpha[40 + VBI_TRANSPARENT_BLACK] = 0;

	png_set_PLTE(png_ptr, info_ptr, palette, 80);
	png_set_tRNS(png_ptr, info_ptr, alpha, 80, NULL);

	png_set_gAMA(png_ptr, info_ptr, 1.0 / 2.2);

	{
		int size = 0;

		if (e->network)
			size = snprintf(title, sizeof(title) - 1, "%s ", e->network);
		else
			title[0] = 0;

		/*
		 *  ISO 8859-1 (Latin-1) character set required,
		 *  see png spec for other
		 */
		if (pg->pgno < 0x100)
			size += snprintf(title + size, sizeof(title) - size - 1,
					 "Closed Caption"); /* no i18n, proper name */
		else if (pg->subno != VBI_ANY_SUBNO)
			size += snprintf(title + size, sizeof(title) - size - 1,
					 /* NLS: .png title, must be Latin-1 */
					 _("Teletext Page %3x.%x"),
					 pg->pgno, pg->subno);
		else
			size += snprintf(title + size, sizeof(title) - size - 1,
					 /* NLS: .png title, must be Latin-1 */
					 _("Teletext Page %3x"), pg->pgno);
	}

	memset(text, 0, sizeof(text));

	text[0].key = "Title";
	text[0].text = title;
	text[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text[1].key = "Software";
	text[1].text = e->creator;
	text[1].compression = PNG_TEXT_COMPRESSION_NONE;

	png_set_text(png_ptr, info_ptr, text, 2);

	png_write_info(png_ptr, info_ptr);

	switch (scale) {
	case 0:
		for (i = 0; i < wh / 2; i++)
			row_pointer[i] = image + i * 2 * ww;
		break;

	case 1:
		for (i = 0; i < wh; i++)
			row_pointer[i] = image + i * ww;
		break;

	case 2:
		for (i = 0; i < wh; i++)
			row_pointer[i * 2 + 0] =
			row_pointer[i * 2 + 1] = image + i * ww;
		break;
	}

	png_write_image(png_ptr, row_pointer);

	png_write_end(png_ptr, info_ptr);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	return 0;

	} if (do_write()) goto write_error; }

	free(row_pointer);
	row_pointer = NULL;

	free(image);
	image = NULL;

	return TRUE;

write_error:
	vbi_export_write_error(e);

unknown_error:
	if (row_pointer)
		free(row_pointer);

	if (image)
		free(image);

	return FALSE;
}

vbi_export_class
vbi_export_class_png = {
	._public = {
		.keyword	= "png",
		.label		= N_("PNG"),
		.tooltip	= N_("Export this page as PNG image"),

		.mime_type	= "image/png",
		.extension	= "png",
	},

	._new			= gfx_new,
	._delete		= gfx_delete,
	.option_enum		= option_enum,
	.option_get		= option_get,
	.option_set		= option_set,
	.export			= png_export
};

VBI_AUTOREG_EXPORT_MODULE(vbi_export_class_png)

#endif /* HAVE_LIBPNG */
