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

/* $Id: export.h,v 1.5 2002/06/25 04:37:10 mschimek Exp $ */

#ifndef EXPORT_H
#define EXPORT_H

#include "bcd.h" /* vbi_bool */
#include "event.h" /* vbi_network */
#include "format.h" /* vbi_page */

/* Public */

#include <stdio.h> /* FILE */

/**
 * vbi_export:
 *
 * One instance of an export module, an opaque object. You can
 * allocate any number of instances you need with vbi_export_new().
 **/
typedef struct vbi_export vbi_export;

/**
 * vbi_export_info:
 *
 * Although export modules can be accessed by a static keyword (see
 * vbi_export_new()) they are by definition opaque. The client
 * can list export modules for the user and manipulate them without knowing
 * about their presence or purpose. To do so, some amount of information
 * about the module is necessary, given in this structure.
 *
 * You can obtain this information with vbi_export_info_enum().
 *
 * @keyword: Unique (within this library) keyword to identify
 *   this export module. Can be stored in configuration files.
 * @label: Name of the export module to be shown to the user.
 *   This can be %NULL to indicate this module shall not be listed.
 * @tooltip: A brief description (or %NULL) for the user.
 * @mime_type: Description of the export format as MIME type,
 *   for example "text/html". May be %NULL.
 * @extension: Suggested filename extension. Multiple strings are
 *   possible, separated by comma. The first string is preferred.
 *   Example: "html,htm". May be %NULL.
 **/
typedef struct vbi_export_info {
	char *			keyword;
	char *			label;		/* or NULL, gettext()ized N_() */
	char *			tooltip;	/* or NULL, gettext()ized N_() */

	char *			mime_type;	/* or NULL */
	char *			extension;	/* or NULL */
} vbi_export_info;

/**
 * vbi_option_type:
 * @VBI_OPTION_BOOL:
 *   A boolean value, either %TRUE (1) or %FALSE (0).
 *   <informaltable frame=none><tgroup cols=2><tbody>
 *   <row><entry>Type:</><entry>int</></row>
 *   <row><entry>Default:</><entry>def.num</></row>
 *   <row><entry>Bounds:</><entry>min.num (0) ... max.num (1), step.num (1)</></row>
 *   <row><entry>Menu:</><entry>%NULL</></row>
 *   </tbody></tgroup></informaltable>
 * @VBI_OPTION_INT:
 *   A signed integer value. When only a few discrete values rather than
 *   a range are permitted @menu points to a vector of integers. Note the
 *   option is still set by value, not by menu index, which may be rejected
 *   or replaced by the closest possible.
 *   <informaltable frame=none><tgroup cols=2><tbody>
 *   <row><entry>Type:</><entry>int</></row>
 *   <row><entry>Default:</><entry>def.num or menu.num[def.num]</></row>
 *   <row><entry>Bounds:</><entry>min.num ... max.num, step.num or menu</></row>
 *   <row><entry>Menu:</><entry>%NULL or menu.num[min.num ... max.num], step.num (1)</></row>
 *   </tbody></tgroup></informaltable>
 * @VBI_OPTION_REAL:
 *   A real value, optional a vector of possible values.
 *   <informaltable frame=none><tgroup cols=2><tbody>
 *   <row><entry>Type:</><entry>double</></row>
 *   <row><entry>Default:</><entry>def.dbl or menu.dbl[def.num]</></row>
 *   <row><entry>Bounds:</><entry>min.dbl ... max.dbl, step.dbl or menu</></row>
 *   <row><entry>Menu:</><entry>%NULL or menu.dbl[min.num ... max.num], step.num (1)</></row>
 *   </tbody></tgroup></informaltable>
 * @VBI_OPTION_STRING:
 *   A null terminated string. Note the menu version differs from
 *   VBI_OPTION_MENU in its argument, which is the string itself. For example:
 *   <programlisting>
 *   menu.str[0] = "red"
 *   menu.str[1] = "blue"
 *   ... and perhaps other colors not explicitely listed
 *   </programlisting>
 *   <informaltable frame=none><tgroup cols=2><tbody>
 *   <row><entry>Type:</><entry>char *</></row>
 *   <row><entry>Default:</><entry>def.str or menu.str[def.num]</></row>
 *   <row><entry>Bounds:</><entry>not applicable</></row>
 *   <row><entry>Menu:</><entry>%NULL or menu.str[min.num ... max.num], step.num (1)</></row>
 *   </tbody></tgroup></informaltable>
 * @VBI_OPTION_MENU:
 *   Choice between a number of named options. For example:
 *   <programlisting>
 *   menu.str[0] = "up"
 *   menu.str[1] = "down"
 *   menu.str[2] = "strange"
 *   </programlisting>
 *   <informaltable frame=none><tgroup cols=2><tbody>
 *   <row><entry>Type:</><entry>int</></row>
 *   <row><entry>Default:</><entry>def.num</></row>
 *   <row><entry>Bounds:</><entry>min.num ... max.num, step.num (1)</></row>
 *   <row><entry>Menu:</><entry>menu.str[min.num ... max.num], step.num (1).
 *      These strings are gettext'ized N_(), see the gettext() manuals for details.</></row>
 *   </tbody></tgroup></informaltable>
 **/
typedef enum {
	VBI_OPTION_BOOL = 1,
	VBI_OPTION_INT,
	VBI_OPTION_REAL,
	VBI_OPTION_STRING,
	VBI_OPTION_MENU
} vbi_option_type;

typedef union vbi_option_value {
	int			num;
	double			dbl;
	char *			str;
} vbi_option_value;

typedef union vbi_option_value_ptr {
	int *			num;
	double *		dbl;
	char **			str;
} vbi_option_value_ptr;

/**
 * vbi_option_info:
 *
 * Although export options can be accessed by a static keyword (see
 * vbi_export_option_set()) they are by definition opaque. The client
 * can present them to the user and manipulate them without knowing
 * about their presence or purpose. To do so, some amount of information
 * about the option is necessary, given in this structure.
 *
 * You can obtain this information with vbi_export_option_info_enum().
 *
 * @type: Type of the option, see #vbi_option_type for details.
 *
 * @keyword: Unique (within this export module) keyword to identify
 *   this option. Can be stored in configuration files.
 *
 * @label: Name of the option to be shown to the user.
 *   This can be %NULL to indicate this option shall not be listed.
 *   The string can be translated with gettext(), see the gettext manual.
 *
 * @def, @min, @max, @step, @menu: See #vbi_option_type for details.
 *
 * @tooltip: A brief description (or %NULL) for the user.
 *   The string can be translated with gettext(), see the gettext manual.
 **/
typedef struct vbi_option_info {
	vbi_option_type		type;
	char *			keyword;
	char *			label;
	vbi_option_value	def;
	vbi_option_value	min;
	vbi_option_value	max;
	vbi_option_value	step;
	vbi_option_value_ptr	menu;
	char *			tooltip;
} vbi_option_info;

extern vbi_export_info *	vbi_export_info_enum(int index);
extern vbi_export_info *	vbi_export_info_keyword(const char *keyword);
extern vbi_export_info *	vbi_export_info_export(vbi_export *);

extern vbi_export *		vbi_export_new(const char *keyword, char **errstr);
extern void			vbi_export_delete(vbi_export *);

extern vbi_option_info *	vbi_export_option_info_enum(vbi_export *, int index);
extern vbi_option_info *	vbi_export_option_info_keyword(vbi_export *, const char *keyword);

extern vbi_bool			vbi_export_option_set(vbi_export *, const char *keyword, ...);
extern vbi_bool			vbi_export_option_get(vbi_export *, const char *keyword,
						      vbi_option_value *value);
extern vbi_bool			vbi_export_option_menu_set(vbi_export *, const char *keyword, int entry);
extern vbi_bool			vbi_export_option_menu_get(vbi_export *, const char *keyword, int *entry);

extern vbi_bool			vbi_export_stdio(vbi_export *, FILE *fp, vbi_page *pg);
extern vbi_bool			vbi_export_file(vbi_export *, const char *name, vbi_page *pg);

extern char *			vbi_export_errstr(vbi_export *);

/* Private */

#include <stdarg.h>
#include <stddef.h>

#ifndef _
#  ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) gettext (String)
#    ifdef gettext_noop
#      define N_(String) gettext_noop (String)
#    else
#      define N_(String) (String)
#    endif
#  else /* Stubs that do something close enough.  */
#    define gettext(Msgid) ((const char *) (Msgid))
#    define dgettext(Domainname, Msgid) ((const char *) (Msgid))
#    define dcgettext(Domainname, Msgid, Category) ((const char *) (Msgid))
#    define ngettext(Msgid1, Msgid2, N) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define dngettext(Domainname, Msgid1, Msgid2, N) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define dcngettext(Domainname, Msgid1, Msgid2, N, Category) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define textdomain(Domainname) ((const char *) (Domainname))
#    define bindtextdomain(Domainname, Dirname) ((const char *) (Dirname))
#    define bind_textdomain_codeset(Domainname, Codeset) ((const char *) (Codeset))
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#endif

typedef struct vbi_export_class vbi_export_class;

/**
 * vbi_export:
 *
 * This is the semi-public (to the library export interface, but not the
 * client) part of an export module instance. Export modules can read, but
 * do not normally write this, as it is maintained by the interface functions.
 **/
struct vbi_export {
	vbi_export_class *	class;
	char *			errstr;

	char *			name;		/* of the file we're writing,
						   NULL if unknown */
	/* Generic options */

	char *			network;	/* network name or NULL */
	char *			creator;	/* creator name [libzvbi] or NULL */
	vbi_bool		reveal;		/* reveal hidden chars */
};

/**
 * vbi_export_class:
 *
 * This is the semi-public (to the library export interface, but not the
 * client) description of the particular export module class. Export modules
 * must initialize these fields (except @next, see exp-tmpl.c for a detailed
 * discussion) and call vbi_export_register_module() to become accessible.
 **/
struct vbi_export_class {
	vbi_export_class *	next;
	vbi_export_info		public;

	vbi_export *		(* new)(void);
	void			(* delete)(vbi_export *);

	vbi_option_info *	(* option_enum)(vbi_export *, int index);
	vbi_bool		(* option_set)(vbi_export *, const char *keyword,
					       va_list);
	vbi_bool		(* option_get)(vbi_export *, const char *keyword,
					       vbi_option_value *value);

	vbi_bool		(* export)(vbi_export *, FILE *fp, vbi_page *pg);
};

/*
 *  Helper functions
 */

extern void			vbi_export_write_error(vbi_export *);
extern void			vbi_export_unknown_option(vbi_export *, const char *keyword);
extern void			vbi_export_invalid_option(vbi_export *, const char *keyword, ...);
extern char *			vbi_export_strdup(vbi_export *, char **d, const char *s);
extern void			vbi_export_error_printf(vbi_export *, const char *templ, ...);

extern void			vbi_register_export_module(vbi_export_class *);

extern int			vbi_ucs2be(void);

/* See exp-templ.c for an example on how to use these. */

#define VBI_OPTION_BOUNDS_INITIALIZER_(type_, def_, min_, max_, step_)	\
  { type_ = def_ }, { type_ = min_ }, { type_ = max_ }, { type_ = step_ }

#define VBI_OPTION_BOOL_INITIALIZER(key_, label_, def_, tip_)		\
  { VBI_OPTION_BOOL, key_, label_, VBI_OPTION_BOUNDS_INITIALIZER_(	\
  .num, def_, 0, 1, 1),	{ .num = NULL }, tip_ }

#define VBI_OPTION_INT_RANGE_INITIALIZER(key_, label_, def_, min_,	\
  max_,	step_, tip_) { VBI_OPTION_INT, key_, label_,			\
  VBI_OPTION_BOUNDS_INITIALIZER_(.num, def_, min_, max_, step_),	\
  { .num = NULL }, tip_ }

#define VBI_OPTION_INT_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, entries_, tip_) { VBI_OPTION_INT, key_, label_,		\
  VBI_OPTION_BOUNDS_INITIALIZER_(.num, def_, 0, (entries_) - 1, 1),	\
  { .num = menu_ }, tip_ }

#define VBI_OPTION_REAL_RANGE_INITIALIZER(key_, label_, def_, min_,	\
  max_, step_, tip_) { VBI_OPTION_REAL, key_, label_,			\
  VBI_OPTION_BOUNDS_INITIALIZER_(.dbl, def_, min_, max_, step_),	\
  { .dbl = NULL }, tip_ }

#define VBI_OPTION_REAL_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, entries_, tip_) { VBI_OPTION_REAL, key_, label_,		\
  VBI_OPTION_BOUNDS_INITIALIZER_(.num, def_, 0, (entries_) - 1, 1),	\
  { .dbl = menu_ }, tip_ }

#define VBI_OPTION_STRING_INITIALIZER(key_, label_, def_, tip_)		\
  { VBI_OPTION_STRING, key_, label_, VBI_OPTION_BOUNDS_INITIALIZER_(	\
  .str, def_, NULL, NULL, NULL), { .str = NULL }, tip_ }

#define VBI_OPTION_STRING_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, entries_, tip_) { VBI_OPTION_STRING, key_, label_,		\
  VBI_OPTION_BOUNDS_INITIALIZER_(.str, def_, 0, (entries_) - 1, 1),	\
  { .str = (char **)(menu_) }, tip_ }

#define VBI_OPTION_MENU_INITIALIZER(key_, label_, def_, menu_,		\
  entries_, tip_) { VBI_OPTION_MENU, key_, label_,			\
  VBI_OPTION_BOUNDS_INITIALIZER_(.num, def_, 0, (entries_) - 1, 1),	\
  { .str = (char **)(menu_) }, tip_ }

/* See exp-templ.c for an example */

/* Doesn't work, sigh. */
#define VBI_AUTOREG_EXPORT_MODULE(name)
/*
#define VBI_AUTOREG_EXPORT_MODULE(name)					\
static void vbi_autoreg_##name(void) __attribute__ ((constructor));	\
static void vbi_autoreg_##name(void) {					\
	vbi_register_export_module(&name);				\
}
*/

#endif /* EXPORT_H */
