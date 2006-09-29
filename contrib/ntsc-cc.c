/* cc.c -- closed caption decoder 
 * Mike Baker (mbm@linux.com)
 * (based on code by timecop@japan.co.jp)
 * Buffer overflow bugfix by Mark K. Kim (dev@cbreak.org), 2003.05.22
 * -p fix and libzvbi port (C) 2005 Michael H. Schimek <mschimek@users.sf.net>
 * More fixes and improvements (C) 2006 Michael H. Schimek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif

#define HAVE_ZVBI 1
#include <assert.h>
#include "src/libzvbi.h"

#ifndef X_DISPLAY_MISSING
# include <X11/X.h>
# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include <X11/Xproto.h>
Display *dpy;
Window Win,Root;
char dpyname[256] = "";
GC WinGC;
GC WinGC0;
GC WinGC1;
int x;
#endif

#undef PROGRAM
#define PROGRAM "CCDecoder"
#undef VERSION
#define VERSION "0.11"

#define N_ELEMENTS(array) (sizeof (array) / sizeof (*(array)))

static char *			my_name;
static int			channel = -1;

//XDSdecode
static struct {
	char				packet[34];
	uint8_t				length;
	int				print : 1;
}				info[8][25];
char	newinfo[8][25][34];
char	*infoptr=newinfo[0][0];
int	mode,type;
static vbi_bool			in_xds;
char	infochecksum;
static const char *		xds_info_prefix = "\33[33m% ";
static const char *		xds_info_suffix = "\33[0m\n";
static FILE *			xds_fp;

//ccdecode
const char    *ratings[] = {"(NOT RATED)","TV-Y","TV-Y7","TV-G","TV-PG","TV-14","TV-MA","(NOT RATED)"};
int     rowdata[] = {11,-1,1,2,3,4,12,13,14,15,5,6,7,8,9,10};
const char	*specialchar[] = {"®","°","½","¿","(TM)","¢","£","o/~ ","à"," ","è","â","ê","î","ô","û"};
const char	*modes[]={"current","future","channel","miscellaneous","public service","reserved","invalid","invalid","invalid","invalid"};
int	lastcode;
int	ccmode=1;		//cc1 or cc2
char	ccbuf[3][256];		//cc is 32 columns per row, this allows for extra characters
int	keywords=0;
char	*keyword[32];
static FILE *			cc_fp;

//args (this should probably be put into a structure later)
char useraw=0;
char semirawdata=0;
char usexds=0;
char usecc=0;
char plain=0;
char usesen=0;
char debugwin=0;
char test=0;
char usewebtv=1;

char rawline=-1;

int sen;
int inval;

static int parityok(int n)	/* check parity for 2 bytes packed in n */
{
    int mask=0;
    int j, k;
    for (k = 1, j = 0; j < 7; j++) {
	  if (n & (1<<j)) 
	    k++;
	}
    if ((k & 1) == ((n>>7)&1))
	  mask|=0x00FF;
    for (k = 1, j = 8; j < 15; j++) {
	  if (n & (1<<j)) 
	    k++;
	}
    if ((k & 1) == ((n>>15)&1))
	    mask|=0xFF00;
   return mask;
}

static int decodebit(unsigned char *data, int threshold)
{
    int i, sum = 0;
    for (i = 0; i < 23; i++)
	  sum += data[i];
    return (sum > threshold*23);
}

static int decode(unsigned char *vbiline)
{
    int max[7], min[7], val[7], i, clk, tmp, sample, packedbits = 0;
    
    for (clk=0; clk<7; clk++)
	  max[clk] = min[clk] = val[clk] = -1;
    clk = tmp = 0;
    i=30;

    while (i < 600 && clk < 7) {	/* find and lock all 7 clocks */
	sample = vbiline[i];
	if (max[clk] < 0) { /* find maximum value before drop */
	    if (sample > 85 && sample > val[clk])
		(val[clk] = sample, tmp = i);	/* mark new maximum found */
	    else if (val[clk] - sample > 30)	/* far enough */
		(max[clk] = tmp, i = tmp + 10);
	} else { /* find minimum value after drop */
	    if (sample < 85 && sample < val[clk])
		(val[clk] = sample, tmp = i);	/* mark new minimum found */
	    else if (sample - val[clk] > 30)	/* searched far enough */
		(min[clk++] = tmp, i = tmp + 10);
	}
	i++;
    } 

   i=min[6]=min[5]-max[5]+max[6]; 
   
    if (clk != 7 || vbiline[max[3]] - vbiline[min[5]] < 45)		/* failure to locate clock lead-in */
	return -1;

#ifndef X_DISPLAY_MISSING
    if (debugwin) {
      for (clk=0;clk<7;clk++)
	{
	  XDrawLine(dpy,Win,WinGC,min[clk]/2,0,min[clk]/2,128);
	  XDrawLine(dpy,Win,WinGC1,max[clk]/2,0,max[clk]/2,128);
	}
      XFlush(dpy);
    }
#endif
 
    
    /* calculate threshold */
    for (i=0,sample=0;i<7;i++)
	    sample=(sample + vbiline[min[i]] + vbiline[max[i]])/3;

    for(i=min[6];vbiline[i]<sample;i++);

#ifndef X_DISPLAY_MISSING
    if (debugwin) {
      for (clk=i;clk<i+57*18;clk+=57)
	XDrawLine(dpy,Win,WinGC,clk/2,0,clk/2,128);
      XFlush(dpy);
    }
#endif
 
    
    tmp = i+57;
    for (i = 0; i < 16; i++)
	if(decodebit(&vbiline[tmp + i * 57], sample))
	    packedbits |= 1<<i;
    return packedbits&parityok(packedbits);
} /* decode */

static void
print_xds_info			(unsigned int		mode,
				 unsigned int		type)
{
	const char *infoptr;

	if (!info[mode][type].print)
		return;

	infoptr = info[mode][type].packet;

	switch ((mode << 8) + type) {
	case 0x0101:
		fprintf (xds_fp,
			 "%sTIMECODE: %d/%02d %d:%02d%s",
			 xds_info_prefix,
			 infoptr[3]&0x0f,infoptr[2]&0x1f,
			 infoptr[1]&0x1f,infoptr[0]&0x3f,
			 xds_info_suffix);
	case 0x0102:
		if ((infoptr[1]&0x3f)>5)
			break;
		fprintf (xds_fp,
			 "%s  LENGTH: %d:%02d:%02d of %d:%02d:00%s",
			 xds_info_prefix,
			 infoptr[3]&0x3f,infoptr[2]&0x3f,
			 infoptr[4]&0x3f,infoptr[1]&0x3f,
			 infoptr[0]&0x3f,
			 xds_info_suffix);
		break;
	case 0x0103:
		fprintf (xds_fp,
			 "%s   TITLE: %s%s",
			 xds_info_prefix,
			 infoptr,
			 xds_info_suffix);
		break;
	case 0x0105:
		fprintf (xds_fp,
			 "%s  RATING: %s (%d)",
			 xds_info_prefix,
			 ratings[infoptr[0]&0x07],infoptr[0]);
		if ((infoptr[0]&0x07)>0) {
			if (infoptr[0]&0x20) fputs (" VIOLENCE", xds_fp);
			if (infoptr[0]&0x10) fputs (" SEXUAL", xds_fp);
			if (infoptr[0]&0x08) fputs (" LANGUAGE", xds_fp);
		}
		fputs (xds_info_suffix, xds_fp);
		break;
	case 0x0501:
		fprintf (xds_fp,
			 "%s NETWORK: %s%s",
			 xds_info_prefix,
			 infoptr,
			 xds_info_suffix);
		break;
	case 0x0502:
		fprintf (xds_fp,
			 "%s    CALL: %s%s",
			 xds_info_prefix,
			 infoptr,
			 xds_info_suffix);
		break;
	case 0x0701:
		fprintf (xds_fp,
			 "%sCUR.TIME: %d:%02d %d/%02d/%04d UTC%s",
			 xds_info_prefix,
			 infoptr[1]&0x1F,infoptr[0]&0x3f,
			 infoptr[3]&0x0f,infoptr[2]&0x1f,
			 (infoptr[5]&0x3f)+1990,
			 xds_info_suffix);
		break;
	case 0x0704: //timezone
		fprintf (xds_fp,
			 "%sTIMEZONE: UTC-%d%s",
			 xds_info_prefix,
			 infoptr[0]&0x1f,
			 xds_info_suffix);
		break;
	case 0x0104: //program genere
		break;
	case 0x0110:
	case 0x0111:
	case 0x0112:
	case 0x0113:
	case 0x0114:
	case 0x0115:
	case 0x0116:
	case 0x0117:
		fprintf (xds_fp,
			 "%s    DESC: %s%s",
			 xds_info_prefix,
			 infoptr,
			 xds_info_suffix);
		break;
	}

	fflush (xds_fp);
}

static int XDSdecode(int data)
{
	int b1, b2, length;

	if (data == -1)
		return -1;
	
	b1 = data & 0x7F;
	b2 = (data>>8) & 0x7F;

	if (0 == b1) {
		/* Filler, discard. */
		return -1;
	}
	else if (b1 < 15) // start packet 
	{
		mode = b1;
		type = b2;
		infochecksum = b1 + b2 + 15;
		if (mode > 8 || type > 20)
		{
//			printf("%% Unsupported mode %s(%d) [%d]\n",modes[(mode-1)>>1],mode,type);
			mode=0; type=0;
		}
		infoptr = newinfo[mode][type];
		in_xds = TRUE;
	}
	else if (b1 == 15) // eof (next byte is checksum)
	{
#if 0 //debug
		if (mode == 0)
		{
			length=infoptr - newinfo[0][0];
			infoptr[1]=0;
			printf("LEN: %d\n",length);
			for (y=0;y<length;y++)
				printf(" %03d",newinfo[0][0][y]);
			printf(" --- %s\n",newinfo[0][0]);
		}
#endif
		if (mode == 0) return 0;
		if (b2 != 128-((infochecksum%128)&0x7F)) return 0;

		length = infoptr - newinfo[mode][type];

		//don't bug the user with repeated data
		//only parse it if it's different
		if (info[mode][type].length != length
		    || 0 != memcmp (info[mode][type].packet,
				    newinfo[mode][type],
				    length))
		{
			memcpy (info[mode][type].packet,
				newinfo[mode][type], 32);
			info[mode][type].packet[length] = 0;
			info[mode][type].length = length;
			print_xds_info (mode, type);
		}
		mode = 0; type = 0;
		in_xds = 0;
	} else if (b1 <= 31) {
		/* Caption control code. */
		in_xds = 0;
	} else if (in_xds) {
		if (infoptr >= &newinfo[mode][type][32]) {
			/* Bad packet. */
			mode = 0;
			type = 0;
			in_xds = 0;
		} else {
			infoptr[0] = b1; infoptr++;
			infoptr[0] = b2; infoptr++;
			infochecksum += b1 + b2;
		}
	}
	return 0;
}

static int webtv_check(char * buf,int len)
{
	unsigned long   sum;
	unsigned long   nwords;
	unsigned short  csum=0;
	char temp[9];
	int nbytes=0;
	
	while (buf[0]!='<' && len > 6)  //search for the start
	{
		buf++; len--;
	}
	
	if (len == 6) //failure to find start
		return 0;
				
	
	while (nbytes+6 <= len)
	{
		//look for end of object checksum, it's enclosed in []'s and there shouldn't be any [' after
		if (buf[nbytes] == '[' && buf[nbytes+5] == ']' && buf[nbytes+6] != '[')
			break;
		else
			nbytes++;
	}
	if (nbytes+6>len) //failure to find end
		return 0;
	
	nwords = nbytes >> 1; sum = 0;

	//add up all two byte words
	while (nwords-- > 0) {
		sum += *buf++ << 8;
		sum += *buf++;
	}
	if (nbytes & 1) {
		sum += *buf << 8;
	}
	csum = (unsigned short)(sum >> 16);
	while(csum !=0) {
		sum = csum + (sum & 0xffff);
		csum = (unsigned short)(sum >> 16);
	}
	sprintf(temp,"%04X\n",(int)~sum&0xffff);
	buf++;
	if(!strncmp(buf,temp,4))
	{
		buf[5]=0;
		if (!plain)
			fprintf(cc_fp, "\33[35mWEBTV: %s\33[0m\n",buf-nbytes-1);
		else
			fprintf(cc_fp, "WEBTV: %s\n",buf-nbytes-1);
		fflush (cc_fp);
	}
	return 0;
}

static int CCdecode(int data)
{
	int b1, b2, row, len, x,y;
	if (data == -1) //invalid data. flush buffers to be safe.
	{
		memset(ccbuf[1],0,255);
		memset(ccbuf[2],0,255);
		return -1;
	}
	b1 = data & 0x7f;
	b2 = (data>>8) & 0x7f;
	if(ccmode >= 3) ccmode = 0;
	len = strlen(ccbuf[ccmode]);

	if (b1&0x60 && data != lastcode) // text
	{
		if(len < 255) ccbuf[ccmode][len++]=b1;
		if ((b2&0x60) && (len < 255)) ccbuf[ccmode][len++]=b2;
		if ((b1 == ']' || b2 == ']') && usewebtv)
			webtv_check(ccbuf[ccmode],len);
	}
	else if ((b1&0x10) && (b2>0x1F) && (data != lastcode)) //codes are always transmitted twice (apparently not, ignore the second occurance)
	{
		if (channel < 0) {
			/* WTF? */
			ccmode=((b1>>3)&1)+1;
		}
		len = strlen(ccbuf[ccmode]);

		if (b2 & 0x40)	//preamble address code (row & indent)
		{
			row=rowdata[((b1<<1)&14)|((b2>>5)&1)];
			if (len!=0)
				ccbuf[ccmode][len++]='\n';

			if (b2&0x10) //row contains indent flag
				for (x=0;x<(b2&0x0F)<<1;x++)
					ccbuf[ccmode][len++]=' ';
		}
		else
		{
			switch (b1 & 0x07)
			{
				case 0x00:	//attribute
					fprintf (cc_fp, "<ATTRIBUTE %d %d>\n",b1,b2);
					fflush (cc_fp);
					break;
				case 0x01:	//midrow or char
					switch (b2&0x70)
					{
						case 0x20: //midrow attribute change
							switch (b2&0x0e)
							{
								case 0x00: //italics off
									if (!plain)
									  strcat(ccbuf[ccmode],"\33[0m ");
									break;
								case 0x0e: //italics on
									if (!plain)
									  strcat(ccbuf[ccmode],"\33[36m ");
									break;
							}
							if (b2&0x01) { //underline
									if (!plain)
									  strcat(ccbuf[ccmode],"\33[4m");
							} else {
							  if (!plain)
								strcat(ccbuf[ccmode],"\33[24m");
							}
							break;
						case 0x30: //special character..
							strcat(ccbuf[ccmode],specialchar[b2&0x0f]);
							break;
					}
					break;
				case 0x04:	//misc
				case 0x05:	//misc + F
//					fprintf (cc_fp, "ccmode %d cmd %02x\n",ccmode,b2);
					switch (b2)
					{
						size_t n;

						case 0x21: //backspace
							ccbuf[ccmode][len--]=0;
							break;
							
						/* these codes are insignifigant if we're ignoring positioning */
						case 0x25: //2 row caption
						case 0x26: //3 row caption
						case 0x27: //4 row caption
						case 0x29: //resume direct caption
						case 0x2B: //resume text display
						case 0x2C: //erase displayed memory
							break;
							
						case 0x2D: //carriage return
							if (ccmode==2)
								break;
						case 0x2F: //end caption + swap memory
						case 0x20: //resume caption (new caption)
							if (!strlen(ccbuf[ccmode]))
									break;
							for (n=0;n<strlen(ccbuf[ccmode]);n++)
								for (y=0;y<keywords;y++)
									if (!strncasecmp(keyword[y], ccbuf[ccmode]+n, strlen(keyword[y])))
										fprintf (cc_fp, "\a");
							if (!plain)
								fprintf (cc_fp, "%s\33[m\n",ccbuf[ccmode]);
							else
								fprintf (cc_fp, "%s\n",ccbuf[ccmode]);
							fflush (cc_fp);
							/* FALL */
						case 0x2A: //text restart
						case 0x2E: //erase non-displayed memory
							memset(ccbuf[ccmode],0,255);
							break;
					}
					break;
				case 0x07:	//misc (TAB)
					for(x=0;x<(b2&0x03);x++)
						ccbuf[ccmode][len++]=' ';
					break;
			}
		}
	}
	lastcode=data;
	return 0;
}

static int print_raw(int data)
{
	int b1, b2;
	if (data == -1)
		return -1;

	// this is just null data with two parity bits
	// 100000010000000 = 0x8080
	if (data == 0x8080)
	  return -1;

	b1 = data & 0x7f;
	b2 = (data>>8) & 0x7f;

	if (!semirawdata) {
	  fprintf(stderr,"%c%c",b1,b2);
	  fflush(stderr);
	  return 0;
	}

	// semi-raw data output begins here... 

	// a control code.
	if ( ( b1 >= 0x10 ) && ( b1 <= 0x1F ) ) {
	  if ( ( b2 >= 0x20 ) && ( b2 <= 0x7F ) ) 
	    fprintf(stderr,"[%02X-%02X]",b1,b2); 
	    fflush(stderr);
	  return 0;
	}

	// next two rules:
	// supposed to be one printable char
	// and the other char to be discarded
	if ( ( b1 >= 0x0 ) && ( b1 <= 0xF ) ) {
	  fprintf(stderr,"(%02x)%c",b1,b2);
	  //fprintf(stderr,"%c",b2);
	  //fprintf(stderr,"%c%c",0,b2);
	  fflush(stderr);
	  return 0;
	}
	if ( ( b2 >= 0x0 ) && ( b2 <= 0xF ) ) {
	  fprintf(stderr,"%c{%02x}",b1,b2);
	  //fprintf(stderr,"%c",b1);
	  //fprintf(stderr,"%c%c",b1,1);
	  fflush(stderr);
	  return 0;
	}

	// just classic two chars to print.
	fprintf(stderr,"%c%c",b1,b2);
	fflush(stderr);

	return 0;
}

static int sentence(int data)
{
	int b1, b2;
	if (data == -1)
		return -1;
	b1 = data & 0x7f;
	b2 = (data>>8) & 0x7f;
	inval++;
	if (data==lastcode)
	{
		if (sen==1)
		{
			fprintf (cc_fp, " ");
			fflush (cc_fp);
			sen=0;
		}
		if (inval>10 && sen)
		{
			fprintf (cc_fp, "\n");
			fflush (cc_fp);
			sen=0;
		}
		return 0;
	}
	lastcode=data;

	if (b1&96)
	{
		inval=0;
		if (sen==2 && b1!='.' && b2!='.' && b1!='!' && b2!='!' && b1!='?' && b2!='?' && b1!=')' && b2!=')')
		{
			fprintf (cc_fp, "\n");
			sen=1;
		}
		else if (b1=='.' || b2=='.' || b1=='!' || b2=='!' || b1=='?' || b2=='?' || b1==')' || b2==')')
			sen=2;
		else
			sen=1;
		fprintf (cc_fp, "%c%c",tolower(b1),tolower(b2));
		fflush (cc_fp);
	}
	return 0;
}

#ifndef X_DISPLAY_MISSING
static unsigned long getColor(const char *colorName, float dim)
{
	XColor Color;
	XWindowAttributes Attributes;

	XGetWindowAttributes(dpy, Root, &Attributes);
	Color.pixel = 0;

	XParseColor (dpy, Attributes.colormap, colorName, &Color);
	Color.red=(unsigned short)(Color.red/dim);
	Color.blue=(unsigned short)(Color.blue/dim);
	Color.green=(unsigned short)(Color.green/dim);
	Color.flags=DoRed | DoGreen | DoBlue;
	XAllocColor (dpy, Attributes.colormap, &Color);

	return Color.pixel;
}
#endif

static int
caption_filter			(unsigned int		c1,
				 unsigned int		c2)
{
	static int cur_ch = -1;
	static unsigned int in_xds = 0;
	unsigned int p;
	
	p = c1 + c2 * 256;
	p ^= p >> 4;
	p ^= p >> 2;
	p ^= p >> 1;

	c1 &= 0x7F;
	c2 &= 0x7F;

	if (0x0101 != (p & 0x0101)) {
		/* Parity error. */
		cur_ch = -1;
	} else if (0 == c1) {
		/* Filler. */
	} else if (c1 < 0x10) {
		in_xds = TRUE;
	} else if (c1 < 0x20) {
		in_xds = FALSE;

		if (c2 < 0x20) {
			/* Invalid. */
		} else {
			cur_ch &= ~1;
			cur_ch |= (c1 >> 3) & 1;

			if (c2 < 0x30 && 0x14 == (c1 & 0xF6)) {
				cur_ch &= ~2;
				cur_ch |= (c1 << 1) & 2;

				switch (c2) {
				case 0x20: /* RCL */
				case 0x25: /* RU2 */
				case 0x26: /* RU3 */
				case 0x27: /* RU4 */
				case 0x29: /* RDC */
					cur_ch &= 3;
					break;

				case 0x2A: /* TR */
				case 0x2B: /* RTD */
					cur_ch &= 3; /* now >= 0 */
					cur_ch |= 4;
					break;

				default:
					break;
				}
			}
		}
	}

	if (0) {
		fprintf (stderr, "in_xds=%d cur_ch=%d channel=%d\n",
			 in_xds, cur_ch, channel);
	}

	return (!in_xds && cur_ch == channel);
}

static ssize_t
read_test_stream		(vbi_sliced *		sliced,
				 int *			n_lines,
				 unsigned int		max_lines)
{
	char buf[256];
	double dt;
	unsigned int n_items;
	vbi_sliced *s;

	if (ferror (stdin) || !fgets (buf, 255, stdin)) {
		fprintf (stderr, "End of test stream\n");
		exit (EXIT_SUCCESS);
	}

	dt = strtod (buf, NULL);
	n_items = fgetc (stdin);

	assert (n_items < max_lines);

	s = sliced;

	while (n_items-- > 0) {
		int index;

		index = fgetc (stdin);

		s->line = (fgetc (stdin)
			   + 256 * fgetc (stdin)) & 0xFFF;

		if (index < 0) {
			fprintf (stderr, "Bad index in test stream\n");
			exit (EXIT_FAILURE);
		}

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
				 "\nUnknown data type %d "
				 "in test stream\n", index);
			exit (EXIT_FAILURE);
		}

		++s;
	}

	*n_lines = s - sliced;

	return 1; /* success */
}

static void
xds_filter_option		(const char *		optarg)
{
	const char *s;

	/* Attention: may be called multiple times. */

	if (NULL == optarg
	    || 0 == strcasecmp (optarg, "all")) {
		unsigned int i;

		for (i = 0; i < (N_ELEMENTS (info)
				 * N_ELEMENTS (info[0])); ++i) {
			info[0][i].print = TRUE;
		}

		return;
	}

	s = optarg;

	while (0 != *s) {
		char buf[16];
		unsigned int len;

		for (;;) {
			if (0 == *s)
				return;
			if (isalnum (*s))
				break;
			++s;
		}

		for (len = 0; len < N_ELEMENTS (buf) - 1; ++len) {
			if (!isalnum (*s))
				break;
			buf[len] = *s++;
		}

		buf[len] = 0;

		if (0 == strcasecmp (buf, "timecode")) {
			info[1][1].print = TRUE;
		} else if (0 == strcasecmp (buf, "length")) {
			info[1][2].print = TRUE;
		} else if (0 == strcasecmp (buf, "title")) {
			info[1][3].print = TRUE;
		} else if (0 == strcasecmp (buf, "rating")) {
			info[1][5].print = TRUE;
		} else if (0 == strcasecmp (buf, "network")) {
			info[5][1].print = TRUE;
		} else if (0 == strcasecmp (buf, "call")) {
			info[5][2].print = TRUE;
		} else if (0 == strcasecmp (buf, "time")) {
			info[7][1].print = TRUE;
		} else if (0 == strcasecmp (buf, "timezone")) {
			info[7][4].print = TRUE;
		} else if (0 == strcasecmp (buf, "desc")) {
			info[1][0x10].print = TRUE;
			info[1][0x11].print = TRUE;
			info[1][0x12].print = TRUE;
			info[1][0x13].print = TRUE;
			info[1][0x14].print = TRUE;
			info[1][0x15].print = TRUE;
			info[1][0x16].print = TRUE;
			info[1][0x17].print = TRUE;
		} else {
			fprintf (stderr, "Unknown XDS info '%s'\n", buf);
		}
	}
}

static void
usage				(FILE *			fp)
{
	fprintf (fp, "\
" PROGRAM " " VERSION " -- Closed Caption and XDS decoder\n\
Copyright (C) 2003-2006 Mike Baker, Mark K. Kim, Michael H. Schimek\n\
<mschimek@users.sf.net>; Based on code by timecop@japan.co.jp.\n\
This program is licensed under GPL 2 or later. NO WARRANTIES.\n\n\
Usage: %s [options]\n\
Options:\n\
-? | -h | --help | --usage  Print this message and exit\n\
-b | --no-webtv             Do not print WebTV links\n\
-c | --cc                   Print Closed Caption (includes WebTV)\n\
-d | --device filename      VBI device [/dev/vbi]\n\
-f | --filter type[,type]*  Select XDS info: all, call, desc, length,\n\
                            network, rating, time, timecode, timezone,\n\
                            title. Multiple -f options accumulate. [all]\n\
-k | --keyword string       Break caption line at this word (broken?).\n\
                            Multiple -k options accumulate.\n\
-l | --channel number       Select caption channel 1 ... 4 [no filter]\n\
-p | --plain-ascii          Print plain ASCII, else insert VT.100 color,\n\
                            italic and underline control codes\n\
-r | --raw line-number      Dump raw VBI data\n\
-s | --sentences            Decode caption by sentences\n\
-v | --verbose              Increase verbosity\n\
-w | --window               Open debugging window (with -r option)\n\
-x | --xds                  Print XDS info\n\
-C | --cc-file filename     Append caption to this file [stdout]\n\
-R | --semi-raw             Dump semi-raw VBI data (with -r option)\n\
-X | --xds-file filename    Append XDS info to this file [stdout]\n\
",
		 my_name);
}

static const char
short_options [] = "?bcd:f:hkl:pr:stvwxC:RX:";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "help",	no_argument,		NULL,		'?' },
	{ "no-webtv",	no_argument,		NULL,		'b' },
	{ "cc",		no_argument,		NULL,		'c' },
	{ "device",	required_argument,	NULL,		'd' },
	{ "filter",	required_argument,	NULL,		'f' },
	{ "help",	no_argument,		NULL,		'h' },
	{ "keyword",	required_argument,	NULL,		'k' },
	{ "channel",	required_argument,	NULL,		'l' },
	{ "plain-ascii", no_argument,		NULL,		'p' },
	{ "raw",	required_argument,	NULL,		'r' },
	{ "sentences",	no_argument,		NULL,		's' },
	{ "test",	no_argument,		NULL,		't' },
	{ "verbose",	no_argument,		NULL,		'v' },
	{ "window",	no_argument,		NULL,		'w' },
	{ "xds",	no_argument,		NULL,		'x' },
	{ "usage",	no_argument,		NULL,		'u' },
	{ "cc-file",	required_argument,	NULL,		'C' },
	{ "semi-raw",	no_argument,		NULL,		'R' },
	{ "xds-file",	required_argument,	NULL,		'X' },
	{ NULL, 0, 0, 0 }
};
#else
#  define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static int			option_index;

static FILE *
open_output_file		(const char *		name)
{
	FILE *fp;

	if (NULL == name
	    || 0 == strcmp (name, "-")) {
		fp = stdout;
	} else {
		fp = fopen (name, "a");
		if (NULL == fp) {
			fprintf (stderr,
				 "Couldn't open '%s' for appending: %s.\n",
				 name, strerror (errno));
			exit (EXIT_FAILURE);
		}
	}

	return fp;
}

int main(int argc,char **argv)
{
   unsigned char buf[65536];
   int arg;
   int args=0;
   int vbifd;
   fd_set rfds;
   int x;
	const char *device_file_name;
	const char *cc_file_name;
	const char *xds_file_name;
	int verbose;
	int have_xds_filter_option;

#ifdef HAVE_ZVBI

   vbi_capture *cap;
   char *errstr;
   unsigned int services;
   int scanning;
   int strict;
   int ignore_read_error;
   vbi_raw_decoder *par;
   unsigned int src_w;
   unsigned int src_h;
   uint8_t *raw;
   vbi_sliced *sliced;
   struct timeval timeout;

#endif

	my_name = argv[0];

	device_file_name = "/dev/vbi";
	cc_file_name = "-";
	xds_file_name = "-";

	verbose = 0;

	have_xds_filter_option = FALSE;	

	for (;;) {
		int c;

		c = getopt_long (argc, argv, short_options,
				 long_options, &option_index);
		if (-1 == c)
			break;

		switch (c) {
		case '?':
		case 'h':
			usage (stdout);
			exit (EXIT_SUCCESS);

		case 'b':
			usewebtv=0; /* sic, compatibility */
			break;

		case 'c':
			usecc=1;
			break;

		case 'd':
			assert (NULL != optarg);
			device_file_name = optarg;
			break;

		case 'f':
			usexds = TRUE;
			xds_filter_option (optarg);
			have_xds_filter_option = TRUE;
			break;

		case 'l':
			assert (NULL != optarg);
			channel = strtol (optarg, NULL, 0) - 1;
			if (channel < 0 || channel > 7) {
				fprintf (stderr,
					 "Invalid channel number %d, "
					 "should be 1 ... 8.\n",
					 channel + 1);
				exit (EXIT_FAILURE);
			}
			break;

		case 'k':
			keyword[keywords++]=optarg;
			break;

		case 'p':
			plain=1;
			xds_info_prefix = "% ";
			xds_info_suffix = "\n";
			break;

		case 'r':
			assert (NULL != optarg);
			useraw=1;
			rawline=atoi(optarg);
			break;

		case 's':
			usesen=1;
			break;

		case 't':
			test=1;
			break;

		case 'v':
			++verbose;
			break;

		case 'w':
			debugwin=1;
			break;

		case 'x':
			usexds=1;
			break;

		case 'C':
			assert (NULL != optarg);
			cc_file_name = optarg;
			break;

		case 'R':
			semirawdata=1;
			break;

		case 'X':
			assert (NULL != optarg);
			xds_file_name = optarg;
			break;

		default:
			usage (stderr);
			exit (EXIT_FAILURE);
		}
	}

	if (!(usecc | usexds | useraw)) {
		fprintf (stderr, "Give one of the -c, -x or -r options "
			 "or -h for help.\n");
		exit (EXIT_FAILURE);
	}

	if (usexds && !have_xds_filter_option)
		xds_filter_option ("all");

#ifdef HAVE_ZVBI

   errstr = NULL;

   /* What we want. */
   services = VBI_SLICED_CAPTION_525;

   /* This is a hint in case the device can't tell
      the current video standard. */
   scanning = 525;

   /* Strict sampling parameter matching: 0, 1, 2 */
   strict = 1;

   ignore_read_error = 1;

   do {
      if (test) {
	 break;
      }

      /* Linux */

      /* DVB interface omitted, doesn't support NTSC/ATSC. */

      cap = vbi_capture_v4l2_new (device_file_name,
				  /* buffers */ 5,
				  &services,
				  strict,
				  &errstr,
				  !!verbose);
      if (cap) {
	 break;
      }

      fprintf (stderr, "Cannot capture vbi data with v4l2 interface:\n"
	       "%s\nWill try v4l.\n", errstr);

      free (errstr);

      cap = vbi_capture_v4l_new (device_file_name,
				 scanning,
				 &services,
				 strict,
				 &errstr,
				 !!verbose);
      if (cap)
	 break;

      fprintf (stderr, "Cannot capture vbi data with v4l interface:\n"
	       "%s\n", errstr);

      /* FreeBSD to do */

      free (errstr);

      exit(EXIT_FAILURE);

   } while (0);

   if (test) {
	   src_w = 1440;
	   src_h = 50;
   } else {
	   par = vbi_capture_parameters (cap);
	   assert (NULL != par);

	   src_w = par->bytes_per_line / 1;
	   src_h = par->count[0] + par->count[1];

	   if (useraw && (unsigned int) rawline >= src_h) {
		   fprintf (stderr, "-r must be in range 0 ... %u\n",
			    src_h - 1);
		   exit (EXIT_FAILURE);
	   }
   }

   raw = calloc (1, src_w * src_h);
   sliced = malloc (sizeof (vbi_sliced) * src_h);

   assert (NULL != raw);
   assert (NULL != sliced);

   /* How long to wait for a frame. */
   timeout.tv_sec = 2;
   timeout.tv_usec = 0;

#else

   if ((vbifd = open(device_file_name, O_RDONLY)) < 0) {
	perror(vbifile);
	exit(1);
   }

#endif

   if (usecc)
	   cc_fp = open_output_file (cc_file_name);

   if (usexds)
	   xds_fp = open_output_file (xds_file_name);

   for (x=0;x<keywords;x++)
	printf("Keyword(%d): %s\n",x,keyword[x]);

#ifndef X_DISPLAY_MISSING
   if (debugwin) {
     dpy=XOpenDisplay(dpyname);
     Root=DefaultRootWindow(dpy);
     Win = XCreateSimpleWindow(dpy, Root, 10, 10, 1024, 128,0,0,0);
     WinGC = XCreateGC(dpy, Win, 0, NULL);
     WinGC0 = XCreateGC(dpy, Win, 0, NULL);
     WinGC1 = XCreateGC(dpy, Win, 0, NULL);
     XSetForeground(dpy, WinGC, getColor("blue",1));
     XSetForeground(dpy, WinGC0, getColor("green",1));
     XSetForeground(dpy, WinGC1, getColor("red",1));

     if (useraw)
       XMapWindow(dpy, Win);
   }
#endif
	
#ifdef HAVE_ZVBI

   for (;;) {
      double timestamp;
      int n_lines;
      int r;
      int i;

      if (test) {
	      r = read_test_stream (sliced, &n_lines, src_h);
      } else {
	      r = vbi_capture_read (cap, raw, sliced,
				    &n_lines, &timestamp, &timeout);
      }
      switch (r) {
      case -1:
	 fprintf (stderr, "VBI read error: %d, %s%s\n",
		  errno, strerror (errno),
		  ignore_read_error ? " (ignored)" : "");
	 if (ignore_read_error)
	    continue;
	 else
	    exit (EXIT_FAILURE);

      case 0: 
	 fprintf (stderr, "VBI read timeout%s\n",
		  ignore_read_error ? " (ignored)" : "");
	 if (ignore_read_error)
	    continue;
	 else
	    exit (EXIT_FAILURE);

      case 1: /* ok */
	 break;

      default:
	 assert (0);
      }

		if (useraw)
		{
#ifndef X_DISPLAY_MISSING
		  if (debugwin) {
		    XClearArea(dpy,Win,0,0,1024,128,0);
		    XDrawLine(dpy,Win,WinGC1,0,128-85/2,1024,128-85/2);
		    for (x=0;x<src_w/2;x++)
		      if (raw[src_w * rawline+x*2]/2<128 && raw[src_w * rawline+x*2+2]/2 < 128)
			XDrawLine(dpy,Win,WinGC0,x,128-raw[src_w * rawline+x*2]/2,
				  x+1,128-raw[src_w * rawline+x*2+2]/2);
		  }
#endif
		  for (i = 0; i < n_lines; ++i) {
		     if (sliced[i].line == rawline) {
			print_raw(sliced[i].data[0]
				  + sliced[i].data[1] * 256);
		     }
		  }
#ifndef X_DISPLAY_MISSING
		  if (debugwin) {
		    XFlush(dpy);
		    usleep(100);
		  }
#endif
		}

		if (0 == n_lines && verbose > 2)
		  fprintf (stderr, "No data in this frame\n");

		for (i = 0; i < n_lines; ++i) {
		   unsigned int c1, c2;

		   c1 = sliced[i].data[0];
		   c2 = sliced[i].data[1];

		   if (verbose > 2)
		     fprintf (stderr, "Line %3d %02x %02x\n",
		    	      sliced[i].line, c1, c2);
		   /* No need to check sliced[i].id because we
		      requested only caption. */
		   if (21 == sliced[i].line) {
		      if (channel < 0 /* compatibility */
		          || caption_filter (c1, c2)) {
		         if (usecc)
			    CCdecode(c1 + c2 * 256);
		         if (usesen)
			    sentence(c1 + c2 * 256);
		      }
		   } else if (284 == sliced[i].line) {
		      if (channel >= 4 && caption_filter (c1, c2)) {
		         if (usecc)
			    CCdecode(c1 + c2 * 256);
		         if (usesen)
			    sentence(c1 + c2 * 256);
		      }
		      if (usexds)
			 XDSdecode(c1 + c2 * 256);
		   }
		}
#ifndef X_DISPLAY_MISSING
		if (debugwin) {
			XFlush(dpy);
			usleep(100);
		}
#endif
   }

#else /* !HAVE_ZVBI */

   //mainloop
   while(1){
	FD_ZERO(&rfds);
	FD_SET(vbifd, &rfds);
	select(FD_SETSIZE, &rfds, NULL, NULL, NULL);
	if (FD_ISSET(vbifd, &rfds)) {
	    if (read(vbifd, buf , 65536)!=65536)
		    printf("read error\n");
		if (useraw)
		{
#ifndef X_DISPLAY_MISSING
		  if (debugwin) {
		    XClearArea(dpy,Win,0,0,1024,128,0);
		    XDrawLine(dpy,Win,WinGC1,0,128-85/2,1024,128-85/2);
		    for (x=0;x<1024;x++)
		      if (buf[2048 * rawline+x*2]/2<128 && buf[2048 * rawline+x*2+2]/2 < 128)
			XDrawLine(dpy,Win,WinGC0,x,128-buf[2048 * rawline+x*2]/2,
				  x+1,128-buf[2048 * rawline+x*2+2]/2);
		  }
#endif
		  print_raw(decode(&buf[2048 * rawline]));
#ifndef X_DISPLAY_MISSING
		  if (debugwin) {
		    XFlush(dpy);
		    usleep(100);
		  }
#endif
		}
		if (usexds)
			XDSdecode(decode(&buf[2048 * 27]));
		if (usecc)
			CCdecode(decode(&buf[2048 * 11]));
		if (usesen)
			sentence(decode(&buf[2048 * 11]));
#ifndef X_DISPLAY_MISSING
		if (debugwin) {
			XFlush(dpy);
			usleep(100);
		}
#endif
	}
   }

#endif /* !HAVE_ZVBI */

   return 0;
}
