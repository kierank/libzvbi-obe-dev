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

/* $Id: export.h,v 1.13 2007/07/23 20:01:17 mschimek Exp $ */

#ifndef EXPORT_H
#define EXPORT_H

#include "bcd.h" /* vbi_bool */
#include "event.h" /* vbi_network */
#include "format.h" /* vbi_page */

/* Public */

#include <stdio.h> /* FILE */

/**
 * @ingroup Export
 * @brief Export module instance, an opaque object.
 *
 * Allocate with vbi_export_new().
 */
typedef struct vbi_export vbi_export;

/**
 * @ingroup Export
 * @brief Information about an export module.
 *
 * Although export modules can be accessed by a static keyword (see
 * vbi_export_new()) they are by definition opaque. The client
 * can list export modules for the user and manipulate them without knowing
 * about their availability or purpose. To do so, information
 * about the module is necessary, given in this structure.
 *
 * You can obtain this information with vbi_export_info_enum().
 */
typedef struct vbi_export_info {
	/**
	 * Unique (within this library) keyword to identify
	 * this export module. Can be stored in configuration files.
	 */
	char *			keyword;
	/**
	 * Name of the export module to be shown to the user.
	 * Can be @c NULL indicating the module shall not be listed.
	 * Clients are encouraged to localize this with dgettext("zvbi", label).
	 */
	char *			label;
	/**
	 * A brief description (or @c NULL) for the user.
	 * Clients are encouraged to localize this with dgettext("zvbi", label).
	 */
	char *			tooltip;
	/**
	 * Description of the export format as MIME type,
	 * for example "text/html". May be @c NULL.
	 */
	char *			mime_type;
	/**
	 * Suggested filename extension. Multiple strings are
	 * possible, separated by comma. The first string is preferred.
	 * Example: "html,htm". May be @c NULL.
	 */
	char *			extension;
} vbi_export_info;

/**
 * @ingroup Export
 */
typedef enum {
	/**
	 * A boolean value, either @c TRUE (1) or @c FALSE (0).
	 * <table>
	 * <tr><td>Type:</td><td>int</td></tr>
	 * <tr><td>Default:</td><td>def.num</td></tr>
	 * <tr><td>Bounds:</td><td>min.num (0) ... max.num (1),
	 * step.num (1)</td></tr>
	 * <tr><td>Menu:</td><td>%NULL</td></tr>
	 * </table>
	 */
	VBI_OPTION_BOOL = 1,

	/**
	 * A signed integer value. When only a few discrete values rather than
	 * a range are permitted @p menu points to a vector of integers. Note the
	 * option is still set by value, not by menu index. Setting the value may
	 * fail, or it may be replaced by the closest possible.
	 * <table>
	 * <tr><td>Type:</td><td>int</td></tr>
	 * <tr><td>Default:</td><td>def.num or menu.num[def.num]</td></tr>
	 * <tr><td>Bounds:</td><td>min.num ... max.num, step.num or menu</td></tr>
	 * <tr><td>Menu:</td><td>%NULL or menu.num[min.num ... max.num],
	 * step.num (1)</td></tr>
	 * </table>
	 */
	VBI_OPTION_INT,

	/**
	 * A real value, optional a vector of suggested values.
	 * <table>
	 * <tr><td>Type:</td><td>double</td></tr>
	 * <tr><td>Default:</td><td>def.dbl or menu.dbl[def.num]</td></tr>
	 * <tr><td>Bounds:</td><td>min.dbl ... max.dbl,
	 * step.dbl or menu</td></tr>
	 * <tr><td>Menu:</td><td>%NULL or menu.dbl[min.num ... max.num],
	 * step.num (1)</td></tr>
	 * </table>
	 */
	VBI_OPTION_REAL,

	/**
	 * A null terminated string. Note the menu version differs from
	 * VBI_OPTION_MENU in its argument, which is the string itself. For example:
	 * @code
	 * menu.str[0] = "red"
	 * menu.str[1] = "blue"
	 * ... and the option may accept other color strings not explicitely listed
	 * @endcode
	 * <table>
	 * <tr><td>Type:</td><td>char *</td></tr>
	 * <tr><td>Default:</td><td>def.str or menu.str[def.num]</td></tr>
	 * <tr><td>Bounds:</td><td>not applicable</td></tr>
	 * <tr><td>Menu:</td><td>%NULL or menu.str[min.num ... max.num],
	 * step.num (1)</td></tr>
	 * </table>
	 */
	VBI_OPTION_STRING,

	/**
	 * Choice between a number of named options. For example:
	 * @code
	 * menu.str[0] = "up"
	 * menu.str[1] = "down"
	 * menu.str[2] = "strange"
	 * @endcode
	 * <table>
	 * <tr><td>Type:</td><td>int</td></tr>
	 * <tr><td>Default:</td><td>def.num</td></tr>
	 * <tr><td>Bounds:</td><td>min.num (0) ... max.num, 
	 *    step.num (1)</td></tr>
	 * <tr><td>Menu:</td><td>menu.str[min.num ... max.num],
	 *    step.num (1).
	 * The menu strings are nationalized N_("text"), client
	 * applications are encouraged to localize with dgettext("zvbi", menu.str[n]).
	 * For details see info gettext.
	 * </td></tr>
	 * </table>
	 */
	VBI_OPTION_MENU
} vbi_option_type;

/**
 * @ingroup Export
 * @brief Result of an option query.
 */
typedef union {
	int			num;
	double			dbl;
	char *			str;
} vbi_option_value;

/**
 * @ingroup Export
 * @brief Option menu types.
 */
typedef union {
	int *			num;
	double *		dbl;
	char **			str;
} vbi_option_value_ptr;

/**
 * @ingroup Export
 * @brief Information about an export option.
 *
 * Although export options can be accessed by a static keyword they are
 * by definition opaque: the client can present them to the user and
 * manipulate them without knowing about their presence or purpose.
 * To do so, some information about the option is necessary,
 * given in this structure.
 * 
 * You can obtain this information with vbi_export_option_info_enum().
 */
typedef struct {
  	vbi_option_type		type;	/**< @see vbi_option_type */

	/**
	 * Unique (within the respective export module) keyword to identify
	 * this option. Can be stored in configuration files.
	 */
	char *			keyword;

	/**
	 * Name of the option to be shown to the user.
	 * This can be @c NULL to indicate this option shall not be listed.
	 * Can be localized with dgettext("zvbi", label).
	 */
	char *			label;

	vbi_option_value	def;	/**< @see vbi_option_type */
	vbi_option_value	min;	/**< @see vbi_option_type */
	vbi_option_value	max;	/**< @see vbi_option_type */
	vbi_option_value	step;	/**< @see vbi_option_type */
	vbi_option_value_ptr	menu;	/**< @see vbi_option_type */

	/**
	 * A brief description (or @c NULL) for the user.
	 *  Can be localized with dgettext("zvbi", tooltip).
	 */
	char *			tooltip;
} vbi_option_info;

/**
 * @addtogroup Export
 * @{
 */
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
/** @} */

/* Private */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <stdarg.h>
#include <stddef.h>

extern const char _zvbi_intl_domainname[];

#include "version.h"
#include "intl-priv.h"

#endif /* !DOXYGEN_SHOULD_SKIP_THIS */

typedef struct vbi_export_class vbi_export_class;

/**
 * @ingroup Exmod
 *
 * Structure representing an export module instance, part of the private
 * export module interface.
 *
 * Export modules can read, but do not normally write its fields, as
 * they are maintained by the public libzvbi export functions.
 */
struct vbi_export {
	/**
	 * Points back to export module description.
	 */
	vbi_export_class *	_class;
	char *			errstr;		/**< Frontend private. */
	/**
	 * Name of the file we are writing, @c NULL if none (may be
	 * an anonymous FILE though).
	 */
	char *			name;
	/**
	 * Generic option: Network name or @c NULL.
	 */
	char *			network;	/* network name or NULL */
	/**
	 * Generic option: Creator name [by default "libzvbi"] or @c NULL.
	 */
	char *			creator;
	/**
	 * Generic option: Reveal hidden characters.
	 */
	vbi_bool		reveal;
};

/**
 * @ingroup Exmod
 *
 * Structure describing an export module, part of the private
 * export module interface. One required for each module.
 *
 * Export modules must initialize these fields (except @a next, see
 * exp-tmpl.c for a detailed discussion) and call vbi_export_register_module()
 * to become accessible.
 */
struct vbi_export_class {
	vbi_export_class *	next;
	vbi_export_info	*	_public;

	vbi_export *		(* _new)(void);
	void			(* _delete)(vbi_export *);

	vbi_option_info *	(* option_enum)(vbi_export *, int index);
	vbi_bool		(* option_set)(vbi_export *, const char *keyword,
					       va_list);
	vbi_bool		(* option_get)(vbi_export *, const char *keyword,
					       vbi_option_value *value);

	vbi_bool		(* export)(vbi_export *, FILE *fp, vbi_page *pg);
};

/**
 * @example exp-templ.c
 * @ingroup Exmod
 *
 * Template for internal export module.
 */

/*
 *  Helper functions
 */

/**
 * @addtogroup Exmod
 * @{
 */
extern void			vbi_register_export_module(vbi_export_class *);

extern void			vbi_export_write_error(vbi_export *);
extern void			vbi_export_unknown_option(vbi_export *, const char *keyword);
extern void			vbi_export_invalid_option(vbi_export *, const char *keyword, ...);
extern char *			vbi_export_strdup(vbi_export *, char **d, const char *s);
extern void			vbi_export_error_printf(vbi_export *, const char *templ, ...);

extern int			vbi_ucs2be(void);

/* Option info building */

#define VBI_OPTION_BOUNDS_INITIALIZER_(type_, def_, min_, max_, step_)	\
  { type_ = def_ }, { type_ = min_ }, { type_ = max_ }, { type_ = step_ }

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * vbi_option_info myinfo = VBI_OPTION_BOOL_INITIALIZER
 *   ("mute", N_("Switch sound on/off"), FALSE, N_("I am a tooltip"));
 * @endcode
 *
 * N_() marks the string for i18n, see info gettext for details.
 */
#define VBI_OPTION_BOOL_INITIALIZER(key_, label_, def_, tip_)		\
  { VBI_OPTION_BOOL, key_, label_, VBI_OPTION_BOUNDS_INITIALIZER_(	\
  .num, def_, 0, 1, 1),	{ .num = NULL }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * vbi_option_info myinfo = VBI_OPTION_INT_RANGE_INITIALIZER
 *   ("sampling", N_("Sampling rate"), 44100, 8000, 48000, 100, NULL);
 * @endcode
 *
 * Here we have no tooltip (@c NULL).
 */
#define VBI_OPTION_INT_RANGE_INITIALIZER(key_, label_, def_, min_,	\
  max_,	step_, tip_) { VBI_OPTION_INT, key_, label_,			\
  VBI_OPTION_BOUNDS_INITIALIZER_(.num, def_, min_, max_, step_),	\
  { .num = NULL }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * int mymenu[] = { 29, 30, 31 };
 *
 * vbi_option_info myinfo = VBI_OPTION_INT_MENU_INITIALIZER
 *   ("days", NULL, 1, mymenu, 3, NULL);
 * @endcode
 *
 * No label and tooltip (@c NULL), i. e. this option is not to be
 * listed in the user interface. Default is entry 1 ("30") of 3 entries. 
 */
#define VBI_OPTION_INT_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, entries_, tip_) { VBI_OPTION_INT, key_, label_,		\
  VBI_OPTION_BOUNDS_INITIALIZER_(.num, def_, 0, (entries_) - 1, 1),	\
  { .num = menu_ }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like
 * VBI_OPTION_INT_RANGE_INITIALIZER(), just with doubles but ints.
 */
#define VBI_OPTION_REAL_RANGE_INITIALIZER(key_, label_, def_, min_,	\
  max_, step_, tip_) { VBI_OPTION_REAL, key_, label_,			\
  VBI_OPTION_BOUNDS_INITIALIZER_(.dbl, def_, min_, max_, step_),	\
  { .dbl = NULL }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like
 * VBI_OPTION_INT_MENU_INITIALIZER(), just with an array of doubles but ints.
 */
#define VBI_OPTION_REAL_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, entries_, tip_) { VBI_OPTION_REAL, key_, label_,		\
  VBI_OPTION_BOUNDS_INITIALIZER_(.num, def_, 0, (entries_) - 1, 1),	\
  { .dbl = menu_ }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * vbi_option_info myinfo = VBI_OPTION_STRING_INITIALIZER
 *   ("comment", N_("Comment"), "bububaba", "Please enter a string");
 * @endcode
 */
#define VBI_OPTION_STRING_INITIALIZER(key_, label_, def_, tip_)		\
  { VBI_OPTION_STRING, key_, label_, VBI_OPTION_BOUNDS_INITIALIZER_(	\
  .str, def_, NULL, NULL, NULL), { .str = NULL }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * char *mymenu[] = { "txt", "html" };
 *
 * vbi_option_info myinfo = VBI_OPTION_STRING_MENU_INITIALIZER
 *   ("extension", "Ext", 0, mymenu, 2, N_("Select an extension"));
 * @endcode
 *
 * Remember this is like VBI_OPTION_STRING_INITIALIZER() in the sense
 * that the vbi client can pass any string as option value, not just those
 * proposed in the menu. In contrast a plain menu option as with
 * VBI_OPTION_MENU_INITIALIZER() expects menu indices as input.
 */
#define VBI_OPTION_STRING_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, entries_, tip_) { VBI_OPTION_STRING, key_, label_,		\
  VBI_OPTION_BOUNDS_INITIALIZER_(.str, def_, 0, (entries_) - 1, 1),	\
  { .str = menu_ }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * char *mymenu[] = { N_("Monday"), N_("Tuesday") };
 *
 * vbi_option_info myinfo = VBI_OPTION_MENU_INITIALIZER
 *   ("weekday", "Weekday", 0, mymenu, 2, N_("Select a weekday"));
 * @endcode
 */
#define VBI_OPTION_MENU_INITIALIZER(key_, label_, def_, menu_,		\
  entries_, tip_) { VBI_OPTION_MENU, key_, label_,			\
  VBI_OPTION_BOUNDS_INITIALIZER_(.num, def_, 0, (entries_) - 1, 1),	\
  { .str = (char **)(menu_) }, tip_ }

/* See exp-templ.c for an example */

/** Doesn't work, sigh. */
#define VBI_AUTOREG_EXPORT_MODULE(name)
/*
#define VBI_AUTOREG_EXPORT_MODULE(name)					\
static void vbi_autoreg_##name(void) __attribute__ ((constructor));	\
static void vbi_autoreg_##name(void) {					\
	vbi_register_export_module(&name);				\
}
*/

/** @} */

#endif /* EXPORT_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
