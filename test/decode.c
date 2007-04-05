/*
 *  zvbi-decode -- decode sliced VBI data using low-level
 *		   libzvbi functions
 *
 *  Copyright (C) 2004, 2006, 2007 Michael H. Schimek
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

/* $Id: decode.c,v 1.21 2007/04/05 04:59:25 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <locale.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif

#include "src/version.h"
#if 2 == VBI_VERSION_MINOR
#  include "src/bcd.h"
#  include "src/conv.h"
#  include "src/pfc_demux.h"
#  include "src/dvb_demux.h"
#  include "src/idl_demux.h"
#  include "src/xds_demux.h"
#  include "src/vps.h"
#  include "src/hamm.h"
#  include "src/lang.h"
#  include "sliced.h"		/* sliced data from file */
#else /* 0.3 */
#  include "src/zvbi.h"
#  include "src/misc.h"		/* _vbi_to_ascii() */
#endif

#define _(x) x /* i18n TODO */

/* Will be installed one day. */
#define PROGRAM_NAME "zvbi-decode"

#ifndef HAVE_PROGRAM_INVOCATION_NAME
static char *			program_invocation_name;
static char *			program_invocation_short_name;
#endif

static vbi_bool			source_is_pes; /* ATSC/DVB */

static vbi_pgno			option_pfc_pgno;
static unsigned int		option_pfc_stream;

static vbi_bool			option_decode_ttx;
static vbi_bool			option_decode_8301;
static vbi_bool			option_decode_8302;
static vbi_bool			option_decode_caption;
static vbi_bool			option_decode_xds;
static vbi_bool			option_decode_idl;
static vbi_bool			option_decode_vps;
static vbi_bool			option_decode_vps_other;
static vbi_bool			option_decode_wss;

static vbi_bool			option_dump_network;
static vbi_bool			option_dump_hex;
static vbi_bool			option_dump_bin;
static vbi_bool			option_dump_time;
static double			option_metronome_tick;

static vbi_pgno			option_pfc_pgno	= 0;
static unsigned int		option_pfc_stream = 0;

static unsigned int		option_idl_channel = 0;
static unsigned int		option_idl_address = 0;

/* Demultiplexers. */

static vbi_pfc_demux *		pfc;
static vbi_dvb_demux *		dvb;
static vbi_idl_demux *		idl;
static vbi_xds_demux *		xds;

extern void
_vbi_pfc_block_dump		(const vbi_pfc_block *	pb,
				 FILE *			fp,
				 vbi_bool		binary);

static void
error_exit			(const char *		template,
				 ...)
{
	va_list ap;

	fprintf (stderr, "%s: ", program_invocation_short_name);

	va_start (ap, template);
	vfprintf (stderr, template, ap);
	va_end (ap);         

	fputc ('\n', stderr);

	exit (EXIT_FAILURE);
}

static void
no_mem_exit			(void)
{
	error_exit (_("Out of memory."));
}

static void
put_cc_char			(unsigned int		c1,
				 unsigned int		c2)
{
	uint16_t ucs2_str[1];

	/* All caption characters are representable
	   in UTF-8, but not necessarily in ASCII. */
	ucs2_str[0] = vbi_caption_unicode ((c1 * 256 + c2) & 0x777F,
					   /* to_upper */ FALSE);

	vbi_fputs_iconv_ucs2 (stdout,
			       vbi_locale_codeset (),
			       ucs2_str, 1,
			       /* repl_char */ '?');
}

static void
caption_command			(unsigned int		line,
				 unsigned int		c1,
				 unsigned int		c2)
{
	unsigned int ch;
	unsigned int a7;
	unsigned int f;
	unsigned int b7;
	unsigned int u;

	printf ("CC line=%3u cmd 0x%02x 0x%02x ", line, c1, c2);

	if (0 == c1) {
		printf ("null\n");
		return;
	} else if (c2 < 0x20) {
		printf ("invalid\n");
		return;
	}

	/* Some common bit groups. */

	ch = (c1 >> 3) & 1; /* channel */
	a7 = c1 & 7;
	f = c1 & 1; /* field */
	b7 = (c2 >> 1) & 7;
	u = c2 & 1; /* underline */

	if (c2 >= 0x40) {
		static const int row [16] = {
			/* 0 */ 10,			/* 0x1040 */
			/* 1 */ -1,			/* unassigned */
			/* 2 */ 0, 1, 2, 3,		/* 0x1140 ... 0x1260 */
			/* 6 */ 11, 12, 13, 14,		/* 0x1340 ... 0x1460 */
			/* 10 */ 4, 5, 6, 7, 8, 9	/* 0x1540 ... 0x1760 */
		};
		unsigned int rrrr;

		/* Preamble Address Codes -- 001 crrr  1ri bbbu */
  
		rrrr = a7 * 2 + ((c2 >> 5) & 1);

		if (c2 & 0x10)
			printf ("PAC ch=%u row=%u column=%u u=%u\n",
				ch, row[rrrr], b7 * 4, u);
		else
			printf ("PAC ch=%u row=%u color=%u u=%u\n",
				ch, row[rrrr], b7, u);
		return;
	}

	/* Control codes -- 001 caaa  01x bbbu */

	switch (a7) {
	case 0:
		if (c2 < 0x30) {
			const char *mnemo [16] = {
				"BWO", "BWS", "BGO", "BGS",
				"BBO", "BBS", "BCO", "BCS",
				"BRO", "BRS", "BYO", "BYS",
				"BMO", "BMS", "BAO", "BAS"
			};

			printf ("%s ch=%u\n", mnemo[c2 & 0xF], ch);
			return;
		}

		break;

	case 1:
		if (c2 < 0x30) {
			printf ("mid-row ch=%u color=%u u=%u\n", ch, b7, u);
		} else {
			printf ("special character ch=%u 0x%02x%02x='",
				ch, c1, c2);
			put_cc_char (c1, c2);
			puts ("'");
		}

		return;

	case 2: /* first group */
	case 3: /* second group */
		printf ("extended character ch=%u 0x%02x%02x='", ch, c1, c2);
		put_cc_char (c1, c2);
		puts ("'");
		return;

	case 4: /* f=0 */
	case 5: /* f=1 */
		if (c2 < 0x30) {
			const char *mnemo [16] = {
				"RCL", "BS",  "AOF", "AON",
				"DER", "RU2", "RU3", "RU4",
				"FON", "RDC", "TR",  "RTD",
				"EDM", "CR",  "ENM", "EOC"
			};

			printf ("%s ch=%u f=%u\n", mnemo[c2 & 0xF], ch, f);
			return;
		}

		break;

	case 6: /* reserved */
		break;

	case 7:
		switch (c2) {
		case 0x21 ... 0x23:
			printf ("TO%u ch=%u\n", c2 - 0x20, ch);
			return;

		case 0x2D:
			printf ("BT ch=%u\n", ch);
			return;

		case 0x2E:
			printf ("FA ch=%u\n", ch);
			return;

		case 0x2F:
			printf ("FAU ch=%u\n", ch);
			return;

		default:
			break;
		}

		break;
	}

	printf ("unknown\n");
}

static vbi_bool
xds_cb				(vbi_xds_demux *	xd,
				 const vbi_xds_packet *	xp,
				 void *			user_data)
{
	xd = xd;
	user_data = user_data;

	_vbi_xds_packet_dump (xp, stdout);

	return TRUE; /* no errors */
}

static void
caption				(const uint8_t		buffer[2],
				 unsigned int		line)
{
	if (option_decode_xds && 284 == line) {
		if (!vbi_xds_demux_feed (xds, buffer)) {
			printf (_("Parity error in XDS data.\n"));
		}
	}

	if (option_decode_caption
	    && (21 == line || 284 == line /* NTSC */
		|| 22 == line /* PAL */)) {
		int c1;
		int c2;

		c1 = vbi_unpar8 (buffer[0]);
		c2 = vbi_unpar8 (buffer[1]);

		if ((c1 | c2) < 0) {
			printf (_("Parity error in CC line=%u "
				  " %s0x%02x %s0x%02x.\n"),
				line,
				(c1 < 0) ? ">" : "", buffer[0] & 0xFF,
				(c2 < 0) ? ">" : "", buffer[1] & 0xFF);
		} else if (c1 >= 0x20) {
			char text[2];

			printf ("CC line=%3u text 0x%02x 0x%02x '",
				line, c1, c2);

			/* All caption characters are representable
			   in UTF-8, but not necessarily in ASCII. */
			text[0] = c1;
			text[1] = c2; /* may be zero */

			/* Error ignored. */
			vbi_fputs_iconv (stdout,
					 vbi_locale_codeset (),
					 /* src_codeset */ "EIA-608",
					 text, 2, /* repl_char */ '?');

			puts ("'");
		} else if (0 == c1 || c1 >= 0x10) {
			caption_command (line, c1, c2);
		} else if (option_decode_xds) {
			printf ("CC line=%3u cmd 0x%02x 0x%02x ",
				line, c1, c2);
			if (0x0F == c1)
				puts ("XDS packet end");
			else
				puts ("XDS packet start/continue");
		}
	}
}

#if 3 == VBI_VERSION_MINOR /* XXX port me back */

static void
dump_cni			(vbi_cni_type		type,
				 unsigned int		cni)
{
	vbi_network nk;
	vbi_bool success;

	if (!option_dump_network)
		return;

	success = vbi_network_init (&nk);
	if (!success)
		no_mem_exit ();

	success = vbi_network_set_cni (&nk, type, cni);
	if (!success)
		no_mem_exit ();

	_vbi_network_dump (&nk, stdout);
	putchar ('\n');

	vbi_network_destroy (&nk);
}

#endif /* 3 == VBI_VERSION_MINOR */

static void
dump_bytes			(const uint8_t *	buffer,
				 unsigned int		n_bytes)
{
	unsigned int j;

	if (option_dump_bin) {
		fwrite (buffer, 1, n_bytes, stdout);
		return;
	}

	if (option_dump_hex) {
		for (j = 0; j < n_bytes; ++j)
			printf ("%02x ", buffer[j]);
	}

	putchar ('>');

	for (j = 0; j < n_bytes; ++j) {
		/* For Teletext: Not all characters are representable
		   in ASCII or even UTF-8, but at this stage we don't
		   know the Teletext code page for a proper conversion. */
		char c = _vbi_to_ascii (buffer[j]);

		putchar (c);
	}

	puts ("<");
}

#if 3 == VBI_VERSION_MINOR /* XXX port me back */

static void
packet_8301			(const uint8_t		buffer[42],
				 unsigned int		designation)
{
	unsigned int cni;
	time_t time;
	int gmtoff;
	struct tm tm;

	if (!option_decode_8301)
		return;

	if (!vbi_decode_teletext_8301_cni (&cni, buffer)) {
		printf (_("Error in Teletext "
			  "packet 8/30 format 1 CNI.\n"));
		return;
	}

	if (!vbi_decode_teletext_8301_local_time (&time, &gmtoff, buffer)) {
		printf (_("Error in Teletext "
			  "packet 8/30 format 1 local time.\n"));
		return;
	}

	printf ("Teletext packet 8/30/%u cni=%x time=%u gmtoff=%d ",
		designation, cni, (unsigned int) time, gmtoff);

	gmtime_r (&time, &tm);

	printf ("(%4u-%02u-%02u %02u:%02u:%02u UTC)\n",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);

	if (0 != cni)
		dump_cni (VBI_CNI_TYPE_8301, cni);
}

static void
packet_8302			(const uint8_t		buffer[42],
				 unsigned int		designation)
{
	unsigned int cni;
	vbi_program_id pi;

	if (!option_decode_8302)
		return;

	if (!vbi_decode_teletext_8302_cni (&cni, buffer)) {
		printf (_("Error in Teletext "
			  "packet 8/30 format 2 CNI.\n"));
		return;
	}

	if (!vbi_decode_teletext_8302_pdc (&pi, buffer)) {
		printf (_("Error in Teletext "
			  "packet 8/30 format 2 PDC data.\n"));
		return;
	}

	printf ("Teletext packet 8/30/%u cni=%x ", designation, cni);

	_vbi_program_id_dump (&pi, stdout);

	putchar ('\n');

	if (0 != pi.cni)
		dump_cni (pi.cni_type, pi.cni);
}

#endif /* 3 == VBI_VERSION_MINOR */

static vbi_bool
page_function_clear_cb		(vbi_pfc_demux *	dx,
		                 const vbi_pfc_block *	block,
				 void *			user_data)
{
	dx = dx; /* unused */
	user_data = user_data;

	_vbi_pfc_block_dump (block, stdout, option_dump_bin);

	return TRUE;
}

static vbi_bool
idl_format_a_cb			(vbi_idl_demux *	idl,
				 const uint8_t *	buffer,
				 unsigned int		n_bytes,
				 unsigned int		flags,
				 void *			user_data)
{
	idl = idl;
	user_data = user_data;

	if (!option_dump_bin) {
		printf ("IDL-A%s%s ",
			(flags & VBI_IDL_DATA_LOST) ? " <data lost>" : "",
			(flags & VBI_IDL_DEPENDENT) ? " <dependent>" : "");
	}

	dump_bytes (buffer, n_bytes);

	return TRUE;
}

static void
packet_idl			(const uint8_t		buffer[42],
				 unsigned int		channel)
{
	int pa; /* packet address */
	int ft; /* format type */

	printf ("IDL ch=%u ", channel);

	switch (channel) {
	case 0:
		assert (0);

	case 4:
	case 12:
		printf (_("(Low bit rate audio) "));

		dump_bytes (buffer, 42);

		break;

	case 5:
	case 6:
	case 13:
	case 14:
		pa = vbi_unham8 (buffer[3]);
		pa |= vbi_unham8 (buffer[4]) << 4;
		pa |= vbi_unham8 (buffer[5]) << 8;

		if (pa < 0) {
			printf (_("Hamming error in Datavideo "
				  "packet-address byte.\n"));
			return;
		}

		printf ("(Datavideo) pa=0x%x ", pa);

		dump_bytes (buffer, 42);

		break;

	case 8:
	case 9:
	case 10:
	case 11:
	case 15:
		if ((ft = vbi_unham8 (buffer[2])) < 0) {
			printf (_("Hamming error in IDL format "
				  "A or B format-type byte.\n"));
			return;
		}

		if (0 == (ft & 1)) {
			int ial; /* interpretation and address length */
			unsigned int spa_length;
			int spa; /* service packet address */
			unsigned int i;

			if ((ial = vbi_unham8 (buffer[3])) < 0) {
				printf (_("Hamming error in IDL format "
					  "A interpretation-and-address-"
					  "length byte.\n"));
				return;
			}

			spa_length = (unsigned int) ial & 7;
			if (7 == spa_length) {
				printf ("(Format A?) ");
				dump_bytes (buffer, 42);
				return;
			}

			spa = 0;

			for (i = 0; i < spa_length; ++i)
				spa |= vbi_unham8 (buffer[4 + i]) << (4 * i);

			if (spa < 0) {
				printf (_("Hamming error in IDL format "
					  "A service-packet-address byte.\n"));
				return;
			}

			printf ("(Format A) spa=0x%x ", spa);
		} else if (1 == (ft & 3)) {
			int an; /* application number */
			int ai; /* application identifier */

			an = (ft >> 2);

			if ((ai = vbi_unham8 (buffer[3])) < 0) {
				printf (_("Hamming error in IDL format "
					  "B application-number byte.\n"));
				return;
			}

			printf ("(Format B) an=%d ai=%d ", an, ai);
		}

		dump_bytes (buffer, 42);

		break;

	default:
		dump_bytes (buffer, 42);

		break;
	}
}

static void
teletext			(const uint8_t		buffer[42],
				 unsigned int		line)
{
	int pmag;
	unsigned int magazine;
	unsigned int packet;

	if (NULL != pfc) {
		if (!vbi_pfc_demux_feed (pfc, buffer)) {
			printf (_("Error in Teletext "
				  "PFC packet.\n"));
			return;
		}
	}

	if (!(option_decode_ttx |
	      option_decode_8301 |
	      option_decode_8302 |
	      option_decode_idl))
		return;

	pmag = vbi_unham16p (buffer);
	if (pmag < 0) {
		printf (_("Hamming error in Teletext "
			  "packet number.\n"));
		return;
	}

	magazine = pmag & 7;
	if (0 == magazine)
		magazine = 8;

	packet = pmag >> 3;

	if (8 == magazine && 30 == packet) {
		int designation;

		designation = vbi_unham8 (buffer[2]);
		if (designation < 0 ) {
			printf (_("Hamming error in Teletext "
				  "packet 8/30 designation byte.\n"));
			return;
		}

		if (designation >= 0 && designation <= 1) {
#if 3 == VBI_VERSION_MINOR /* XXX port me back */
			packet_8301 (buffer, designation);
#endif
			return;
		}

		if (designation >= 2 && designation <= 3) {
#if 3 == VBI_VERSION_MINOR /* XXX port me back */
			packet_8302 (buffer, designation);
#endif
			return;
		}
	}

	if (30 == packet || 31 == packet) {
		if (option_decode_idl) {
#if 1
			packet_idl (buffer, pmag & 15);
#else
			printf ("Teletext IDL packet %u/%2u ",
				magazine, packet);
			dump_bytes (buffer, /* n_bytes */ 42);
#endif
			return;
		}
	}

	if (option_decode_ttx) {
		printf ("Teletext line=%3u %x/%2u ",
			line, magazine, packet);
		dump_bytes (buffer, /* n_bytes */ 42);
		return;
	}
}

static void
vps				(const uint8_t		buffer[13],
				 unsigned int		line)
{
	if (option_decode_vps) {
		unsigned int cni;
#if 3 == VBI_VERSION_MINOR
		vbi_program_id pi;
#endif
		if (option_dump_bin) {
			printf ("VPS line=%3u ", line);
			fwrite (buffer, 1, 13, stdout);
			fflush (stdout);
			return;
		}

		if (!vbi_decode_vps_cni (&cni, buffer)) {
			printf (_("Error in VPS packet CNI.\n"));
			return;
		}

#if 3 == VBI_VERSION_MINOR
		if (!vbi_decode_vps_pdc (&pi, buffer)) {
			printf (_("Error in VPS packet PDC data.\n"));
			return;
		}
		
		printf ("VPS line=%3u ", line);

		_vbi_program_id_dump (&pi, stdout);

		putchar ('\n');

		if (0 != pi.cni)
			dump_cni (pi.cni_type, pi.cni);
#else
		printf ("VPS line=%3u CNI=%x\n", line, cni);
#endif
	}

	if (option_decode_vps_other) {
		static char pr_label[2][20];
		static char label[2][20];
		static int l[2] = { 0, 0 };
		unsigned int i;
		int c;

		i = (line != 16);

		c = vbi_rev8 (buffer[1]);

		if (c & 0x80) {
			label[i][l[i]] = 0;
			strcpy (pr_label[i], label[i]);
			l[i] = 0;
		}

		label[i][l[i]] = _vbi_to_ascii (c);

		l[i] = (l[i] + 1) % 16;
		
		printf ("VPS line=%3u bytes 3-10: "
			"%02x %02x (%02x='%c') %02x %02x "
			"%02x %02x %02x %02x (\"%s\")\n",
			line,
			buffer[0], buffer[1],
			c, _vbi_to_ascii (c),
			buffer[2], buffer[3],
			buffer[4], buffer[5], buffer[6], buffer[7],
			pr_label[i]);
	}
}

#if 3 == VBI_VERSION_MINOR /* XXX port me back */

static void
wss_625				(const uint8_t		buffer[2])
{
	if (option_decode_wss) {  
		vbi_aspect_ratio ar;

		if (!vbi_decode_wss_625 (&ar, buffer)) {
			printf (_("Error in WSS packet.\n"));
			return;
		}

		fputs ("WSS ", stdout);

		_vbi_aspect_ratio_dump (&ar, stdout);

		putchar ('\n');
	}
}

#endif /* 3 == VBI_VERSION_MINOR */

static void
decode				(const vbi_sliced *	s,
				 unsigned int		n_lines,
				 double			sample_time,
				 int64_t		stream_time)
{
	static double metronome = 0.0;
	static double last_sample_time = 0.0;
	static int64_t last_stream_time = 0;

	if (option_dump_time || option_metronome_tick > 0.0) {
		/* Sample time: When we captured the data, in
		   		seconds since 1970-01-01 (gettimeofday()).
		   Stream time: For ATSC/DVB the Presentation TimeStamp.
				For analog the frame number multiplied by
				the nominal frame period (1/25 or
				1001/30000 s). Both given in 90000 kHz units.
		   Note this isn't fully implemented yet. */

		if (option_metronome_tick > 0.0) {
			printf ("ST %f (adv %+f, err %+f) PTS %"
				PRId64 " (adv %+" PRId64 ", err %+f)\n",
				sample_time, sample_time - last_sample_time,
				sample_time - metronome,
				stream_time, stream_time - last_stream_time,
				(double) stream_time - metronome);

			metronome += option_metronome_tick;
		} else {
			printf ("ST %f (%+f) PTS %" PRId64 " (%+" PRId64 ")\n",
				sample_time, sample_time - last_sample_time,
				stream_time, stream_time - last_stream_time);
		}

		last_sample_time = sample_time;
		last_stream_time = stream_time;
	}

	while (n_lines > 0) {
		switch (s->id) {
		case VBI_SLICED_TELETEXT_B_L10_625:
		case VBI_SLICED_TELETEXT_B_L25_625:
		case VBI_SLICED_TELETEXT_B_625:
			teletext (s->data, s->line);
			break;

		case VBI_SLICED_VPS:
		case VBI_SLICED_VPS_F2:
			vps (s->data, s->line);
			break;

		case VBI_SLICED_CAPTION_625_F1:
		case VBI_SLICED_CAPTION_625_F2:
		case VBI_SLICED_CAPTION_625:
		case VBI_SLICED_CAPTION_525_F1:
		case VBI_SLICED_CAPTION_525_F2:
		case VBI_SLICED_CAPTION_525:
			caption (s->data, s->line);
			break;

		case VBI_SLICED_WSS_625:
#if 3 == VBI_VERSION_MINOR /* XXX port me back */
			wss_625 (s->data);
#endif
			break;

		case VBI_SLICED_WSS_CPR1204:
			break;
		}

		++s;
		--n_lines;
	}
}

static void
pes_mainloop			(void)
{
	uint8_t buffer[2048];

	while (1 == fread (buffer, sizeof (buffer), 1, stdin)) {
		const uint8_t *bp;
		unsigned int left;

		bp = buffer;
		left = sizeof (buffer);

		while (left > 0) {
			vbi_sliced sliced[64];
			unsigned int n_lines;
			int64_t pts;

			n_lines = vbi_dvb_demux_cor (dvb, sliced, 64,
						     &pts, &bp, &left);
			if (n_lines > 0)
				decode (sliced, n_lines,
					/* sample_time */ 0,
					/* stream_time */ pts);
		}
	}

	fprintf (stderr, _("\rEnd of stream\n"));
}

#if 3 == VBI_VERSION_MINOR /* XXX replace me, I'm redundant */

static void
old_mainloop			(void)
{
	double time;

	time = 0.0;

	for (;;) {
		char buf[256];
		double dt;
		unsigned int n_items;
		vbi_sliced sliced[40];
		vbi_sliced *s;

		if (ferror (stdin) || !fgets (buf, 255, stdin))
			goto abort;

		dt = strtod (buf, NULL);
		n_items = fgetc (stdin);

		assert (n_items < 40);

		s = sliced;

		while (n_items-- > 0) {
			int index;

			index = fgetc (stdin);

			s->line = (fgetc (stdin)
				   + 256 * fgetc (stdin)) & 0xFFF;

			if (index < 0)
				goto abort;

			switch (index) {
			case 0:
				s->id = VBI_SLICED_TELETEXT_B;
				fread (s->data, 1, 42, stdin);
				break;

			case 1:
				s->id = VBI_SLICED_CAPTION_625; 
				fread (s->data, 1, 2, stdin);
				break; 

			case 2:
				s->id = VBI_SLICED_VPS;
				fread (s->data, 1, 13, stdin);
				break;

			case 3:
				s->id = VBI_SLICED_WSS_625; 
				fread (s->data, 1, 2, stdin);
				break;

			case 4:
				s->id = VBI_SLICED_WSS_CPR1204; 
				fread (s->data, 1, 3, stdin);
				break;

			case 7:
				s->id = VBI_SLICED_CAPTION_525; 
				fread(s->data, 1, 2, stdin);
				break;

			default:
				fprintf (stderr,
					 "\nOops! Unknown data type %d "
					 "in sample file\n", index);
				exit (EXIT_FAILURE);
			}

			++s;
		}

		decode (sliced, s - sliced, time, 0);

		if (feof (stdin) || ferror (stdin))
			goto abort;

		time += dt;
	}

	return;

abort:
	fprintf (stderr, "\rEnd of stream\n");
}

#else /* 2 == VBI_VERSION_MINOR */

static void
old_mainloop			(void)
{
	for (;;) {
		vbi_sliced sliced[40];
		double timestamp;
		int n_lines;

		n_lines = read_sliced (sliced, &timestamp,
				       /* max_lines */ 40);
		if (n_lines < 0)
			break;

		decode (sliced, n_lines,
			/* sample_time */ timestamp,
			/* stream_time */ 0);
	}

	fprintf (stderr, "\rEnd of stream\n");
}

#endif /* 2 == VBI_VERSION_MINOR */

static void
usage				(FILE *			fp)
{
	fprintf (fp, "\
%s %s -- low-level VBI decoder\n\n\
Copyright (C) 2004, 2006, 2007 Michael H. Schimek\n\
This program is licensed under GPL 2 or later. NO WARRANTIES.\n\n\
Usage: %s [options] < sliced VBI data\n\n\
-h | --help | --usage  Print this message and exit\n\
-V | --version         Print the program version and exit\n\
Input options:\n\
-P | --pes             Source is a DVB PES stream [auto-detected]\n\
Decoding options:\n"
#if 3 == VBI_VERSION_MINOR /* XXX port me back */
"-1 | --8301            Teletext packet 8/30 format 1 (local time)\n\
-2 | --8302            Teletext packet 8/30 format 2 (PDC)\n"
#endif
"-c | --cc              Closed Caption\n\
-i | --idl             Any Teletext IDL packets (M/30, M/31)\n\
-t | --ttx             Decode any Teletext packet\n\
-v | --vps             Video Programming System (PDC)\n"
#if 3 == VBI_VERSION_MINOR /* XXX port me back */
"-w | --wss             Wide Screen Signalling\n"
#endif
"-x | --xds             Decode eXtended Data Service (NTSC line 284)\n\
-a | --all             Everything above, e.g.\n\
                       -i     decode IDL packets\n\
                       -a     decode everything\n\
                       -a -i  everything except IDL\n\
-c | --idl-ch N\n\
-d | --idl-addr NNN\n\
                       Decode Teletext IDL format A data from channel N,\n\
                       service packet address NNN [0]\n\
-r | --vps-other       Decode VPS data unrelated to PDC\n\
-p | --pfc-pgno NNN\n\
-s | --pfc-stream NN   Decode Teletext Page Function Clear data\n\
                         from page NNN (for example 1DF), stream NN [0]\n\
Modifying options:\n\
-e | --hex             With -t dump packets in hex and ASCII,\n\
                         otherwise only ASCII\n\
-n | --network         With -1, -2, -v decode CNI and print\n\
                         available information about the network\n\
-b | --bin             With -t, -p, -v dump data in binary format\n\
                         instead of ASCII\n\
-T | --time            Dump capture timestamps\n\
-m | --metronome tick  Compare timestamps against a metronome advancing\n\
                       by tick seconds per frame\n\
",
		 PROGRAM_NAME, VERSION, program_invocation_name);
}

static const char
short_options [] = "12abcd:ehil:m:np:rs:tvwxPTV";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "8301",	no_argument,		NULL,		'1' },
	{ "8302",	no_argument,		NULL,		'2' },
	{ "all",	no_argument,		NULL,		'a' },
	{ "bin",	no_argument,		NULL,		'b' },
	{ "cc",		no_argument,		NULL,		'c' },
	{ "idl-addr",	required_argument,	NULL,		'd' },
	{ "hex",	no_argument,		NULL,		'e' },
	{ "help",	no_argument,		NULL,		'h' },
	{ "usage",	no_argument,		NULL,		'h' },
	{ "idl",	no_argument,		NULL,		'i' },
	{ "idl-ch",	required_argument,	NULL,		'l' },
	{ "metronome",	required_argument,	NULL,		'm' },
	{ "network",	no_argument,		NULL,		'n' },
	{ "pfc-pgno",	required_argument,	NULL,		'p' },
	{ "vps-other",	no_argument,		NULL,		'r' },
	{ "pfc-stream",	required_argument,	NULL,		's' },
	{ "ttx",	no_argument,		NULL,		't' },
	{ "vps",	no_argument,		NULL,		'v' },
	{ "wss",	no_argument,		NULL,		'w' },
	{ "xds",	no_argument,		NULL,		'x' },
	{ "pes",	no_argument,		NULL,		'P' },
	{ "time",	no_argument,		NULL,		'T' },
	{ "version",	no_argument,		NULL,		'V' },
	{ NULL, 0, 0, 0 }
};
#else
#  define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static int			option_index;

int
main				(int			argc,
				 char **		argv)
{
	int c;

#ifndef HAVE_PROGRAM_INVOCATION_NAME
	{
		unsigned int i;

		for (i = strlen (argv[0]); i > 0; --i) {
			if ('/' == argv[0][i - 1])
				break;
		}

		program_invocation_short_name = &argv[0][i];
	}
#endif

	setlocale (LC_ALL, "");

	for (;;) {
		int c;

		c = getopt_long (argc, argv, short_options,
				 long_options, &option_index);
		if (-1 == c)
			break;

		switch (c) {
		case 0: /* getopt_long() flag */
			break;

		case '1':
			option_decode_8301 ^= TRUE;
			break;

		case '2':
			option_decode_8302 ^= TRUE;
			break;

		case 'b':
			option_dump_bin ^= TRUE;
			break;

		case 'c':
			option_decode_caption ^= TRUE;
			break;

		case 'd':
			assert (NULL != optarg);
			option_idl_address = strtol (optarg, NULL, 10);
			break;

		case 'e':
			option_dump_hex ^= TRUE;
			break;

		case 'a':
			option_decode_ttx = TRUE;
			option_decode_8301 = TRUE;
			option_decode_8302 = TRUE;
			option_decode_caption = TRUE;
			option_decode_idl = TRUE;
			option_decode_vps = TRUE;
			option_decode_wss = TRUE;
			option_decode_xds = TRUE;
			option_pfc_pgno = 0x1DF;
			break;

		case 'h':
			usage (stdout);
			exit (EXIT_SUCCESS);

		case 'i':
			option_decode_idl ^= TRUE;
			break;

		case 'l':
			assert (NULL != optarg);
			option_idl_channel = strtol (optarg, NULL, 10);
			break;

		case 'm':
			assert (NULL != optarg);
			option_metronome_tick = strtod (optarg, NULL);
			break;

		case 'n':
			option_dump_network ^= TRUE;
			break;

		case 'p':
			assert (NULL != optarg);
			option_pfc_pgno = strtol (optarg, NULL, 16);
			break;

		case 'r':
			option_decode_vps_other ^= TRUE;
			break;

		case 's':
			assert (NULL != optarg);
			option_pfc_stream = strtol (optarg, NULL, 10);
			break;

		case 't':
			option_decode_ttx ^= TRUE;
			break;

		case 'v':
			option_decode_vps ^= TRUE;
			break;

		case 'w':
			option_decode_wss ^= TRUE;
			break;

		case 'x':
			option_decode_xds ^= TRUE;
			break;

		case 'P':
			source_is_pes ^= TRUE;
			break;

		case 'T':
			option_dump_time ^= TRUE;
			break;

		case 'V':
			printf (PROGRAM_NAME " " VERSION "\n");
			exit (EXIT_SUCCESS);

		default:
			usage (stderr);
			exit (EXIT_FAILURE);
		}
	}

	if (isatty (STDIN_FILENO))
		error_exit (_("No VBI data on standard input."));

	if (0 != option_pfc_pgno) {
		pfc = vbi_pfc_demux_new (option_pfc_pgno,
					 option_pfc_stream,
					 page_function_clear_cb,
					 /* user_data */ NULL);
		if (NULL == pfc)
			no_mem_exit ();
	}

	if (0 != option_idl_channel) {
		idl = vbi_idl_a_demux_new (option_idl_channel,
					   option_idl_address,
					   idl_format_a_cb,
					   /* user_data */ NULL);
		if (NULL == idl)
			no_mem_exit ();
	}

	if (option_decode_xds) {
		xds = vbi_xds_demux_new (xds_cb,
					 /* used_data */ NULL);
		if (NULL == xds)
			no_mem_exit ();
	}

	c = getchar ();
	ungetc (c, stdin);

	if (0 == c || source_is_pes) {
		dvb = vbi_dvb_pes_demux_new (/* callback */ NULL,
					     /* user_data */ NULL);
		if (NULL == dvb)
			no_mem_exit ();

		pes_mainloop ();
	} else {
#if 2 == VBI_VERSION_MINOR /* XXX port me */
		open_sliced_read (stdin);
#endif
		old_mainloop ();
	}

	vbi_dvb_demux_delete (dvb);
	vbi_idl_demux_delete (idl);
	vbi_pfc_demux_delete (pfc);
	vbi_xds_demux_delete (xds);

	exit (EXIT_SUCCESS);

	return 0;
}
