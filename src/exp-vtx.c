/*
 *  libzvbi - VTX export function
 *
 *  Copyright (C) 2001 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
 *  Copyright (C) 1999 Paul Ortyl <ortylp@from.pl>
 *
 *  Based on code from VideoteXt 0.6
 *  Copyright (C) 1995, 1996, 1997 Martin Buck <martin-2.buck@student.uni-ulm.de>
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

/* $Id: exp-vtx.c,v 1.5 2002/12/24 15:44:32 mschimek Exp $ */

/*
 *  VTX is the file format used by VideoteXt. It stores Teletext pages in
 *  raw level 1.0 format. Level 1.5 additional characters (e.g. accents), the
 *  FLOF and TOP navigation bars and the level 2.5 chrome will be lost.
 *  (People interested in an XML based successor to VTX drop us a mail.)
 *
 *  Since restoring the raw page from a fmt_page is complicated we violate
 *  encapsulation by fetching a raw copy from the cache. :-(
 */

#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

#include "vbi.h"	/* cache, vt.h */
#include "hamm.h"	/* bit_reverse */
#include "export.h"

struct header {
	char			signature[5];
	unsigned char		pagenum_l;
	unsigned char		pagenum_h;
	unsigned char		hour;
	unsigned char		minute;
	unsigned char		charset;
	unsigned char		wst_flags;
	unsigned char		vtx_flags;
};

/*
 *  VTX - VideoteXt File (VTXV4)
 */

static vbi_bool
export				(vbi_export *		e,
				 FILE *			fp,
				 vbi_page *		pg)
{
	const vt_page *vtp;
	struct header h;

	if (pg->pgno < 0x100 || pg->pgno > 0x8FF) {
		vbi_export_error_printf (e, _("Can only export Teletext pages."));
		return FALSE;
	}

	/**/

	assert(pg->vbi != NULL);

	if (!(vtp = vbi_cache_get (pg->vbi, NUID0,
				   pg->pgno, pg->subno, ~0,
				   /* user access */ FALSE))) {
		vbi_export_error_printf (e, _("Page is not cached, sorry."));
		return FALSE;
	}

	/**/

	if (vtp->function != PAGE_FUNCTION_UNKNOWN
	    && vtp->function != PAGE_FUNCTION_LOP) {
		vbi_export_error_printf (e, _("Cannot export this page, not displayable."));
		goto error;
	}

	memcpy (h.signature, "VTXV4", 5);

	h.pagenum_l = vtp->pgno & 0xFF;
	h.pagenum_h = (vtp->pgno >> 8) & 15;

	h.hour = 0;
	h.minute = 0;

	h.charset = vtp->national & 7;

	h.wst_flags = vtp->flags & C4_ERASE_PAGE;
	h.wst_flags |= vbi_bit_reverse[vtp->flags >> 12];
	h.vtx_flags = (0 << 7) | (0 << 6) | (0 << 5) | (0 << 4) | (0 << 3);
	/* notfound, pblf (?), hamming error, virtual, seven bits */

	if (fwrite (&h, sizeof (h), 1, fp) != 1)
		goto write_error;

	if (fwrite (vtp->data.lop.raw, 40 * 24, 1, fp) != 1)
		goto write_error;

	vbi_cache_unref (pg->vbi, vtp);

	return TRUE;

 write_error:
	vbi_export_write_error (e);

 error:
	vbi_cache_unref (pg->vbi, vtp);

	return FALSE;
}

static vbi_export_info
info_vtx = {
	.keyword	= "vtx",
	.label		= N_("VTX"),
	.tooltip	= N_("Export this page as VTX file, the format "
			     "used by VideoteXt and vbidecode"),

	/* From VideoteXt examples/mime.types */
	.mime_type	= "application/videotext",
	.extension	= "vtx",
};

vbi_export_class
vbi_export_class_vtx = {
	._public		= &info_vtx,

	/* no private data, no options */
	.export			= export
};

VBI_AUTOREG_EXPORT_MODULE (vbi_export_class_vtx)
