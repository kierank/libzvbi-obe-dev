/*
 *  libzvbi test
 *
 *  Copyright (C) 2000, 2001 Michael H. Schimek
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

/* $Id: unicode.c,v 1.4 2002/10/22 04:43:18 mschimek Exp $ */

#undef NDEBUG

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <libzvbi.h>

/* Note these are private functions, used here for testing only. */

extern unsigned int	vbi_teletext_unicode (int, int, int);
extern unsigned int	vbi_teletext_composed_unicode (unsigned int a, unsigned int c);
extern unsigned int	vbi_caption_unicode (unsigned int c);

static void
putwchar (unsigned int c)
{
	if (c < 0x80) {
		putchar (c);
        } else if (c < 0x800) {
		putchar (0xC0 | (c >> 6));
		putchar (0x80 | (c & 0x3F));
	} else if (c < 0x10000) {
		putchar (0xE0 | (c >> 12));
		putchar (0x80 | ((c >> 6) & 0x3F));
		putchar (0x80 | (c & 0x3F));
	} else if (c < 0x200000) {
		putchar (0xF0 | (c >> 18));
		putchar (0x80 | ((c >> 12) & 0x3F));
		putchar (0x80 | ((c >> 6) & 0x3F));
		putchar (0x80 | (c & 0x3F));
	}	
}

static void
putwstr (const char *s)
{
	for (; *s; s++)
		putwchar (*s);
}

static const unsigned int
national[] = {
	0x23, 0x24, 0x40, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x7B, 0x7C, 0x7D, 0x7E
};

static void
print_set (const char *name, unsigned int s)
{
	unsigned int i, j;

	putwstr (name);
	putwchar ('\n');
	putwchar ('\n');

	for (i = 0; i < 16; i++) {
		for (j = 2; j < 8; j++) {
			putwchar (vbi_teletext_unicode (s, 0, j * 16 + i));
			putwchar (' ');
		}

		putwchar ('\n');
	}

	putwchar ('\n');
}

int
main (int argc, char **argv)
{
	unsigned int i, j;

	putwstr ("libzvbi unicode test -*- coding: utf-8 -*-\n\n");
	putwstr ("ETS 300 706 Table 36: Latin National Option Sub-sets\n\n");

	for (i = 1; i < 14; i++) {
		for (j = 0; j < sizeof (national) / sizeof (national[0]); j++) {
			putwchar (vbi_teletext_unicode (1, i, national[j]));
			putwchar (' ');
		}

		putwchar ('\n');
	}

	putwchar ('\n');

	print_set ("ETS 300 706 Table 35: Latin G0 Primary Set\n", 1);
	print_set ("ETS 300 706 Table 37: Latin G2 Supplementary Set\n", 2);
	print_set ("ETS 300 706 Table 38: Cyrillic G0 Primary Set - Option 1 - Serbian/Croatian\n", 3);
	print_set ("ETS 300 706 Table 39: Cyrillic G0 Primary Set - Option 2 - Russian/Bulgarian\n", 4);
	print_set ("ETS 300 706 Table 40: Cyrillic G0 Primary Set - Option 3 - Ukrainian\n", 5);
	print_set ("ETS 300 706 Table 41: Cyrillic G2 Supplementary Set\n", 6);
	print_set ("ETS 300 706 Table 42: Greek G0 Primary Set\n", 7);
	print_set ("ETS 300 706 Table 43: Greek G2 Supplementary Set\n", 8);
	print_set ("ETS 300 706 Table 44: Arabic G0 Primary Set\n", 9);
	print_set ("ETS 300 706 Table 45: Arabic G2 Supplementary Set\n", 10);
	print_set ("ETS 300 706 Table 46: Hebrew G0 Primary Set\n", 11);

	putwstr ("ETS 300 706 Table 47: G1 Block Mosaics Set\n\n");

	for (i = 0; i < 16; i++) {
		for (j = 2; j < 8; j++) {
			if (j == 4 || j == 5)
				putwchar (' ');
			else
				putwchar (vbi_teletext_unicode (12, 0, j * 16 + i));

			putwchar (' ');
		}

		putwchar ('\n');
	}

	putwchar ('\n');

	print_set ("ETS 300 706 Table 48: G3 Smooth Mosaics and Line Drawing Set\n", 13);

	putwstr ("Teletext composed glyphs\n\n   ");

	for (i = 0x40; i < 0x60; i++)
		putwchar (vbi_teletext_unicode (1, 0, i));

	putwstr ("\n\n");

	for (i = 0; i < 16; i++) {
		putwchar (vbi_teletext_unicode (2, 0, 0x40 + i));
		putwstr ("  ");

		for (j = 0x40; j < 0x60; j++) {
			unsigned int c = vbi_teletext_composed_unicode (i, j);

			putwchar ((c == 0) ? '-' : c);
		}

		putwchar ('\n');
	}

	putwchar ('\n');

	putwstr ("Teletext composed glyphs\n\n   ");

	for (i = 0x60; i < 0x80; i++)
		putwchar (vbi_teletext_unicode (1, 0, i));

	putwstr ("\n\n");

	for (i = 0; i < 16; i++) {
		putwchar (vbi_teletext_unicode (2, 0, 0x40 + i));
		putwstr ("  ");

		for (j = 0x60; j < 0x80; j++) {
			unsigned int c = vbi_teletext_composed_unicode (i, j);

			putwchar ((c == 0) ? '-' : c);
		}

		putwchar ('\n');
	}

	putwchar ('\n');

	putwstr ("EIA 608 Closed Captioning Basic Character Set\n\n");

	for (i = 0; i < 8; i++) {
		for (j = 0x20; j < 0x80; j += 8) {
			putwchar (vbi_caption_unicode (j + i));
			putwchar (' ');
		}

		putwchar ('\n');
	}

	putwchar ('\n');

	putwstr ("EIA 608 Closed Captioning Special Characters\n\n");

	for (i = 0; i < 16; i++) {
		putwchar (vbi_caption_unicode (i));
	}

	putwchar ('\n');

	exit (EXIT_SUCCESS);
}
