/*
 *  libzvbi test
 *
 *  Copyright (C) 2005 Michael H. Schimek
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

/* $Id: sliced.h,v 1.5 2007/07/09 23:40:24 mschimek Exp $ */

#include <stdio.h>
#include <sys/time.h>

#include "src/macros.h"
#include "src/sliced.h"

/* Reader and write for old test/capture --sliced output.
   Attn: this code is not reentrant. */

extern vbi_bool
write_sliced_xml		(vbi_sliced *		sliced,
				 unsigned int		n_lines,
				 unsigned int		n_frame_lines,
				 int64_t		stream_time,
				 struct timeval		capture_time);
extern vbi_bool
write_sliced			(vbi_sliced *		sliced,
				 unsigned int		n_lines,
				 double			timestamp);
extern vbi_bool
open_sliced_write		(FILE *			fp,
				 double			timestamp);
extern int
read_sliced			(vbi_sliced *		sliced,
				 double *		timestamp,
				 unsigned int		max_lines);
extern vbi_bool
open_sliced_read		(FILE *			fp);
