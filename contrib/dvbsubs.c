/*
   dvbsubs - a program for decoding DVB subtitles (ETS 300 743)

   File: dvbsubs.c

   Copyright (C) Dave Chapman 2002
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html
*/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>
#include <sys/poll.h>
#include <ctype.h>
#include <sys/time.h>


#include "dvbsubs.h"
#include "ctools.h"

page_t page;
region_t regions[MAX_REGIONS];
textsub_t textsub;

int y=0;
int x=0;

int fd_osd;
int num_windows=1;
int acquired=0;
struct timeval start_tv;

unsigned int curr_obj;
unsigned int curr_reg[64];

unsigned char white[4]={255,255,255,0xff};
unsigned char green[4]={0,255,0,0xdf} ;
unsigned char blue[4]={0,0,255,0xbf} ;
unsigned char yellow[4]={255,255,0,0xbf} ;
unsigned char black[4]={0,0,0,0xff} ; 
unsigned char red[4]={255,0,0,0xbf} ;
unsigned char magenta[4]={255,0,255,0xff};
unsigned char othercol[4]={0,255,255,0xff};

unsigned char transp[4]={0,255,0,0};

//unsigned char buf[100000];
uint8_t * buf;;
int i=0;
int nibble_flag=0;
int in_scanline=0;

int sub_idx=0;

FILE * outfile;

static void
output_textsub(FILE * outf) {
  int temp, h, m, s, ms;
  int i;

  temp = textsub.start_pts;
  h = temp / 3600000;
  temp %= 3600000;
  m = temp / 60000;
  temp %= 60000;
  s = temp / 1000;
  temp %= 1000;
  ms = temp;
  fprintf(outf, "%d\n%02d:%02d:%02d,%03d --> ", ++sub_idx, h, m, s, ms);

  temp = textsub.end_pts;
  h = temp / 3600000;
  temp %= 3600000;
  m = temp / 60000;
  temp %= 60000;
  s = temp / 1000;
  temp %= 1000;
  ms = temp;
  fprintf(outf, "%02d:%02d:%02d,%03d\n", h, m, s, ms);

  for (i=0; i<MAX_REGIONS; i++) {
    if (strlen(textsub.regions[i]) > 0) 
      fprintf(outf,"%s", textsub.regions[i]);
  }
  fprintf(outf, "\n");
  fflush(outf);
}

static void
process_gocr_output(const char *const fname, int region)
{
  FILE *file;
  int read;
  file = fopen(fname, "r");
  if (file == NULL) {
    perror("fopen failed");
    return;
  }
  read = fread(textsub.regions[region],sizeof(char), 64, file);
  if (read <= 0)
    perror("error reading");
  textsub.regions[region][read] = '\0';
  if (textsub.regions[region][0] == ',' && textsub.regions[region][1] == '\n') {
    char tmp[66];
    char * eol;
    strcpy(tmp,textsub.regions[region]+2);
    eol = rindex(tmp,'\n');
    strcpy(eol,",\n");
    strcpy(textsub.regions[region], tmp);
  }
  fclose(file);
}



static void
output_pgm(FILE *f, int r)
{
  int x, y;
  fprintf(f,
    "P5\n"
    "%d %d\n"
    "255\n",
    regions[r].width, regions[r].height);
  for (y = 0; y < regions[r].height; ++y) {
    for (x = 0; x < regions[r].width; ++x) {
      int res = 0;
      int pix = regions[r].img[y*regions[r].width+x];
      if (regions[r].alpha[pix])
        res = regions[r].palette[pix] * regions[r].alpha[pix];
      else
        res = 0;
      res = (65535 - res) >> 8;
      putc(res&0xff, f);
    }
  }
  putc('\n', f);
}

#define GOCR_PROGRAM "gocr"
static void
run_ocr(int region, unsigned long long pts) {
  FILE *f;
  char inbuf[128];
  char outbuf[128];
  char cmd[512];
  int cmdres;
  //const char *const tmpfname = tmpnam(NULL);
  sprintf(inbuf, "subtitle-%llu-%d.pgm", pts / 90, region);
  sprintf(outbuf, "tmp.txt");
  f = fopen(inbuf, "w");
  output_pgm(f, region);
  fclose(f);
  //sprintf(cmd, GOCR_PROGRAM" -v 1 -s 7 -d 0 -m 130 -m 256 -m 32 -i %s -o %s", inbuf, outbuf);
  sprintf(cmd, GOCR_PROGRAM" -s 8 -d 0 -m 130  -i %s -o %s", inbuf, outbuf);
  cmdres = system(cmd);
  if (cmdres < 0) {
    perror("system failed");
    exit(EXIT_FAILURE);
  }
  else if (cmdres) {
    fprintf(stderr, GOCR_PROGRAM" returned %d\n", cmdres);
    exit(cmdres);
  }
  process_gocr_output(outbuf,region);
  unlink(inbuf);
  unlink(outbuf);
}

void init_data() {
  int i;

  for (i=0;i<MAX_REGIONS;i++) {
    page.regions[i].is_visible=0;
    regions[i].win=-1;
  }
}

void create_region(int region_id,int region_width,int region_height,int region_depth) {
  regions[region_id].win=num_windows++;
  //fprintf(stderr,"region %d created - win=%d, height=%d, width=%d, depth=%d\n",region_id,regions[region_id].win,region_height,region_width,region_depth);
  regions[region_id].width=region_width;
  regions[region_id].height=region_height;

  memset(regions[region_id].img,15,sizeof(regions[region_id].img));
}

void test_OSD() {
  return;
}

void do_plot(int r,int x, int y, unsigned char pixel) {
  int i;
  if ((y >= 0) && (y < regions[r].height)) {
    i=(y*regions[r].width)+x;
    regions[r].img[i]=pixel;
  } else {
    fprintf(stderr,"plot out of region: x=%d, y=%d - r=%d, height=%d\n",x,y,r,regions[r].height);
  }
}

void plot(int r,int run_length, unsigned char pixel) {
  int x2=x+run_length;

//  fprintf(stderr,"plot: x=%d,y=%d,length=%d,pixel=%d\n",x,y,run_length,pixel);
  while (x < x2) {
    do_plot(r,x,y,pixel);
    //fprintf(stderr,"%s",pixel==0?"  ":pixel==5?"..":pixel==6?"oo":pixel==7?"xx":pixel==8?"OO":"XX");
    x++;
  }
    
  //  x+=run_length;
}

ssize_t safe_read(int fd, void *buf, size_t count) {
 ssize_t n,tot;

 tot=0;
 while (tot < count) {
   n=read(fd,buf,count-tot);
   tot+=n;
 }
 return(tot);
}

unsigned char next_nibble () {
  unsigned char x;

  if (nibble_flag==0) {
    x=(buf[i]&0xf0)>>4;
    nibble_flag=1;
  } else {
    x=(buf[i++]&0x0f);
    nibble_flag=0;
  }
  return(x);
}

/* function taken from "dvd2sub.c" in the svcdsubs packages in the
   vcdimager contribs directory.  Author unknown, but released under GPL2.
*/


/*void set_palette(int region_id,int id,int Y_value, int Cr_value, int Cb_value, int T_value) {
 int Y,Cr,Cb,R,G,B;
 unsigned char colour[4];

 Y=Y_value;
 Cr=Cr_value;
 Cb=Cb_value;
 B = 1.164*(Y - 16)                    + 2.018*(Cb - 128);
 G = 1.164*(Y - 16) - 0.813*(Cr - 128) - 0.391*(Cb - 128);
 R = 1.164*(Y - 16) + 1.596*(Cr - 128);
 if (B<0) B=0; if (B>255) B=255;
 if (G<0) G=0; if (G>255) G=255;
 if (R<0) R=0; if (R>255) R=255;
 colour[0]=R;
 colour[1]=B;
 colour[2]=G;
 if (Y==0) {
   colour[3]=0;
 } else {
   colour[3]=255;
 }

 //fprintf(stderr,"setting palette: region=%d,id=%d, R=%d,G=%d,B=%d,T=%d\n",region_id,id,R,G,B,T_value);

} 
*/

static inline void set_palette(int region_id,int id,int Y_value, int Cr_value, int Cb_value, int T_value) {
  regions[region_id].palette[id] = Y_value;
  if (Y_value == 0) T_value = 0;
  regions[region_id].alpha[id] = T_value;
  //fprintf(stderr,"setting palette: region=%d,id=%d, val=%d,alpha=%d\n",region_id,id,Y_value,T_value);
}

void decode_4bit_pixel_code_string(int r, int object_id, int ofs, int n) {
  int next_bits,
      switch_1,
      switch_2,
      switch_3,
      run_length,
      pixel_code=0;

  int bits;
  unsigned int data;
  int j;

  if (in_scanline==0) {
    // printf("<scanline>\n");
    //fprintf(stderr,"\n");
    in_scanline=1;
  }
  nibble_flag=0;
  j=i+n;
  while(i < j) {
//    printf("start of loop, i=%d, nibble-flag=%d\n",i,nibble_flag);
//    printf("buf=%02x %02x %02x %02x\n",buf[i],buf[i+1],buf[i+2],buf[i+3]);

    pixel_code = 0;
    bits=0;
    next_bits=next_nibble();

    if (next_bits!=0) {
      pixel_code=next_bits;
      // printf("<pixel run_length=\"1\" pixel_code=\"%d\" />\n",pixel_code);
      plot(r,1,pixel_code);
      bits+=4;
    } else {
      bits+=4;
      data=next_nibble();
      switch_1=(data&0x08)>>3;
      bits++;
      if (switch_1==0) {
        run_length=(data&0x07);
        bits+=3;
        if (run_length!=0) {
          // printf("<pixel run_length=\"%d\" pixel_code=\"0\" />\n",run_length+2);
          plot(r,run_length+2,pixel_code);
        } else {
//          printf("end_of_string - run_length=%d\n",run_length);
          break;
        }
      } else {
        switch_2=(data&0x04)>>2;
        bits++;
        if (switch_2==0) {
          run_length=(data&0x03);
          bits+=2;
          pixel_code=next_nibble();
          bits+=4;
          //printf("<pixel run_length=\"%d\" pixel_code=\"%d\" />\n",run_length+4,pixel_code);
          plot(r,run_length+4,pixel_code);
        } else {
          switch_3=(data&0x03);
          bits+=2;
          switch (switch_3) {
            case 0: // printf("<pixel run_length=\"1\" pixel_code=\"0\" />\n");
                    plot(r,1,pixel_code);
                    break;
            case 1: // printf("<pixel run_length=\"2\" pixel_code=\"0\" />\n");
                    plot(r,2,pixel_code);
                    break;
            case 2: run_length=next_nibble();
                    bits+=4;
                    pixel_code=next_nibble();
                    bits+=4;
                    // printf("<pixel run_length=\"%d\", pixel_code=\"%d\" />\n",run_length+9,pixel_code);
                    plot(r,run_length+9,pixel_code);
                    break;
            case 3: run_length=next_nibble();
                    run_length=(run_length<<4)|next_nibble();
                    bits+=8;
                    pixel_code=next_nibble();
                    bits+=4;
                    // printf("<pixel run_length=\"%d\" pixel_code=\"%d\" />\n",run_length+25,pixel_code);
                    plot(r,run_length+25,pixel_code);
          }
        }
      }
    }

//    printf("used %d bits\n",bits);
  }
  if (nibble_flag==1) {
    i++;
    nibble_flag=0;
  }
}


void process_pixel_data_sub_block(int r, int o, int ofs, int n) {
  int data_type;
  int j;

  j=i+n;

  x=(regions[r].object_pos[o])>>16;
  y=((regions[r].object_pos[o])&0xffff)+ofs;
//  fprintf(stderr,"process_pixel_data_sub_block: r=%d, x=%d, y=%d\n",r,x,y);
//  printf("process_pixel_data: %02x %02x %02x %02x %02x %02x\n",buf[i],buf[i+1],buf[i+2],buf[i+3],buf[i+4],buf[i+5]);
  while (i < j) {
    data_type=buf[i++];

//    printf("<data_type>%02x</data_type>\n",data_type);

    switch(data_type) {
      case 0x11: decode_4bit_pixel_code_string(r,o,ofs,n-1);
                 break;
      case 0xf0: // printf("</scanline>\n");
                 in_scanline=0;
                 x=(regions[r].object_pos[o])>>16;
                 y+=2;
                 break;
      default: fprintf(stderr,"unimplemented data_type %02x in pixel_data_sub_block\n",data_type);
    }
  }

  i=j;
}
int process_page_composition_segment() {
  int page_id,
      segment_length,
      page_time_out,
      page_version_number,
      page_state;
  int region_id,region_x,region_y;
  int j;

  page_id=(buf[i]<<8)|buf[i+1]; i+=2;
  segment_length=(buf[i]<<8)|buf[i+1]; i+=2;

  j=i+segment_length;

  page_time_out=buf[i++];
  page_version_number=(buf[i]&0xf0)>>4;
  page_state=(buf[i]&0x0c)>>2;
  i++;

  // printf("<page_composition_segment page_id=\"0x%02x\">\n",page_id);
  // printf("<page_time_out>%d</page_time_out>\n",page_time_out);
  // printf("<page_version_number>%d</page_version_number>\n",page_version_number);
  // printf("<page_state>");
  //fprintf(stderr,"page_state=%d (",page_state);
  switch(page_state) {
     case 0: //fprintf(stderr,"normal_case)\n");
       break ;
     case 1: //fprintf(stderr,"acquisition_point)\n");
       break ;
     case 2: //fprintf(stderr,"mode_change)\n");
       break ;
     case 3: //fprintf(stderr,"reserved)\n");
       break ;
  }
  // printf("</page_state>\n");

  if ((acquired==0) && (page_state!=2) && (page_state!=1)) {
    //fprintf(stderr,"waiting for mode_change\n");
    return 1;
  } else {
    //fprintf(stderr,"acquired=1\n");
    acquired=1;
  }
  // printf("<page_regions>\n");
  // IF the packet contains no data (i.e. is  used to clear a
  // previous subtitle), do nothing
  if (i>=j) {
    //fprintf(stderr,"Empty sub, return\n");
    return 1;
  }

  while (i<j) {
    region_id=buf[i++];
    i++; // reserved
    region_x=(buf[i]<<8)|buf[i+1]; i+=2;
    region_y=(buf[i]<<8)|buf[i+1]; i+=2;

    page.regions[region_id].x=region_x;
    page.regions[region_id].y=region_y;
    page.regions[region_id].is_visible=1;
 
    //fprintf(stderr,"page_region id=%02x x=%d y=%d\n",region_id,region_x,region_y);
  }  
  // printf("</page_regions>\n");
  // printf("</page_composition_segment>\n");
  return 0;

}

void process_region_composition_segment() {
  int page_id,
      segment_length,
      region_id,
      region_version_number,
      region_fill_flag,
      region_width,
      region_height,
      region_level_of_compatibility,
      region_depth,
      CLUT_id,
      region_8_bit_pixel_code,
      region_4_bit_pixel_code,
      region_2_bit_pixel_code;
  int object_id,
      object_type,
      object_provider_flag,
      object_x,
      object_y,
      foreground_pixel_code,
      background_pixel_code;
  int j;
  int o;

  page_id=(buf[i]<<8)|buf[i+1]; i+=2;
  segment_length=(buf[i]<<8)|buf[i+1]; i+=2;

  j=i+segment_length;

  region_id=buf[i++];
  region_version_number=(buf[i]&0xf0)>>4;
  region_fill_flag=(buf[i]&0x08)>>3;
  i++;
  region_width=(buf[i]<<8)|buf[i+1]; i+=2;
  region_height=(buf[i]<<8)|buf[i+1]; i+=2;
  region_level_of_compatibility=(buf[i]&0xe0)>>5;
  region_depth=(buf[i]&0x1c)>>2;
  i++;
  CLUT_id=buf[i++];
  region_8_bit_pixel_code=buf[i++];
  region_4_bit_pixel_code=(buf[i]&0xf0)>>4;
  region_2_bit_pixel_code=(buf[i]&0x0c)>>2;
  i++;


  if (regions[region_id].win < 0) {
    // If the region doesn't exist, then open it.
    create_region(region_id,region_width,region_height,region_depth);
    regions[region_id].CLUT_id=CLUT_id;
  }

  if (region_fill_flag==1) {
    //fprintf(stderr,"filling region %d with %d\n",region_id,region_4_bit_pixel_code);
    memset(regions[region_id].img,region_4_bit_pixel_code,sizeof(regions[region_id].img));
  }

  // printf("<region_composition_segment page_id=\"0x%02x\" region_id=\"0x%02x\">\n",page_id,region_id);

  // printf("<region_version_number>%d</region_version_number>\n",region_version_number);
  // printf("<region_fill_flag>%d</region_fill_flag>\n",region_fill_flag);
  // printf("<region_width>%d</region_width>\n",region_width);
  // printf("<region_height>%d</region_height>\n",region_height);
  // printf("<region_level_of_compatibility>%d</region_level_of_compatibility>\n",region_level_of_compatibility);
  // printf("<region_depth>%d</region_depth>\n",region_depth);
  // printf("<CLUT_id>%d</CLUT_id>\n",CLUT_id);
  // printf("<region_8_bit_pixel_code>%d</region_8_bit_pixel_code>\n",region_8_bit_pixel_code);
  // printf("<region_4_bit_pixel_code>%d</region_4_bit_pixel_code>\n",region_4_bit_pixel_code);
  // printf("<region_2_bit_pixel_code>%d</region_2_bit_pixel_code>\n",region_2_bit_pixel_code);

  regions[region_id].objects_start=i;  
  regions[region_id].objects_end=j;  

  for (o=0;o<65536;o++) {
    regions[region_id].object_pos[o]=0xffffffff;
  }

  // printf("<objects>\n");
  while (i < j) {
    object_id=(buf[i]<<8)|buf[i+1]; i+=2;
    object_type=(buf[i]&0xc0)>>6;
    object_provider_flag=(buf[i]&0x30)>>4;
    object_x=((buf[i]&0x0f)<<8)|buf[i+1]; i+=2;
    object_y=((buf[i]&0x0f)<<8)|buf[i+1]; i+=2;

    regions[region_id].object_pos[object_id]=(object_x<<16)|object_y;
      
    // printf("<object id=\"0x%02x\" type=\"0x%02x\">\n",object_id,object_type);
    // printf("<object_provider_flag>%d</object_provider_flag>\n",object_provider_flag);
    // printf("<object_x>%d</object_x>\n",object_x);
    // printf("<object_y>%d</object_y>\n",object_y);
    if ((object_type==0x01) || (object_type==0x02)) {
      foreground_pixel_code=buf[i++];
      background_pixel_code=buf[i++];
      // printf("<foreground_pixel_code>%d</foreground_pixel_code>\n",foreground_pixel_code);
      // printf("<background_pixel_code>%d</background_pixel_code>\n",background_pixel_code);
    }

    // printf("</object>\n");
  }
  // printf("</objects>\n");
  // printf("</region_composition_segment>\n");
}

void process_CLUT_definition_segment() {
  int page_id,
      segment_length,
      CLUT_id,
      CLUT_version_number;

  int CLUT_entry_id,
      CLUT_flag_8_bit,
      CLUT_flag_4_bit,
      CLUT_flag_2_bit,
      full_range_flag,
      Y_value,
      Cr_value,
      Cb_value,
      T_value;

  int j;
  int r;

  page_id=(buf[i]<<8)|buf[i+1]; i+=2;
  segment_length=(buf[i]<<8)|buf[i+1]; i+=2;
  j=i+segment_length;

  CLUT_id=buf[i++];
  CLUT_version_number=(buf[i]&0xf0)>>4;
  i++;

  // printf("<CLUT_definition_segment page_id=\"0x%02x\" CLUT_id=\"0x%02x\">\n",page_id,CLUT_id);

  // printf("<CLUT_version_number>%d</CLUT_version_number>\n",CLUT_version_number);
  // printf("<CLUT_entries>\n");
  while (i < j) {
    CLUT_entry_id=buf[i++];
      
    // printf("<CLUT_entry id=\"0x%02x\">\n",CLUT_entry_id);
    CLUT_flag_2_bit=(buf[i]&0x80)>>7;
    CLUT_flag_4_bit=(buf[i]&0x40)>>6;
    CLUT_flag_8_bit=(buf[i]&0x20)>>5;
    full_range_flag=buf[i]&1;
    i++;
    // printf("<CLUT_flag_2_bit>%d</CLUT_flag_2_bit>\n",CLUT_flag_2_bit);
    // printf("<CLUT_flag_4_bit>%d</CLUT_flag_4_bit>\n",CLUT_flag_4_bit);
    // printf("<CLUT_flag_8_bit>%d</CLUT_flag_8_bit>\n",CLUT_flag_8_bit);
    // printf("<full_range_flag>%d</full_range_flag>\n",full_range_flag);
    if (full_range_flag==1) {
      Y_value=buf[i++];
      Cr_value=buf[i++];
      Cb_value=buf[i++];
      T_value=buf[i++];
    } else {
      Y_value=(buf[i]&0xfc)>>2;
      Cr_value=(buf[i]&0x2<<2)|((buf[i+1]&0xc0)>>6);
      Cb_value=(buf[i+1]&0x2c)>>2;
      T_value=buf[i+1]&2;
      i+=2;
    }
    // printf("<Y_value>%d</Y_value>\n",Y_value);
    // printf("<Cr_value>%d</Cr_value>\n",Cr_value);
    // printf("<Cb_value>%d</Cb_value>\n",Cb_value);
    // printf("<T_value>%d</T_value>\n",T_value);
    // printf("</CLUT_entry>\n");

    // Apply CLUT to every region it applies to.
    for (r=0;r<MAX_REGIONS;r++) {
      if (regions[r].win >= 0) {
        if (regions[r].CLUT_id==CLUT_id) {
          set_palette(r,CLUT_entry_id,Y_value,Cr_value,Cb_value,255-T_value);
        }
      }
    }
  }
  // printf("</CLUT_entries>\n");
  // printf("</CLUT_definition_segment>\n");
}

void process_object_data_segment() {
  int page_id,
      segment_length,
      object_id,
      object_version_number,
      object_coding_method,
      non_modifying_colour_flag;
      
  int top_field_data_block_length,
      bottom_field_data_block_length;
      
  int j;
  int old_i;
  int r;

  page_id=(buf[i]<<8)|buf[i+1]; i+=2;
  segment_length=(buf[i]<<8)|buf[i+1]; i+=2;
  j=i+segment_length;
  
  object_id=(buf[i]<<8)|buf[i+1]; i+=2;
  curr_obj=object_id;
  object_version_number=(buf[i]&0xf0)>>4;
  object_coding_method=(buf[i]&0x0c)>>2;
  non_modifying_colour_flag=(buf[i]&0x02)>>1;
  i++;

  // printf("<object_data_segment page_id=\"0x%02x\" object_id=\"0x%02x\">\n",page_id,object_id);

  // printf("<object_version_number>%d</object_version_number>\n",object_version_number);
  // printf("<object_coding_method>%d</object_coding_method>\n",object_coding_method);
  // printf("<non_modifying_colour_flag>%d</non_modifying_colour_flag>\n",non_modifying_colour_flag);

  // fprintf(stderr,"decoding object %d\n",object_id);
  old_i=i;
  for (r=0;r<MAX_REGIONS;r++) {
    // If this object is in this region...
   if (regions[r].win >= 0) {
    //fprintf(stderr,"testing region %d, object_pos=%08x\n",r,regions[r].object_pos[object_id]);
    if (regions[r].object_pos[object_id]!=0xffffffff) {
      //fprintf(stderr,"rendering object %d into region %d\n",object_id,r);
      i=old_i;
      if (object_coding_method==0) {
        top_field_data_block_length=(buf[i]<<8)|buf[i+1]; i+=2;
        bottom_field_data_block_length=(buf[i]<<8)|buf[i+1]; i+=2;

        process_pixel_data_sub_block(r,object_id,0,top_field_data_block_length);

        process_pixel_data_sub_block(r,object_id,1,bottom_field_data_block_length);
      }
    }
   }
  }
  // Data should be word-aligned, pass the next byte if necessary
  if (((old_i - i) & 0x1) == 0)
    i++;
}

void process_pes_packet() {
  int n;
  unsigned long long PTS;
  unsigned char PTS_1;
  unsigned short PTS_2,PTS_3;
  double PTS_secs;
  int r; 

  int segment_length,
      segment_type;
  int empty_sub = 0;

  init_data();
  gettimeofday(&start_tv,NULL);

  // printf("<?xml version=\"1.0\" ?>\n");
  i=6;

  i++;  // Skip some boring PES flags
  if (buf[i]!=0x80) {
   fprintf(stdout,"UNEXPECTED PES HEADER: %02x\n",buf[i]);
   exit(-1);
  }
  i++; 
  if (buf[i]!=5) {
   fprintf(stdout,"UNEXPECTED PES HEADER DATA LENGTH: %d\n",buf[i]);
   exit(-1);
  }
  i++;  // Header data length
  PTS_1=(buf[i++]&0x0e)>>1;  // 3 bits
  PTS_2=(buf[i]<<7)|((buf[i+1]&0xfe)>>1);         // 15 bits
  i+=2;
  PTS_3=(buf[i]<<7)|((buf[i+1]&0xfe)>>1);         // 15 bits
  i+=2;

  PTS=PTS_1;
  PTS=(PTS << 15)|PTS_2;
  PTS=(PTS << 15)|PTS_3;

  PTS_secs=(PTS/90000.0);

  // printf("<pes_packet data_identifier=\"0x%02x\" pts_secs=\"%.02f\">\n",buf[i++],PTS_secs);
  i++;
  // printf("<subtitle_stream id=\"0x%02x\">\n",buf[i++]);
  i++;
  while (buf[i]==0x0f) {
    /* SUBTITLING SEGMENT */
    i++;  // sync_byte
    segment_type=buf[i++];

    /* SEGMENT_DATA_FIELD */
    switch(segment_type) {
      case 0x10: empty_sub = process_page_composition_segment(); 
                 break;
      case 0x11: process_region_composition_segment();
                 break;
      case 0x12: process_CLUT_definition_segment();
                 break;
      case 0x13: process_object_data_segment();
                 break;
      default:
        segment_length=(buf[i+2]<<8)|buf[i+3];
        i+=segment_length+4;
//        printf("SKIPPING segment %02x, length %d\n",segment_type,segment_length);
    }
  }   
  // printf("</subtitle_stream>\n");
  // printf("</pes_packet>\n");
  // fprintf(stderr,"End of PES packet - time=%.2f\n",PTS_secs);
  if (empty_sub) {
    int i;
    if (textsub.start_pts < 0)
      return;
    textsub.end_pts = PTS/90;
    output_textsub(outfile);
    textsub.end_pts = textsub.start_pts = -1;
    for(i=0; i<MAX_REGIONS; i++)
      textsub.regions[i][0] = '\0';
  }
  else {
    textsub.start_pts = PTS/90;
    n=0;
    for (r=0;r<MAX_REGIONS;r++) {
      if (regions[r].win >= 0) {
        if (page.regions[r].is_visible) {
          //int xx,yy;
          //fprintf(stderr,"displaying region %d at %d,%d width=%d,height=%d PTS = %g\n",
        //r,page.regions[r].x,page.regions[r].y,regions[r].width,regions[r].height, PTS_secs);
          /*for(yy=0;yy<regions[r].height;yy++) {
            for(xx=0;xx<regions[r].width;xx++) {
              unsigned char pix = regions[r].img[+yy*regions[r].width+xx];
              fprintf(stderr,"%s",pix==0?"  ":pix==5?"..":pix==6?"oo":pix==7?"xx":pix==8?"OO":"XX");
            }
            fprintf(stderr,"\n");
          }*/
          run_ocr(r, PTS);
          n++;
        }
        /* else {
          //fprintf(stderr,"hiding region %d\n",r);
        }*/
      }
    }
    /*if (n > 0) {
      fprintf(stderr,"%d regions visible - showing\n",n);
    } else {
      fprintf(stderr,"%d regions visible - hiding\n",n);
    }*/
  }
//    if (acquired) { sleep(1); }
}

#define PID_MASK_HI 0x1F
uint16_t get_pid(uint8_t *pid)
{
  uint16_t pp = 0;

  pp = (pid[0] & PID_MASK_HI) << 8;
  pp |= pid[1];

  return pp;
}

/* From dvb-mpegtools ctools.c, (C) 2000-2002 Marcus Metzler,
   license GPLv2+. */
ssize_t save_read(int fd, void *buf, size_t count)
{
	ssize_t neof = 1;
	size_t re = 0;
	
	while(neof >= 0 && re < count){
		neof = read(fd, buf+re, count - re);
		if (neof > 0) re += neof;
		else break;
	}

	if (neof < 0 && re == 0) return neof;
	else return re;
}

#define TS_SIZE 188
#define IN_SIZE TS_SIZE*10
uint8_t * get_sub_packets(int fdin, uint16_t pids) {
  uint8_t buffer[IN_SIZE];
  uint8_t mbuf[TS_SIZE];
  uint8_t * packet = NULL;
  uint8_t * next_write = NULL;
  int packet_current_size = 0;
  int packet_size = 0;
  int i;
  int count = 1;
  uint16_t pid;

  // fprintf(stderr,"extract pid %d\n", pids);
  if ((count = save_read(fdin,mbuf,TS_SIZE))<0)
      perror("reading");

  for ( i = 0; i < 188 ; i++){
    if ( mbuf[i] == 0x47 ) break;
  }
  if ( i == 188){
    fprintf(stderr,"Not a TS\n");
    return NULL;
  } else {
    memcpy(buffer,mbuf+i,TS_SIZE-i);
    if ((count = save_read(fdin,mbuf,i))<0)
      perror("reading");
    memcpy(buffer+TS_SIZE-i,mbuf,i);
    i = 188;
  }

  count = 1;
  while (count > 0){
    if ((count = save_read(fdin,buffer+i,IN_SIZE-i)+i)<0)
      perror("reading");
    for( i = 0; i < count; i+= TS_SIZE){
      uint8_t off = 0;

      if ( count - i < TS_SIZE) break;
      pid = get_pid(buffer+i+1);
      if (!(buffer[3+i]&0x10)) // no payload?
        continue;
      if ( buffer[1+i]&0x80){
        fprintf(stderr,"Error in TS for PID: %d\n", pid);
      }
      if (pid != pids)
        continue;

      if ( buffer[3+i] & 0x20) {  // adaptation field?
        off = buffer[4+i] + 1;
      }
      if ( !packet && 
           ! buffer[i+off+4] && ! buffer[i+off+5] && buffer[i+off+6] && 
           buffer[i+off+7] == 0xbd) {
        packet_size = buffer[i+off+8]<<8 | buffer[i+off+9];
        packet_size += 6; // for the prefix, the stream ID and the size field
        
        //fprintf(stderr,"Packet start, size = %d\n",packet_size);
        packet = (uint8_t *)malloc(packet_size);
        next_write = packet;
        packet_current_size = 0;
      }
      if (packet) {
        if (packet_current_size + TS_SIZE-4-off <= packet_size) {
          memcpy(next_write, buffer+4+off+i, TS_SIZE-4-off);
          next_write+=TS_SIZE-4-off;
          packet_current_size += TS_SIZE-4-off;
        }
        else {
          fprintf(stderr,"write beyond buffer limit?\n");
          free(packet);
          packet = NULL;
          next_write = NULL;
          packet_current_size = 0;
          packet_size = 0;
          continue;
        }
        if (packet_current_size == packet_size) {
          // process packet
          //int j=0;
          buf = packet;
          /*for(j=0;j<packet_size;j++) {
            fprintf(stderr,"%02x ", packet[j]);
          }
          fprintf(stderr,"\n");
          */
          process_pes_packet();
          free(packet);
          packet = NULL;
          next_write = NULL;
          packet_current_size = 0;
          packet_size = 0;
        }
      }
    }
    i = 0;
  }
  return NULL;
}

int main(int argc, char* argv[]) {
  int fd;
  int pid;
 
  if (argc!=4) {
    fprintf(stderr,"USAGE: dvbsubs PID input_file output_file\n");
    exit(0);
  }

  pid=atoi(argv[1]);
  fd=open(argv[2],O_RDONLY);
  outfile=fopen(argv[3],"w");
  textsub.start_pts = textsub.end_pts = -1;
  get_sub_packets(fd,pid);
  fclose(outfile);
  close(fd);
  return 0;
}


