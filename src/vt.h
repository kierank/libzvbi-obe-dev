/*
 *  libzvbi - Teletext decoder
 *
 *  Copyright (C) 2000, 2001 Michael H. Schimek
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

/* $Id: vt.h,v 1.8 2005/10/04 10:06:33 mschimek Exp $ */

#ifndef VT_H
#define VT_H

#include <inttypes.h>

#include "bcd.h"
#include "format.h"

#ifndef VBI_DECODER
#define VBI_DECODER
typedef struct vbi_decoder vbi_decoder;
#endif

/**
 * @internal
 *
 * Page function code according to ETS 300 706, Section 9.4.2,
 * Table 3: Page function and page coding bits (packets X/28/0 Format 1,
 * X/28/3 and X/28/4).
 */
typedef enum {
	PAGE_FUNCTION_EPG = -4,		/* libzvbi private */
	PAGE_FUNCTION_TRIGGER = -3,	/* libzvbi private */
	PAGE_FUNCTION_DISCARD = -2,	/* libzvbi private */
	PAGE_FUNCTION_UNKNOWN = -1,	/* libzvbi private */
	PAGE_FUNCTION_LOP,
	PAGE_FUNCTION_DATA_BROADCAST,
	PAGE_FUNCTION_GPOP,
	PAGE_FUNCTION_POP,
	PAGE_FUNCTION_GDRCS,
	PAGE_FUNCTION_DRCS,
	PAGE_FUNCTION_MOT,
	PAGE_FUNCTION_MIP,
	PAGE_FUNCTION_BTT,
	PAGE_FUNCTION_AIT,
	PAGE_FUNCTION_MPT,
	PAGE_FUNCTION_MPT_EX
} page_function;

/**
 * @internal
 * Page coding code according to ETS 300 706, Section 9.4.2,
 * Table 3: Page function and page coding bits (packets X/28/0 Format 1,
 * X/28/3 and X/28/4).
 */
typedef enum {
	PAGE_CODING_UNKNOWN = -1,	/* libzvbi private */
	PAGE_CODING_PARITY,
	PAGE_CODING_BYTES,
	PAGE_CODING_TRIPLETS,
	PAGE_CODING_HAMMING84,
	PAGE_CODING_AIT,
	PAGE_CODING_META84
} page_coding;

/**
 * @internal
 *
 * DRCS character coding according to ETS 300 706, Section 14.2,
 * Table 31: DRCS modes; Section 9.4.6, Table 9: Coding of Packet
 * X/28/3 for DRCS Downloading Pages.
 */
typedef enum {
	DRCS_MODE_12_10_1,
	DRCS_MODE_12_10_2,
	DRCS_MODE_12_10_4,
	DRCS_MODE_6_5_4,
	DRCS_MODE_SUBSEQUENT_PTU = 14,
	DRCS_MODE_NO_DATA
} drcs_mode;

/*
    Only a minority of pages need this
 */

typedef struct {
	int		black_bg_substitution;
	int		left_side_panel;
	int		right_side_panel;
	int		left_panel_columns;
} ext_fallback;

#define VBI_TRANSPARENT_BLACK 8

typedef struct {
	unsigned int	designations;

	int		char_set[2];		/* primary, secondary */

	int		def_screen_color;
	int		def_row_color;

	int		foreground_clut;	/* 0, 8, 16, 24 */
	int		background_clut;

	ext_fallback	fallback;

	uint8_t		drcs_clut[2 + 2 * 4 + 2 * 16];
						/* f/b, dclut4, dclut16 */
	vbi_rgba	color_map[40];
} vt_extension;

/**
 * @internal
 *
 * Packet X/26 code triplet according to ETS 300 706, Section 12.3.1.
 */
typedef struct vt_triplet {
	unsigned	address : 8;
	unsigned	mode : 8;
	unsigned	data : 8;
} /* __attribute__ ((packed)) */ vt_triplet;

typedef struct vt_pagenum {
	unsigned	type : 8;
	unsigned	pgno : 16;
	unsigned	subno : 16;
} pagenum;

typedef struct {
	pagenum	        page;
	uint8_t		text[12];
} ait_entry;

typedef vt_triplet enhancement[16 * 13 + 1];

#define NO_PAGE(pgno) (((pgno) & 0xFF) == 0xFF)

/*                              0xE03F7F    nat. char. subset and sub-page */
#define C4_ERASE_PAGE		0x000080 /* erase previously stored packets */
#define C5_NEWSFLASH		0x004000 /* box and overlay */
#define C6_SUBTITLE		0x008000 /* box and overlay */
#define C7_SUPPRESS_HEADER	0x010000 /* row 0 not to be displayed */
#define C8_UPDATE		0x020000
#define C9_INTERRUPTED		0x040000
#define C10_INHIBIT_DISPLAY	0x080000 /* rows 1-24 not to be displayed */
#define C11_MAGAZINE_SERIAL	0x100000

/**
 * @internal
 *
 * This structure holds a raw Teletext page as decoded by
 * vbi_teletext_packet(), stored in the Teletext page cache, and
 * formatted by vbi_format_vt_page() creating a vbi_page. It is
 * thus not directly accessible by the client. Note the size
 * (of the union) will vary in order to save cache memory.
 **/
typedef struct vt_page {
	/**
	 * Defines the page function and which member of the
	 * union applies.
	 */ 
	page_function		function;

	/**
	 * Page and subpage number.
	 */
	vbi_pgno		pgno;
	vbi_subno		subno;

	/**
	 * National character set designator 0 ... 7.
	 */
	int			national;

	/**
	 * Page flags C4_ERASE_PAGE ... C11_MAGAZIN_SERIAL.
	 */
	int			flags;

	/**
	 * One bit for each LOP and enhancement packet
	 */
	int			lop_lines;
	int			enh_lines;

	union {
		struct lop {
			unsigned char	raw[26][40];
		        pagenum	        link[6 * 6];		/* X/27/0-5 links */
			vbi_bool	flof, ext;
		}		unknown, lop;
		struct {
			struct lop	lop;
			enhancement	enh;
		}		enh_lop;
		struct {
			struct lop	lop;
			enhancement	enh;
			vt_extension	ext;
		}		ext_lop;
		struct {
			uint16_t	pointer[96];
			vt_triplet	triplet[39 * 13 + 1];
// XXX preset [+1] mode (not 0xFF) or catch
		}		gpop, pop;
		struct {
			uint8_t			raw[26][40];
			uint8_t			bits[48][12 * 10 / 2];
			uint8_t			mode[48];
			uint64_t		invalid;
		}		gdrcs, drcs;

		ait_entry	ait[46];

	}		data;

	/* 
	 *  Dynamic size, add no fields below unless
	 *  vt_page is statically allocated.
	 */
} vt_page;

/**
 * @internal
 * @param vtp Teletext page in question.
 * 
 * @return Storage size required for the raw Teletext page,
 * depending on its function and the data union member used.
 **/
static inline int
vtp_size(vt_page *vtp)
{
	switch (vtp->function) {
	case PAGE_FUNCTION_UNKNOWN:
	case PAGE_FUNCTION_LOP:
		if (vtp->data.lop.ext)
			return sizeof(*vtp) - sizeof(vtp->data)	+ sizeof(vtp->data.ext_lop);
		else if (vtp->enh_lines)
			return sizeof(*vtp) - sizeof(vtp->data)	+ sizeof(vtp->data.enh_lop);
		else
			return sizeof(*vtp) - sizeof(vtp->data)	+ sizeof(vtp->data.lop);

	case PAGE_FUNCTION_GPOP:
	case PAGE_FUNCTION_POP:
		return sizeof(*vtp) - sizeof(vtp->data)	+ sizeof(vtp->data.pop);

	case PAGE_FUNCTION_GDRCS:
	case PAGE_FUNCTION_DRCS:
		return sizeof(*vtp) - sizeof(vtp->data)	+ sizeof(vtp->data.drcs);

	case PAGE_FUNCTION_AIT:
		return sizeof(*vtp) - sizeof(vtp->data)	+ sizeof(vtp->data.ait);

	default:
		;
	}

	return sizeof(*vtp);
}

/**
 * @internal
 *
 * TOP BTT page class.
 */
typedef enum {
	BTT_NO_PAGE = 0,
	BTT_SUBTITLE,
	BTT_PROGR_INDEX_S,
	BTT_PROGR_INDEX_M,
	BTT_BLOCK_S,
	BTT_BLOCK_M,
	BTT_GROUP_S,
	BTT_GROUP_M,
	BTT_NORMAL_S,
	BTT_NORMAL_9, /* ? */
	BTT_NORMAL_M,
	BTT_NORMAL_11 /* ? */
	/* 12 ... 15 ? */
} btt_page_class;

/**
 * @internal
 *
 * Enhancement object type according to ETS 300 706, Section 12.3.1,
 * Table 28: Function of Row Address triplets.
 */
typedef enum {
	LOCAL_ENHANCEMENT_DATA = 0,
	OBJ_TYPE_NONE = 0,
	OBJ_TYPE_ACTIVE,
	OBJ_TYPE_ADAPTIVE,
	OBJ_TYPE_PASSIVE
} object_type;

/**
 * @internal
 *
 * MOT default, POP and GPOP object address.
 *
 * n8  n7  n6  n5  n4  n3  n2  n1  n0
 * packet  triplet lsb ----- s1 -----
 *
 * According to ETS 300 706, Section 12.3.1, Table 28
 * (under Mode 10001 - Object Invocation ff.)
 */
typedef int object_address;

typedef struct {
	int		pgno;
	ext_fallback	fallback;
	struct {
		object_type	type;
		object_address	address;
	}		default_obj[2];
} vt_pop_link;

typedef struct {
	vt_extension	extension;

	uint8_t		pop_lut[256];
	uint8_t		drcs_lut[256];

    	vt_pop_link	pop_link[16];
	vbi_pgno	drcs_link[16];
} vt_magazine;

struct raw_page {
	vt_page			page[1];
        uint8_t		        drcs_mode[48];
	int			num_triplets;
	int			ait_page;
};

/* Public */

/**
 * @ingroup HiDec
 * @brief Teletext implementation level.
 */
typedef enum {
	VBI_WST_LEVEL_1,   /**< 1 - Basic Teletext pages */
	VBI_WST_LEVEL_1p5, /**< 1.5 - Additional national and graphics characters */
	/**
	 * 2.5 - Additional text styles, more colors and DRCS. You should
	 * enable Level 2.5 only if you can render and/or export such pages.
	 */
	VBI_WST_LEVEL_2p5,
	VBI_WST_LEVEL_3p5  /**< 3.5 - Multicolor DRCS, proportional script */
} vbi_wst_level;

/* Private */

struct teletext {
	vbi_wst_level		max_level;

	pagenum                 header_page;
	uint8_t		        header[40];

        pagenum		        initial_page;
	vt_magazine		magazine[9];		/* 1 ... 8; #0 unmodified level 1.5 default */

	int                     region;

	struct page_info {
		unsigned 		code : 8;
		unsigned		language : 8;
		unsigned 		subcode : 16;
	}			page_info[0x800];

	/*
	 *  Property of cache.c in the network context:
	 *  0: page not cached, 1-3F80: highest subno + 1
	 */
	uint16_t		cached[0x800];

	pagenum		        btt_link[15];
	vbi_bool		top;			/* use top navigation, flof overrides */

	struct raw_page		raw_page[8];
	struct raw_page		*current;
};

/* Public */

/**
 * @addtogroup Service
 * @{
 */
extern void		vbi_teletext_set_default_region(vbi_decoder *vbi, int default_region);
extern void		vbi_teletext_set_level(vbi_decoder *vbi, int level);
/** @} */
/**
 * @addtogroup Cache
 * @{
 */
extern vbi_bool		vbi_fetch_vt_page(vbi_decoder *vbi, vbi_page *pg,
					  vbi_pgno pgno, vbi_subno subno,
					  vbi_wst_level max_level, int display_rows,
					  vbi_bool navigation);
extern int		vbi_page_title(vbi_decoder *vbi, int pgno, int subno, char *buf);
/** @} */
/**
 * @addtogroup Event
 * @{
 */
extern void		vbi_resolve_link(vbi_page *pg, int column, int row,
					 vbi_link *ld);
extern void		vbi_resolve_home(vbi_page *pg, vbi_link *ld);
/** @} */

/* Private */

/* packet.c */

extern void		vbi_teletext_init(vbi_decoder *vbi);
extern void		vbi_teletext_destroy(vbi_decoder *vbi);
extern vbi_bool		vbi_decode_teletext(vbi_decoder *vbi, uint8_t *p);
extern void		vbi_teletext_desync(vbi_decoder *vbi);
extern void             vbi_teletext_channel_switched(vbi_decoder *vbi);
extern vt_page *	vbi_convert_page(vbi_decoder *vbi, vt_page *vtp,
					 vbi_bool cached, page_function new_function);

extern void		vbi_decode_vps(vbi_decoder *vbi, uint8_t *p);

/* teletext.c */

extern vbi_bool		vbi_format_vt_page(vbi_decoder *, vbi_page *,
					   vt_page *, vbi_wst_level max_level,
					   int display_rows, vbi_bool navigation);

#endif
