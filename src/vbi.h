/*
 *  libzvbi - VBI decoding library
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
 *  Copyright (C) 2000, 2001 Iñaki García Etxebarria
 *
 *  Based on AleVT 1.5.1
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

/* $Id: vbi.h,v 1.5 2002/09/26 20:49:36 mschimek Exp $ */

#ifndef VBI_H
#define VBI_H

#include <pthread.h>

#include "vt.h"
#include "cc.h"
#include "decoder.h"
#include "event.h"
#include "cache.h"
#include "trigger.h"

struct event_handler {
	struct event_handler *	next;
	int			event_mask;
	vbi_event_handler	handler;
	void *			user_data;
};

struct page_clear {
	int			ci;		/* continuity index */
	int			packet;
	int			num_packets;
	int			bi;		/* block index */
	int			left;
	pfc_block		pfc;
};

struct vbi_decoder {
#if 0 // obsolete
	fifo			*source;
        pthread_t		mainloop_thread_id;
	int			quit;		/* XXX */
#endif
	double			time;

	pthread_mutex_t		chswcd_mutex;
        int                     chswcd;

	vbi_event		network;

	vbi_trigger *		triggers;

	pthread_mutex_t         prog_info_mutex;
	vbi_program_info        prog_info[2];
	int                     aspect_source;

	int			brightness;
	int			contrast;

	struct teletext		vt;
	struct caption		cc;

	struct cache		cache;

	struct page_clear	epg_pc[2];

	/* preliminary */
	int			pageref;

	pthread_mutex_t		event_mutex;
	int			event_mask;
	struct event_handler *	handlers;
	struct event_handler *	next_handler;

	unsigned char		wss_last[2];
	int			wss_rep_ct;
	double			wss_time;

	/* Property of the vbi_push_video caller */
#if 0 // obsolete
	enum tveng_frame_pixformat
				video_fmt;
	int			video_width; 
	double			video_time;
	vbi_bit_slicer_fn *	wss_slicer_fn;
	vbi_bit_slicer		wss_slicer;
	producer		wss_producer;
#endif
};

#ifndef VBI_DECODER
#define VBI_DECODER
/**
 * @ingroup Service
 * @brief Opaque VBI data service decoder object.
 *
 * Allocate with vbi_decoder_new().
 */
typedef struct vbi_decoder vbi_decoder;
#endif

/*
 *  vbi_page_type, the page identification codes,
 *  are derived from the MIP code scheme:
 *
 *  MIP 0x01 ... 0x51 -> 0x01 (subpages)
 *  MIP 0x70 ... 0x77 -> 0x70 (language)
 *  MIP 0x7B -> 0x7C (subpages)
 *  MIP 0x7E -> 0x7F (subpages)
 *  MIP 0x81 ... 0xD1 -> 0x81 (subpages)
 *  MIP reserved -> 0xFF (VBI_UNKNOWN_PAGE)
 *
 *  MIP 0x80 and 0xE0 ... 0xFE are not returned by
 *  vbi_classify_page().
 *
 *  TOP BTT mapping:
 *
 *  BTT 0 -> 0x00 (VBI_NOPAGE)
 *  BTT 1 -> 0x70 (VBI_SUBTITLE_PAGE)
 *  BTT 2 ... 3 -> 0x7F (VBI_PROGR_INDEX)
 *  BTT 4 ... 5 -> 0xFA (VBI_TOP_BLOCK -> VBI_NORMAL_PAGE) 
 *  BTT 6 ... 7 -> 0xFB (VBI_TOP_GROUP -> VBI_NORMAL_PAGE)
 *  BTT 8 ... 11 -> 0x01 (VBI_NORMAL_PAGE)
 *  BTT 12 ... 15 -> 0xFF (VBI_UNKNOWN_PAGE)
 *
 *  0xFA, 0xFB, 0xFF are reserved MIP codes used
 *  by libzvbi to identify TOP and unknown pages.
 */

/* Public */

/**
 * @ingroup Service
 * @brief Page classification.
 *
 * See vbi_classify_page().
 */
typedef enum {
	VBI_NO_PAGE = 0x00,
	VBI_NORMAL_PAGE = 0x01,
	VBI_SUBTITLE_PAGE = 0x70,
	VBI_SUBTITLE_INDEX = 0x78,
	VBI_NONSTD_SUBPAGES = 0x79,
	VBI_PROGR_WARNING = 0x7A,
	VBI_CURRENT_PROGR = 0x7C,
	VBI_NOW_AND_NEXT = 0x7D,
	VBI_PROGR_INDEX = 0x7F,
	VBI_PROGR_SCHEDULE = 0x81,
	VBI_UNKNOWN_PAGE = 0xFF,
/* Private */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
	VBI_NOT_PUBLIC = 0x80,
	VBI_CA_DATA_BROADCAST =	0xE0,
	VBI_EPG_DATA = 0xE3,
	VBI_SYSTEM_PAGE = 0xE7,
	VBI_DISP_SYSTEM_PAGE = 0xF7,
	VBI_KEYWORD_SEARCH_LIST = 0xF9,
	VBI_TOP_BLOCK = 0xFA,
	VBI_TOP_GROUP = 0xFB,
	VBI_TRIGGER_DATA = 0xFC,
	VBI_ACI = 0xFD,
	VBI_TOP_PAGE = 0xFE
#endif
/* Public */
} vbi_page_type;

/**
 * @addtogroup Render
 * @{
 */
extern void		vbi_set_brightness(vbi_decoder *vbi, int brightness);
extern void		vbi_set_contrast(vbi_decoder *vbi, int contrast);
/** @} */

/**
 * @addtogroup Service
 * @{
 */
extern vbi_decoder *	vbi_decoder_new(void);
extern void		vbi_decoder_delete(vbi_decoder *vbi);
extern void		vbi_decode(vbi_decoder *vbi, vbi_sliced *sliced,
				   int lines, double timestamp);
extern void             vbi_channel_switched(vbi_decoder *vbi, vbi_nuid nuid);
extern vbi_page_type	vbi_classify_page(vbi_decoder *vbi, vbi_pgno pgno,
					  vbi_subno *subno, char **language);
/** @} */

/* Private */

extern pthread_once_t	vbi_init_once;
extern void		vbi_init(void);

extern void		vbi_transp_colormap(vbi_decoder *vbi, vbi_rgba *d, vbi_rgba *s, int entries);
extern void             vbi_chsw_reset(vbi_decoder *vbi, vbi_nuid nuid);

extern void		vbi_asprintf(char **errstr, char *templ, ...);

#endif /* VBI_H */
