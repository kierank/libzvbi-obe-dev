/*
 *  libzvbi - WSS decoder
 *
 *  Copyright (C) 2001, 2002 Michael H. Schimek
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

/* $Id: wss.h,v 1.2 2007/07/23 20:01:18 mschimek Exp $ */

extern void		vbi_decode_wss_625(vbi_decoder *vbi, uint8_t *buf, double time);
extern void		vbi_decode_wss_cpr1204(vbi_decoder *vbi, uint8_t *buf);

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
