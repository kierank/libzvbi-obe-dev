/* Generated file, do not edit! */

#include <stdio.h>
#include "io.h"

#ifndef __GNUC__
#undef __attribute__
#define __attribute__(x)
#endif

static void
fprint_struct_video_play_mode (FILE *fp, int rw __attribute__ ((unused)), const struct video_play_mode *t)
{
fprintf (fp, "mode=%ld "
"p1=%ld "
"p2=%ld ",
(long) t->mode, 
(long) t->p1, 
(long) t->p2);
}

static void
fprint_struct_video_capture (FILE *fp, int rw __attribute__ ((unused)), const struct video_capture *t)
{
fprintf (fp, "x=%lu "
"y=%lu "
"width=%lu "
"height=%lu "
"decimation=%lu "
"flags=",
(unsigned long) t->x, 
(unsigned long) t->y, 
(unsigned long) t->width, 
(unsigned long) t->height, 
(unsigned long) t->decimation);
fprint_symbolic (fp, 2, t->flags,
"ODD", (unsigned long) VIDEO_CAPTURE_ODD,
"EVEN", (unsigned long) VIDEO_CAPTURE_EVEN,
(void *) 0);
fputs (" ", fp);
}

static void
fprint_struct_video_code (FILE *fp, int rw __attribute__ ((unused)), const struct video_code *t)
{
fprintf (fp, "loadwhat=\"%.*s\" "
"datasize=%ld "
"data=%p ",
16, (const char *) t->loadwhat, 
(long) t->datasize, 
(const void *) t->data);
}

static void
fprint_struct_video_unit (FILE *fp, int rw __attribute__ ((unused)), const struct video_unit *t)
{
fprintf (fp, "video=%ld "
"vbi=%ld "
"radio=%ld "
"audio=%ld "
"teletext=%ld ",
(long) t->video, 
(long) t->vbi, 
(long) t->radio, 
(long) t->audio, 
(long) t->teletext);
}

static void
fprint_struct_video_window (FILE *fp, int rw __attribute__ ((unused)), const struct video_window *t)
{
fprintf (fp, "x=%lu "
"y=%lu "
"width=%lu "
"height=%lu "
"chromakey=%lu "
"flags=",
(unsigned long) t->x, 
(unsigned long) t->y, 
(unsigned long) t->width, 
(unsigned long) t->height, 
(unsigned long) t->chromakey);
fprint_symbolic (fp, 2, t->flags,
"INTERLACE", (unsigned long) VIDEO_WINDOW_INTERLACE,
"CHROMAKEY", (unsigned long) VIDEO_WINDOW_CHROMAKEY,
(void *) 0);
fprintf (fp, " clips=%p "
"clipcount=%ld ",
(const void *) t->clips, 
(long) t->clipcount);
}

static void
fprint_symbol_video_palette_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"GREY", (unsigned long) VIDEO_PALETTE_GREY,
"HI240", (unsigned long) VIDEO_PALETTE_HI240,
"RGB565", (unsigned long) VIDEO_PALETTE_RGB565,
"RGB24", (unsigned long) VIDEO_PALETTE_RGB24,
"RGB32", (unsigned long) VIDEO_PALETTE_RGB32,
"RGB555", (unsigned long) VIDEO_PALETTE_RGB555,
"YUV422", (unsigned long) VIDEO_PALETTE_YUV422,
"YUYV", (unsigned long) VIDEO_PALETTE_YUYV,
"UYVY", (unsigned long) VIDEO_PALETTE_UYVY,
"YUV420", (unsigned long) VIDEO_PALETTE_YUV420,
"YUV411", (unsigned long) VIDEO_PALETTE_YUV411,
"RAW", (unsigned long) VIDEO_PALETTE_RAW,
"YUV422P", (unsigned long) VIDEO_PALETTE_YUV422P,
"YUV411P", (unsigned long) VIDEO_PALETTE_YUV411P,
"YUV420P", (unsigned long) VIDEO_PALETTE_YUV420P,
"YUV410P", (unsigned long) VIDEO_PALETTE_YUV410P,
"PLANAR", (unsigned long) VIDEO_PALETTE_PLANAR,
"COMPONENT", (unsigned long) VIDEO_PALETTE_COMPONENT,
(void *) 0);
}

static void
fprint_struct_video_picture (FILE *fp, int rw __attribute__ ((unused)), const struct video_picture *t)
{
fprintf (fp, "brightness=%lu "
"hue=%lu "
"colour=%lu "
"contrast=%lu "
"whiteness=%lu "
"depth=%lu "
"palette=",
(unsigned long) t->brightness, 
(unsigned long) t->hue, 
(unsigned long) t->colour, 
(unsigned long) t->contrast, 
(unsigned long) t->whiteness, 
(unsigned long) t->depth);
fprint_symbol_video_palette_ (fp, rw, t->palette);
fputs (" ", fp);
}

static void
fprint_struct_video_mmap (FILE *fp, int rw __attribute__ ((unused)), const struct video_mmap *t)
{
fprintf (fp, "frame=%lu "
"height=%ld "
"width=%ld "
"format=%lu ",
(unsigned long) t->frame, 
(long) t->height, 
(long) t->width, 
(unsigned long) t->format);
}

static void
fprint_struct_video_buffer (FILE *fp, int rw __attribute__ ((unused)), const struct video_buffer *t)
{
fprintf (fp, "base=%p "
"height=%ld "
"width=%ld "
"depth=%ld "
"bytesperline=%ld ",
(const void *) t->base, 
(long) t->height, 
(long) t->width, 
(long) t->depth, 
(long) t->bytesperline);
}

static void
fprint_struct_video_mbuf (FILE *fp, int rw __attribute__ ((unused)), const struct video_mbuf *t)
{
fprintf (fp, "size=%ld "
"frames=%ld "
"offsets[]=? ",
(long) t->size, 
(long) t->frames);
}

static void
fprint_symbol_video_audio_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 2, value,
"MUTE", (unsigned long) VIDEO_AUDIO_MUTE,
"MUTABLE", (unsigned long) VIDEO_AUDIO_MUTABLE,
"VOLUME", (unsigned long) VIDEO_AUDIO_VOLUME,
"BASS", (unsigned long) VIDEO_AUDIO_BASS,
"TREBLE", (unsigned long) VIDEO_AUDIO_TREBLE,
(void *) 0);
}

static void
fprint_symbol_video_sound_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"MONO", (unsigned long) VIDEO_SOUND_MONO,
"STEREO", (unsigned long) VIDEO_SOUND_STEREO,
"LANG1", (unsigned long) VIDEO_SOUND_LANG1,
"LANG2", (unsigned long) VIDEO_SOUND_LANG2,
(void *) 0);
}

static void
fprint_struct_video_audio (FILE *fp, int rw __attribute__ ((unused)), const struct video_audio *t)
{
fprintf (fp, "audio=%ld "
"volume=%lu "
"bass=%lu "
"treble=%lu "
"flags=",
(long) t->audio, 
(unsigned long) t->volume, 
(unsigned long) t->bass, 
(unsigned long) t->treble);
fprint_symbol_video_audio_ (fp, rw, t->flags);
fprintf (fp, " name=\"%.*s\" "
"mode=",
16, (const char *) t->name);
fprint_symbol_video_sound_ (fp, rw, t->mode);
fprintf (fp, " balance=%lu "
"step=%lu ",
(unsigned long) t->balance, 
(unsigned long) t->step);
}

static void
fprint_struct_vbi_format (FILE *fp, int rw __attribute__ ((unused)), const struct vbi_format *t)
{
fprintf (fp, "sampling_rate=%lu "
"samples_per_line=%lu "
"sample_format=%lu "
"start[]=? "
"count[]=? "
"flags=",
(unsigned long) t->sampling_rate, 
(unsigned long) t->samples_per_line, 
(unsigned long) t->sample_format);
fprint_symbolic (fp, 2, t->flags,
"UNSYNC", (unsigned long) VBI_UNSYNC,
"INTERLACED", (unsigned long) VBI_INTERLACED,
(void *) 0);
fputs (" ", fp);
}

static void
fprint_struct_video_info (FILE *fp, int rw __attribute__ ((unused)), const struct video_info *t)
{
fprintf (fp, "frame_count=%lu "
"h_size=%lu "
"v_size=%lu "
"smpte_timecode=%lu "
"picture_type=%lu "
"temporal_reference=%lu "
"user_data[]=? ",
(unsigned long) t->frame_count, 
(unsigned long) t->h_size, 
(unsigned long) t->v_size, 
(unsigned long) t->smpte_timecode, 
(unsigned long) t->picture_type, 
(unsigned long) t->temporal_reference);
}

static void
fprint_struct_video_channel (FILE *fp, int rw __attribute__ ((unused)), const struct video_channel *t)
{
fprintf (fp, "channel=%ld "
"name=\"%.*s\" "
"tuners=%ld "
"flags=",
(long) t->channel, 
32, (const char *) t->name, 
(long) t->tuners);
fprint_symbolic (fp, 2, t->flags,
"TUNER", (unsigned long) VIDEO_VC_TUNER,
"AUDIO", (unsigned long) VIDEO_VC_AUDIO,
(void *) 0);
fputs (" type=", fp);
fprint_symbolic (fp, 0, t->type,
"TV", (unsigned long) VIDEO_TYPE_TV,
"CAMERA", (unsigned long) VIDEO_TYPE_CAMERA,
(void *) 0);
fprintf (fp, " norm=%lu ",
(unsigned long) t->norm);
}

static void
fprint_symbol_video_tuner_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 2, value,
"PAL", (unsigned long) VIDEO_TUNER_PAL,
"NTSC", (unsigned long) VIDEO_TUNER_NTSC,
"SECAM", (unsigned long) VIDEO_TUNER_SECAM,
"LOW", (unsigned long) VIDEO_TUNER_LOW,
"NORM", (unsigned long) VIDEO_TUNER_NORM,
"STEREO_ON", (unsigned long) VIDEO_TUNER_STEREO_ON,
"RDS_ON", (unsigned long) VIDEO_TUNER_RDS_ON,
"MBS_ON", (unsigned long) VIDEO_TUNER_MBS_ON,
(void *) 0);
}

static void
fprint_symbol_video_mode_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"PAL", (unsigned long) VIDEO_MODE_PAL,
"NTSC", (unsigned long) VIDEO_MODE_NTSC,
"SECAM", (unsigned long) VIDEO_MODE_SECAM,
"AUTO", (unsigned long) VIDEO_MODE_AUTO,
(void *) 0);
}

static void
fprint_struct_video_tuner (FILE *fp, int rw __attribute__ ((unused)), const struct video_tuner *t)
{
fprintf (fp, "tuner=%ld "
"name=\"%.*s\" "
"rangelow=%lu "
"rangehigh=%lu "
"flags=",
(long) t->tuner, 
32, (const char *) t->name, 
(unsigned long) t->rangelow, 
(unsigned long) t->rangehigh);
fprint_symbol_video_tuner_ (fp, rw, t->flags);
fputs (" mode=", fp);
fprint_symbol_video_mode_ (fp, rw, t->mode);
fprintf (fp, " signal=%lu ",
(unsigned long) t->signal);
}

static void
fprint_struct_video_key (FILE *fp, int rw __attribute__ ((unused)), const struct video_key *t)
{
fprintf (fp, "key[]=? "
"flags=0x%lx ",
(unsigned long) t->flags);
}

static void
fprint_symbol_vid_type_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"CAPTURE", (unsigned long) VID_TYPE_CAPTURE,
"TUNER", (unsigned long) VID_TYPE_TUNER,
"TELETEXT", (unsigned long) VID_TYPE_TELETEXT,
"OVERLAY", (unsigned long) VID_TYPE_OVERLAY,
"CHROMAKEY", (unsigned long) VID_TYPE_CHROMAKEY,
"CLIPPING", (unsigned long) VID_TYPE_CLIPPING,
"FRAMERAM", (unsigned long) VID_TYPE_FRAMERAM,
"SCALES", (unsigned long) VID_TYPE_SCALES,
"MONOCHROME", (unsigned long) VID_TYPE_MONOCHROME,
"SUBCAPTURE", (unsigned long) VID_TYPE_SUBCAPTURE,
"MPEG_DECODER", (unsigned long) VID_TYPE_MPEG_DECODER,
"MPEG_ENCODER", (unsigned long) VID_TYPE_MPEG_ENCODER,
"MJPEG_DECODER", (unsigned long) VID_TYPE_MJPEG_DECODER,
"MJPEG_ENCODER", (unsigned long) VID_TYPE_MJPEG_ENCODER,
(void *) 0);
}

static void
fprint_struct_video_capability (FILE *fp, int rw __attribute__ ((unused)), const struct video_capability *t)
{
fprintf (fp, "name=\"%.*s\" "
"type=",
32, (const char *) t->name);
fprint_symbol_vid_type_ (fp, rw, t->type);
fprintf (fp, " channels=%ld "
"audios=%ld "
"maxwidth=%ld "
"maxheight=%ld "
"minwidth=%ld "
"minheight=%ld ",
(long) t->channels, 
(long) t->audios, 
(long) t->maxwidth, 
(long) t->maxheight, 
(long) t->minwidth, 
(long) t->minheight);
}

static void
fprint_ioctl_arg (FILE *fp, unsigned int cmd, int rw, void *arg)
{
switch (cmd) {
case VIDIOCSPLAYMODE:
if (!arg) { fputs ("VIDIOCSPLAYMODE", fp); return; }
 fprint_struct_video_play_mode (fp, rw, arg);
break;
case VIDIOCGCAPTURE:
if (!arg) { fputs ("VIDIOCGCAPTURE", fp); return; }
case VIDIOCSCAPTURE:
if (!arg) { fputs ("VIDIOCSCAPTURE", fp); return; }
 fprint_struct_video_capture (fp, rw, arg);
break;
case VIDIOCCAPTURE:
if (!arg) { fputs ("VIDIOCCAPTURE", fp); return; }
case VIDIOCSYNC:
if (!arg) { fputs ("VIDIOCSYNC", fp); return; }
case VIDIOCSWRITEMODE:
if (!arg) { fputs ("VIDIOCSWRITEMODE", fp); return; }
 fprintf (fp, "%ld", (long) * (int *) arg);
break;
case VIDIOCSMICROCODE:
if (!arg) { fputs ("VIDIOCSMICROCODE", fp); return; }
 fprint_struct_video_code (fp, rw, arg);
break;
case VIDIOCGFREQ:
if (!arg) { fputs ("VIDIOCGFREQ", fp); return; }
case VIDIOCSFREQ:
if (!arg) { fputs ("VIDIOCSFREQ", fp); return; }
 fprintf (fp, "%lu", (unsigned long) * (unsigned long *) arg);
break;
case VIDIOCGUNIT:
if (!arg) { fputs ("VIDIOCGUNIT", fp); return; }
 fprint_struct_video_unit (fp, rw, arg);
break;
case VIDIOCGWIN:
if (!arg) { fputs ("VIDIOCGWIN", fp); return; }
case VIDIOCSWIN:
if (!arg) { fputs ("VIDIOCSWIN", fp); return; }
 fprint_struct_video_window (fp, rw, arg);
break;
case VIDIOCGPICT:
if (!arg) { fputs ("VIDIOCGPICT", fp); return; }
case VIDIOCSPICT:
if (!arg) { fputs ("VIDIOCSPICT", fp); return; }
 fprint_struct_video_picture (fp, rw, arg);
break;
case VIDIOCMCAPTURE:
if (!arg) { fputs ("VIDIOCMCAPTURE", fp); return; }
 fprint_struct_video_mmap (fp, rw, arg);
break;
case VIDIOCGFBUF:
if (!arg) { fputs ("VIDIOCGFBUF", fp); return; }
case VIDIOCSFBUF:
if (!arg) { fputs ("VIDIOCSFBUF", fp); return; }
 fprint_struct_video_buffer (fp, rw, arg);
break;
case VIDIOCGMBUF:
if (!arg) { fputs ("VIDIOCGMBUF", fp); return; }
 fprint_struct_video_mbuf (fp, rw, arg);
break;
case VIDIOCGAUDIO:
if (!arg) { fputs ("VIDIOCGAUDIO", fp); return; }
case VIDIOCSAUDIO:
if (!arg) { fputs ("VIDIOCSAUDIO", fp); return; }
 fprint_struct_video_audio (fp, rw, arg);
break;
case VIDIOCGVBIFMT:
if (!arg) { fputs ("VIDIOCGVBIFMT", fp); return; }
case VIDIOCSVBIFMT:
if (!arg) { fputs ("VIDIOCSVBIFMT", fp); return; }
 fprint_struct_vbi_format (fp, rw, arg);
break;
case VIDIOCGPLAYINFO:
if (!arg) { fputs ("VIDIOCGPLAYINFO", fp); return; }
 fprint_struct_video_info (fp, rw, arg);
break;
case VIDIOCGCHAN:
if (!arg) { fputs ("VIDIOCGCHAN", fp); return; }
case VIDIOCSCHAN:
if (!arg) { fputs ("VIDIOCSCHAN", fp); return; }
 fprint_struct_video_channel (fp, rw, arg);
break;
case VIDIOCGTUNER:
if (!arg) { fputs ("VIDIOCGTUNER", fp); return; }
case VIDIOCSTUNER:
if (!arg) { fputs ("VIDIOCSTUNER", fp); return; }
 fprint_struct_video_tuner (fp, rw, arg);
break;
case VIDIOCKEY:
if (!arg) { fputs ("VIDIOCKEY", fp); return; }
 fprint_struct_video_key (fp, rw, arg);
break;
case VIDIOCGCAP:
if (!arg) { fputs ("VIDIOCGCAP", fp); return; }
 fprint_struct_video_capability (fp, rw, arg);
break;
	default:
		if (!arg) { fprint_unknown_ioctl (fp, cmd, arg); return; }
		break;
	}
}

static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGCAP (struct video_capability *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGCHAN (struct video_channel *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSCHAN (const struct video_channel *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGTUNER (struct video_tuner *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSTUNER (const struct video_tuner *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGPICT (struct video_picture *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSPICT (const struct video_picture *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCCAPTURE (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGWIN (struct video_window *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSWIN (const struct video_window *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGFBUF (struct video_buffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSFBUF (const struct video_buffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCKEY (struct video_key *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGFREQ (unsigned long *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSFREQ (const unsigned long *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGAUDIO (struct video_audio *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSAUDIO (const struct video_audio *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSYNC (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCMCAPTURE (const struct video_mmap *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGMBUF (struct video_mbuf *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGUNIT (struct video_unit *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGCAPTURE (struct video_capture *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSCAPTURE (const struct video_capture *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSPLAYMODE (const struct video_play_mode *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSWRITEMODE (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGPLAYINFO (struct video_info *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSMICROCODE (const struct video_code *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGVBIFMT (struct vbi_format *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSVBIFMT (const struct vbi_format *arg __attribute__ ((unused))) {}


/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
