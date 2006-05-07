/* Generated file, do not edit! */

#include <stdio.h>
#include "io.h"

#ifndef __GNUC__
#undef __attribute__
#define __attribute__(x)
#endif

static void
fprint_struct_v4l2_performance (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_performance *t)
{
fprintf (fp, "frames=%ld "
"framesdropped=%ld "
"bytesin=%lu "
"bytesout=%lu "
"reserved[] ",
(long) t->frames, 
(long) t->framesdropped, 
(unsigned long) t->bytesin, 
(unsigned long) t->bytesout);
}

static void
fprint_struct_v4l2_compression (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_compression *t)
{
fprintf (fp, "quality=%ld "
"keyframerate=%ld "
"pframerate=%ld "
"reserved[] ",
(long) t->quality, 
(long) t->keyframerate, 
(long) t->pframerate);
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
"YVU422P", (unsigned long) V4L2_PIX_FMT_YVU422P,
"YVU411P", (unsigned long) V4L2_PIX_FMT_YVU411P,
"Y41P", (unsigned long) V4L2_PIX_FMT_Y41P,
"YUV410", (unsigned long) V4L2_PIX_FMT_YUV410,
"YUV420", (unsigned long) V4L2_PIX_FMT_YUV420,
"YYUV", (unsigned long) V4L2_PIX_FMT_YYUV,
"HI240", (unsigned long) V4L2_PIX_FMT_HI240,
"WNVA", (unsigned long) V4L2_PIX_FMT_WNVA,
(void *) 0);
}

static void
fprint_struct_v4l2_fmtdesc (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_fmtdesc *t)
{
fprintf (fp, "index=%ld "
"description=\"%.*s\" "
"pixelformat=",
(long) t->index, 
32, (const char *) t->description);
fprint_symbol_v4l2_pix_fmt_ (fp, rw, t->pixelformat);
fprintf (fp, " flags=0x%lx "
"depth=%lu "
"reserved[] ",
(unsigned long) t->flags, 
(unsigned long) t->depth);
}

static void
fprint_symbol_v4l2_transm_std_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"B", (unsigned long) V4L2_TRANSM_STD_B,
"D", (unsigned long) V4L2_TRANSM_STD_D,
"G", (unsigned long) V4L2_TRANSM_STD_G,
"H", (unsigned long) V4L2_TRANSM_STD_H,
"I", (unsigned long) V4L2_TRANSM_STD_I,
"K", (unsigned long) V4L2_TRANSM_STD_K,
"K1", (unsigned long) V4L2_TRANSM_STD_K1,
"L", (unsigned long) V4L2_TRANSM_STD_L,
"M", (unsigned long) V4L2_TRANSM_STD_M,
"N", (unsigned long) V4L2_TRANSM_STD_N,
(void *) 0);
}

static void
fprint_struct_v4l2_standard (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_standard *t)
{
fprintf (fp, "name=\"%.*s\" ",
24, (const char *) t->name);
fprintf (fp, "framerate={numerator=%lu "
"denominator=%lu ",
(unsigned long) t->framerate.numerator, 
(unsigned long) t->framerate.denominator);
fprintf (fp, "} framelines=%lu "
"reserved1 "
"colorstandard=",
(unsigned long) t->framelines);
fprint_symbolic (fp, 0, t->colorstandard,
"PAL", (unsigned long) V4L2_COLOR_STD_PAL,
"NTSC", (unsigned long) V4L2_COLOR_STD_NTSC,
"SECAM", (unsigned long) V4L2_COLOR_STD_SECAM,
(void *) 0);
fputs (" ", fp);
fputs ("colorstandard_data={", fp);
if (V4L2_COLOR_STD_PAL == t->colorstandard) {
fputs ("pal={", fp);
}
fprintf (fp, "colorsubcarrier=%lu ",
(unsigned long) t->colorstandard_data.pal.colorsubcarrier);
if (V4L2_COLOR_STD_PAL == t->colorstandard) {
fputs ("} ", fp);
}
if (V4L2_COLOR_STD_NTSC == t->colorstandard) {
fputs ("ntsc={", fp);
}
fprintf (fp, "colorsubcarrier=%lu ",
(unsigned long) t->colorstandard_data.ntsc.colorsubcarrier);
if (V4L2_COLOR_STD_NTSC == t->colorstandard) {
fputs ("} ", fp);
}
if (V4L2_COLOR_STD_SECAM == t->colorstandard) {
fputs ("secam={", fp);
}
fprintf (fp, "f0b=%lu "
"f0r=%lu ",
(unsigned long) t->colorstandard_data.secam.f0b, 
(unsigned long) t->colorstandard_data.secam.f0r);
if (V4L2_COLOR_STD_SECAM == t->colorstandard) {
fputs ("} ", fp);
}
fputs ("reserved[] ", fp);
fputs ("} transmission=", fp);
fprint_symbol_v4l2_transm_std_ (fp, rw, t->transmission);
fputs (" reserved2 ", fp);
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
fprintf (fp, "input=%ld "
"name=\"%.*s\" "
"std={",
(long) t->input, 
32, (const char *) t->name);
fprint_struct_v4l2_standard (fp, rw, &t->std);
fputs ("} capability=", fp);
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
fprint_symbol_v4l2_type_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"CAPTURE", (unsigned long) V4L2_TYPE_CAPTURE,
"CODEC", (unsigned long) V4L2_TYPE_CODEC,
"OUTPUT", (unsigned long) V4L2_TYPE_OUTPUT,
"FX", (unsigned long) V4L2_TYPE_FX,
"VBI", (unsigned long) V4L2_TYPE_VBI,
"VTR", (unsigned long) V4L2_TYPE_VTR,
"VTX", (unsigned long) V4L2_TYPE_VTX,
"RADIO", (unsigned long) V4L2_TYPE_RADIO,
"VBI_INPUT", (unsigned long) V4L2_TYPE_VBI_INPUT,
"VBI_OUTPUT", (unsigned long) V4L2_TYPE_VBI_OUTPUT,
"PRIVATE", (unsigned long) V4L2_TYPE_PRIVATE,
(void *) 0);
}

static void
fprint_symbol_v4l2_flag_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 2, value,
"READ", (unsigned long) V4L2_FLAG_READ,
"WRITE", (unsigned long) V4L2_FLAG_WRITE,
"STREAMING", (unsigned long) V4L2_FLAG_STREAMING,
"PREVIEW", (unsigned long) V4L2_FLAG_PREVIEW,
"SELECT", (unsigned long) V4L2_FLAG_SELECT,
"TUNER", (unsigned long) V4L2_FLAG_TUNER,
"MONOCHROME", (unsigned long) V4L2_FLAG_MONOCHROME,
"DATA_SERVICE", (unsigned long) V4L2_FLAG_DATA_SERVICE,
(void *) 0);
}

static void
fprint_struct_v4l2_capability (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_capability *t)
{
fprintf (fp, "name=\"%.*s\" "
"type=",
32, (const char *) t->name);
fprint_symbol_v4l2_type_ (fp, rw, t->type);
fprintf (fp, " inputs=%ld "
"outputs=%ld "
"audios=%ld "
"maxwidth=%ld "
"maxheight=%ld "
"minwidth=%ld "
"minheight=%ld "
"maxframerate=%ld "
"flags=",
(long) t->inputs, 
(long) t->outputs, 
(long) t->audios, 
(long) t->maxwidth, 
(long) t->maxheight, 
(long) t->minwidth, 
(long) t->minheight, 
(long) t->maxframerate);
fprint_symbol_v4l2_flag_ (fp, rw, t->flags);
fputs (" reserved[] ", fp);
}

static void
fprint_struct_v4l2_enumstd (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_enumstd *t)
{
fprintf (fp, "index=%ld "
"std={",
(long) t->index);
fprint_struct_v4l2_standard (fp, rw, &t->std);
fprintf (fp, "} inputs=%lu "
"outputs=%lu "
"reserved[] ",
(unsigned long) t->inputs, 
(unsigned long) t->outputs);
}

static void
fprint_symbol_v4l2_cid_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"BASE", (unsigned long) V4L2_CID_BASE,
"PRIVATE_BASE", (unsigned long) V4L2_CID_PRIVATE_BASE,
"EFFECT_BASE", (unsigned long) V4L2_CID_EFFECT_BASE,
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
fprint_symbol_v4l2_ctrl_type_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
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
fprintf (fp, " name=\"%.*s\" "
"minimum=%ld "
"maximum=%ld "
"step=%lu "
"default_value=%ld "
"type=",
32, (const char *) t->name, 
(long) t->minimum, 
(long) t->maximum, 
(unsigned long) t->step, 
(long) t->default_value);
fprint_symbol_v4l2_ctrl_type_ (fp, rw, t->type);
fputs (" flags=", fp);
fprint_symbolic (fp, 2, t->flags,
"DISABLED", (unsigned long) V4L2_CTRL_FLAG_DISABLED,
"GRABBED", (unsigned long) V4L2_CTRL_FLAG_GRABBED,
(void *) 0);
fputs (" category=", fp);
fprint_symbolic (fp, 0, t->category,
"VIDEO", (unsigned long) V4L2_CTRL_CAT_VIDEO,
"AUDIO", (unsigned long) V4L2_CTRL_CAT_AUDIO,
"EFFECT", (unsigned long) V4L2_CTRL_CAT_EFFECT,
(void *) 0);
fprintf (fp, " group=\"%.*s\" "
"reserved[] ",
32, (const char *) t->group);
}

static void
fprint_struct_v4l2_modulator (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_modulator *t)
{
fprintf (fp, "output=%ld "
"name=\"%.*s\" "
"std={",
(long) t->output, 
32, (const char *) t->name);
fprint_struct_v4l2_standard (fp, rw, &t->std);
fputs ("} capability=", fp);
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
fprint_struct_v4l2_input (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_input *t)
{
fprintf (fp, "index=%ld "
"name=\"%.*s\" "
"type=",
(long) t->index, 
32, (const char *) t->name);
fprint_symbolic (fp, 0, t->type,
"TUNER", (unsigned long) V4L2_INPUT_TYPE_TUNER,
"CAMERA", (unsigned long) V4L2_INPUT_TYPE_CAMERA,
(void *) 0);
fputs (" capability=", fp);
fprint_symbolic (fp, 0, t->capability,
"AUDIO", (unsigned long) V4L2_INPUT_CAP_AUDIO,
(void *) 0);
fprintf (fp, " assoc_audio=%ld "
"reserved[] ",
(long) t->assoc_audio);
}

static void
fprint_symbol_v4l2_buf_type_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"field", (unsigned long) V4L2_BUF_TYPE_field,
"CAPTURE", (unsigned long) V4L2_BUF_TYPE_CAPTURE,
"CODECIN", (unsigned long) V4L2_BUF_TYPE_CODECIN,
"CODECOUT", (unsigned long) V4L2_BUF_TYPE_CODECOUT,
"EFFECTSIN", (unsigned long) V4L2_BUF_TYPE_EFFECTSIN,
"EFFECTSIN2", (unsigned long) V4L2_BUF_TYPE_EFFECTSIN2,
"EFFECTSOUT", (unsigned long) V4L2_BUF_TYPE_EFFECTSOUT,
"VIDEOOUT", (unsigned long) V4L2_BUF_TYPE_VIDEOOUT,
"FXCONTROL", (unsigned long) V4L2_BUF_TYPE_FXCONTROL,
"VBI", (unsigned long) V4L2_BUF_TYPE_VBI,
"PRIVATE", (unsigned long) V4L2_BUF_TYPE_PRIVATE,
(void *) 0);
}

static void
fprint_symbol_v4l2_fmt_flag_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 2, value,
"COMPRESSED", (unsigned long) V4L2_FMT_FLAG_COMPRESSED,
"BYTESPERLINE", (unsigned long) V4L2_FMT_FLAG_BYTESPERLINE,
"NOT_INTERLACED", (unsigned long) V4L2_FMT_FLAG_NOT_INTERLACED,
"INTERLACED", (unsigned long) V4L2_FMT_FLAG_INTERLACED,
"TOPFIELD", (unsigned long) V4L2_FMT_FLAG_TOPFIELD,
"BOTFIELD", (unsigned long) V4L2_FMT_FLAG_BOTFIELD,
"ODDFIELD", (unsigned long) V4L2_FMT_FLAG_ODDFIELD,
"EVENFIELD", (unsigned long) V4L2_FMT_FLAG_EVENFIELD,
"COMBINED", (unsigned long) V4L2_FMT_FLAG_COMBINED,
"FIELD_field", (unsigned long) V4L2_FMT_FLAG_FIELD_field,
"SWCONVERSION", (unsigned long) V4L2_FMT_FLAG_SWCONVERSION,
(void *) 0);
}

static void
fprint_struct_v4l2_pix_format (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_pix_format *t)
{
fprintf (fp, "width=%lu "
"height=%lu "
"depth=%lu "
"pixelformat=",
(unsigned long) t->width, 
(unsigned long) t->height, 
(unsigned long) t->depth);
fprint_symbol_v4l2_pix_fmt_ (fp, rw, t->pixelformat);
fputs (" flags=", fp);
fprint_symbol_v4l2_fmt_flag_ (fp, rw, t->flags);
fprintf (fp, " bytesperline=%lu "
"sizeimage=%lu "
"priv=%lu ",
(unsigned long) t->bytesperline, 
(unsigned long) t->sizeimage, 
(unsigned long) t->priv);
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
fprint_symbolic (fp, 0, t->sample_format,
"UBYTE", (unsigned long) V4L2_VBI_SF_UBYTE,
(void *) 0);
fputs (" start[]=? "
"count[]=? "
"flags=", fp);
fprint_symbolic (fp, 2, t->flags,
"SF_UBYTE", (unsigned long) V4L2_VBI_SF_UBYTE,
"UNSYNC", (unsigned long) V4L2_VBI_UNSYNC,
"INTERLACED", (unsigned long) V4L2_VBI_INTERLACED,
(void *) 0);
fputs (" reserved2 ", fp);
}

static void
fprint_struct_v4l2_format (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_format *t)
{
fputs ("type=", fp);
fprint_symbol_v4l2_buf_type_ (fp, rw, t->type);
fputs (" ", fp);
fputs ("fmt={", fp);
if (V4L2_BUF_TYPE_CAPTURE == t->type) {
fputs ("pix={", fp);
fprint_struct_v4l2_pix_format (fp, rw, &t->fmt.pix);
fputs ("} ", fp);
}
if (V4L2_BUF_TYPE_VBI == t->type) {
fputs ("vbi={", fp);
fprint_struct_v4l2_vbi_format (fp, rw, &t->fmt.vbi);
fputs ("} ", fp);
}
fputs ("raw_data[]=? ", fp);
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
"TOPFIELD", (unsigned long) V4L2_BUF_FLAG_TOPFIELD,
"BOTFIELD", (unsigned long) V4L2_BUF_FLAG_BOTFIELD,
"ODDFIELD", (unsigned long) V4L2_BUF_FLAG_ODDFIELD,
"EVENFIELD", (unsigned long) V4L2_BUF_FLAG_EVENFIELD,
"TIMECODE", (unsigned long) V4L2_BUF_FLAG_TIMECODE,
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
fprintf (fp, "frames=%lu "
"seconds=%lu "
"minutes=%lu "
"hours=%lu "
"userbits[]=? "
"flags=",
(unsigned long) t->frames, 
(unsigned long) t->seconds, 
(unsigned long) t->minutes, 
(unsigned long) t->hours);
fprint_symbolic (fp, 2, t->flags,
"DROPFRAME", (unsigned long) V4L2_TC_FLAG_DROPFRAME,
"COLORFRAME", (unsigned long) V4L2_TC_FLAG_COLORFRAME,
(void *) 0);
fputs (" type=", fp);
fprint_symbol_v4l2_tc_type_ (fp, rw, t->type);
fputs (" ", fp);
}

static void
fprint_struct_v4l2_buffer (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_buffer *t)
{
fprintf (fp, "index=%ld "
"type=",
(long) t->index);
fprint_symbol_v4l2_buf_type_ (fp, rw, t->type);
fprintf (fp, " offset=%lu "
"length=%lu "
"bytesused=%lu "
"flags=",
(unsigned long) t->offset, 
(unsigned long) t->length, 
(unsigned long) t->bytesused);
fprint_symbol_v4l2_buf_flag_ (fp, rw, t->flags);
fprintf (fp, " timestamp=%ld "
"timecode={",
(long) t->timestamp);
fprint_struct_v4l2_timecode (fp, rw, &t->timecode);
fprintf (fp, "} sequence=%lu "
"reserved[] ",
(unsigned long) t->sequence);
}

static void
fprint_struct_v4l2_cvtdesc (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_cvtdesc *t)
{
fprintf (fp, "index=%ld ",
(long) t->index);
fprintf (fp, "in={pixelformat=%lu "
"flags=0x%lx "
"depth=%lu "
"reserved[] ",
(unsigned long) t->in.pixelformat, 
(unsigned long) t->in.flags, 
(unsigned long) t->in.depth);
fputs ("} ", fp);
fprintf (fp, "out={pixelformat=%lu "
"flags=0x%lx "
"depth=%lu "
"reserved[] ",
(unsigned long) t->out.pixelformat, 
(unsigned long) t->out.flags, 
(unsigned long) t->out.depth);
fputs ("} ", fp);
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
fprint_struct_v4l2_captureparm (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_captureparm *t)
{
fputs ("capability=", fp);
fprint_symbolic (fp, 0, t->capability,
"TIMEPERFRAME", (unsigned long) V4L2_CAP_TIMEPERFRAME,
(void *) 0);
fputs (" capturemode=", fp);
fprint_symbolic (fp, 0, t->capturemode,
"HIGHQUALITY", (unsigned long) V4L2_MODE_HIGHQUALITY,
(void *) 0);
fprintf (fp, " timeperframe=%lu "
"extendedmode=%lu "
"reserved[] ",
(unsigned long) t->timeperframe, 
(unsigned long) t->extendedmode);
}

static void
fprint_struct_v4l2_outputparm (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_outputparm *t)
{
fprintf (fp, "capability=%lu "
"outputmode=%lu "
"timeperframe=%lu "
"extendedmode=%lu "
"reserved[] ",
(unsigned long) t->capability, 
(unsigned long) t->outputmode, 
(unsigned long) t->timeperframe, 
(unsigned long) t->extendedmode);
}

static void
fprint_struct_v4l2_streamparm (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_streamparm *t)
{
fputs ("type=", fp);
fprint_symbol_v4l2_buf_type_ (fp, rw, t->type);
fputs (" ", fp);
fputs ("parm={", fp);
if (V4L2_BUF_TYPE_CAPTURE == t->type) {
fputs ("capture={", fp);
fprint_struct_v4l2_captureparm (fp, rw, &t->parm.capture);
fputs ("} ", fp);
}
if (V4L2_BUF_TYPE_VIDEOOUT == t->type) {
fputs ("output={", fp);
fprint_struct_v4l2_outputparm (fp, rw, &t->parm.output);
fputs ("} ", fp);
}
fputs ("raw_data[]=? ", fp);
fputs ("} ", fp);
}

static void
fprint_struct_v4l2_fxdesc (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_fxdesc *t)
{
fprintf (fp, "index=%ld "
"name=\"%.*s\" "
"flags=0x%lx "
"inputs=%lu "
"controls=%lu "
"reserved[] ",
(long) t->index, 
32, (const char *) t->name, 
(unsigned long) t->flags, 
(unsigned long) t->inputs, 
(unsigned long) t->controls);
}

static void
fprint_struct_v4l2_querymenu (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_querymenu *t)
{
fputs ("id=", fp);
fprint_symbol_v4l2_cid_ (fp, rw, t->id);
fprintf (fp, " index=%ld "
"name=\"%.*s\" "
"reserved ",
(long) t->index, 
32, (const char *) t->name);
}

static void
fprint_struct_v4l2_audioout (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_audioout *t)
{
fprintf (fp, "audio=%ld "
"name=\"%.*s\" "
"capability=%lu "
"mode=%lu "
"reserved[] ",
(long) t->audio, 
32, (const char *) t->name, 
(unsigned long) t->capability, 
(unsigned long) t->mode);
}

static void
fprint_struct_v4l2_requestbuffers (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_requestbuffers *t)
{
fprintf (fp, "count=%ld "
"type=",
(long) t->count);
fprint_symbol_v4l2_buf_type_ (fp, rw, t->type);
fputs (" reserved[] ", fp);
}

static void
fprint_symbol_v4l2_audmode_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"LOUDNESS", (unsigned long) V4L2_AUDMODE_LOUDNESS,
"AVL", (unsigned long) V4L2_AUDMODE_AVL,
"STEREO_field", (unsigned long) V4L2_AUDMODE_STEREO_field,
"STEREO_LINEAR", (unsigned long) V4L2_AUDMODE_STEREO_LINEAR,
"STEREO_PSEUDO", (unsigned long) V4L2_AUDMODE_STEREO_PSEUDO,
"STEREO_SPATIAL30", (unsigned long) V4L2_AUDMODE_STEREO_SPATIAL30,
"STEREO_SPATIAL50", (unsigned long) V4L2_AUDMODE_STEREO_SPATIAL50,
(void *) 0);
}

static void
fprint_struct_v4l2_audio (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_audio *t)
{
fprintf (fp, "audio=%ld "
"name=\"%.*s\" "
"capability=",
(long) t->audio, 
32, (const char *) t->name);
fprint_symbolic (fp, 0, t->capability,
"EFFECTS", (unsigned long) V4L2_AUDCAP_EFFECTS,
"LOUDNESS", (unsigned long) V4L2_AUDCAP_LOUDNESS,
"AVL", (unsigned long) V4L2_AUDCAP_AVL,
(void *) 0);
fputs (" mode=", fp);
fprint_symbol_v4l2_audmode_ (fp, rw, t->mode);
fputs (" reserved[] ", fp);
}

static void
fprint_struct_v4l2_output (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_output *t)
{
fprintf (fp, "index=%ld "
"name=\"%.*s\" "
"type=",
(long) t->index, 
32, (const char *) t->name);
fprint_symbolic (fp, 0, t->type,
"MODULATOR", (unsigned long) V4L2_OUTPUT_TYPE_MODULATOR,
"ANALOG", (unsigned long) V4L2_OUTPUT_TYPE_ANALOG,
"ANALOGVGAOVERLAY", (unsigned long) V4L2_OUTPUT_TYPE_ANALOGVGAOVERLAY,
(void *) 0);
fputs (" capability=", fp);
fprint_symbolic (fp, 0, t->capability,
"AUDIO", (unsigned long) V4L2_OUTPUT_CAP_AUDIO,
(void *) 0);
fprintf (fp, " assoc_audio=%ld "
"reserved[] ",
(long) t->assoc_audio);
}

static void
fprint_struct_v4l2_window (FILE *fp, int rw __attribute__ ((unused)), const struct v4l2_window *t)
{
fprintf (fp, "x=%ld "
"y=%ld "
"width=%ld "
"height=%ld "
"chromakey=%lu "
"clips=%p "
"clipcount=%ld "
"bitmap=%p ",
(long) t->x, 
(long) t->y, 
(long) t->width, 
(long) t->height, 
(unsigned long) t->chromakey, 
(const void *) t->clips, 
(long) t->clipcount, 
(const void *) t->bitmap);
}

static void
fprint_symbol_v4l2_fbuf_cap_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"EXTERNOVERLAY", (unsigned long) V4L2_FBUF_CAP_EXTERNOVERLAY,
"CHROMAKEY", (unsigned long) V4L2_FBUF_CAP_CHROMAKEY,
"CLIPPING", (unsigned long) V4L2_FBUF_CAP_CLIPPING,
"SCALEUP", (unsigned long) V4L2_FBUF_CAP_SCALEUP,
"SCALEDOWN", (unsigned long) V4L2_FBUF_CAP_SCALEDOWN,
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
fputs (" base[]=? "
"fmt={", fp);
fprint_struct_v4l2_pix_format (fp, rw, &t->fmt);
fputs ("} ", fp);
}

static void
fprint_ioctl_arg (FILE *fp, unsigned int cmd, int rw, void *arg)
{
switch (cmd) {
case VIDIOC_G_PERF:
if (!arg) { fputs ("VIDIOC_G_PERF", fp); return; }
 fprint_struct_v4l2_performance (fp, rw, arg);
break;
case VIDIOC_PREVIEW:
if (!arg) { fputs ("VIDIOC_PREVIEW", fp); return; }
case VIDIOC_STREAMON:
if (!arg) { fputs ("VIDIOC_STREAMON", fp); return; }
case VIDIOC_STREAMOFF:
if (!arg) { fputs ("VIDIOC_STREAMOFF", fp); return; }
case VIDIOC_G_FREQ:
if (!arg) { fputs ("VIDIOC_G_FREQ", fp); return; }
case VIDIOC_S_FREQ:
if (!arg) { fputs ("VIDIOC_S_FREQ", fp); return; }
case VIDIOC_G_INPUT:
if (!arg) { fputs ("VIDIOC_G_INPUT", fp); return; }
case VIDIOC_S_INPUT:
if (!arg) { fputs ("VIDIOC_S_INPUT", fp); return; }
case VIDIOC_G_OUTPUT:
if (!arg) { fputs ("VIDIOC_G_OUTPUT", fp); return; }
case VIDIOC_S_OUTPUT:
if (!arg) { fputs ("VIDIOC_S_OUTPUT", fp); return; }
case VIDIOC_G_EFFECT:
if (!arg) { fputs ("VIDIOC_G_EFFECT", fp); return; }
case VIDIOC_S_EFFECT:
if (!arg) { fputs ("VIDIOC_S_EFFECT", fp); return; }
 fprintf (fp, "%ld", (long) * (int *) arg);
break;
case VIDIOC_G_COMP:
if (!arg) { fputs ("VIDIOC_G_COMP", fp); return; }
case VIDIOC_S_COMP:
if (!arg) { fputs ("VIDIOC_S_COMP", fp); return; }
 fprint_struct_v4l2_compression (fp, rw, arg);
break;
case VIDIOC_ENUM_PIXFMT:
if (!arg) { fputs ("VIDIOC_ENUM_PIXFMT", fp); return; }
case VIDIOC_ENUM_FBUFFMT:
if (!arg) { fputs ("VIDIOC_ENUM_FBUFFMT", fp); return; }
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
case VIDIOC_ENUMSTD:
if (!arg) { fputs ("VIDIOC_ENUMSTD", fp); return; }
 fprint_struct_v4l2_enumstd (fp, rw, arg);
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
case VIDIOC_G_FMT:
if (!arg) { fputs ("VIDIOC_G_FMT", fp); return; }
case VIDIOC_S_FMT:
if (!arg) { fputs ("VIDIOC_S_FMT", fp); return; }
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
case VIDIOC_ENUMCVT:
if (!arg) { fputs ("VIDIOC_ENUMCVT", fp); return; }
 fprint_struct_v4l2_cvtdesc (fp, rw, arg);
break;
case VIDIOC_G_CTRL:
if (!arg) { fputs ("VIDIOC_G_CTRL", fp); return; }
case VIDIOC_S_CTRL:
if (!arg) { fputs ("VIDIOC_S_CTRL", fp); return; }
 fprint_struct_v4l2_control (fp, rw, arg);
break;
case VIDIOC_G_PARM:
if (!arg) { fputs ("VIDIOC_G_PARM", fp); return; }
case VIDIOC_S_PARM:
if (!arg) { fputs ("VIDIOC_S_PARM", fp); return; }
 fprint_struct_v4l2_streamparm (fp, rw, arg);
break;
case VIDIOC_ENUMFX:
if (!arg) { fputs ("VIDIOC_ENUMFX", fp); return; }
 fprint_struct_v4l2_fxdesc (fp, rw, arg);
break;
case VIDIOC_QUERYMENU:
if (!arg) { fputs ("VIDIOC_QUERYMENU", fp); return; }
 fprint_struct_v4l2_querymenu (fp, rw, arg);
break;
case VIDIOC_G_AUDOUT:
if (!arg) { fputs ("VIDIOC_G_AUDOUT", fp); return; }
case VIDIOC_S_AUDOUT:
if (!arg) { fputs ("VIDIOC_S_AUDOUT", fp); return; }
 fprint_struct_v4l2_audioout (fp, rw, arg);
break;
case VIDIOC_REQBUFS:
if (!arg) { fputs ("VIDIOC_REQBUFS", fp); return; }
 fprint_struct_v4l2_requestbuffers (fp, rw, arg);
break;
case VIDIOC_G_AUDIO:
if (!arg) { fputs ("VIDIOC_G_AUDIO", fp); return; }
case VIDIOC_S_AUDIO:
if (!arg) { fputs ("VIDIOC_S_AUDIO", fp); return; }
 fprint_struct_v4l2_audio (fp, rw, arg);
break;
case VIDIOC_ENUMOUTPUT:
if (!arg) { fputs ("VIDIOC_ENUMOUTPUT", fp); return; }
 fprint_struct_v4l2_output (fp, rw, arg);
break;
case VIDIOC_G_WIN:
if (!arg) { fputs ("VIDIOC_G_WIN", fp); return; }
case VIDIOC_S_WIN:
if (!arg) { fputs ("VIDIOC_S_WIN", fp); return; }
 fprint_struct_v4l2_window (fp, rw, arg);
break;
case VIDIOC_G_FBUF:
if (!arg) { fputs ("VIDIOC_G_FBUF", fp); return; }
case VIDIOC_S_FBUF:
if (!arg) { fputs ("VIDIOC_S_FBUF", fp); return; }
 fprint_struct_v4l2_framebuffer (fp, rw, arg);
break;
case VIDIOC_G_STD:
if (!arg) { fputs ("VIDIOC_G_STD", fp); return; }
case VIDIOC_S_STD:
if (!arg) { fputs ("VIDIOC_S_STD", fp); return; }
 fprint_struct_v4l2_standard (fp, rw, arg);
break;
	default:
		if (!arg) { fprint_unknown_ioctl (fp, cmd, arg); return; }
		break;
	}
}

static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYCAP (struct v4l2_capability *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUM_PIXFMT (struct v4l2_fmtdesc *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUM_FBUFFMT (struct v4l2_fmtdesc *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_FMT (struct v4l2_format *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_FMT (struct v4l2_format *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_COMP (struct v4l2_compression *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_COMP (const struct v4l2_compression *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_REQBUFS (struct v4l2_requestbuffers *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYBUF (struct v4l2_buffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_FBUF (struct v4l2_framebuffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_FBUF (const struct v4l2_framebuffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_WIN (struct v4l2_window *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_WIN (const struct v4l2_window *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_PREVIEW (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QBUF (struct v4l2_buffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_DQBUF (struct v4l2_buffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_STREAMON (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_STREAMOFF (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_PERF (struct v4l2_performance *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_PARM (struct v4l2_streamparm *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_PARM (const struct v4l2_streamparm *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_STD (struct v4l2_standard *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_STD (const struct v4l2_standard *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMSTD (struct v4l2_enumstd *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMINPUT (struct v4l2_input *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_CTRL (struct v4l2_control *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_CTRL (const struct v4l2_control *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_TUNER (struct v4l2_tuner *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_TUNER (const struct v4l2_tuner *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_FREQ (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_FREQ (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_AUDIO (struct v4l2_audio *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_AUDIO (const struct v4l2_audio *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYCTRL (struct v4l2_queryctrl *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYMENU (struct v4l2_querymenu *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_INPUT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_INPUT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMCVT (struct v4l2_cvtdesc *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_OUTPUT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_OUTPUT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMOUTPUT (struct v4l2_output *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_AUDOUT (struct v4l2_audioout *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_AUDOUT (const struct v4l2_audioout *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMFX (struct v4l2_fxdesc *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_EFFECT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_EFFECT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_MODULATOR (struct v4l2_modulator *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_MODULATOR (const struct v4l2_modulator *arg __attribute__ ((unused))) {}

