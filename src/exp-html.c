/*
 *  libzvbi - Closed Caption and Teletext HTML export functions
 *
 *  Copyright (C) 2001, 2002 Michael H. Schimek
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

/* $Id: exp-html.c,v 1.6 2002/10/22 04:42:40 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <iconv.h>

#include "lang.h"
#include "export.h"
#include "vt.h"

typedef struct style {
	struct style *		next;
	int			ref_count;
	int			foreground;
	int			background;
	unsigned		flash : 1;
} style;

typedef struct html_instance {
	vbi_export		export;

	/* Options */
	unsigned int		gfx_chr;
	unsigned		color : 1;
	unsigned		headerless : 1;

	FILE *			fp;
	iconv_t			cd;

	int			foreground;
	int			background;
	unsigned int		underline : 1;
	unsigned int		bold : 1;
	unsigned int		italic : 1;
	unsigned int		flash : 1;
	unsigned int		span : 1;
	unsigned int		link : 1;

	style *			styles;
	style			def;
} html_instance;

static vbi_export *
html_new(void)
{
	html_instance *html;

	if (!(html = calloc(1, sizeof(*html))))
		return NULL;

	return &html->export;
}

static void
html_delete(vbi_export *e)
{
	free(PARENT(e, html_instance, export));
}

static vbi_option_info
html_options[] = {
	VBI_OPTION_STRING_INITIALIZER
	  ("gfx_chr", N_("Graphics char"),
	   "#", N_("Replacement for block graphic characters: "
		   "a single character or decimal (32) or hex (0x20) code")),
	VBI_OPTION_BOOL_INITIALIZER
	  ("color", N_("Color (CSS)"),
	   TRUE, N_("Store the page colors using CSS attributes")),
	VBI_OPTION_BOOL_INITIALIZER
	  ("header", N_("HTML header"),
	   TRUE, N_("Include HTML page header"))
};

#define elements(array) (sizeof(array) / sizeof(array[0]))

static vbi_option_info *
option_enum(vbi_export *e, int index)
     /* XXX unsigned index */
{
	if (index < 0 || index >= (int) elements(html_options))
		return NULL;
	else
		return html_options + index;
}

static vbi_bool
option_get(vbi_export *e, const char *keyword, vbi_option_value *value)
{
	html_instance *html = PARENT(e, html_instance, export);

	if (strcmp(keyword, "gfx_chr") == 0) {
		if (!(value->str = vbi_export_strdup(e, NULL, "x")))
			return FALSE;
		value->str[0] = html->gfx_chr;
	} else if (strcmp(keyword, "color") == 0) {
		value->num = html->color;
	} else if (strcmp(keyword, "header") == 0) {
		value->num = !html->headerless;
	} else {
		vbi_export_unknown_option(e, keyword);
		return FALSE;
	}

	return TRUE;
}

static vbi_bool
option_set(vbi_export *e, const char *keyword, va_list args)
{
	html_instance *html = PARENT(e, html_instance, export);

	if (strcmp(keyword, "gfx_chr") == 0) {
		char *s, *string = va_arg(args, char *);
		int value;

		if (!string || !string[0]) {
			vbi_export_invalid_option(e, keyword, string);
			return FALSE;
		} else if (strlen(string) == 1) {
			value = string[0];
		} else {
			value = strtol(string, &s, 0);
			if (s == string)
				value = string[0];
		}
		html->gfx_chr = (value < 0x20 || value > 0xE000) ? 0x20 : value;
	} else if (strcmp(keyword, "color") == 0) {
		html->color = !!va_arg(args, int);
	} else if (strcmp(keyword, "header") == 0) {
		html->headerless = !va_arg(args, int);
	} else {
		vbi_export_unknown_option(e, keyword);
		return FALSE;
	}

	return TRUE;
}

#define TEST 0
#define LF "\n"	/* optional "" */

static void
hash_color(FILE *fp, vbi_rgba color)
{
	fprintf(fp, "#%02x%02x%02x", VBI_R(color), VBI_G(color), VBI_B(color));
}

static void
escaped_fputc(FILE *fp, int c)
{
	switch (c) {
	case '<':
		fputs("&lt;", fp);
		break;

	case '>':
		fputs("&gt;", fp);
		break;

	case '&':
		fputs("&amp;", fp);
		break;

	default:
		putc(c, fp);
		break;
	}
}

static void
escaped_fputs(FILE *fp, char *s)
{
	while (*s)
		escaped_fputc(fp, *s++);
}

static const char *html_underline[]	= { "</u>", "<u>" };
static const char *html_bold[]		= { "</b>", "<b>" };
static const char *html_italic[]	= { "</i>", "<i>" };

static void
title(html_instance *html, vbi_page *pg)
{
	if (pg->pgno < 0x100) {
		fprintf(html->fp, "<title lang=\"en\">");
	} else {
		/* TRANSLATORS: "lang=\"en\" refers to the page title
		   "Teletext Page ...". Please specify "de", "fr", "es" etc. */
		fprintf(html->fp, _("<title lang=\"en\">"));
	}

	if (html->export.network) {
		escaped_fputs(html->fp, html->export.network);
		putc(' ', html->fp);
	}

	if (pg->pgno < 0x100) {
		fprintf(html->fp, "Closed Caption"); /* no i18n, proper name */
	} else if (pg->subno != VBI_ANY_SUBNO) {
		fprintf(html->fp, _("Teletext Page %3x.%x"), pg->pgno, pg->subno);
	} else {
		fprintf(html->fp, _("Teletext Page %3x"), pg->pgno);
	}

	fputs("</title>", html->fp);
}

static vbi_bool
header(html_instance *html, vbi_page *pg)
{
	char *charset, *lang = NULL, *dir = NULL;

	switch (pg->font[0] - vbi_font_descriptors) {
	case 0:	 /* English */
	case 16: /* English */
		lang = "en";

	case 1:	 /* German */
	case 9:	 /* German */
	case 17: /* German */
	case 33: /* German */
		if (!lang) lang = "de";

	case 2:	 /* Swedish/Finnish/Hungarian */
	case 10: /* Swedish/Finnish/Hungarian */
	case 18: /* Swedish/Finnish/Hungarian */
		if (!lang) lang = "sv";

	case 3:	 /* Italian */
	case 11: /* Italian */
	case 19: /* Italian */
		if (!lang) lang = "it";

	case 4:	 /* French */
	case 12: /* French */
	case 20: /* French */
		if (!lang) lang = "fr";

	case 5:	 /* Portuguese/Spanish */
	case 21: /* Portuguese/Spanish */
		if (!lang) lang = "es";

	default:
		charset = "iso-8859-1";
		break;

	case 6:	 /* Czech/Slovak */
	case 14: /* Czech/Slovak */
	case 38: /* Czech/Slovak */
		lang = "cz";

	case 8:	 /* Polish */
		if (!lang) lang = "pl";

	case 29: /* Serbian/Croatian/Slovenian */
		if (!lang) lang = "hr";

	case 31: /* Romanian */
		if (!lang) lang = "ro";
		charset = "iso-8859-2";
		break;

	case 34: /* Estonian */
		lang = "et";

	case 35: /* Lettish/Lithuanian */
		if (!lang) lang = "lt";
		charset = "iso-8859-4";
		break;

	case 32: /* Serbian/Croatian */
		lang = "sr";
		charset = "iso-8859-5";
		break;

	case 36: /* Russian/Bulgarian */
		lang = "ru";
		charset = "koi8-r";
		break;

	case 37: /* Ukranian */
		lang = "uk";
		charset = "koi8-u";
		break;

	case 64: /* Arabic/English */
	case 68: /* Arabic/French */
	case 71: /* Arabic */
	case 87: /* Arabic */
		lang = "ar";
		dir = ""; /* visually ordered */
		charset = "iso-8859-6";	/* XXX needs further examination */
		break;

	case 55: /* Greek */
		lang = "el";
		charset = "iso-8859-7";
		break;

	case 85: /* Hebrew */
		lang = "he";
		dir = ""; /* visually ordered */
		charset = "iso-8859-8";
		break;

	case 22: /* Turkish */
	case 54: /* Turkish */
		lang = "tr";
		charset = "iso-8859-9";
		break;

	case 99: /* Klingon */
		lang = "x-klingon";
		charset = "iso-10646";
		break;
	}

	if ((html->cd = iconv_open(charset, "UCS-2")) == (iconv_t) -1) {
		vbi_export_error_printf(&html->export,
					_("Character conversion Unicode (UCS-2) "
					  "to %s not supported."), charset);
		return FALSE;
	}

	if (!html->headerless) {
		style *s;
		int ord;

		fprintf(html->fp,
			"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\" "
				"\"http://www.w3.org/TR/REC-html40/loose.dtd\">" LF
			"<html>" LF "<head>" LF
			"<meta name=\"generator\" lang=\"en\" content=\"%s\">" LF
			"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=%s\">" LF,
			html->export.creator, charset);

		if (html->color) {
			fputs("<style type=\"text/css\">" LF "<!--" LF, html->fp);

			for (s = html->styles, ord = 1; s; s = s->next)
				if (s != &html->def && s->ref_count > 1) {
					fprintf(html->fp, "span.c%d { color:", ord);
					hash_color(html->fp, pg->color_map[s->foreground]);
					fputs("; background-color:", html->fp);
					hash_color(html->fp, pg->color_map[s->background]);
					if (s->flash)
						fputs("; text-decoration: blink", html->fp);
					fputs(" }" LF, html->fp);
					ord++;
				}

			fputs("//-->" LF "</style>" LF, html->fp);
		}

		title(html, pg);

		fputs(LF "</head>" LF "<body ", html->fp);

		if (lang && *lang)
			fprintf(html->fp, "lang=\"%s\" ", lang);

		if (dir && *dir)
			fprintf(html->fp, "dir=\"%s\" ", dir);

		fputs("text=\"#FFFFFF\" bgcolor=\"", html->fp);

		hash_color(html->fp, pg->color_map[pg->screen_color]);

		fputs("\">" LF, html->fp);
	}

	if (ferror(html->fp)) {
		vbi_export_write_error(&html->export);
		return FALSE;
	}

	html->foreground	= VBI_WHITE;
	html->background	= pg->screen_color;
	html->underline		= FALSE;
	html->bold		= FALSE;
	html->italic		= FALSE;
	html->flash		= FALSE;
	html->span		= FALSE;
	html->link		= FALSE;

	return TRUE;
}

static vbi_bool
export(vbi_export *e, FILE *fp, vbi_page *pgp)
{
	html_instance *html = PARENT(e, html_instance, export);
	int endian = vbi_ucs2be();
	vbi_page pg;
	vbi_char *acp;
	int i, j;

	if (endian < 0) {
		vbi_export_error_printf(&html->export, _("Character conversion failed."));
		return FALSE;
	}

	pg = *pgp;

#if TEST
	html->underline	= FALSE;
	html->bold	= FALSE;
	html->italic	= FALSE;
	html->flash	= FALSE;
#endif

	html->styles = &html->def;
	html->def.next = NULL;
	html->def.ref_count = 2;
	html->def.foreground = html->foreground;
	html->def.background = html->background;
	html->def.flash = FALSE;

	for (acp = pg.text, i = 0; i < pg.rows; acp += pg.columns, i++) {
		int blank = 0;

		for (j = 0; j < pg.columns; j++) {
			int unicode = (acp[j].conceal && !e->reveal) ?
				0x0020 : acp[j].unicode;
#if TEST
			acp[j].underline = underline;
			acp[j].bold	 = bold;
			acp[j].italic	 = italic;
			acp[j].flash	 = flash;

			if ((rand() & 15) == 0)
				html->underline = rand() & 1;
			if ((rand() & 15) == 1)
				html->bold	  = rand() & 1;
			if ((rand() & 15) == 2)
				html->italic = rand() & 1;
			if ((rand() & 15) == 3)
				html->flash  = rand() & 1;
#endif
			if (acp[j].size > VBI_DOUBLE_SIZE)
				unicode = 0x0020;

			if (unicode == 0x0020 || unicode == 0x00A0) {
				blank++;
				continue;
			}

			if (blank > 0) {
				vbi_char ac = acp[j];

				ac.unicode = 0x0020;

				/* XXX should match fg and bg transitions */
				while (blank > 0) {
					ac.background = acp[j - blank].background;
					ac.link = acp[j - blank].link;
					acp[j - blank] = ac;
					blank--;
				}
			}

			acp[j].unicode = unicode;
		}

		if (blank > 0) {
			vbi_char ac;

			if (blank < pg.columns)
				ac = acp[pg.columns - 1 - blank];
			else {
				memset(&ac, 0, sizeof(ac));
				ac.foreground = 7;
			}

			ac.unicode = 0x0020;

			while (blank > 0) {
				ac.background = acp[pg.columns - blank].background;
				ac.link = acp[pg.columns - blank].link;
				acp[pg.columns - blank] = ac;
				blank--;
			}
		}

		for (j = 0; j < pg.columns; j++) {
			vbi_char ac = acp[j];
			style *s, **sp;

			for (sp = &html->styles; (s = *sp); sp = &s->next) {
				if (s->background != ac.background
				    || ac.flash != s->flash)
					continue;
				if (ac.unicode == 0x0020 || s->foreground == ac.foreground)
					break;
			}

			if (!s) {
				s = calloc(1, sizeof(style));
				*sp = s;
				s->foreground = ac.foreground;
				s->background = ac.background;
				s->flash = ac.flash;
			}

			s->ref_count++;
		}
	}

	html->fp = fp;

	if (!header(html, &pg))
		return FALSE;

	fputs("<pre>", html->fp);

	html->underline  = FALSE;
	html->bold	 = FALSE;
	html->italic     = FALSE;
	html->flash      = FALSE;
	html->span	 = FALSE;
	html->link	 = FALSE;

	/* XXX this can get extremely large and ugly, should be improved. */
	for (acp = pg.text, i = 0; i < pg.rows; acp += pg.columns, i++) {
		for (j = 0; j < pg.columns; j++) {
			if ((html->color
			     && ((acp[j].unicode != 0x0020
				  && acp[j].foreground != html->foreground)
				 || acp[j].background != html->background))
			    || html->link != acp[j].link
			    || html->flash != acp[j].flash) {
				style *s;
				int ord;

				if (html->italic)
					fputs(html_italic[0], html->fp);
				if (html->bold)
					fputs(html_bold[0], html->fp);
				if (html->underline)
					fputs(html_underline[0], html->fp);
				if (html->span)
					fputs("</span>", html->fp);
				if (html->link && !acp[j].link) {
					fputs("</a>", html->fp);
					html->link = FALSE;
				}

				html->underline  = FALSE;
				html->bold	 = FALSE;
				html->italic     = FALSE;

				if (acp[j].link && !html->link) {
					vbi_link link;

					vbi_resolve_link(pgp, j, i, &link);

					switch (link.type) {
					case VBI_LINK_HTTP:
					case VBI_LINK_FTP:
					case VBI_LINK_EMAIL:
						fprintf(html->fp, "<a href=\"%s\">", link.url);
						html->link = TRUE;

					default:
						break;
					}
				}

				if (html->color) {
					for (s = html->styles, ord = 0; s; s = s->next)
						if (s->ref_count > 1) {
							if ((acp[j].unicode == 0x0020
							     || s->foreground == acp[j].foreground)
							    && s->background == acp[j].background
							    && s->flash == acp[j].flash)
								break;
							ord++;
						}

					if (s != &html->def) {
						if (s && !html->headerless) {
							html->foreground = s->foreground;
							html->background = s->background;
							html->flash = s->flash;
							fprintf(html->fp, "<span class=\"c%d\">", ord);
						} else {
							html->foreground = acp[j].foreground;
							html->background = acp[j].background;
							html->flash = s->flash;
							fputs("<span style=\"color:", html->fp);
							hash_color(html->fp, pg.color_map[html->foreground]);
							fputs(";background-color:", html->fp);
							hash_color(html->fp, pg.color_map[html->background]);
							if (html->flash)
								fputs("; text-decoration: blink", html->fp);
							fputs("\">", html->fp);
						}
						
						html->span = TRUE;
					} else {
						html->foreground = s->foreground;
						html->background = s->background;
						html->flash = s->flash;
						html->span = FALSE;
					}
				}
			}

			if (acp[j].underline != html->underline) {
				html->underline = acp[j].underline;
				fputs(html_underline[html->underline], html->fp);
			}

			if (acp[j].bold != html->bold) {
				html->bold = acp[j].bold;
				fputs(html_bold[html->bold], html->fp);
			}

			if (acp[j].italic != html->italic) {
				html->italic = acp[j].italic;
				fputs(html_italic[html->italic], html->fp);
			}

			if (vbi_is_print(acp[j].unicode)) {
				char in[2], out[1], *ip = in, *op = out;
				size_t li = sizeof(in), lo = sizeof(out);

				in[0 + endian] = acp[j].unicode;
				in[1 - endian] = acp[j].unicode >> 8;

				if (iconv(html->cd, (void *) &ip, &li, (void *) &op, &lo) == -1
				    || (out[0] == 0x40 && acp[j].unicode != 0x0040))
					fprintf(html->fp, "&#%u;", acp[j].unicode);
				else
					escaped_fputc(html->fp, out[0]);
			} else if (vbi_is_gfx(acp[j].unicode)) {
				putc(html->gfx_chr, html->fp);
			} else {
				putc(0x20, html->fp);
			}
		}

		putc('\n', html->fp);
	}

	if (html->italic)
		fputs(html_italic[0], html->fp);
	if (html->bold)
		fputs(html_bold[0], html->fp);
	if (html->underline)
		fputs(html_underline[0], html->fp);
	if (html->span)
		fputs("</span>", html->fp);
	if (html->link)
		fputs("</a>", html->fp);

	fputs("</pre>", html->fp);

	{
		style *s;

		while ((s = html->styles)) {
			html->styles = s->next;
			if (s != &html->def)
				free(s);
		}
	}

	if (!html->headerless)
		fputs(LF "</body>" LF "</html>", html->fp);

	putc('\n', html->fp);

	iconv_close(html->cd);

	if (ferror(html->fp)) {
		vbi_export_write_error(e);
		return FALSE;
	}

	return TRUE;
}

static vbi_export_info
info_html = {
	.keyword	= "html",
	.label		= N_("HTML"),
	.tooltip	= N_("Export this page as HTML page"),

	.mime_type	= "text/html",
	.extension	= "html,htm",
};

vbi_export_class
vbi_export_class_html = {
	._public		= &info_html,
	._new			= html_new,
	._delete		= html_delete,
	.option_enum		= option_enum,
	.option_get		= option_get,
	.option_set		= option_set,
	.export			= export
};

VBI_AUTOREG_EXPORT_MODULE(vbi_export_class_html)
