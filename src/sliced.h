/*
 *  libzvbi - Sliced vbi data object
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

/* $Id: sliced.h,v 1.1 2002/01/12 16:18:54 mschimek Exp $ */

#ifndef SLICED_H
#define SLICED_H

/* Public */

/* Keep this file in sync with rte */

#include <inttypes.h>

/* Known data services */

#define VBI_SLICED_NONE			0
/**
 * VBI_SLICED_NONE:
 *
 * No data service, blank #vbi_sliced structure.
 **/
#define VBI_SLICED_TELETEXT_B_L10_625	0x00000001
#define VBI_SLICED_TELETEXT_B_L25_625	0x00000002
#define VBI_SLICED_TELETEXT_B		(VBI_SLICED_TELETEXT_B_L10_625 | VBI_SLICED_TELETEXT_B_L25_625)
/**
 * VBI_SLICED_TELETEXT_B:
 *
 * Teletext System B.
 *
 * Note this is separated into Level 1.0 and Level 2.5+ since the latter
 * permits occupation of PAL/SECAM scan line 6 which is frequently out of
 * range of VBI capture drivers. Clients should request decoding of both,
 * may then verify Level 2.5 is covered. Also sliced data can be tagged
 * as both Level 1.0 and 2.5+, i. e. VBI_SLICED_TELETEXT_B.
 *
 * Reference: <ulink url="http://www.etsi.org"><citetitle>ETS 300 706
 * "Enhanced Teletext specification"</citetitle></ulink>.
 *
 * vbi_sliced payload: Last 42 of the 45 byte Teletext packet, that is
 * without clock run-in and framing code, lsb first transmitted.
 **/
#define VBI_SLICED_VPS			0x00000004
/**
 * VBI_SLICED_VPS:
 *
 * Video Program System.
 *
 * Reference: <ulink url="http://www.etsi.org"><citetitle>ETS 300 231
 * "Specification of the domestic video Programme Delivery Control system (PDC)"
 * </citetitle></ulink>.
 *
 * vbi_sliced payload: Byte number 3 to 15 according to Figure 9,
 * lsb first transmitted.
 **/
#define VBI_SLICED_CAPTION_625_F1	0x00000008
#define VBI_SLICED_CAPTION_625_F2	0x00000010
#define VBI_SLICED_CAPTION_625		(VBI_SLICED_CAPTION_625_F1 | VBI_SLICED_CAPTION_625_F2)
/**
 * VBI_SLICED_CAPTION_625:
 *
 * Closed Caption for 625 line systems (PAL, SECAM).
 *
 * Note this is split into field one and two services since for basic
 * caption decoding only field one is required. Clients should compare
 * the vbi_sliced line number, not the type to determine the source field.
 *
 * Reference: <ulink url="http://global.ihs.com"><citetitle>EIA 608
 * "Recommended Practice for Line 21 Data Service"</citetitle></ulink>.
 *
 * vbi_sliced payload: First and second byte including parity,
 * lsb first transmitted.
 **/
#define VBI_SLICED_CAPTION_525_F1	0x00000020
#define VBI_SLICED_CAPTION_525_F2	0x00000040
#define VBI_SLICED_CAPTION_525		(VBI_SLICED_CAPTION_525_F1 | VBI_SLICED_CAPTION_525_F2)
/**
 * VBI_SLICED_CAPTION_525:
 *
 * Closed Caption for 525 line systems (NTSC).
 *
 * Note this is split into field one and two services since for basic
 * caption decoding only field one is required. Clients should compare
 * the vbi_sliced line number, not the type to determine the source field.
 * VBI_SLICED_CAPTION_525 also covers XDS (Extended Data Service), V-Chip data
 * and ITV data.
 *
 * Reference: <ulink url="http://global.ihs.com"><citetitle>EIA 608
 * "Recommended Practice for Line 21 Data Service"</citetitle></ulink>.
 *
 * vbi_sliced payload: First and second byte including parity,
 * lsb first transmitted.
 **/
#define VBI_SLICED_2xCAPTION_525	0x00000080
/**
 * VBI_SLICED_2xCAPTION_525:
 *
 * Closed Caption at double bit rate.
 *
 * Reference: ?
 *
 * vbi_sliced payload: First to fourth byte including parity bit,
 * lsb first transmitted.
 **/
#define VBI_SLICED_NABTS		0x00000100
/**
 * VBI_SLICED_NABTS:
 *
 * North American Basic Teletext Specification.
 *
 * (Supposedly this standard fell into disuse.)
 *
 * Reference: <ulink url="http://global.ihs.com"><citetitle>EIA-516
 * "North American Basic Teletext Specification (NABTS)"</citetitle></ulink>.
 *
 * vbi_sliced payload: 33 bytes.
 **/
#define VBI_SLICED_TELETEXT_BD_525	0x00000200
/**
 * VBI_SLICED_TELETEXT_BD_525:
 *
 * ?
 *
 * Reference: ?
 *
 * vbi_sliced payload: 34 bytes.
 **/
#define VBI_SLICED_WSS_625		0x00000400
/**
 * VBI_SLICED_WSS_625:
 *
 * Wide Screen Signalling for 625 line systems (PAL, SECAM)
 *
 * Reference: <ulink url="http://www.etsi.org"><citetitle>EN 300 294
 * "625-line television Wide Screen Signalling (WSS)"</citetitle></ulink>.
 *
 * vbi_sliced payload:
 * <screen>
 * Byte         0                  1
 * !     msb         lsb  msb             lsb
 * bit   7 6 5 4 3 2 1 0  x x 13 12 11 10 9 8
 * </> according to Table 1, lsb first transmitted. 
 **/
#define VBI_SLICED_WSS_CPR1204		0x00000800
/**
 * VBI_SLICED_WSS_CPR1204:
 *
 * Wide Screen Signalling for NTSC Japan
 *
 * Reference: <citetitle>EIA-J CPR-1024 (?)</citetitle>
 *
 * vbi_sliced payload:
 * <screen>
 * Byte         0                    1                  2
 * !     msb         lsb  msb               lsb  msb             lsb
 * bit   7 6 5 4 3 2 1 0  15 14 13 12 11 10 9 8  x x x x 19 18 17 16
 * </>
 **/
/* capture catch-all */
#define VBI_SLICED_VBI_625		0x20000000
/**
 * VBI_SLICED_VBI_625:
 *
 * No actual data service. This symbol is used to request capturing
 * of all PAL/SECAM VBI data lines, as opposed to just those lines
 * used to transmit the requested data services.
 **/
#define VBI_SLICED_VBI_525		0x40000000
/**
 * VBI_SLICED_VBI_525:
 *
 * No actual data service. This symbol is used to request capturing
 * of all NTSC VBI data lines, as opposed to just those lines
 * used to transmit the requested data services.
 **/

/**
 * vbi_sliced:
 * 
 * This structure holds one scan line of sliced vbi data, for
 * example the contents of NTSC line 21, two bytes of Closed Caption
 * data. Usually an array of vbi_sliced is used, covering all
 * VBI lines.
 *
 * @id: A VBI_SLICED_ symbol. Under cirumstances (see #VBI_SLICED_TELETEXT_B)
 *   this can be a logical 'or' of VBI_SLICED_ values.
 *
 * @line: Source line number according to the ITU-R line numbering scheme,
 *   a value of 0 if the exact line number is unknown. Note that some
 *   data services cannot be reliable decoded without line number.
 *
 * @data: The actual payload. See the VBI_SLICED_ documentation for details.
 **/
typedef struct vbi_sliced {
	uint32_t		id;
	uint32_t		line;
	uint8_t			data[56];
} vbi_sliced;

/* Private */

#endif /* SLICED_H */
