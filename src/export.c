/*
 *  libzvbi - Export modules
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

/* $Id: export.c,v 1.23 2006/02/10 06:25:37 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <iconv.h>

#include "export.h"
#include "vbi.h" /* vbi_asprintf */

extern const char _zvbi_intl_domainname[];

/**
 * @addtogroup Export Exporting formatted Teletext and Closed Caption pages
 * @ingroup HiDec
 * 
 * Once libzvbi received, decoded and formatted a Teletext or Closed
 * Caption page you will want to render it on screen, print it as
 * text or store it in various formats.
 *
 * Fortunately you don't have to do it all by yourself. libzvbi provides
 * export modules converting a vbi_page into the desired format or
 * rendering directly into memory.
 *
 * A minimalistic export example:
 *
 * @code
 * static void
 * export_my_page (vbi_page *pg)
 * {
 *         vbi_export *ex;
 *         char *errstr;
 *
 *         if (!(ex = vbi_export_new ("html", &errstr))) {
 *                 fprintf (stderr, "Cannot export as HTML: %s\n", errstr);
 *                 free (errstr);
 *                 return;
 *         }
 *
 *         if (!vbi_export_file (ex, "my_page.html", pg))
 *                 puts (vbi_export_errstr (ex));
 *
 *         vbi_export_delete (ex);
 * }
 * @endcode
 */

/**
 * @addtogroup Exmod Internal export module interface
 * @ingroup Export
 *
 * This is the private interface between the public libzvbi export
 * functions and export modules. libzvbi client applications
 * don't use this.
 *
 * Export modules @c #include @c "export.h" to get these
 * definitions. See example module exp-templ.c.
 */

/**
 * @addtogroup Render Teletext and Closed Caption page render functions
 * @ingroup Export
 * 
 * These are functions to render Teletext and Closed Caption pages
 * directly into memory, essentially a more direct interface to the
 * functions of some important export modules.
 */

static vbi_bool initialized;
static vbi_export_class *vbi_export_modules;

/**
 * @param new_module Static pointer to initialized vbi_export_class structure.
 * 
 * Registers a new export module.
 */
void
vbi_register_export_module(vbi_export_class *new_module)
{
	vbi_export_class **xcp;

	if (0)
		fprintf(stderr, "libzvbi:vbi_register_export_module(\"%s\")\n",
		        new_module->_public->keyword);

	for (xcp = &vbi_export_modules; *xcp; xcp = &(*xcp)->next)
		if (strcmp(new_module->_public->keyword, (*xcp)->_public->keyword) < 0)
			break;

	new_module->next = *xcp;
	*xcp = new_module;
}

extern vbi_export_class vbi_export_class_ppm;
extern vbi_export_class vbi_export_class_png;
extern vbi_export_class vbi_export_class_html;
extern vbi_export_class vbi_export_class_tmpl;
extern vbi_export_class vbi_export_class_text;
extern vbi_export_class vbi_export_class_vtx;

/* AUTOREG not reliable, sigh. */
static void
initialize(void)
{
	static vbi_export_class *modules[] = {
		&vbi_export_class_ppm,
#ifdef HAVE_LIBPNG
		&vbi_export_class_png,
#endif
		&vbi_export_class_html,
		&vbi_export_class_text,
		&vbi_export_class_vtx,
		NULL,
		&vbi_export_class_tmpl,
	};

	vbi_export_class **xcp;

	pthread_once (&vbi_init_once, vbi_init);

	if (!vbi_export_modules)
		for (xcp = modules; *xcp; xcp++)
			vbi_register_export_module(*xcp);

	initialized = TRUE;
}

/**
 * Helper function for export modules, since iconv seems
 * undecided what it really wants (not every iconv supports
 * UCS-2LE/BE).
 * 
 * @return 
 * 1 if iconv "UCS-2" is BE on this machine, 0 if LE,
 * -1 if unknown.
 */
/* XXX provide a vbi_iconv wrapper. */
int
vbi_ucs2be(void)
{
	iconv_t cd;
	char c = 'b', *cp = &c;
	char uc[2] = { 'a', 'a' }, *up = uc;
	size_t in = sizeof(c), out = sizeof(uc);
	int endianess = -1;

	if ((cd = iconv_open("UCS-2", /* from */ "ISO-8859-1")) != (iconv_t) -1) {
		iconv(cd, (void *) &cp, &in, (void *) &up, &out);

		if (uc[0] == 0 && uc[1] == 'b')
			endianess = 1;
		else if (uc[1] == 0 && uc[0] == 'b')
			endianess = 0;

		iconv_close(cd);
	}

	return endianess;
}

/*
 *  This is old stuff, we'll see if it's still needed.
 */

#if 0 

static char *
hexnum(char *buf, unsigned int num)
{
    char *p = buf + 5;

    num &= 0xffff;
    *--p = 0;
    do
    {
	*--p = "0123456789abcdef"[num % 16];
	num /= 16;
    } while (num);
    return p;
}

static char *
adjust(char *p, char *str, char fill, int width, int deq)
{
    int c, l = width - strlen(str);

    while (l-- > 0)
	*p++ = fill;
    while ((c = *str++)) {
	if (deq && strchr(" /?*", c))
    	    c = '_';
	*p++ = c;
    }
    return p;
}

char *
vbi_export_mkname(vbi_export *e, char *fmt,
	int pgno, int subno, char *usr)
{
    char bbuf[1024];
    char *s = bbuf;

    while ((*s = *fmt++))
	if (*s++ == '%')
	{
	    char buf[32], buf2[32];
	    int width = 0;

	    s--;
	    while (*fmt >= '0' && *fmt <= '9')
		width = width*10 + *fmt++ - '0';

	    switch (*fmt++)
	    {
		case '%':
		    s = adjust(s, "%", '%', width, 0);
		    break;
		case 'e':	// extension
		    s = adjust(s, e->mod->extension, '.', width, 1);
		    break;
		case 'n':	// network label
		    s = adjust(s, e->network.label, ' ', width, 1);
		    break;
		case 'p':	// pageno[.subno]
		    if (subno)
			s = adjust(s,strcat(strcat(hexnum(buf, pgno),
				"-"), hexnum(buf2, subno)), ' ', width, 0);
		    else
			s = adjust(s, hexnum(buf, pgno), ' ', width, 0);
		    break;
		case 'S':	// subno
		    s = adjust(s, hexnum(buf, subno), '0', width, 0);
		    break;
		case 'P':	// pgno
		    s = adjust(s, hexnum(buf, pgno), '0', width, 0);
		    break;
		case 's':	// user strin
		    s = adjust(s, usr, ' ', width, 0);
		    break;
		//TODO: add date, ...
	    }
	}
    s = strdup(bbuf);
    if (! s)
	vbi_export_error_printf(e, "out of memory");
    return s;
}

#endif /* OLD STUFF */

static vbi_option_info
generic_options[] = {
	VBI_OPTION_STRING_INITIALIZER
	  ("creator", NULL, PACKAGE " " VERSION, NULL),
	VBI_OPTION_STRING_INITIALIZER
	  ("network", NULL, "", NULL),
	VBI_OPTION_BOOL_INITIALIZER
	  ("reveal", NULL, FALSE, NULL)
};

#define GENERIC (sizeof(generic_options) / sizeof(generic_options[0]))

static void
reset_error(vbi_export *e)
{
	if (e->errstr) {
		free(e->errstr);
		e->errstr = NULL;
	}
}

/**
 * @param index Index into the export module list, 0 ... n.
 *
 * Enumerates all available export modules. You should start at index 0,
 * incrementing.
 *
 * Some modules may depend on machine features or the presence of certain
 * libraries, thus the list can vary from session to session.
 *
 * @return
 * Static pointer to a vbi_export_info structure (no need to be freed),
 * @c NULL if the index is out of bounds.
 */
vbi_export_info *
vbi_export_info_enum(int index)
{
	vbi_export_class *xc;

	if (!initialized)
		initialize();

	for (xc = vbi_export_modules; xc && index > 0; xc = xc->next, index--);

	return xc ? xc->_public : NULL;
}

/**
 * @param keyword Export module identifier as in vbi_export_info and
 *           vbi_export_new().
 * 
 * Similar to vbi_export_info_enum(), but this function attempts to find an
 * export module by keyword.
 * 
 * @return
 * Static pointer to a vbi_export_info structure, @c NULL if the named export
 * module has not been found.
 */
vbi_export_info *
vbi_export_info_keyword(const char *keyword)
{
	vbi_export_class *xc;
	int keylen;

	if (!keyword)
		return NULL;

	if (!initialized)
		initialize();

	for (keylen = 0; keyword[keylen]; keylen++)
		if (keyword[keylen] == ';' || keyword[keylen] == ',')
			break;

	for (xc = vbi_export_modules; xc; xc = xc->next)
		if (strncmp(keyword, xc->_public->keyword, keylen) == 0)
			return xc->_public;

	return NULL;
}

/**
 * @param export Pointer to a vbi_export object previously allocated with
 *   vbi_export_new().
 *
 * Returns the export module info for the given @a export object.
 *
 * @return
 * A static vbi_export_info pointer or @c NULL if @a export
 * is @c NULL.
 */
vbi_export_info *
vbi_export_info_export(vbi_export *export)
{
	if (!export)
		return NULL;

	return export->_class->_public;
}

static void
reset_options(vbi_export *e)
{
	vbi_option_info *oi;
	int i;

	for (i = 0; (oi = vbi_export_option_info_enum(e, i)); i++)
		switch (oi->type) {
		case VBI_OPTION_BOOL:
		case VBI_OPTION_INT:
			if (oi->menu.num)
				vbi_export_option_set(e, oi->keyword, oi->menu.num[oi->def.num]);
			else
				vbi_export_option_set(e, oi->keyword, oi->def.num);
			break;

		case VBI_OPTION_REAL:
			if (oi->menu.dbl)
				vbi_export_option_set(e, oi->keyword, oi->menu.dbl[oi->def.num]);
			else
				vbi_export_option_set(e, oi->keyword, oi->def.dbl);
			break;

		case VBI_OPTION_STRING:
			if (oi->menu.str)
				vbi_export_option_set(e, oi->keyword, oi->menu.str[oi->def.num]);
			else
				vbi_export_option_set(e, oi->keyword, oi->def.str);
			break;

		case VBI_OPTION_MENU:
			vbi_export_option_set(e, oi->keyword, oi->def.num);
			break;

		default:
			fprintf(stderr,	"%s: unknown export option type %d\n",
				__PRETTY_FUNCTION__, oi->type);
			exit(EXIT_FAILURE);
		}
}

static vbi_bool
option_string(vbi_export *e, const char *s2)
{
	vbi_option_info *oi;
	char *s, *s1, *keyword, *string, quote;
	vbi_bool r = TRUE;

	if (!(s = s1 = vbi_export_strdup(e, NULL, s2)))
		return FALSE;

	do {
		while (isspace(*s))
			s++;

		if (*s == ',' || *s == ';') {
			s++;
			continue;
		}

		if (!*s) {
			free(s1);
			return TRUE;
		}

		keyword = s;

		while (isalnum(*s) || *s == '_')
			s++;

		if (!*s)
			goto invalid;

		*s++ = 0;

		while (isspace(*s) || *s == '=')
			s++;

		if (!*s) {
 invalid:
			vbi_export_error_printf(e, _("Invalid option string \"%s\"."), s2);
			break;
		}

		if (!(oi = vbi_export_option_info_keyword(e, keyword)))
			break;

		switch (oi->type) {
		case VBI_OPTION_BOOL:
		case VBI_OPTION_INT:
		case VBI_OPTION_MENU:
			r = vbi_export_option_set(e, keyword, (int) strtol(s, &s, 0));
			break;

		case VBI_OPTION_REAL:
			r = vbi_export_option_set(e, keyword, (double) strtod(s, &s));
			break;

		case VBI_OPTION_STRING:
			quote = 0;
			if (*s == '\'' || *s == '\"')
				quote = *s++;
			string = s;

			while (*s && *s != quote
			       && (quote || (*s != ',' && *s != ';')))
				s++;
			if (*s)
				*s++ = 0;

			r = vbi_export_option_set(e, keyword, string);
			break;

		default:
			fprintf(stderr, "%s: unknown export option type %d\n",
				__PRETTY_FUNCTION__, oi->type);
			exit(EXIT_FAILURE);
		}

	} while (r);

	free(s1);

	return FALSE;
}

/**
 * @param keyword Export module identifier as in vbi_export_info.
 * @param errstr If not @c NULL this function stores a pointer to an error
 *   description here. You must free() this string when no longer needed.
 * 
 * Creates a new export module instance to export a vbi_page in
 * the respective module format. As a special service you can
 * initialize options by appending to the @param keyword like this:
 * 
 * @code
 * vbi_export_new ("keyword; quality=75.5, comment=\"example text\"");
 * @endcode
 * 
 * @return
 * Pointer to a newly allocated vbi_export object which must be
 * freed by calling vbi_export_delete(). @c NULL is returned and
 * the @a errstr may be set (else @a NULL) if some problem occurred.
 */
vbi_export *
vbi_export_new(const char *keyword, char **errstr)
{
	char key[256];
	vbi_export_class *xc;
	vbi_export *e;
	unsigned int keylen;

	if (!initialized)
		initialize();

	if (!keyword)
		keyword = "";

	for (keylen = 0; keyword[keylen] && keylen < (sizeof(key) - 1)
		     && keyword[keylen] != ';' && keyword[keylen] != ','; keylen++)
		     key[keylen] = keyword[keylen];
	key[keylen] = 0;

	for (xc = vbi_export_modules; xc; xc = xc->next)
		if (strcmp(key, xc->_public->keyword) == 0)
			break;

	if (!xc) {
		vbi_asprintf(errstr, _("Unknown export module '%s'."), key);
		return NULL;
	}

	if (!xc->_new)
		e = calloc(1, sizeof(*e));
	else
		e = xc->_new();

	if (!e) {
		vbi_asprintf(errstr, _("Cannot initialize export module '%s', "
				       "probably lack of memory."),
			     xc->_public->label ? xc->_public->label : keyword);
		return NULL;
	}

	e->_class = xc;
	e->errstr = NULL;

	e->name = NULL;

	reset_options(e);

	if (keyword[keylen] && !option_string(e, keyword + keylen + 1)) {
		if (errstr)
			*errstr = strdup(vbi_export_errstr(e));
		vbi_export_delete(e);
		return NULL;
	}

	if (errstr)
		errstr = NULL;

	return e;
}

/**
 * @param export Pointer to a vbi_export object previously allocated with
 *	     vbi_export_new(). Can be @c NULL.
 * 
 * This function frees all resources associated with the vbi_export
 * object.
 */
void
vbi_export_delete(vbi_export *export)
{
	vbi_export_class *xc;

	if (!export)
		return;

	if (export->errstr)
		free(export->errstr);

	if (export->network)
		free(export->network);
	if (export->creator)
		free(export->creator);

	xc = export->_class;

	if (xc->_new && xc->_delete)
		xc->_delete(export);
	else
		free(export);
}

/**
 * @param export Pointer to a initialized vbi_export object.
 * @param index Index in the option table 0 ... n.
 *
 * Enumerates the options available for the given export module. You
 * should start at index 0, incrementing.
 *
 * @return Static pointer to a vbi_option_info structure,
 * @c NULL if @a index is out of bounds.
 */
/* XXX unsigned index */
vbi_option_info *
vbi_export_option_info_enum(vbi_export *export, int index)
{
	vbi_export_class *xc;

	if (!export)
		return NULL;

	reset_error(export);

	if (index < (int) GENERIC)
		return generic_options + index;

	xc = export->_class;

	if (xc->option_enum)
		return xc->option_enum(export, index - GENERIC);
	else
		return NULL;
}

/**
 * @param export Pointer to a initialized vbi_export object.
 * @param keyword Keyword of the option as in vbi_option_info.
 *
 * Similar to vbi_export_option_info_enum(), but tries to find the
 * option info based on the given keyword.
 *
 * @return Static pointer to a vbi_option_info structure,
 * @c NULL if the keyword wasn't found.
 */
vbi_option_info *
vbi_export_option_info_keyword(vbi_export *export, const char *keyword)
{
	vbi_export_class *xc;
	vbi_option_info *oi;
	unsigned int i;

	if (!export || !keyword)
		return NULL;

	reset_error(export);

	for (i = 0; i < GENERIC; i++)
		if (strcmp(keyword, generic_options[i].keyword) == 0)
			return generic_options + i;

	xc = export->_class;

	if (!xc->option_enum)
		return NULL;

	for (i = 0; (oi = xc->option_enum(export, i)); i++)
		if (strcmp(keyword, oi->keyword) == 0)
			return oi;

	vbi_export_unknown_option(export, keyword);

	return NULL;
}

/**
 * @param export Pointer to a initialized vbi_export object.
 * @param keyword Keyword identifying the option, as in vbi_option_info.
 * @param value A place to store the current option value.
 *
 * This function queries the current value of the named option.
 * When the option is of type VBI_OPTION_STRING @a value.str must be
 * freed with free() when you don't need it any longer. When the
 * option is of type VBI_OPTION_MENU then @a value.num contains the
 * selected entry.
 *
 * @return @c TRUE on success, otherwise @a value unchanged.
 */
vbi_bool
vbi_export_option_get(vbi_export *export, const char *keyword,
		      vbi_option_value *value)
{
	vbi_export_class *xc;
	vbi_bool r = TRUE;

	if (!export || !keyword || !value)
		return FALSE;

	reset_error(export);

	if (strcmp(keyword, "reveal") == 0) {
		value->num = export->reveal;
	} else if (strcmp(keyword, "network") == 0) {
		if (!(value->str = vbi_export_strdup(export, NULL,
						     export->network ? : "")))
			return FALSE;
	} else if (strcmp(keyword, "creator") == 0) {
		if (!(value->str = vbi_export_strdup(export, NULL, export->creator)))
			return FALSE;
	} else {
		xc = export->_class;

		if (xc->option_get)
			r = xc->option_get(export, keyword, value);
		else {
			vbi_export_unknown_option(export, keyword);
			r = FALSE;
		}
	}

	return r;
}

/**
 * @param export Pointer to a initialized vbi_export object.
 * @param keyword Keyword identifying the option, as in vbi_option_info.
 * @param ... New value to set.
 *
 * Sets the value of the named option. Make sure the value is casted
 * to the correct type (int, double, char *).
 *
 * Typical usage of vbi_export_option_set():
 * @code
 * vbi_export_option_set (export, "quality", 75.5);
 * @endcode
 *
 * Mind that options of type @c VBI_OPTION_MENU must be set by menu
 * entry number (int), all other options by value. If necessary it will
 * be replaced by the closest value possible. Use function
 * vbi_export_option_menu_set() to set options with menu
 * by menu entry.  
 *
 * @return
 * @c TRUE on success, otherwise the option is not changed.
 */
vbi_bool
vbi_export_option_set(vbi_export *export, const char *keyword, ...)
{
	vbi_export_class *xc;
	vbi_bool r = TRUE;
	va_list args;

	if (!export || !keyword)
		return FALSE;

	reset_error(export);

	va_start(args, keyword);

	if (strcmp(keyword, "reveal") == 0) {
		export->reveal = !!va_arg(args, int);
	} else if (strcmp(keyword, "network") == 0) {
		char *network = va_arg(args, char *);
		if (!network || !network[0]) {
			if (export->network) {
				free(export->network);
				export->network = NULL;
			}
		} else if (!vbi_export_strdup(export, &export->network, network)) {
			return FALSE;
		}
	} else if (strcmp(keyword, "creator") == 0) {
		char *creator = va_arg(args, char *);
		if (!vbi_export_strdup(export, &export->creator, creator))
			return FALSE;
	} else {
		xc = export->_class;

		if (xc->option_set)
			r = xc->option_set(export, keyword, args);
		else
			r = FALSE;
	}

	va_end(args);

	return r;
}

/**
 * @param export Pointer to a initialized vbi_export object.
 * @param keyword Keyword identifying the option, as in vbi_option_info.
 * @param entry A place to store the current menu entry.
 * 
 * Similar to vbi_export_option_get() this function queries the current
 * value of the named option, but returns this value as number of the
 * corresponding menu entry. Naturally this must be an option with
 * menu.
 * 
 * @return 
 * @c TRUE on success, otherwise @a value remained unchanged.
 */
vbi_bool
vbi_export_option_menu_get(vbi_export *export, const char *keyword,
			   int *entry)
{
	vbi_option_info *oi;
	vbi_option_value val;
	vbi_bool r;
	int i;

	if (!export || !keyword || !entry)
		return FALSE;

	reset_error(export);

	if (!(oi = vbi_export_option_info_keyword(export, keyword)))
		return FALSE;

	if (!vbi_export_option_get(export, keyword, &val))
		return FALSE;

	r = FALSE;

	for (i = oi->min.num; i <= oi->max.num; i++) {
		switch (oi->type) {
		case VBI_OPTION_BOOL:
		case VBI_OPTION_INT:
			if (!oi->menu.num)
				return FALSE;
			r = (oi->menu.num[i] == val.num);
			break;

		case VBI_OPTION_REAL:
			if (!oi->menu.dbl)
				return FALSE;
			/* XXX unsafe */
			r = (oi->menu.dbl[i] == val.dbl);
			break;

		case VBI_OPTION_MENU:
			r = (i == val.num);
			break;

		default:
			fprintf(stderr,	"%s: unknown export option type %d\n",
				__PRETTY_FUNCTION__, oi->type);
			exit(EXIT_FAILURE);
		}

		if (r) {
			*entry = i;
			break;
		}
	}

	return r;
}

/**
 * @param export Pointer to a initialized vbi_export object.
 * @param keyword Keyword identifying the option, as in vbi_option_info.
 * @param entry Menu entry to be selected.
 * 
 * Similar to vbi_export_option_set() this function sets the value of
 * the named option, however it does so by number of the corresponding
 * menu entry. Naturally this must be an option with menu.
 *
 * @return 
 * @c TRUE on success, otherwise the option is not changed.
 */
vbi_bool
vbi_export_option_menu_set(vbi_export *export, const char *keyword,
			   int entry)
{
	vbi_option_info *oi;

	if (!export || !keyword)
		return FALSE;

	reset_error(export);

	if (!(oi = vbi_export_option_info_keyword(export, keyword)))
		return FALSE;

	if (entry < oi->min.num || entry > oi->max.num)
		return FALSE;

	switch (oi->type) {
	case VBI_OPTION_BOOL:
	case VBI_OPTION_INT:
		if (!oi->menu.num)
			return FALSE;
		return vbi_export_option_set(export, keyword, oi->menu.num[entry]);

	case VBI_OPTION_REAL:
		if (!oi->menu.dbl)
			return FALSE;
		return vbi_export_option_set(export, keyword, oi->menu.dbl[entry]);

	case VBI_OPTION_MENU:
		return vbi_export_option_set(export, keyword, entry);

	default:
		fprintf(stderr, "%s: unknown export option type %d\n",
			__PRETTY_FUNCTION__, oi->type);
		exit(EXIT_FAILURE);
	}
}

/**
 * @param export Pointer to a initialized vbi_export object.
 * @param fp Buffered i/o stream to write to.
 * @param pg Page to be exported.
 * 
 * This function writes the @a pg contents, converted to the respective
 * export module format, to the stream @a fp. The caller is responsible for
 * opening and closing the stream, don't forget to check for i/o
 * errors after closing. Note this function may write incomplete files
 * when an error occurs.
 *
 * You can call this function as many times as you want, it does not
 * change vbi_export state or the vbi_page.
 * 
 * @return 
 * @c TRUE on success.
 */
vbi_bool
vbi_export_stdio(vbi_export *export, FILE *fp, vbi_page *pg)
{
	vbi_bool success;

	if (!export || !fp || !pg)
		return FALSE;

	reset_error(export);
	clearerr(fp);

	success = export->_class->export(export, fp, pg);

	if (success && ferror(fp)) {
		vbi_export_write_error(export);
		success = FALSE;
	}

	return success;
}

/**
 * @param export Pointer to a initialized vbi_export object.
 * @param name File to be created.
 * @param pg Page to be exported.
 * 
 * This function writes the @a pg contents, converted to the respective
 * export format, into a new file of the given @a name. When an error
 * occured the incomplete file will be deleted.
 * 
 * You can call this function as many times as you want, it does not
 * change vbi_export state or the vbi_page.
 * 
 * @return 
 * @c TRUE on success.
 */
vbi_bool
vbi_export_file(vbi_export *export, const char *name,
		vbi_page *pg)
{
	struct stat st;
	vbi_bool success;
	FILE *fp;

	if (!export || !name || !pg)
		return FALSE;

	reset_error(export);

	if (!(fp = fopen(name, "w"))) {
		vbi_export_error_printf(export,
					_("Cannot create file '%s': %s."),
					name, strerror(errno));
		return FALSE;
	}

	export->name = (char *) name;

	success = export->_class->export(export, fp, pg);

	if (success && ferror(fp)) {
		vbi_export_write_error(export);
		success = FALSE;
	}

	if (fclose(fp))
		if (success) {
			vbi_export_write_error(export);
			success = FALSE;
		}

	if (!success && stat(name, &st) == 0 && S_ISREG(st.st_mode))
		remove(name);

	export->name = NULL;

	return success;
}

/**
 * @param export Pointer to a initialized vbi_export object.
 * @param templ See printf().
 * @param ... See printf(). 
 * 
 * Store an error description in the @a export object. Including the current
 * error description (to append or prepend) is safe.
 */
void
vbi_export_error_printf(vbi_export *export, const char *templ, ...)
{
	char buf[512];
	va_list ap;

	if (!export)
		return;

	va_start(ap, templ);
	vsnprintf(buf, sizeof(buf) - 1, templ, ap);
	va_end(ap);

	reset_error(export);

	export->errstr = strdup(buf);
}

/**
 * @param export Pointer to a initialized vbi_export object.
 * 
 * Similar to vbi_export_error_printf this function stores an error
 * description in the @a export object, after examining the errno
 * variable and choosing an appropriate message. Only export
 * modules call this function.
 */
void
vbi_export_write_error(vbi_export *export)
{
	char *t, buf[256];

	if (!export)
		return;

	if (export->name)
		snprintf(t = buf, sizeof(buf),
			_("Error while writing file '%s'"), export->name);
	else
 		t = _("Error while writing file");

	if (errno) {
		vbi_export_error_printf(export, "%s: Error %d, %s", t,
					errno, strerror(errno));
	} else {
		vbi_export_error_printf(export, "%s.", t);
	}
}

static char *
module_name			(vbi_export *		export)
{
	vbi_export_class *xc = export->_class;

	if (xc->_public->label)
		return _(xc->_public->label);
	else
		return xc->_public->keyword;
}

/**
 * @param export Pointer to a initialized vbi_export object.
 * @param keyword Name of the unknown option.
 * 
 * Store an error description in the @a export object.
 */
void
vbi_export_unknown_option(vbi_export *export, const char *keyword)
{
	vbi_export_error_printf (export, _("Export module '%s' has no option '%s'."),
				 module_name (export), keyword);
}

/**
 * @param export Pointer to a initialized vbi_export object.
 * @param keyword Name of the unknown option.
 * @param ... Invalid value, type depending on the option.
 * 
 * Store an error description in the @a export object.
 */
void
vbi_export_invalid_option(vbi_export *export, const char *keyword, ...)
{
	char buf[256];
	vbi_option_info *oi;

	if ((oi = vbi_export_option_info_keyword(export, keyword))) {
		va_list args;
		char *s;

		va_start(args, keyword);

		switch (oi->type) {
		case VBI_OPTION_BOOL:
		case VBI_OPTION_INT:
		case VBI_OPTION_MENU:
			snprintf(buf, sizeof(buf) - 1, "'%d'", va_arg(args, int));
			break;
		case VBI_OPTION_REAL:
			snprintf(buf, sizeof(buf) - 1, "'%f'", va_arg(args, double));
			break;
		case VBI_OPTION_STRING:
			s = va_arg(args, char *);
			if (s == NULL)
				strcpy(buf, "NULL");
			else
				snprintf(buf, sizeof(buf) - 1, "'%s'", s);
			break;
		default:
			fprintf(stderr, "%s: unknown export option type %d\n",
				__PRETTY_FUNCTION__, oi->type);
			strcpy(buf, "?");
			break;
		}

		va_end(args);
	} else
		buf[0] = 0;

	vbi_export_error_printf (export, _("Invalid argument %s for option %s of export module %s."),
				 buf, keyword, module_name (export));
}

/**
 * @param export Pointer to a initialized vbi_export object.
 * @param d If non-zero, store pointer to allocated string here. When *d
 *   is non-zero, free(*d) the old string first.
 * @param s String to be duplicated.
 * 
 * Helper function for export modules.
 *
 * Same as the libc strdup(), except for @a d argument and setting
 * the @a export error string on failure.
 * 
 * @return 
 * @c NULL on failure, pointer to malloc()ed string otherwise.
 */
char *
vbi_export_strdup(vbi_export *export, char **d, const char *s)
{
	char *new = strdup(s ? s : "");

	if (!new) {
		vbi_export_error_printf (export, _("Out of memory in export module '%s'."),
					 module_name (export));
		errno = ENOMEM;
		return NULL;
	}

	if (d) {
		if (*d)
			free(*d);
		*d = new;
	}

	return new;
}

/**
 * @param export Pointer to a initialized vbi_export object.
 * 
 * @return 
 * After an export function failed, this function returns a pointer
 * to a more detailed error description. Do not free this string. It
 * remains valid until the next call of an export function.
 */
char *
vbi_export_errstr(vbi_export *export)
{
	if (!export || !export->errstr)
		return _("Unknown error.");

	return export->errstr;
}
