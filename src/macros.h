/*
 *  libzvbi - Useful macros
 *
 *  Copyright (C) 2002-2004 Michael H. Schimek
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

/* $Id: macros.h,v 1.2 2006/05/07 20:55:24 mschimek Exp $ */

#ifndef __ZVBI_MACROS_H__
#define __ZVBI_MACROS_H__

#ifdef __cplusplus
#  define VBI_BEGIN_DECLS extern "C" {
#  define VBI_END_DECLS }
#else
#  define VBI_BEGIN_DECLS
#  define VBI_END_DECLS
#endif

VBI_BEGIN_DECLS

#if __GNUC__ >= 4
#  define _vbi_sentinel sentinel(0)
#else
#  define _vbi_sentinel
#  define __restrict__
#endif

#if (__GNUC__ == 3 && __GNUC_MINOR__ >= 3) || __GNUC__ >= 4
#  define _vbi_nonnull(args...) nonnull(args)
#else
#  define _vbi_nonnull(args...)
#endif

#if __GNUC__ >= 3
#  define _vbi_pure pure
#  define _vbi_alloc malloc
#else
#  define _vbi_pure
#  define _vbi_alloc
#endif

#if __GNUC__ >= 2
#  define vbi_inline static __inline__
#else
#  define vbi_inline static
#  define __attribute__(args...)
#endif

VBI_END_DECLS

#endif /* __ZVBI_MACROS_H__ */
