/*
 *  libzvbi - BCD arithmetic for Teletext page numbers
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

/* $Id: bcd.h,v 1.2 2002/03/06 00:11:32 mschimek Exp $ */

#ifndef BCD_H
#define BCD_H

/* Public */

#undef TRUE
#undef FALSE
#define TRUE 1
#define FALSE 0

typedef int vbi_bool;

/**
 * vbi_pgno:
 * 
 * Teletext or Closed Caption page number. For Teletext pages
 * this is a bcd number in range 0x100 ... 0x8FF. Page numbers
 * containing digits 0xA to 0xF are reserved for various system
 * purposes, these pages are not intended for display.
 * 
 * Closed Caption page numbers between 1 ... 8 correspond
 * to the four Caption and Text channels:
 * <informaltable frame=none><tgroup cols=3><tbody>
 * <row><entry>1</><entry>Caption 1</><entry>
 *   "Primary synchronous caption service [English]"</></row>
 * <row><entry>2</><entry>Caption 2</><entry>
 *   "Special non-synchronous data that is intended to
 *   augment information carried in the program"</></row>
 * <row><entry>3</><entry>Caption 3</><entry>
 *   "Secondary synchronous caption service, usually
 *    second language [Spanish, French]"</></row>
 * <row><entry>4</><entry>Caption 4</><entry>
 *   "Special non-synchronous data similar to Caption 2"</></row>
 * <row><entry>5</><entry>Text 1</><entry>
 *   "First text service, data usually not program related"</></row>
 * <row><entry>6</><entry>Text 2</><entry>
 *   "Second text service, additional data usually not program related
 *    [ITV data]"</></row>
 * <row><entry>7</><entry>Text 3</><entry>
 *   "Additional text channel"</></row>
 * <row><entry>8</><entry>Text 4</><entry>
 *   "Additional text channel"</></row>
 * </tbody></tgroup></informaltable>
 **/
typedef int vbi_pgno;

/**
 * vbi_subno:
 * 
 * This is the subpage number only applicable to Teletext pages,
 * a BCD number in range 0x00 ... 0x99. On special 'clock' pages
 * (for example listing the current time in different time zones)
 * it can assume values between 0x0000 ... 0x2359 expressing
 * local time. These are not actually subpages.
 **/
typedef int vbi_subno;

#define VBI_ANY_SUBNO 0x3F7F
#define VBI_NO_SUBNO 0x3F7F

/**
 * vbi_dec2bcd:
 * @dec: Decimal number.
 * 
 * Converts a decimal number between 0 ... 999 to a bcd number in range
 * number 0x000 ... 0x999. Extra digits in the input will be discarded.
 * 
 * Return value:
 * BCD number.
 **/
static inline unsigned int
vbi_dec2bcd(unsigned int dec)
{
	return (dec % 10) + ((dec / 10) % 10) * 16 + ((dec / 100) % 10) * 256;
}

/**
 * vbi_bcd2dec:
 * @bcd: BCD number.
 * 
 * Converts a bcd number between 0x000 ... 0xFFF to a decimal number
 * in range 0 ... 999. Extra digits in the input will be discarded.
 * 
 * Return value:
 * Decimal number. The result is undefined when the bcd number contains
 * hex digits 0xA ... 0xF.
 **/
static inline unsigned int
vbi_bcd2dec(unsigned int bcd)
{
	return (bcd & 15) + ((bcd >> 4) & 15) * 10 + ((bcd >> 8) & 15) * 100;
}

/**
 * vbi_add_bcd:
 * @a: BCD number.
 * @b: BCD number.
 * 
 * Adds two bcd numbers, returning a bcd sum. The result will be in
 * range 0x00000000 ... 0xFFFFFFFF, discarding carry and extra digits
 * in the inputs. To subtract you can add the two's complement,
 * e. g. -0x1 = +0x99999999.
 * 
 * Return value:
 * BCD number. The result is undefined when the bcd number contains
 * hex digits 0xA ... 0xF.
 **/
static inline unsigned int
vbi_add_bcd(unsigned int a, unsigned int b)
{
	unsigned int t;

	a += 0x06666666;
	t  = a + b;
	b ^= a ^ t;
	b  = (~b & 0x11111110) >> 3;
	b |= b * 2;

	return t - b;
}

/**
 * vbi_is_bcd:
 * @bcd: BCD number.
 * 
 * Tests if @bcd forms a valid BCD number.
 * 
 * Return value:
 * FALSE if @bcd contains hex digits 0xA ... 0xF.
 **/
static inline vbi_bool
vbi_is_bcd(unsigned int bcd)
{
	static const unsigned int x = 0x06666666;

	return (((bcd + x) ^ (bcd ^ x)) & 0x11111110) == 0;
}

/* Private */

#undef ABS
#define ABS(n)								\
({									\
	register int _n = n, _t = _n;					\
									\
	_t >>= sizeof(_t) * 8 - 1;					\
	_n ^= _t;							\
	_n -= _t;							\
})

#undef MIN
#define MIN(x, y)							\
({									\
	typeof(x) _x = x;						\
	typeof(y) _y = y;						\
									\
	(void)(&_x == &_y); /* alert when type mismatch */		\
	(_x < _y) ? _x : _y;						\
})

#undef MAX
#define MAX(x, y)							\
({									\
	typeof(x) _x = x;						\
	typeof(y) _y = y;						\
									\
	(void)(&_x == &_y); /* alert when type mismatch */		\
	(_x > _y) ? _x : _y;						\
})

#undef SATURATE
#define SATURATE(n, min, max) MIN(MAX(n, min), max)

/*
 *  Return a pointer to a structure of @type from
 *  a @ptr to one of its @members.
 */
#define PARENT(ptr, type, member)					\
  ((type *)(((char *) ptr) - offsetof(type, member)))

#endif /* BCD_H */
