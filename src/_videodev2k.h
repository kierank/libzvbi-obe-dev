/* Generated file, do not edit! */

#include <stdio.h>
#include "io.h"

#ifndef __GNUC__
#undef __attribute__
#define __attribute__(x)
#endif

static void
fprint_enum_v4l2_buf_type (FILE *fp, int rw __attribute__ ((unused)), int value)
{
fprint_symbolic (fp, 1, value,
"VIDEO_CAPTURE", (unsigned long) V4L2_BUF_TYPE_VIDEO_CAPTURE,
"VIDEO_OUTPUT", (unsigned long) V4L2_BUF_TYPE_VIDEO_OUTPUT,
"VIDEO_OVERLAY", (unsigned long) V4L2_BUF_TYPE_VIDEO_OVERLAY,
"VBI_CAPTURE", (unsigned long) V4L2_BUF_TYPE_VBI_CAPTURE,
"VBI_OUTPUT", (unsigned long) V4L2_BUF_TYPE_VBI_OUTPUT,
"SLICED_VBI_CAPTURE", (unsigned long) V4L2_BUF_TYPE_SLICED_VBI_CAPTURE,
"SLICED_VBI_OUTPUT", (unsigned long) V4L2_BUF_TYPE_SLICED_VBI_OUTPUT,
"PRIVATE", (unsigned long) V4L2_BUF_TYPE_PRIVATE,
(void *) 0);
}

static void
fprint_struct_v4l2_rect (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_rect *t)
{
fprintf (fp, "left=%ld "
"top=%ld "
"width=%ld "
"height=%ld ",
(long) t->left, 
(long) t->top, 
(long) t->width, 
(long) t->height);
}

static void
fprint_struct_v4l2_crop (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_crop *t)
{
fputs ("type=", fp);
fprint_enum_v4l2_buf_type (fp, rw, t->type);
fputs (" c={", fp);
fprint_struct_v4l2_rect (fp, rw, &t->c);
fputs ("} ", fp);
}

static void
fprint_struct_v4l2_fmtdesc (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_fmtdesc *t)
{
fprintf (fp, "index=%lu "
"type=",
(unsigned long) t->index);
fprint_enum_v4l2_buf_type (fp, rw, t->type);
fputs (" flags=", fp);
fprint_symbolic (fp, 2, t->flags,
"COMPRESSED", (unsigned long) V4L2_FMT_FLAG_COMPRESSED,
(void *) 0);
fprintf (fp, " description=\"%.*s\" "
"pixelformat=\"%.4s\"=0x%lx "
"reserved[] ",
32, (const char *) t->description, 
(const char *) & t->pixelformat, (unsigned long) t->pixelformat);
}

static void
fprint_enum_v4l2_tuner_type (FILE *fp, int rw __attribute__ ((unused)), int value)
{
fprint_symbolic (fp, 1, value,
"RADIO", (unsigned long) V4L2_TUNER_RADIO,
"ANALOG_TV", (unsigned long) V4L2_TUNER_ANALOG_TV,
"DIGITAL_TV", (unsigned long) V4L2_TUNER_DIGITAL_TV,
(void *) 0);
}

static void
fprint_symbol_v4l2_tuner_cap_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"LOW", (unsigned long) V4L2_TUNER_CAP_LOW,
"NORM", (unsigned long) V4L2_TUNER_CAP_NORM,
"STEREO", (unsigned long) V4L2_TUNER_CAP_STEREO,
"LANG2", (unsigned long) V4L2_TUNER_CAP_LANG2,
"SAP", (unsigned long) V4L2_TUNER_CAP_SAP,
"LANG1", (unsigned long) V4L2_TUNER_CAP_LANG1,
(void *) 0);
}

static void
fprint_symbol_v4l2_tuner_sub_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"MONO", (unsigned long) V4L2_TUNER_SUB_MONO,
"STEREO", (unsigned long) V4L2_TUNER_SUB_STEREO,
"LANG2", (unsigned long) V4L2_TUNER_SUB_LANG2,
"SAP", (unsigned long) V4L2_TUNER_SUB_SAP,
"LANG1", (unsigned long) V4L2_TUNER_SUB_LANG1,
(void *) 0);
}

static void
fprint_symbol_v4l2_tuner_mode_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"MONO", (unsigned long) V4L2_TUNER_MODE_MONO,
"STEREO", (unsigned long) V4L2_TUNER_MODE_STEREO,
"LANG2", (unsigned long) V4L2_TUNER_MODE_LANG2,
"SAP", (unsigned long) V4L2_TUNER_MODE_SAP,
"LANG1", (unsigned long) V4L2_TUNER_MODE_LANG1,
(void *) 0);
}

static void
fprint_struct_v4l2_tuner (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_tuner *t)
{
fprintf (fp, "index=%lu "
"name=\"%.*s\" "
"type=",
(unsigned long) t->index, 
32, (const char *) t->name);
fprint_enum_v4l2_tuner_type (fp, rw, t->type);
fputs (" capability=", fp);
fprint_symbol_v4l2_tuner_cap_ (fp, rw, t->capability);
fprintf (fp, " rangelow=%lu "
"rangehigh=%lu "
"rxsubchans=",
(unsigned long) t->rangelow, 
(unsigned long) t->rangehigh);
fprint_symbol_v4l2_tuner_sub_ (fp, rw, t->rxsubchans);
fputs (" audmode=", fp);
fprint_symbol_v4l2_tuner_mode_ (fp, rw, t->audmode);
fprintf (fp, " signal=%ld "
"afc=%ld "
"reserved[] ",
(long) t->signal, 
(long) t->afc);
}

static void
fprint_symbol_v4l2_cap_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"VIDEO_CAPTURE", (unsigned long) V4L2_CAP_VIDEO_CAPTURE,
"VIDEO_OUTPUT", (unsigned long) V4L2_CAP_VIDEO_OUTPUT,
"VIDEO_OVERLAY", (unsigned long) V4L2_CAP_VIDEO_OVERLAY,
"VBI_CAPTURE", (unsigned long) V4L2_CAP_VBI_CAPTURE,
"VBI_OUTPUT", (unsigned long) V4L2_CAP_VBI_OUTPUT,
"SLICED_VBI_CAPTURE", (unsigned long) V4L2_CAP_SLICED_VBI_CAPTURE,
"SLICED_VBI_OUTPUT", (unsigned long) V4L2_CAP_SLICED_VBI_OUTPUT,
"RDS_CAPTURE", (unsigned long) V4L2_CAP_RDS_CAPTURE,
"TUNER", (unsigned long) V4L2_CAP_TUNER,
"AUDIO", (unsigned long) V4L2_CAP_AUDIO,
"RADIO", (unsigned long) V4L2_CAP_RADIO,
"READWRITE", (unsigned long) V4L2_CAP_READWRITE,
"ASYNCIO", (unsigned long) V4L2_CAP_ASYNCIO,
"STREAMING", (unsigned long) V4L2_CAP_STREAMING,
"TIMEPERFRAME", (unsigned long) V4L2_CAP_TIMEPERFRAME,
(void *) 0);
}

static void
fprint_struct_v4l2_capability (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_capability *t)
{
fprintf (fp, "driver=\"%.*s\" "
"card=\"%.*s\" "
"bus_info=\"%.*s\" "
"version=0x%lx "
"capabilities=",
16, (const char *) t->driver, 
32, (const char *) t->card, 
32, (const char *) t->bus_info, 
(unsigned long) t->version);
fprint_symbol_v4l2_cap_ (fp, rw, t->capabilities);
fputs (" reserved[] ", fp);
}

static void
fprint_symbol_v4l2_cid_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"BASE", (unsigned long) V4L2_CID_BASE,
"PRIVATE_BASE", (unsigned long) V4L2_CID_PRIVATE_BASE,
"BRIGHTNESS", (unsigned long) V4L2_CID_BRIGHTNESS,
"CONTRAST", (unsigned long) V4L2_CID_CONTRAST,
"SATURATION", (unsigned long) V4L2_CID_SATURATION,
"HUE", (unsigned long) V4L2_CID_HUE,
"AUDIO_VOLUME", (unsigned long) V4L2_CID_AUDIO_VOLUME,
"AUDIO_BALANCE", (unsigned long) V4L2_CID_AUDIO_BALANCE,
"AUDIO_BASS", (unsigned long) V4L2_CID_AUDIO_BASS,
"AUDIO_TREBLE", (unsigned long) V4L2_CID_AUDIO_TREBLE,
"AUDIO_MUTE", (unsigned long) V4L2_CID_AUDIO_MUTE,
"AUDIO_LOUDNESS", (unsigned long) V4L2_CID_AUDIO_LOUDNESS,
"BLACK_LEVEL", (unsigned long) V4L2_CID_BLACK_LEVEL,
"AUTO_WHITE_BALANCE", (unsigned long) V4L2_CID_AUTO_WHITE_BALANCE,
"DO_WHITE_BALANCE", (unsigned long) V4L2_CID_DO_WHITE_BALANCE,
"RED_BALANCE", (unsigned long) V4L2_CID_RED_BALANCE,
"BLUE_BALANCE", (unsigned long) V4L2_CID_BLUE_BALANCE,
"GAMMA", (unsigned long) V4L2_CID_GAMMA,
"WHITENESS", (unsigned long) V4L2_CID_WHITENESS,
"EXPOSURE", (unsigned long) V4L2_CID_EXPOSURE,
"AUTOGAIN", (unsigned long) V4L2_CID_AUTOGAIN,
"GAIN", (unsigned long) V4L2_CID_GAIN,
"HFLIP", (unsigned long) V4L2_CID_HFLIP,
"VFLIP", (unsigned long) V4L2_CID_VFLIP,
"HCENTER", (unsigned long) V4L2_CID_HCENTER,
"VCENTER", (unsigned long) V4L2_CID_VCENTER,
"LASTP1", (unsigned long) V4L2_CID_LASTP1,
(void *) 0);
}

static void
fprint_enum_v4l2_ctrl_type (FILE *fp, int rw __attribute__ ((unused)), int value)
{
fprint_symbolic (fp, 1, value,
"INTEGER", (unsigned long) V4L2_CTRL_TYPE_INTEGER,
"BOOLEAN", (unsigned long) V4L2_CTRL_TYPE_BOOLEAN,
"MENU", (unsigned long) V4L2_CTRL_TYPE_MENU,
"BUTTON", (unsigned long) V4L2_CTRL_TYPE_BUTTON,
(void *) 0);
}

static void
fprint_struct_v4l2_queryctrl (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_queryctrl *t)
{
fputs ("id=", fp);
fprint_symbol_v4l2_cid_ (fp, rw, t->id);
fputs (" type=", fp);
fprint_enum_v4l2_ctrl_type (fp, rw, t->type);
fprintf (fp, " name=\"%.*s\" "
"minimum=%ld "
"maximum=%ld "
"step=%ld "
"default_value=%ld "
"flags=",
32, (const char *) t->name, 
(long) t->minimum, 
(long) t->maximum, 
(long) t->step, 
(long) t->default_value);
fprint_symbolic (fp, 2, t->flags,
"DISABLED", (unsigned long) V4L2_CTRL_FLAG_DISABLED,
"GRABBED", (unsigned long) V4L2_CTRL_FLAG_GRABBED,
(void *) 0);
fputs (" reserved[] ", fp);
}

static void
fprint_struct_v4l2_modulator (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_modulator *t)
{
fprintf (fp, "index=%lu "
"name=\"%.*s\" "
"capability=",
(unsigned long) t->index, 
32, (const char *) t->name);
fprint_symbol_v4l2_tuner_cap_ (fp, rw, t->capability);
fprintf (fp, " rangelow=%lu "
"rangehigh=%lu "
"txsubchans=",
(unsigned long) t->rangelow, 
(unsigned long) t->rangehigh);
fprint_symbol_v4l2_tuner_sub_ (fp, rw, t->txsubchans);
fputs (" reserved[] ", fp);
}

static void
fprint_symbol_v4l2_std_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"PAL_B", (unsigned long) V4L2_STD_PAL_B,
"PAL_B1", (unsigned long) V4L2_STD_PAL_B1,
"PAL_G", (unsigned long) V4L2_STD_PAL_G,
"PAL_H", (unsigned long) V4L2_STD_PAL_H,
"PAL_I", (unsigned long) V4L2_STD_PAL_I,
"PAL_D", (unsigned long) V4L2_STD_PAL_D,
"PAL_D1", (unsigned long) V4L2_STD_PAL_D1,
"PAL_K", (unsigned long) V4L2_STD_PAL_K,
"PAL_M", (unsigned long) V4L2_STD_PAL_M,
"PAL_N", (unsigned long) V4L2_STD_PAL_N,
"PAL_Nc", (unsigned long) V4L2_STD_PAL_Nc,
"PAL_60", (unsigned long) V4L2_STD_PAL_60,
"NTSC_M", (unsigned long) V4L2_STD_NTSC_M,
"NTSC_M_JP", (unsigned long) V4L2_STD_NTSC_M_JP,
"SECAM_B", (unsigned long) V4L2_STD_SECAM_B,
"SECAM_D", (unsigned long) V4L2_STD_SECAM_D,
"SECAM_G", (unsigned long) V4L2_STD_SECAM_G,
"SECAM_H", (unsigned long) V4L2_STD_SECAM_H,
"SECAM_K", (unsigned long) V4L2_STD_SECAM_K,
"SECAM_K1", (unsigned long) V4L2_STD_SECAM_K1,
"SECAM_L", (unsigned long) V4L2_STD_SECAM_L,
"ATSC_8_VSB", (unsigned long) V4L2_STD_ATSC_8_VSB,
"ATSC_16_VSB", (unsigned long) V4L2_STD_ATSC_16_VSB,
"PAL_BG", (unsigned long) V4L2_STD_PAL_BG,
"PAL_DK", (unsigned long) V4L2_STD_PAL_DK,
"PAL", (unsigned long) V4L2_STD_PAL,
"NTSC", (unsigned long) V4L2_STD_NTSC,
"SECAM_DK", (unsigned long) V4L2_STD_SECAM_DK,
"SECAM", (unsigned long) V4L2_STD_SECAM,
"525_60", (unsigned long) V4L2_STD_525_60,
"625_50", (unsigned long) V4L2_STD_625_50,
"ATSC", (unsigned long) V4L2_STD_ATSC,
"UNKNOWN", (unsigned long) V4L2_STD_UNKNOWN,
"ALL", (unsigned long) V4L2_STD_ALL,
(void *) 0);
}

static void
fprint_symbol_v4l2_in_st_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"NO_POWER", (unsigned long) V4L2_IN_ST_NO_POWER,
"NO_SIGNAL", (unsigned long) V4L2_IN_ST_NO_SIGNAL,
"NO_COLOR", (unsigned long) V4L2_IN_ST_NO_COLOR,
"NO_H_LOCK", (unsigned long) V4L2_IN_ST_NO_H_LOCK,
"COLOR_KILL", (unsigned long) V4L2_IN_ST_COLOR_KILL,
"NO_SYNC", (unsigned long) V4L2_IN_ST_NO_SYNC,
"NO_EQU", (unsigned long) V4L2_IN_ST_NO_EQU,
"NO_CARRIER", (unsigned long) V4L2_IN_ST_NO_CARRIER,
"MACROVISION", (unsigned long) V4L2_IN_ST_MACROVISION,
"NO_ACCESS", (unsigned long) V4L2_IN_ST_NO_ACCESS,
"VTR", (unsigned long) V4L2_IN_ST_VTR,
(void *) 0);
}

static void
fprint_struct_v4l2_input (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_input *t)
{
fprintf (fp, "index=%lu "
"name=\"%.*s\" "
"type=",
(unsigned long) t->index, 
32, (const char *) t->name);
fprint_symbolic (fp, 0, t->type,
"TUNER", (unsigned long) V4L2_INPUT_TYPE_TUNER,
"CAMERA", (unsigned long) V4L2_INPUT_TYPE_CAMERA,
(void *) 0);
fprintf (fp, " audioset=%lu "
"tuner=%lu "
"std=",
(unsigned long) t->audioset, 
(unsigned long) t->tuner);
fprint_symbol_v4l2_std_ (fp, rw, t->std);
fputs (" status=", fp);
fprint_symbol_v4l2_in_st_ (fp, rw, t->status);
fputs (" reserved[] ", fp);
}

static void
fprint_enum_v4l2_mpeg_streamtype (FILE *fp, int rw __attribute__ ((unused)), int value)
{
fprint_symbolic (fp, 1, value,
(void *) 0);
}

static void
fprint_enum_v4l2_bitrate_mode (FILE *fp, int rw __attribute__ ((unused)), int value)
{
fprint_symbolic (fp, 1, value,
"", (unsigned long) V4L2_BITRATE_NONE,
(void *) 0);
}

static void
fprint_struct_v4l2_bitrate (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_bitrate *t)
{
fputs ("mode=", fp);
fprint_enum_v4l2_bitrate_mode (fp, rw, t->mode);
fprintf (fp, " min=%lu "
"target=%lu "
"max=%lu ",
(unsigned long) t->min, 
(unsigned long) t->target, 
(unsigned long) t->max);
}

static void
fprint_enum_v4l2_mpeg_audiotype (FILE *fp, int rw __attribute__ ((unused)), int value)
{
fprint_symbolic (fp, 1, value,
(void *) 0);
}

static void
fprint_enum_v4l2_mpeg_videotype (FILE *fp, int rw __attribute__ ((unused)), int value)
{
fprint_symbolic (fp, 1, value,
(void *) 0);
}

static void
fprint_enum_v4l2_mpeg_aspectratio (FILE *fp, int rw __attribute__ ((unused)), int value)
{
fprint_symbolic (fp, 1, value,
"SQUARE", (unsigned long) V4L2_MPEG_ASPECT_SQUARE,
"4_3", (unsigned long) V4L2_MPEG_ASPECT_4_3,
"16_9", (unsigned long) V4L2_MPEG_ASPECT_16_9,
"1_221", (unsigned long) V4L2_MPEG_ASPECT_1_221,
(void *) 0);
}

static void
fprint_struct_v4l2_mpeg_compression (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_mpeg_compression *t)
{
fputs ("st_type=", fp);
fprint_enum_v4l2_mpeg_streamtype (fp, rw, t->st_type);
fputs (" st_bitrate={", fp);
fprint_struct_v4l2_bitrate (fp, rw, &t->st_bitrate);
fprintf (fp, "} ts_pid_pmt=%lu "
"ts_pid_audio=%lu "
"ts_pid_video=%lu "
"ts_pid_pcr=%lu "
"ps_size=%lu "
"reserved_1 "
"au_type=",
(unsigned long) t->ts_pid_pmt, 
(unsigned long) t->ts_pid_audio, 
(unsigned long) t->ts_pid_video, 
(unsigned long) t->ts_pid_pcr, 
(unsigned long) t->ps_size);
fprint_enum_v4l2_mpeg_audiotype (fp, rw, t->au_type);
fputs (" au_bitrate={", fp);
fprint_struct_v4l2_bitrate (fp, rw, &t->au_bitrate);
fprintf (fp, "} au_sample_rate=%lu "
"au_pesid=%lu "
"reserved_2[] "
"vi_type=",
(unsigned long) t->au_sample_rate, 
(unsigned long) t->au_pesid);
fprint_enum_v4l2_mpeg_videotype (fp, rw, t->vi_type);
fputs (" vi_aspect_ratio=", fp);
fprint_enum_v4l2_mpeg_aspectratio (fp, rw, t->vi_aspect_ratio);
fputs (" vi_bitrate={", fp);
fprint_struct_v4l2_bitrate (fp, rw, &t->vi_bitrate);
fprintf (fp, "} vi_frame_rate=%lu "
"vi_frames_per_gop=%lu "
"vi_bframes_count=%lu "
"vi_pesid=%lu "
"reserved_3[] "
"reserved_5[] ",
(unsigned long) t->vi_frame_rate, 
(unsigned long) t->vi_frames_per_gop, 
(unsigned long) t->vi_bframes_count, 
(unsigned long) t->vi_pesid);
}

static void
fprint_struct_v4l2_sliced_vbi_cap (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_sliced_vbi_cap *t)
{
fprintf (fp, "service_set=%lu "
"reserved[] ",
(unsigned long) t->service_set);
}

static void
fprint_symbol_v4l2_pix_fmt_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"RGB332", (unsigned long) V4L2_PIX_FMT_RGB332,
"RGB555", (unsigned long) V4L2_PIX_FMT_RGB555,
"RGB565", (unsigned long) V4L2_PIX_FMT_RGB565,
"RGB555X", (unsigned long) V4L2_PIX_FMT_RGB555X,
"RGB565X", (unsigned long) V4L2_PIX_FMT_RGB565X,
"BGR24", (unsigned long) V4L2_PIX_FMT_BGR24,
"RGB24", (unsigned long) V4L2_PIX_FMT_RGB24,
"BGR32", (unsigned long) V4L2_PIX_FMT_BGR32,
"RGB32", (unsigned long) V4L2_PIX_FMT_RGB32,
"GREY", (unsigned long) V4L2_PIX_FMT_GREY,
"YVU410", (unsigned long) V4L2_PIX_FMT_YVU410,
"YVU420", (unsigned long) V4L2_PIX_FMT_YVU420,
"YUYV", (unsigned long) V4L2_PIX_FMT_YUYV,
"UYVY", (unsigned long) V4L2_PIX_FMT_UYVY,
"YUV422P", (unsigned long) V4L2_PIX_FMT_YUV422P,
"YUV411P", (unsigned long) V4L2_PIX_FMT_YUV411P,
"Y41P", (unsigned long) V4L2_PIX_FMT_Y41P,
"NV12", (unsigned long) V4L2_PIX_FMT_NV12,
"NV21", (unsigned long) V4L2_PIX_FMT_NV21,
"YUV410", (unsigned long) V4L2_PIX_FMT_YUV410,
"YUV420", (unsigned long) V4L2_PIX_FMT_YUV420,
"YYUV", (unsigned long) V4L2_PIX_FMT_YYUV,
"HI240", (unsigned long) V4L2_PIX_FMT_HI240,
"SBGGR8", (unsigned long) V4L2_PIX_FMT_SBGGR8,
"MJPEG", (unsigned long) V4L2_PIX_FMT_MJPEG,
"JPEG", (unsigned long) V4L2_PIX_FMT_JPEG,
"DV", (unsigned long) V4L2_PIX_FMT_DV,
"MPEG", (unsigned long) V4L2_PIX_FMT_MPEG,
"WNVA", (unsigned long) V4L2_PIX_FMT_WNVA,
"SN9C10X", (unsigned long) V4L2_PIX_FMT_SN9C10X,
"PWC1", (unsigned long) V4L2_PIX_FMT_PWC1,
"PWC2", (unsigned long) V4L2_PIX_FMT_PWC2,
(void *) 0);
}

static void
fprint_enum_v4l2_field (FILE *fp, int rw __attribute__ ((unused)), int value)
{
fprint_symbolic (fp, 1, value,
"ANY", (unsigned long) V4L2_FIELD_ANY,
"NONE", (unsigned long) V4L2_FIELD_NONE,
"TOP", (unsigned long) V4L2_FIELD_TOP,
"BOTTOM", (unsigned long) V4L2_FIELD_BOTTOM,
"INTERLACED", (unsigned long) V4L2_FIELD_INTERLACED,
"SEQ_TB", (unsigned long) V4L2_FIELD_SEQ_TB,
"SEQ_BT", (unsigned long) V4L2_FIELD_SEQ_BT,
"ALTERNATE", (unsigned long) V4L2_FIELD_ALTERNATE,
(void *) 0);
}

static void
fprint_enum_v4l2_colorspace (FILE *fp, int rw __attribute__ ((unused)), int value)
{
fprint_symbolic (fp, 1, value,
"SMPTE170M", (unsigned long) V4L2_COLORSPACE_SMPTE170M,
"SMPTE240M", (unsigned long) V4L2_COLORSPACE_SMPTE240M,
"REC709", (unsigned long) V4L2_COLORSPACE_REC709,
"BT878", (unsigned long) V4L2_COLORSPACE_BT878,
"470_SYSTEM_M", (unsigned long) V4L2_COLORSPACE_470_SYSTEM_M,
"470_SYSTEM_BG", (unsigned long) V4L2_COLORSPACE_470_SYSTEM_BG,
"JPEG", (unsigned long) V4L2_COLORSPACE_JPEG,
"SRGB", (unsigned long) V4L2_COLORSPACE_SRGB,
(void *) 0);
}

static void
fprint_struct_v4l2_pix_format (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_pix_format *t)
{
fprintf (fp, "width=%lu "
"height=%lu "
"pixelformat=",
(unsigned long) t->width, 
(unsigned long) t->height);
fprint_symbol_v4l2_pix_fmt_ (fp, rw, t->pixelformat);
fputs (" field=", fp);
fprint_enum_v4l2_field (fp, rw, t->field);
fprintf (fp, " bytesperline=%lu "
"sizeimage=%lu "
"colorspace=",
(unsigned long) t->bytesperline, 
(unsigned long) t->sizeimage);
fprint_enum_v4l2_colorspace (fp, rw, t->colorspace);
fprintf (fp, " priv=%lu ",
(unsigned long) t->priv);
}

static void
fprint_struct_v4l2_window (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_window *t)
{
fputs ("w={", fp);
fprint_struct_v4l2_rect (fp, rw, &t->w);
fputs ("} field=", fp);
fprint_enum_v4l2_field (fp, rw, t->field);
fprintf (fp, " chromakey=%lu "
"clips=%p "
"clipcount=%lu "
"bitmap=%p ",
(unsigned long) t->chromakey, 
(const void *) t->clips, 
(unsigned long) t->clipcount, 
(const void *) t->bitmap);
}

static void
fprint_struct_v4l2_vbi_format (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_vbi_format *t)
{
fprintf (fp, "sampling_rate=%lu "
"offset=%lu "
"samples_per_line=%lu "
"sample_format=",
(unsigned long) t->sampling_rate, 
(unsigned long) t->offset, 
(unsigned long) t->samples_per_line);
fprint_symbol_v4l2_pix_fmt_ (fp, rw, t->sample_format);
fputs (" start[]=? "
"count[]=? "
"flags=", fp);
fprint_symbolic (fp, 2, t->flags,
"UNSYNC", (unsigned long) V4L2_VBI_UNSYNC,
"INTERLACED", (unsigned long) V4L2_VBI_INTERLACED,
(void *) 0);
fputs (" reserved[] ", fp);
}

static void
fprint_struct_v4l2_sliced_vbi_format (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_sliced_vbi_format *t)
{
fprintf (fp, "service_set=%lu "
"io_size=%lu "
"reserved[] ",
(unsigned long) t->service_set, 
(unsigned long) t->io_size);
}

static void
fprint_struct_v4l2_format (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_format *t)
{
fputs ("type=", fp);
fprint_enum_v4l2_buf_type (fp, rw, t->type);
fputs (" ", fp);
fputs ("fmt={", fp);
if (V4L2_BUF_TYPE_VIDEO_CAPTURE == t->type) {
fputs ("pix={", fp);
fprint_struct_v4l2_pix_format (fp, rw, &t->fmt.pix);
fputs ("} ", fp);
}
if (V4L2_BUF_TYPE_VIDEO_OVERLAY == t->type) {
fputs ("win={", fp);
fprint_struct_v4l2_window (fp, rw, &t->fmt.win);
fputs ("} ", fp);
}
if (V4L2_BUF_TYPE_VBI_CAPTURE == t->type) {
fputs ("vbi={", fp);
fprint_struct_v4l2_vbi_format (fp, rw, &t->fmt.vbi);
fputs ("} ", fp);
}
fputs ("sliced={", fp);
fprint_struct_v4l2_sliced_vbi_format (fp, rw, &t->fmt.sliced);
fputs ("} raw_data[]=? ", fp);
fputs ("} ", fp);
}

static void
fprint_symbol_v4l2_buf_flag_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 2, value,
"MAPPED", (unsigned long) V4L2_BUF_FLAG_MAPPED,
"QUEUED", (unsigned long) V4L2_BUF_FLAG_QUEUED,
"DONE", (unsigned long) V4L2_BUF_FLAG_DONE,
"KEYFRAME", (unsigned long) V4L2_BUF_FLAG_KEYFRAME,
"PFRAME", (unsigned long) V4L2_BUF_FLAG_PFRAME,
"BFRAME", (unsigned long) V4L2_BUF_FLAG_BFRAME,
"TIMECODE", (unsigned long) V4L2_BUF_FLAG_TIMECODE,
"INPUT", (unsigned long) V4L2_BUF_FLAG_INPUT,
(void *) 0);
}

static void
fprint_symbol_v4l2_tc_type_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"24FPS", (unsigned long) V4L2_TC_TYPE_24FPS,
"25FPS", (unsigned long) V4L2_TC_TYPE_25FPS,
"30FPS", (unsigned long) V4L2_TC_TYPE_30FPS,
"50FPS", (unsigned long) V4L2_TC_TYPE_50FPS,
"60FPS", (unsigned long) V4L2_TC_TYPE_60FPS,
(void *) 0);
}

static void
fprint_struct_v4l2_timecode (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_timecode *t)
{
fputs ("type=", fp);
fprint_symbol_v4l2_tc_type_ (fp, rw, t->type);
fputs (" flags=", fp);
fprint_symbolic (fp, 2, t->flags,
"DROPFRAME", (unsigned long) V4L2_TC_FLAG_DROPFRAME,
"COLORFRAME", (unsigned long) V4L2_TC_FLAG_COLORFRAME,
(void *) 0);
fprintf (fp, " frames=%lu "
"seconds=%lu "
"minutes=%lu "
"hours=%lu "
"userbits[]=? ",
(unsigned long) t->frames, 
(unsigned long) t->seconds, 
(unsigned long) t->minutes, 
(unsigned long) t->hours);
}

static void
fprint_enum_v4l2_memory (FILE *fp, int rw __attribute__ ((unused)), int value)
{
fprint_symbolic (fp, 1, value,
"MMAP", (unsigned long) V4L2_MEMORY_MMAP,
"USERPTR", (unsigned long) V4L2_MEMORY_USERPTR,
"OVERLAY", (unsigned long) V4L2_MEMORY_OVERLAY,
(void *) 0);
}

static void
fprint_struct_v4l2_buffer (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_buffer *t)
{
fprintf (fp, "index=%lu "
"type=",
(unsigned long) t->index);
fprint_enum_v4l2_buf_type (fp, rw, t->type);
fprintf (fp, " bytesused=%lu "
"flags=",
(unsigned long) t->bytesused);
fprint_symbol_v4l2_buf_flag_ (fp, rw, t->flags);
fputs (" field=", fp);
fprint_enum_v4l2_field (fp, rw, t->field);
fputs (" timestamp=? timecode={", fp);
fprint_struct_v4l2_timecode (fp, rw, &t->timecode);
fprintf (fp, "} sequence=%lu "
"memory=",
(unsigned long) t->sequence);
fprint_enum_v4l2_memory (fp, rw, t->memory);
fputs (" ", fp);
fputs ("m={", fp);
if (V4L2_MEMORY_MMAP == t->memory) {
fprintf (fp, "offset=%lu ",
(unsigned long) t->m.offset);
}
if (V4L2_MEMORY_USERPTR == t->memory) {
fprintf (fp, "userptr=%lu ",
(unsigned long) t->m.userptr);
}
fprintf (fp, "} length=%lu "
"input=%lu "
"reserved ",
(unsigned long) t->length, 
(unsigned long) t->input);
}

static void
fprint_struct_v4l2_control (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_control *t)
{
fputs ("id=", fp);
fprint_symbol_v4l2_cid_ (fp, rw, t->id);
fprintf (fp, " value=%ld ",
(long) t->value);
}

static void
fprint_struct_v4l2_fract (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_fract *t)
{
 fprintf (fp, "%u/%u", t->numerator, t->denominator); 
}

static void
fprint_struct_v4l2_captureparm (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_captureparm *t)
{
fputs ("capability=", fp);
fprint_symbol_v4l2_cap_ (fp, rw, t->capability);
fputs (" capturemode=", fp);
fprint_symbolic (fp, 0, t->capturemode,
"HIGHQUALITY", (unsigned long) V4L2_MODE_HIGHQUALITY,
(void *) 0);
fputs (" timeperframe={", fp);
fprint_struct_v4l2_fract (fp, rw, &t->timeperframe);
fprintf (fp, "} extendedmode=%lu "
"readbuffers=%lu "
"reserved[] ",
(unsigned long) t->extendedmode, 
(unsigned long) t->readbuffers);
}

static void
fprint_struct_v4l2_outputparm (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_outputparm *t)
{
fprintf (fp, "capability=%lu "
"outputmode=%lu "
"timeperframe={",
(unsigned long) t->capability, 
(unsigned long) t->outputmode);
fprint_struct_v4l2_fract (fp, rw, &t->timeperframe);
fprintf (fp, "} extendedmode=%lu "
"writebuffers=%lu "
"reserved[] ",
(unsigned long) t->extendedmode, 
(unsigned long) t->writebuffers);
}

static void
fprint_struct_v4l2_streamparm (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_streamparm *t)
{
fputs ("type=", fp);
fprint_enum_v4l2_buf_type (fp, rw, t->type);
fputs (" ", fp);
fputs ("parm={capture={", fp);
fprint_struct_v4l2_captureparm (fp, rw, &t->parm.capture);
fputs ("} output={", fp);
fprint_struct_v4l2_outputparm (fp, rw, &t->parm.output);
fputs ("} raw_data[]=? ", fp);
fputs ("} ", fp);
}

static void
fprint_struct_v4l2_frequency (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_frequency *t)
{
fprintf (fp, "tuner=%lu "
"type=",
(unsigned long) t->tuner);
fprint_enum_v4l2_tuner_type (fp, rw, t->type);
fprintf (fp, " frequency=%lu "
"reserved[] ",
(unsigned long) t->frequency);
}

static void
fprint_struct_v4l2_querymenu (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_querymenu *t)
{
fputs ("id=", fp);
fprint_symbol_v4l2_cid_ (fp, rw, t->id);
fputs (" index=", fp);
fprint_symbol_v4l2_cid_ (fp, rw, t->index);
fprintf (fp, " name=\"%.*s\" "
"reserved ",
32, (const char *) t->name);
}

static void
fprint_symbol_v4l2_jpeg_marker_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"DHT", (unsigned long) V4L2_JPEG_MARKER_DHT,
"DQT", (unsigned long) V4L2_JPEG_MARKER_DQT,
"DRI", (unsigned long) V4L2_JPEG_MARKER_DRI,
"COM", (unsigned long) V4L2_JPEG_MARKER_COM,
"APP", (unsigned long) V4L2_JPEG_MARKER_APP,
(void *) 0);
}

static void
fprint_struct_v4l2_jpegcompression (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_jpegcompression *t)
{
fprintf (fp, "quality=%ld "
"APPn=%ld "
"APP_len=%ld "
"APP_data=\"%.*s\" "
"COM_len=%ld "
"COM_data=\"%.*s\" "
"jpeg_markers=",
(long) t->quality, 
(long) t->APPn, 
(long) t->APP_len, 
60, (const char *) t->APP_data, 
(long) t->COM_len, 
60, (const char *) t->COM_data);
fprint_symbol_v4l2_jpeg_marker_ (fp, rw, t->jpeg_markers);
fputs (" ", fp);
}

static void
fprint_struct_v4l2_audioout (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_audioout *t)
{
fprintf (fp, "index=%lu "
"name=\"%.*s\" "
"capability=%lu "
"mode=%lu "
"reserved[] ",
(unsigned long) t->index, 
32, (const char *) t->name, 
(unsigned long) t->capability, 
(unsigned long) t->mode);
}

static void
fprint_struct_v4l2_requestbuffers (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_requestbuffers *t)
{
fprintf (fp, "count=%lu "
"type=",
(unsigned long) t->count);
fprint_enum_v4l2_buf_type (fp, rw, t->type);
fputs (" memory=", fp);
fprint_enum_v4l2_memory (fp, rw, t->memory);
fputs (" reserved[] ", fp);
}

static void
fprint_struct_v4l2_cropcap (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_cropcap *t)
{
fputs ("type=", fp);
fprint_enum_v4l2_buf_type (fp, rw, t->type);
fputs (" bounds={", fp);
fprint_struct_v4l2_rect (fp, rw, &t->bounds);
fputs ("} defrect={", fp);
fprint_struct_v4l2_rect (fp, rw, &t->defrect);
fputs ("} pixelaspect={", fp);
fprint_struct_v4l2_fract (fp, rw, &t->pixelaspect);
fputs ("} ", fp);
}

static void
fprint_struct_v4l2_audio (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_audio *t)
{
fprintf (fp, "index=%lu "
"name=\"%.*s\" "
"capability=",
(unsigned long) t->index, 
32, (const char *) t->name);
fprint_symbolic (fp, 0, t->capability,
"STEREO", (unsigned long) V4L2_AUDCAP_STEREO,
"AVL", (unsigned long) V4L2_AUDCAP_AVL,
(void *) 0);
fputs (" mode=", fp);
fprint_symbolic (fp, 0, t->mode,
"AVL", (unsigned long) V4L2_AUDMODE_AVL,
(void *) 0);
fputs (" reserved[] ", fp);
}

static void
fprint_struct_v4l2_output (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_output *t)
{
fprintf (fp, "index=%lu "
"name=\"%.*s\" "
"type=",
(unsigned long) t->index, 
32, (const char *) t->name);
fprint_symbolic (fp, 0, t->type,
"MODULATOR", (unsigned long) V4L2_OUTPUT_TYPE_MODULATOR,
"ANALOG", (unsigned long) V4L2_OUTPUT_TYPE_ANALOG,
"ANALOGVGAOVERLAY", (unsigned long) V4L2_OUTPUT_TYPE_ANALOGVGAOVERLAY,
(void *) 0);
fprintf (fp, " audioset=%lu "
"modulator=%lu "
"std=",
(unsigned long) t->audioset, 
(unsigned long) t->modulator);
fprint_symbol_v4l2_std_ (fp, rw, t->std);
fputs (" reserved[] ", fp);
}

static void
fprint_symbol_v4l2_fbuf_cap_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"EXTERNOVERLAY", (unsigned long) V4L2_FBUF_CAP_EXTERNOVERLAY,
"CHROMAKEY", (unsigned long) V4L2_FBUF_CAP_CHROMAKEY,
"LIST_CLIPPING", (unsigned long) V4L2_FBUF_CAP_LIST_CLIPPING,
"BITMAP_CLIPPING", (unsigned long) V4L2_FBUF_CAP_BITMAP_CLIPPING,
(void *) 0);
}

static void
fprint_struct_v4l2_framebuffer (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_framebuffer *t)
{
fputs ("capability=", fp);
fprint_symbol_v4l2_fbuf_cap_ (fp, rw, t->capability);
fputs (" flags=", fp);
fprint_symbolic (fp, 2, t->flags,
"PRIMARY", (unsigned long) V4L2_FBUF_FLAG_PRIMARY,
"OVERLAY", (unsigned long) V4L2_FBUF_FLAG_OVERLAY,
"CHROMAKEY", (unsigned long) V4L2_FBUF_FLAG_CHROMAKEY,
(void *) 0);
fputs (" base=? "
"fmt={", fp);
fprint_struct_v4l2_pix_format (fp, rw, &t->fmt);
fputs ("} ", fp);
}

static void
fprint_struct_v4l2_standard (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_standard *t)
{
fprintf (fp, "index=%lu "
"id=",
(unsigned long) t->index);
fprint_symbol_v4l2_std_ (fp, rw, t->id);
fprintf (fp, " name=\"%.*s\" "
"frameperiod={",
24, (const char *) t->name);
fprint_struct_v4l2_fract (fp, rw, &t->frameperiod);
fprintf (fp, "} framelines=%lu "
"reserved[] ",
(unsigned long) t->framelines);
}

static void
fprint_ioctl_arg (FILE *fp, unsigned int cmd, int rw, void *arg)
{
switch (cmd) {
case VIDIOC_G_CROP:
if (!arg) { fputs ("VIDIOC_G_CROP", fp); return; }
case VIDIOC_S_CROP:
if (!arg) { fputs ("VIDIOC_S_CROP", fp); return; }
 fprint_struct_v4l2_crop (fp, rw, arg);
break;
case VIDIOC_OVERLAY:
if (!arg) { fputs ("VIDIOC_OVERLAY", fp); return; }
case VIDIOC_STREAMON:
if (!arg) { fputs ("VIDIOC_STREAMON", fp); return; }
case VIDIOC_STREAMOFF:
if (!arg) { fputs ("VIDIOC_STREAMOFF", fp); return; }
case VIDIOC_G_INPUT:
if (!arg) { fputs ("VIDIOC_G_INPUT", fp); return; }
case VIDIOC_S_INPUT:
if (!arg) { fputs ("VIDIOC_S_INPUT", fp); return; }
case VIDIOC_G_OUTPUT:
if (!arg) { fputs ("VIDIOC_G_OUTPUT", fp); return; }
case VIDIOC_S_OUTPUT:
if (!arg) { fputs ("VIDIOC_S_OUTPUT", fp); return; }
case VIDIOC_OVERLAY_OLD:
if (!arg) { fputs ("VIDIOC_OVERLAY_OLD", fp); return; }
 fprintf (fp, "%ld", (long) * (int *) arg);
break;
case VIDIOC_ENUM_FMT:
if (!arg) { fputs ("VIDIOC_ENUM_FMT", fp); return; }
 fprint_struct_v4l2_fmtdesc (fp, rw, arg);
break;
case VIDIOC_G_TUNER:
if (!arg) { fputs ("VIDIOC_G_TUNER", fp); return; }
case VIDIOC_S_TUNER:
if (!arg) { fputs ("VIDIOC_S_TUNER", fp); return; }
 fprint_struct_v4l2_tuner (fp, rw, arg);
break;
case VIDIOC_QUERYCAP:
if (!arg) { fputs ("VIDIOC_QUERYCAP", fp); return; }
 fprint_struct_v4l2_capability (fp, rw, arg);
break;
case VIDIOC_QUERYCTRL:
if (!arg) { fputs ("VIDIOC_QUERYCTRL", fp); return; }
 fprint_struct_v4l2_queryctrl (fp, rw, arg);
break;
case VIDIOC_G_MODULATOR:
if (!arg) { fputs ("VIDIOC_G_MODULATOR", fp); return; }
case VIDIOC_S_MODULATOR:
if (!arg) { fputs ("VIDIOC_S_MODULATOR", fp); return; }
 fprint_struct_v4l2_modulator (fp, rw, arg);
break;
case VIDIOC_ENUMINPUT:
if (!arg) { fputs ("VIDIOC_ENUMINPUT", fp); return; }
 fprint_struct_v4l2_input (fp, rw, arg);
break;
case VIDIOC_G_MPEGCOMP:
if (!arg) { fputs ("VIDIOC_G_MPEGCOMP", fp); return; }
case VIDIOC_S_MPEGCOMP:
if (!arg) { fputs ("VIDIOC_S_MPEGCOMP", fp); return; }
 fprint_struct_v4l2_mpeg_compression (fp, rw, arg);
break;
case VIDIOC_G_SLICED_VBI_CAP:
if (!arg) { fputs ("VIDIOC_G_SLICED_VBI_CAP", fp); return; }
 fprint_struct_v4l2_sliced_vbi_cap (fp, rw, arg);
break;
case VIDIOC_G_FMT:
if (!arg) { fputs ("VIDIOC_G_FMT", fp); return; }
case VIDIOC_S_FMT:
if (!arg) { fputs ("VIDIOC_S_FMT", fp); return; }
case VIDIOC_TRY_FMT:
if (!arg) { fputs ("VIDIOC_TRY_FMT", fp); return; }
 fprint_struct_v4l2_format (fp, rw, arg);
break;
case VIDIOC_QUERYBUF:
if (!arg) { fputs ("VIDIOC_QUERYBUF", fp); return; }
case VIDIOC_QBUF:
if (!arg) { fputs ("VIDIOC_QBUF", fp); return; }
case VIDIOC_DQBUF:
if (!arg) { fputs ("VIDIOC_DQBUF", fp); return; }
 fprint_struct_v4l2_buffer (fp, rw, arg);
break;
case VIDIOC_G_CTRL:
if (!arg) { fputs ("VIDIOC_G_CTRL", fp); return; }
case VIDIOC_S_CTRL:
if (!arg) { fputs ("VIDIOC_S_CTRL", fp); return; }
case VIDIOC_S_CTRL_OLD:
if (!arg) { fputs ("VIDIOC_S_CTRL_OLD", fp); return; }
 fprint_struct_v4l2_control (fp, rw, arg);
break;
case VIDIOC_G_STD:
if (!arg) { fputs ("VIDIOC_G_STD", fp); return; }
case VIDIOC_S_STD:
if (!arg) { fputs ("VIDIOC_S_STD", fp); return; }
case VIDIOC_QUERYSTD:
if (!arg) { fputs ("VIDIOC_QUERYSTD", fp); return; }
 fprint_symbol_v4l2_std_ (fp, rw, * (__u64 *) arg);
break;
case VIDIOC_G_PARM:
if (!arg) { fputs ("VIDIOC_G_PARM", fp); return; }
case VIDIOC_S_PARM:
if (!arg) { fputs ("VIDIOC_S_PARM", fp); return; }
case VIDIOC_S_PARM_OLD:
if (!arg) { fputs ("VIDIOC_S_PARM_OLD", fp); return; }
 fprint_struct_v4l2_streamparm (fp, rw, arg);
break;
case VIDIOC_G_FREQUENCY:
if (!arg) { fputs ("VIDIOC_G_FREQUENCY", fp); return; }
case VIDIOC_S_FREQUENCY:
if (!arg) { fputs ("VIDIOC_S_FREQUENCY", fp); return; }
 fprint_struct_v4l2_frequency (fp, rw, arg);
break;
case VIDIOC_QUERYMENU:
if (!arg) { fputs ("VIDIOC_QUERYMENU", fp); return; }
 fprint_struct_v4l2_querymenu (fp, rw, arg);
break;
case VIDIOC_G_JPEGCOMP:
if (!arg) { fputs ("VIDIOC_G_JPEGCOMP", fp); return; }
case VIDIOC_S_JPEGCOMP:
if (!arg) { fputs ("VIDIOC_S_JPEGCOMP", fp); return; }
 fprint_struct_v4l2_jpegcompression (fp, rw, arg);
break;
case VIDIOC_G_AUDOUT:
if (!arg) { fputs ("VIDIOC_G_AUDOUT", fp); return; }
case VIDIOC_S_AUDOUT:
if (!arg) { fputs ("VIDIOC_S_AUDOUT", fp); return; }
case VIDIOC_ENUMAUDOUT:
if (!arg) { fputs ("VIDIOC_ENUMAUDOUT", fp); return; }
case VIDIOC_G_AUDOUT_OLD:
if (!arg) { fputs ("VIDIOC_G_AUDOUT_OLD", fp); return; }
 fprint_struct_v4l2_audioout (fp, rw, arg);
break;
case VIDIOC_REQBUFS:
if (!arg) { fputs ("VIDIOC_REQBUFS", fp); return; }
 fprint_struct_v4l2_requestbuffers (fp, rw, arg);
break;
case VIDIOC_CROPCAP:
if (!arg) { fputs ("VIDIOC_CROPCAP", fp); return; }
case VIDIOC_CROPCAP_OLD:
if (!arg) { fputs ("VIDIOC_CROPCAP_OLD", fp); return; }
 fprint_struct_v4l2_cropcap (fp, rw, arg);
break;
case VIDIOC_G_AUDIO:
if (!arg) { fputs ("VIDIOC_G_AUDIO", fp); return; }
case VIDIOC_S_AUDIO:
if (!arg) { fputs ("VIDIOC_S_AUDIO", fp); return; }
case VIDIOC_ENUMAUDIO:
if (!arg) { fputs ("VIDIOC_ENUMAUDIO", fp); return; }
case VIDIOC_G_AUDIO_OLD:
if (!arg) { fputs ("VIDIOC_G_AUDIO_OLD", fp); return; }
 fprint_struct_v4l2_audio (fp, rw, arg);
break;
case VIDIOC_ENUMOUTPUT:
if (!arg) { fputs ("VIDIOC_ENUMOUTPUT", fp); return; }
 fprint_struct_v4l2_output (fp, rw, arg);
break;
case VIDIOC_G_FBUF:
if (!arg) { fputs ("VIDIOC_G_FBUF", fp); return; }
case VIDIOC_S_FBUF:
if (!arg) { fputs ("VIDIOC_S_FBUF", fp); return; }
 fprint_struct_v4l2_framebuffer (fp, rw, arg);
break;
case VIDIOC_ENUMSTD:
if (!arg) { fputs ("VIDIOC_ENUMSTD", fp); return; }
 fprint_struct_v4l2_standard (fp, rw, arg);
break;
	default:
		if (!arg) { fprint_unknown_ioctl (fp, cmd, arg); return; }
		break;
	}
}

static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYCAP (struct v4l2_capability *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUM_FMT (struct v4l2_fmtdesc *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_FMT (struct v4l2_format *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_FMT (struct v4l2_format *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_MPEGCOMP (struct v4l2_mpeg_compression *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_MPEGCOMP (const struct v4l2_mpeg_compression *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_REQBUFS (struct v4l2_requestbuffers *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYBUF (struct v4l2_buffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_FBUF (struct v4l2_framebuffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_FBUF (const struct v4l2_framebuffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_OVERLAY (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QBUF (struct v4l2_buffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_DQBUF (struct v4l2_buffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_STREAMON (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_STREAMOFF (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_PARM (struct v4l2_streamparm *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_PARM (struct v4l2_streamparm *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_STD (v4l2_std_id *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_STD (const v4l2_std_id *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMSTD (struct v4l2_standard *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMINPUT (struct v4l2_input *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_CTRL (struct v4l2_control *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_CTRL (struct v4l2_control *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_TUNER (struct v4l2_tuner *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_TUNER (const struct v4l2_tuner *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_AUDIO (struct v4l2_audio *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_AUDIO (const struct v4l2_audio *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYCTRL (struct v4l2_queryctrl *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYMENU (struct v4l2_querymenu *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_INPUT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_INPUT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_OUTPUT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_OUTPUT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMOUTPUT (struct v4l2_output *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_AUDOUT (struct v4l2_audioout *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_AUDOUT (const struct v4l2_audioout *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_MODULATOR (struct v4l2_modulator *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_MODULATOR (const struct v4l2_modulator *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_FREQUENCY (struct v4l2_frequency *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_FREQUENCY (const struct v4l2_frequency *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_CROPCAP (struct v4l2_cropcap *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_CROP (struct v4l2_crop *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_CROP (const struct v4l2_crop *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_JPEGCOMP (struct v4l2_jpegcompression *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_JPEGCOMP (const struct v4l2_jpegcompression *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYSTD (v4l2_std_id *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_TRY_FMT (struct v4l2_format *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMAUDIO (struct v4l2_audio *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMAUDOUT (struct v4l2_audioout *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_PRIORITY (enum v4l2_priority *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_PRIORITY (const enum v4l2_priority *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_SLICED_VBI_CAP (struct v4l2_sliced_vbi_cap *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_OVERLAY_OLD (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_PARM_OLD (const struct v4l2_streamparm *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_CTRL_OLD (const struct v4l2_control *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_AUDIO_OLD (struct v4l2_audio *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_AUDOUT_OLD (struct v4l2_audioout *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_CROPCAP_OLD (struct v4l2_cropcap *arg __attribute__ ((unused))) {}

