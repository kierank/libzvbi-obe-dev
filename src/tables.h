/*
 *  libzvbi - Tables
 *
 *  Copyright (C) 1999, 2000, 2001, 2002 Michael H. Schimek
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

/* $Id: tables.h,v 1.8 2007/07/23 20:01:18 mschimek Exp $ */

#ifndef TABLES_H
#define TABLES_H

#include <inttypes.h>

#include "event.h" /* vbi_rating_auth, vbi_prog_classf */

extern const char *vbi_country_names_en[];

struct vbi_cni_entry {
	int16_t			id; /* arbitrary */
	const char *		country; /* RFC 1766 / ISO 3166-1 alpha-2 */
	const char *		name; /* UTF-8 */
	uint16_t		cni1; /* Teletext packet 8/30 format 1 */
	uint16_t		cni2; /* Teletext packet 8/30 format 2 */
	uint16_t		cni3; /* PDC Method B */
	uint16_t		cni4; /* VPS */
};

extern const struct vbi_cni_entry vbi_cni_table[];

/* Public */

/**
 * @addtogroup Event
 * @{
 */
extern const char *	vbi_rating_string(vbi_rating_auth auth, int id);
extern const char *	vbi_prog_type_string(vbi_prog_classf classf, int id);
/** @} */

/* Private */

#endif /* TABLES_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
