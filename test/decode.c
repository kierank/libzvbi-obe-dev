/*
 *  libzvbi test
 *
 *  Copyright (C) 2004-2005 Michael H. Schimek
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

/* $Id: decode.c,v 1.8 2006/02/10 06:25:38 mschimek Exp $ */

#undef NDEBUG

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif

#include "src/libzvbi.h"
#include "sliced.h"

#ifndef PRId64
#  define PRId64 "lld"
#endif

static vbi_bool			source_pes	= FALSE;

static vbi_bool			decode_ttx	= FALSE;
static vbi_bool			decode_8301	= FALSE;
static vbi_bool			decode_8302	= FALSE;
static vbi_bool			decode_idl	= FALSE;
static vbi_bool			decode_vps	= FALSE;
static vbi_bool			decode_vps_r	= FALSE;
static vbi_bool			decode_wss	= FALSE;
static vbi_bool			decode_xds	= FALSE;
static vbi_bool			dump_network	= FALSE;
static vbi_bool			dump_hex	= FALSE;
static vbi_bool			dump_bin	= FALSE;
static vbi_bool			dump_time	= FALSE;

static vbi_pgno			pfc_pgno	= 0;
static unsigned int		pfc_stream	= 0;

static unsigned int		idl_channel	= 0;
static unsigned int		idl_address	= 0;

static vbi_pfc_demux *		pfc;
static vbi_dvb_demux *		dvb;
static vbi_idl_demux *		idl;
static vbi_xds_demux *		xds;

extern void
_vbi_pfc_block_dump		(const vbi_pfc_block *	pb,
				 FILE *			fp,
				 vbi_bool		binary);

vbi_inline int
vbi_printable			(int			c)
{
	if (c < 0)
		return '?';

	c &= 0x7F;

	if (c < 0x20 || c >= 0x7F)
		return '.';

	return c;
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
	if (decode_xds && 284 == line) {
		if (!vbi_xds_demux_feed (xds, buffer)) {
			printf ("Parity error in XDS data\n");
			return;
		}
	}
}

#if 0

static void
dump_cni			(vbi_cni_type		type,
				 unsigned int		cni)
{
	vbi_network nk;

	if (!dump_network)
		return;

	assert (vbi_network_init (&nk));
	assert (vbi_network_set_cni (&nk, type, cni));

	_vbi_network_dump (&nk, stdout);
	putchar ('\n');

	vbi_network_destroy (&nk);
}

#endif

static void
dump_bytes			(const uint8_t *	buffer,
				 unsigned int		n_bytes)
{
	unsigned int j;

	if (dump_bin) {
		fwrite (buffer, 1, n_bytes, stdout);
		return;
	}

	if (dump_hex) {
		for (j = 0; j < n_bytes; ++j)
			printf ("%02x ", buffer[j]);
	}

	putchar ('>');

	for (j = 0; j < n_bytes; ++j) {
		char c = vbi_printable (buffer[j]);

		putchar (c);
	}

	putchar ('<');
	putchar ('\n');
}

#if 0

static void
packet_8301			(const uint8_t		buffer[42],
				 unsigned int		designation)
{
	unsigned int cni;
	time_t time;
	int gmtoff;
	struct tm tm;

	if (!decode_8301)
		return;

	if (!vbi_decode_teletext_8301_cni (&cni, buffer)) {
		printf ("Hamming error in 8/30 format 1 cni\n");
		return;
	}

	if (!vbi_decode_teletext_8301_local_time (&time, &gmtoff, buffer)) {
		printf ("Hamming error in 8/30 format 1 local time\n");
		return;
	}

	printf ("Packet 8/30/%u cni=%x time=%u gmtoff=%d ",
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

	if (!decode_8302)
		return;

	if (!vbi_decode_teletext_8302_cni (&cni, buffer)) {
		printf ("Hamming error in 8/30 format 2 cni\n");
		return;
	}

	if (!vbi_decode_teletext_8302_pdc (&pi, buffer)) {
		printf ("Hamming error in 8/30 format 2 pdc data\n");
		return;
	}

	printf ("Packet 8/30/%u cni=%x ", designation, cni);

	_vbi_program_id_dump (&pi, stdout);

	putchar ('\n');

	if (0 != pi.cni)
		dump_cni (pi.cni_type, pi.cni);
}

#endif

static vbi_bool
page_function_clear_cb		(vbi_pfc_demux *	pfc,
		                 const vbi_pfc_block *	block,
				 void *			user_data)
{
	pfc = pfc;
	user_data = user_data;

	_vbi_pfc_block_dump (block, stdout, dump_bin);

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

	if (!dump_bin)
		printf ("IDL-A%s%s ",
			(flags & VBI_IDL_DATA_LOST) ? " <data lost>" : "",
			(flags & VBI_IDL_DEPENDENT) ? " <dependent>" : "");

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
		assert (!"reached");

	case 4:
	case 12:
		printf ("(Low bit rate audio) ");

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
			printf ("Hamming error in Datavideo packet pa\n");
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
			printf ("Hamming error in IDL format A or B ft\n");
			return;
		}

		if (0 == (ft & 1)) {
			int ial; /* interpretation and address length */
			unsigned int spa_length;
			int spa; /* service packet address */
			unsigned int i;

			if ((ial = vbi_unham8 (buffer[3])) < 0) {
				printf ("Hamming error in IDL format A ial\n");
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
				printf ("Hamming error in IDL format A spa\n");
				return;
			}

			printf ("(Format A) spa=0x%x ", spa);
		} else if (1 == (ft & 3)) {
			int an; /* application number */
			int ai; /* application identifier */

			an = (ft >> 2);

			if ((ai = vbi_unham8 (buffer[3]))) {
				printf ("Hamming error in IDL format B ai\n");
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

	if (pfc) {
		if (!vbi_pfc_demux_feed (pfc, buffer)) {
			printf ("Hamming error in PFC packet\n");
			return;
		}
	}

	if (idl) {
		if (!vbi_idl_demux_feed (idl, buffer)) {
			printf ("Hamming or CRC error in IDL packet\n");
			return;
		}
	}

	if (!(decode_ttx | decode_8301 | decode_8302 | decode_idl))
		return;

	if ((pmag = vbi_unham16p (buffer)) < 0) {
		printf ("Hamming error in Teletext packet pmag\n");
		return;
	}

	magazine = pmag & 7;
	if (0 == magazine)
		magazine = 8;

	packet = pmag >> 3;

	if (8 == magazine && 30 == packet) {
		int designation;

		if ((designation = vbi_unham8 (buffer[2])) < 0) {
			printf ("Hamming error in Teletext packet "
				"8/30 designation\n");
			return;
		}

		if (designation >= 0 && designation <= 1) {
#if 0
			packet_8301 (buffer, designation);
#endif
			return;
		}

		if (designation >= 2 && designation <= 3) {
#if 0
			packet_8302 (buffer, designation);
#endif
			return;
		}

		printf ("Packet 8/30 with unknown designation %d\n",
			designation);

		return;
	}

	if (30 == packet || 31 == packet) {
		if (decode_idl) {
			packet_idl (buffer, pmag & 15);
			return;
		}
	}

	if (decode_ttx) {
		printf ("TTX L%3u %x/%2u ", line, magazine, packet);
		dump_bytes (buffer, 42);
		return;
	}
}

#if 0

static void
vps				(const uint8_t		buffer[13],
				 unsigned int		line)
{
	if (decode_vps) {
		unsigned int cni;
		vbi_program_id pi;

		if (dump_bin) {
			/* printf ("VPS L%3u ", line); */
			fwrite (buffer, 1, 13, stdout);
			return;
		}

		if (!vbi_decode_vps_cni (&cni, buffer)) {
			printf ("Error in vps cni\n");
			return;
		}
		
		if (!vbi_decode_vps_pdc (&pi, buffer)) {
			printf ("Error in vps pdc data\n");
			return;
		}
		
		printf ("VPS L%3u ", line);

		_vbi_program_id_dump (&pi, stdout);

		putchar ('\n');

		if (0 != pi.cni)
			dump_cni (pi.cni_type, pi.cni);
	}

	if (decode_vps_r) {
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

		label[i][l[i]] = vbi_printable (c);

		l[i] = (l[i] + 1) % 16;
		
		printf ("VPS L%3u 3-10: %02x %02x (%02x='%c') %02x %02x "
			"%02x %02x %02x %02x (\"%s\")\n",
			line,
			buffer[0], buffer[1],
			c, vbi_printable (c),
			buffer[2], buffer[3],
			buffer[4], buffer[5], buffer[6], buffer[7],
			pr_label[i]);
	}
}

static void
wss_625				(const uint8_t		buffer[2])
{
	if (decode_wss) {  
		vbi_aspect_ratio ar;

		if (!vbi_decode_wss_625 (&ar, buffer)) {
			printf ("Error in WSS\n");
			return;
		}

		_vbi_aspect_ratio_dump (&ar, stdout);

		putchar ('\n');
	}
}

#endif

static void
decode				(const vbi_sliced *	s,
				 unsigned int		lines,
				 double			sample_time,
				 int64_t		stream_time)
{
	static double last_sample_time = 0.0;
	static int64_t last_stream_time = 0;

	if (dump_time) {
		printf ("ST %f (%+f) PTS %" PRId64 " (%+" PRId64 ")\n",
			sample_time, sample_time - last_sample_time,
			stream_time, stream_time - last_stream_time);

		last_sample_time = sample_time;
		last_stream_time = stream_time;
	}

	while (lines > 0) {
		switch (s->id) {
		case VBI_SLICED_TELETEXT_B_L10_625:
		case VBI_SLICED_TELETEXT_B_L25_625:
		case VBI_SLICED_TELETEXT_B_625:
			teletext (s->data, s->line);
			break;

		case VBI_SLICED_VPS:
		case VBI_SLICED_VPS_F2:
#if 0
			vps (s->data, s->line);
#endif
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
#if 0
			wss_625 (s->data);
#endif
			break;

		case VBI_SLICED_WSS_CPR1204:
			break;
		}

		++s;
		--lines;
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
			unsigned int lines;
			int64_t pts;

			lines = vbi_dvb_demux_cor (dvb, sliced, 64,
						   &pts, &bp, &left);
			if (lines > 0)
				decode (sliced, lines, 0, pts);
		}
	}

	fprintf (stderr, "\rEnd of stream\n");
}

static void
old_mainloop			(void)
{
	for (;;) {
		vbi_sliced sliced[40];
		double timestamp;
		int n_lines;

		n_lines = read_sliced (sliced, &timestamp, /* max_lines */ 40);
		if (n_lines < 0)
			break;

		decode (sliced, n_lines, timestamp, 0);
	}

	fprintf (stderr, "\rEnd of stream\n");
}

static const char
short_options [] = "12abc:d:ehinp:rs:tvwxPTV";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "8301",	no_argument,		NULL,		'1' },
	{ "8302",	no_argument,		NULL,		'2' },
	{ "all",	no_argument,		NULL,		'a' },
	{ "bin",	no_argument,		NULL,		'b' },
	{ "idl-ch",	required_argument,	NULL,		'c' },
	{ "idl-addr",	required_argument,	NULL,		'd' },
	{ "xds",	no_argument,		NULL,		'e' },
	{ "help",	no_argument,		NULL,		'h' },
	{ "idl",	no_argument,		NULL,		'i' },
	{ "network",	no_argument,		NULL,		'n' },
	{ "pfc-pgno",	required_argument,	NULL,		'p' },
	{ "vps-r",	no_argument,		NULL,		'r' },
	{ "pfc-stream",	required_argument,	NULL,		's' },
	{ "ttx",	no_argument,		NULL,		't' },
	{ "vps",	no_argument,		NULL,		'v' },
	{ "wss",	no_argument,		NULL,		'w' },
	{ "hex",	no_argument,		NULL,		'x' },
	{ "pes",	no_argument,		NULL,		'P' },
	{ "time",	no_argument,		NULL,		'T' },
	{ "version",	no_argument,		NULL,		'V' },
	{ 0, 0, 0, 0 }
};
#else
#  define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static void
usage				(FILE *			fp,
				 char **		argv)
{
	fprintf (fp,
 "Libzvbi test/decode version " VERSION "\n"
 "Copyright (C) 2004-2005 Michael H. Schimek\n"
 "This program is licensed under GPL 2. NO WARRANTIES.\n\n"
 "Usage: %s [options] < sliced vbi data\n\n"
 "Input options:\n"
 "-P | --pes     Source is a DVB PES (autodetected when it starts with a PES\n"
 "               packet header), otherwise test/capture --sliced output\n"
 "\nDecoding options:\n"
#if 0
 "-1 | --8301    Teletext packet 8/30 format 1 (local time)\n"
 "-2 | --8302    Teletext packet 8/30 format 2 (PDC)\n"
#endif
 "-c | --idl-ch N\n"
 "-d | --idl-addr NNN\n"
 "               Decode Teletext IDL format A data from channel N (e. g. 8),\n"
 "               service packet address NNN (optional, default is 0)\n"
 "-e | --xds     Decode eXtended Data Service data (NTSC line 284)\n"
 "-i | --idl     Any Teletext IDL packets (M/30, M/31)\n"
 "-t | --ttx     Decode any Teletext packet\n"
#if 0
 "-v | --vps     VPS (PDC data)\n"
 "-w | --wss     WSS\n"
#endif
 "-a | --all     Everything above, e. g.\n"
 "               -i        decode IDL packets\n"
 "               -a        decode everything\n"
 "               -a -i     everything except IDL\n"
#if 0
 "-r | --vps-r   VPS data unrelated to PDC\n"
#endif
 "-p | --pfc-pgno NNN\n"
 "-s | --pfc-stream NN\n"
 "               Decode Teletext Page Function Clear data from page NNN\n"
 "               (e. g. 1DF), stream NN (optional, default is 0)\n"
 "\nModifying options:\n"
 "-x | --hex     With -t dump packets in hex and ASCII,\n"
 "               otherwise only ASCII\n"
#if 0
 "-n | --network With -1, -2, -v decode CNI and print available information\n"
 "               about the network\n"
#endif
 "-b | --bin     With -t, -p, -v dump data in binary format, otherwise ASCII\n"
 "-T | --time    Dump frame timestamps\n"
 "\nMiscellaneous options:\n"
 "-h | --help    Print this message\n",
		 argv[0]);
}

int
main				(int			argc,
				 char **		argv)
{
	int index;
	int c;

	while (-1 != (c = getopt_long (argc, argv, short_options,
				       long_options, &index))) {
		switch (c) {
		case 0:
			break;

		case '1':
			decode_8301 ^= TRUE;
			break;

		case '2':
			decode_8302 ^= TRUE;
			break;

		case 'b':
			dump_bin ^= TRUE;
			break;

		case 'a':
			decode_8301 ^= TRUE;
			decode_8302 ^= TRUE;
			decode_xds ^= TRUE;
			decode_idl ^= TRUE;
			decode_ttx ^= TRUE;
			decode_vps ^= TRUE;
			decode_wss ^= TRUE;

			if (pfc_pgno)
				pfc_pgno = 0;
			else
				pfc_pgno = 0x1DF;

			break;

		case 'c':
			idl_channel = strtol (optarg, NULL, 10);
			break;

		case 'd':
			idl_address = strtol (optarg, NULL, 10);
			break;

		case 'e':
			decode_xds ^= TRUE;
			break;

		case 'h':
			usage (stdout, argv);
			exit (EXIT_SUCCESS);

		case 'i':
			decode_idl ^= TRUE;
			break;

		case 'n':
			dump_network ^= TRUE;
			break;

		case 'p':
			pfc_pgno = strtol (optarg, NULL, 16);
			break;

		case 'r':
			decode_vps_r ^= TRUE;
			break;

		case 's':
			pfc_stream = strtol (optarg, NULL, 10);
			break;

		case 't':
			decode_ttx ^= TRUE;
			break;

		case 'v':
			decode_vps ^= TRUE;
			break;

		case 'w':
			decode_wss ^= TRUE;
			break;

		case 'x':
			dump_hex ^= TRUE;
			break;

		case 'P':
			source_pes ^= TRUE;
			break;

		case 'T':
			dump_time ^= TRUE;
			break;

		case 'V':
			printf (VERSION "\n");
			exit (EXIT_SUCCESS);

		default:
			usage (stderr, argv);
			exit (EXIT_FAILURE);
		}
	}

	if (isatty (STDIN_FILENO)) {
		fprintf (stderr,
			 "No vbi data on stdin. Try %s -h\n",
			 argv[0]);
		exit (EXIT_FAILURE);
	}

	if (0 != pfc_pgno) {
		pfc = vbi_pfc_demux_new (pfc_pgno,
					 pfc_stream,
					 page_function_clear_cb,
					 /* user_data */ NULL);
		assert (NULL != pfc);
	}

	if (0 != idl_channel) {
		idl = vbi_idl_a_demux_new (idl_channel,
					   idl_address,
					   idl_format_a_cb,
					   /* user_data */ NULL);
		assert (NULL != idl);
	}

	if (decode_xds) {
		xds = vbi_xds_demux_new (xds_cb,
					 /* used_data */ NULL);
		assert (NULL != xds);
	}

	c = getchar ();
	ungetc (c, stdin);

	if (0 == c || source_pes) {
		dvb = vbi_dvb_pes_demux_new (/* callback */ NULL, NULL);
		assert (NULL != dvb);

		pes_mainloop ();
	} else {
		open_sliced_read (stdin);

		old_mainloop ();
	}

	vbi_dvb_demux_delete (dvb);
	vbi_pfc_demux_delete (pfc);
	vbi_idl_demux_delete (idl);

	exit (EXIT_SUCCESS);
}
