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

/* $Id: vbi.c,v 1.2 2002/04/16 05:49:57 mschimek Exp $ */

#include "site_def.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <pthread.h>

#include "vbi.h"
#include "hamm.h"
#include "lang.h"
#include "export.h"
#include "tables.h"
#include "format.h"
#include "wss.h"

/*
 *  Events
 */

/**
 * vbi_event_handler_add:
 * @vbi: Initialized vbi decoding context.
 * @event_mask: Events the handler is waiting for.
 * @handler: Event handler function.
 * @user_data: Pointer passed to the handler.
 * 
 * Registers a new event handler. @event_mask can be any 'or' of VBI_EVENT_*,
 * -1 for all events and 0 for none. When the @handler is already registered,
 * its event_mask will be changed. Any number of handlers can be registered,
 * also different handlers for the same event which will be called in
 * registration order.
 * 
 * Apart of adding handlers this function also enables and disables decoding
 * of data services depending on the presence of at least one handler for the
 * respective data. A VBI_EVENT_TTX_PAGE handler for example enables Teletext
 * decoding.
 * 
 * This function can be safely called at any time, even from a handler.
 * 
 * Return value:
 * FALSE (0) on failure, practically that means lack of memory.
 **/
vbi_bool
vbi_event_handler_add(vbi_decoder *vbi, int event_mask,
		      vbi_event_handler handler, void *user_data) 
{
	struct event_handler *eh, **ehp;
	int found = 0, mask = 0, was_locked;
	int activate;

	/* If was_locked we're a handler, no recursion. */
	was_locked = pthread_mutex_trylock(&vbi->event_mutex);

	ehp = &vbi->handlers;

	while ((eh = *ehp)) {
		if (eh->handler == handler) {
			found = 1;

			if (!event_mask) {
				*ehp = eh->next;

				if (vbi->next_handler == eh)
					vbi->next_handler = eh->next;
						/* in event send loop */
				free(eh);

				continue;
			} else
				eh->event_mask = event_mask;
		}

		mask |= eh->event_mask;	
		ehp = &eh->next;
	}

	if (!found && event_mask) {
		if (!(eh = calloc(1, sizeof(*eh))))
			return FALSE;

		eh->event_mask = event_mask;
		mask |= event_mask;

		eh->handler = handler;
		eh->user_data = user_data;

		*ehp = eh;
	}

	activate = mask & ~vbi->event_mask;

	if (activate & VBI_EVENT_TTX_PAGE)
		vbi_teletext_channel_switched(vbi);
	if (activate & VBI_EVENT_CAPTION)
		vbi_caption_channel_switched(vbi);
	if (activate & VBI_EVENT_NETWORK)
		memset(&vbi->network, 0, sizeof(vbi->network));
	if (activate & VBI_EVENT_TRIGGER)
		vbi_trigger_flush(vbi);
	if (activate & (VBI_EVENT_ASPECT | VBI_EVENT_PROG_INFO)) {
		if (!(vbi->event_mask & (VBI_EVENT_ASPECT | VBI_EVENT_PROG_INFO))) {
			vbi_reset_prog_info(&vbi->prog_info[0]);
			vbi_reset_prog_info(&vbi->prog_info[1]);

			vbi->prog_info[1].future = TRUE;
			vbi->prog_info[0].future = FALSE;

			vbi->aspect_source = 0;
		}
	}

	vbi->event_mask = mask;

	if (!was_locked)
		pthread_mutex_unlock(&vbi->event_mutex);

	return TRUE;
}

/**
 * vbi_event_handler_remove:
 * @vbi: Initialized vbi decoding context.
 * @handler: Event handler function.
 * 
 * Unregisters an event handler.
 *
 * Apart of removing handlers this function also disables decoding
 * of data services depending on the presence of at least one handler for the
 * respective data. Removing the last VBI_EVENT_TTX_PAGE handler for example
 * disables Teletext decoding.
 * 
 * This function can be safely called at any time, even from a handler
 * removing itself or another handler, and regardless if the @handler
 * in question has been actually registered.
 **/
void
vbi_event_handler_remove(vbi_decoder *vbi, vbi_event_handler handler)
{
	vbi_event_handler_add(vbi, 0, handler, NULL);
} 

/**
 * vbi_send_event:
 * @vbi: Initialized vbi decoding context.
 * @ev: The event to send.
 * 
 * Traverses the list of event handlers and calls each handler waiting
 * for this ev->type of event, passing @ev as parameter.
 * 
 * This function is reentrant, but not supposed to be called from
 * different threads to ensure correct event order.
 **/
void
vbi_send_event(vbi_decoder *vbi, vbi_event *ev)
{
	struct event_handler *eh;

	pthread_mutex_lock(&vbi->event_mutex);

	for (eh = vbi->handlers; eh; eh = vbi->next_handler) {
		vbi->next_handler = eh->next;

		if (eh->event_mask & ev->type)
			eh->handler(ev, eh->user_data);
	}

	pthread_mutex_unlock(&vbi->event_mutex);
}

/*
 *  VBI Decoder
 */

static inline double
current_time(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return tv.tv_sec + tv.tv_usec * (1 / 1e6);
}

/**
 * vbi_decode:
 * @vbi: Initialized vbi decoding context.
 * @sliced: Array of #vbi_sliced data packets to be decoded.
 * @lines: Number of #vbi_sliced data packets, i. e. VBI lines.
 * @time: Timestamp associated with <emphasis>all</> sliced data packets.
 *   This is the time in seconds and fractions since 1970-01-01 00:00,
 *   for example from function gettimeofday(). @time should only
 *   increment, the latest time entered is considered the current time.
 * 
 * Main entry to the VBI decoder. Decodes zero or more lines of sliced
 * VBI data, updates the decoder state and calls event handlers.
 * 
 * @timestamp shall advance by 1/30 to 1/25 seconds whenever calling this
 * function. Failure to do so will be interpreted as frame dropping, which
 * starts a resynchronization cycle, and a channel switch may be assumed
 * which resets even more decoder state. So even if a frame did not contain
 * any useful data this function must be called, with @lines set to zero.
 * 
 * This is one of the few <emphasis>not reentrant</> functions, and it
 * must never be called from an event handler.
 **/
void
vbi_decode(vbi_decoder *vbi, vbi_sliced *sliced, int lines, double time)
{
	double d;

	d = time - vbi->time;

	if (vbi->time > 0 && (d < 0.025 || d > 0.050)) {
	  /*
	   *  Since (dropped >= channel switch) we give
	   *  ~1.5 s, then assume a switch.
	   */
	  pthread_mutex_lock(&vbi->chswcd_mutex);

	  if (vbi->chswcd == 0)
		  vbi->chswcd = 40;

	  pthread_mutex_unlock(&vbi->chswcd_mutex);

	  if (0)
		  fprintf(stderr, "vbi frame/s dropped at %f, D=%f\n",
			  time, time - vbi->time);

	  if (vbi->event_mask &
	      (VBI_EVENT_TTX_PAGE | VBI_EVENT_NETWORK))
		  vbi_teletext_desync(vbi);
	  if (vbi->event_mask &
	      (VBI_EVENT_CAPTION | VBI_EVENT_NETWORK))
		  vbi_caption_desync(vbi);
	} else {
		pthread_mutex_lock(&vbi->chswcd_mutex);
		
		if (vbi->chswcd > 0 && --vbi->chswcd == 0) {
			pthread_mutex_unlock(&vbi->chswcd_mutex);
			vbi_chsw_reset(vbi, 0);
		} else
			pthread_mutex_unlock(&vbi->chswcd_mutex);
	}

	if (time > vbi->time)
		vbi->time = time;

	while (lines) {
		if (sliced->id & VBI_SLICED_TELETEXT_B)
			vbi_decode_teletext(vbi, sliced->data);
		else if (sliced->id & (VBI_SLICED_CAPTION_525 | VBI_SLICED_CAPTION_625))
			vbi_decode_caption(vbi, sliced->line, sliced->data);
		else if (sliced->id & VBI_SLICED_VPS)
			vbi_decode_vps(vbi, sliced->data);
		else if (sliced->id & VBI_SLICED_WSS_625)
			vbi_decode_wss_625(vbi, sliced->data, time);
		else if (sliced->id & VBI_SLICED_WSS_CPR1204)
			vbi_decode_wss_cpr1204(vbi, sliced->data);

		sliced++;
		lines--;
	}

	if (vbi->event_mask & VBI_EVENT_TRIGGER)
		vbi_deferred_trigger(vbi);

	if (0 && (rand() % 511) == 0)
		vbi_eacem_trigger(vbi, "<http://zapping.sourceforge.net>[n:Zapping][5450]");
}

void
vbi_chsw_reset(vbi_decoder *vbi, vbi_nuid identified)
{
	vbi_nuid old_nuid = vbi->network.ev.network.nuid;

	if (0)
		fprintf(stderr, "*** chsw identified=%d old nuid=%d\n",
			identified, old_nuid);

	vbi_cache_flush(vbi);

	vbi_teletext_channel_switched(vbi);
	vbi_caption_channel_switched(vbi);

	if (identified == 0) {
		memset(&vbi->network, 0, sizeof(vbi->network));

		if (old_nuid != 0) {
			vbi->network.type = VBI_EVENT_NETWORK;
			vbi_send_event(vbi, &vbi->network);
		}
	} /* else already identified */

	vbi_trigger_flush(vbi); /* sic? */

	if (vbi->aspect_source > 0) {
		vbi_event e;

		e.ev.aspect.first_line = (vbi->aspect_source == 1) ? 23 : 22;
		e.ev.aspect.last_line =	(vbi->aspect_source == 1) ? 310 : 262;
		e.ev.aspect.ratio = 1.0;
		e.ev.aspect.film_mode = 0;
		e.ev.aspect.open_subtitles = VBI_SUBT_UNKNOWN;

		e.type = VBI_EVENT_ASPECT;
		vbi_send_event(vbi, &e);
	}

	vbi_reset_prog_info(&vbi->prog_info[0]);
	vbi_reset_prog_info(&vbi->prog_info[1]);
	/* XXX event? */

	vbi->prog_info[1].future = TRUE;
	vbi->prog_info[0].future = FALSE;

	vbi->aspect_source = 0;

	vbi->wss_last[0] = 0;
	vbi->wss_last[1] = 0;
	vbi->wss_rep_ct = 0;
	vbi->wss_time = 0.0;

	vbi->vt.header_page.pgno = 0;

	pthread_mutex_lock(&vbi->chswcd_mutex);

	vbi->chswcd = 0;

	pthread_mutex_unlock(&vbi->chswcd_mutex);
}

/**
 * vbi_channel_switched:
 * @vbi: VBI decoding context
 * @nuid: Set to zero until further
 * 
 * Call this after switching away from the channel (that is RF
 * channel, baseband video line etc, precisely: the network) from
 * which this context is receiving vbi data, to reset the context
 * accordingly. The decoder attempts to detect channel switches
 * automatically, but this is not 100 % reliable esp. without
 * receiving and decoding Teletext or VPS.
 *
 * Note the reset request is not executed until the next frame
 * is about to be decoded, so you can still receive "old" events
 * after calling this.
 *
 * Side effects: A reset deletes all Teletext and Closed Caption
 * pages cached. Clients may receive a #VBI_EVENT_ASPECT and
 * #VBI_EVENT_NETWORK revoking a previous event. Note the
 * possibility of sending a blank #vbi_network to notify the
 * event handler of the [autodetected] switch and the [temporary]
 * inability to identify the new network.
 **/
void
vbi_channel_switched(vbi_decoder *vbi, vbi_nuid nuid)
{
	/* XXX nuid */

	pthread_mutex_lock(&vbi->chswcd_mutex);

	vbi->chswcd = 1;

	pthread_mutex_unlock(&vbi->chswcd_mutex);
}

static inline int
transp(int val, int brig, int cont)
{
	int r = (((val - 128) * cont) / 64) + brig;

	return SATURATE(r, 0, 255);
}

/**
 * vbi_transp_colormap:
 * @vbi: Initialized vbi decoding context.
 * @d: Destination palette.
 * @s: Source palette.
 * @entries: Size of source and destination palette.
 *
 * Transposes the source palette by vbi->brightness and vbi->contrast.
 **/
void
vbi_transp_colormap(vbi_decoder *vbi, vbi_rgba *d, vbi_rgba *s, int entries)
{
	int brig, cont;

	brig = SATURATE(vbi->brightness, 0, 255);
	cont = SATURATE(vbi->contrast, -128, +127);

	while (entries--) {
		*d++ = VBI_RGBA(transp(VBI_R(*s), brig, cont),
				transp(VBI_G(*s), brig, cont),
				transp(VBI_B(*s), brig, cont));
		s++;
	}
}

/**
 * vbi_set_brightness:
 * @vbi: Initialized vbi decoding context.
 * @brightness: 0 dark ... 255 bright, default 128.
 * 
 * Change brightness of text pages, this affects the
 * color palette of pages fetched with vbi_fetch_vt_page() and
 * vbi_fetch_cc_page().
 **/
void
vbi_set_brightness(vbi_decoder *vbi, int brightness)
{
	vbi->brightness = brightness;

	vbi_caption_color_level(vbi);
}

/**
 * vbi_set_contrast:
 * @vbi: Initialized vbi decoding context.
 * @contrast: -128 inverse ... 0 none ... 127 maximum, default 64.
 * 
 * Change contrast of text pages, this affects the
 * color palette of pages fetched with vbi_fetch_vt_page() and
 * vbi_fetch_cc_page().
 **/
void
vbi_set_contrast(vbi_decoder *vbi, int contrast)
{
	vbi->contrast = contrast;

	vbi_caption_color_level(vbi);
}

/**
 * vbi_classify_page:
 * @vbi: Initialized vbi decoding context.
 * @pgno: Teletext or Closed Caption page to examine, see
 *   #vbi_pgno.
 * @subno: The highest subpage number of this page will be
 *   stored here. @subno can be %NULL.
 * @language: If it is possible to determine the language a page
 *   is written in, a pointer to the language name (Latin-1) will
 *   be stored here, %NULL if the language is unknown. @language
 *   can be %NULL if this information is not needed.
 * 
 * Returns information about the page.
 * 
 * For Closed Caption pages (@pgno 1 ... 8) @subno will always
 * be zero, @language set or %NULL. The return value will be
 * VBI_SUBTITLE_PAGE for page 1 ... 4 (Closed Caption
 * channel 1 ... 4), VBI_NORMAL_PAGE for page 5 ... 8 (Text channel
 * 1 ... 4), or VBI_NO_PAGE if no data is currently transmitted on
 * the channel.
 *
 * For Teletext pages (@pgno 0x100 ... 0x8FF) @subno returns
 * the highest subpage number used. Note this number can be larger
 * (but not smaller) than the number of subpages actually received
 * and cached. Still there is no guarantee the advertised subpages
 * will ever appear or stay in cache.
 * <informaltable frame=none><tgroup cols=2><thead>
 * <row><entry>subno</><entry>meaning</></row>
 * </thead><tbody>
 * <row><entry>0</><entry>single page, no subpages</></row>
 * <row><entry>1</><entry>never</></row>
 * <row><entry>2 ... 0x3F7F</><entry>has subpages 1 ... @subno</></row>
 * <row><entry>0xFFFE</><entry>has unknown number (two or more) of subpages</></row>
 * <row><entry>0xFFFF</><entry>presence of subpages unknown</></row>
 * </tbody></tgroup></informaltable>
 * @language returns the language of a subtitle page, %NULL if unknown
 * or the page is not classified as VBI_SUBTITLE_PAGE.
 *
 * Other page types are:
 * <informaltable frame=none><tgroup cols=2><tbody>
 * <row><entry>VBI_NO_PAGE</><entry>Page is not in transmission</></row>
 * <row><entry>VBI_NORMAL_PAGE</><entry></></row>
 * <row><entry>VBI_SUBTITLE_PAGE</><entry></></row>
 * <row><entry>VBI_SUBTITLE_INDEX</><entry>Subtitle index page</></row>
 * <row><entry>VBI_NONSTD_SUBPAGES</><entry>For example a world time page</></row>
 * <row><entry>VBI_PROGR_WARNING</><entry>Program related warning</></row>
 * <row><entry>VBI_CURRENT_PROGR</><entry>Information about the
 * current program</></row>
 * <row><entry>VBI_NOW_AND_NEXT</><entry>Brief information about the
 * current and next program</></row>
 * <row><entry>VBI_PROGR_INDEX</><entry>Program index page</></row>
 * <row><entry>VBI_PROGR_SCHEDULE</><entry>Program schedule page</></row>
 * <row><entry>VBI_UNKNOWN_PAGE</><entry></></row>
 * </tbody></tgroup></informaltable>
 *
 * <important>The results of this function are volatile: As more information
 * becomes available and pages are edited (e. g. activation of subtitles,
 * news updates, program related pages) subpage numbers can grow, page
 * types, subno 0xFFFE and 0xFFFF and languages can change.
 * </important>
 *
 * Return value: 
 * Page type.
 **/
vbi_page_type
vbi_classify_page(vbi_decoder *vbi, vbi_pgno pgno,
		  vbi_subno *subno, char **language)
{
	struct page_info *pi;
	int code, subc;
	char *lang;

	if (!subno)
		subno = &subc;
	if (!language)
		language = &lang;

	*subno = 0;
	*language = NULL;

	if (pgno < 1) {
		return VBI_UNKNOWN_PAGE;
	} else if (pgno <= 8) {
		if ((current_time() - vbi->cc.channel[pgno - 1].time) > 20)
			return VBI_NO_PAGE;

		*language = vbi->cc.channel[pgno - 1].language;

		return (pgno <= 4) ? VBI_SUBTITLE_PAGE : VBI_NORMAL_PAGE;
	} else if (pgno < 0x100 || pgno > 0x8FF) {
		return VBI_UNKNOWN_PAGE;
	}

	pi = vbi->vt.page_info + pgno - 0x100;
	code = pi->code;

	if (code != VBI_UNKNOWN_PAGE) {
		if (code == VBI_SUBTITLE_PAGE) {
			if (pi->language != 0xFF)
				*language = vbi_font_descriptors[pi->language].label;
		} else if (code == VBI_TOP_BLOCK || code == VBI_TOP_GROUP)
			code = VBI_NORMAL_PAGE;
		else if (code == VBI_NOT_PUBLIC || code > 0xE0)
			return VBI_UNKNOWN_PAGE;

		*subno = pi->subcode;

		return code;
	}

	if ((pgno & 0xFF) <= 0x99) {
		*subno = 0xFFFF;
		return VBI_NORMAL_PAGE; /* wild guess */
	}

	return VBI_UNKNOWN_PAGE;
}

/**
 * vbi_reset_prog_info:
 * @pi: 
 * 
 * Convenience function to set a #vbi_program_info
 * structure to defaults.
 **/
void
vbi_reset_prog_info(vbi_program_info *pi)
{
	int i;

	/* PID */
	pi->month = -1;
	pi->day = -1;
	pi->hour = -1;
	pi->min = -1;
	pi->tape_delayed = 0;
	/* PL */
	pi->length_hour = -1;
	pi->length_min = -1;
	pi->elapsed_hour = -1;
	pi->elapsed_min = -1;
	pi->elapsed_sec = -1;
	/* PN */
	pi->title[0] = 0;
	/* PT */
	pi->type_classf = VBI_PROG_CLASSF_NONE;
	/* PR */
	pi->rating_auth = VBI_RATING_AUTH_NONE;
	/* PAS */
	pi->audio[0].mode = VBI_AUDIO_MODE_UNKNOWN;
	pi->audio[0].language = NULL;
	pi->audio[1].mode = VBI_AUDIO_MODE_UNKNOWN;
	pi->audio[1].language = NULL;
	/* PCS */
	pi->caption_services = -1;
	for (i = 0; i < 8; i++)
		pi->caption_language[i] = NULL;
	/* CGMS */
	pi->cgms_a = -1;
	/* AR */
	pi->aspect.first_line = -1;
	pi->aspect.last_line = -1;
	pi->aspect.ratio = 0.0;
	pi->aspect.film_mode = 0;
	pi->aspect.open_subtitles = VBI_SUBT_UNKNOWN;
	/* PD */
	for (i = 0; i < 8; i++)
		pi->description[i][0] = 0;
}

void
vbi_decoder_delete(vbi_decoder *vbi)
{
	vbi_trigger_flush(vbi);

	vbi_caption_destroy(vbi);

	pthread_mutex_destroy(&vbi->prog_info_mutex);
	pthread_mutex_destroy(&vbi->event_mutex);
	pthread_mutex_destroy(&vbi->chswcd_mutex);

	vbi_cache_destroy(vbi);

	free(vbi);
}

/**
 * vbi_decoder_new:
 * 
 * Allocate a new vbi decoding instance. This is the core
 * structure of libzvbi.
 * 
 * Return value: 
 * #vbi_decoder pointer or %NULL on failure.
 **/
vbi_decoder *
vbi_decoder_new(void)
{
	vbi_decoder *vbi;

	if (!(vbi = calloc(1, sizeof(*vbi))))
		return NULL;

	vbi_cache_init(vbi);

	pthread_mutex_init(&vbi->chswcd_mutex, NULL);
	pthread_mutex_init(&vbi->event_mutex, NULL);
	pthread_mutex_init(&vbi->prog_info_mutex, NULL);

	vbi->time = 0.0;

	vbi->brightness	= 128;
	vbi->contrast	= 64;

	vbi_teletext_init(vbi);

	vbi_teletext_set_level(vbi, VBI_WST_LEVEL_2p5);

	vbi_caption_init(vbi);

	return vbi;
}

/**
 * vbi_asprintf:
 * 
 * libzvbi internal helper function.
 * Note asprintf() is a GNU libc extension.
 **/
void
vbi_asprintf(char **errstr, char *templ, ...)
{
	char buf[512];
	va_list ap;
	int temp;

	if (!errstr)
		return;

	temp = errno;

	va_start(ap, templ);

	vsnprintf(buf, sizeof(buf) - 1, templ, ap);

	va_end(ap);

	*errstr = strdup(buf);

	errno = temp;
}
