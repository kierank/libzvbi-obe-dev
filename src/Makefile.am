## Process this file with automake to produce Makefile.in

SUBDIRS = dvb

structpr = structpr.pl

BUILT_SOURCES = \
	libzvbi.h \
	version.h \
	_videodev.h \
	_videodev2.h \
	_videodev2k.h

EXTRA_DIST = \
	$(structpr) \
	$(BUILT_SOURCES) \
	wstfont2.xbm \
	ccfont2.xbm

INCLUDES  = \
	-Wall \
	-Wunused \
	-Wmissing-prototypes \
	-Wmissing-declarations
# INCLUDES += -D_XOPEN_SOURCE=500 -D_BSD_SOURCE to enable pthread_rwlocks
INCLUDES += \
	-D_REENTRANT \
	-D_XOPEN_SOURCE=500 \
	-D_BSD_SOURCE
INCLUDES += \
	$(X_CFLAGS) \
	$(UNICODE_CFLAGS)

if ENABLE_PROXY
proxy_sources = \
	proxy-client.c proxy-client.h \
	proxy-msg.c proxy-msg.h
else
proxy_sources =
endif

lib_LTLIBRARIES = libzvbi.la

libzvbi_la_SOURCES = \
	bcd.h \
	cache.c cache.h \
	caption.c cc.h \
	decoder.c decoder.h sliced.h \
	exp-gfx.c exp-gfx.h \
	exp-html.c \
	exp-templ.c \
	exp-txt.c exp-txt.h \
	exp-vtx.c \
	export.c export.h format.h \
	hamm.c hamm.h \
	io.c io.h \
	io-bktr.c \
	io-dvb.c \
	io-v4l.c videodev.h \
	io-v4l2.c videodev2.h \
	io-v4l2k.c videodev2k.h \
	lang.c lang.h \
	misc.h \
	packet.c teletext.c vt.h \
	search.c search.h ure.c ure.h \
	tables.c tables.h \
	trigger.c trigger.h \
	vbi.c vbi.h event.h \
	wss.c wss.h \
	$(proxy_sources)

libzvbiincludedir = $(includedir)
libzvbiinclude_HEADERS = libzvbi.h

libzvbi_la_LIBADD = $(LIBS) $(UNICODE_LIBS)
libzvbi_la_LDFLAGS = -version-info $(LIBZVBI_SO_VERSION)

version.h:
	echo -e "\n#define VBI_VERSION_MAJOR" `echo @VERSION@ \
	 | sed "s/\./\\\\\n#define VBI_VERSION_MINOR /" \
	 | sed "s/\./\\\\\n#define VBI_VERSION_MICRO /"` >> $@

# Note order matters.
LIBZVBI_HDRS = \
	misc.h \
	bcd.h \
	event.h \
	format.h \
	lang.h \
	export.h \
	cache.h \
	search.h \
	sliced.h \
	decoder.h \
	io.h \
	proxy-msg.h \
	proxy-client.h \
	exp-gfx.h \
	exp-txt.h \
	cc.h \
	vt.h \
	tables.h \
	vbi.h

# Concat the header files in this order and filter out all lines
#   before "^/* Public", after "^/* Private" and between "^/**"..."**/$"
libzvbi.h: version.h $(LIBZVBI_HDRS)
	sed -ne "1,/\*\//p" $(srcdir)/vbi.h > $@
	echo -e "\n/* Generated file, do not edit! */" >> $@
	echo -e "\n#ifndef __LIBZVBI_H__\n#define __LIBZVBI_H__" >> $@
	cat version.h >> $@
	echo -e "\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n" >> $@
	echo -e "\ntypedef struct vbi_decoder vbi_decoder;\n" >> $@
	for fi in $(LIBZVBI_HDRS); do \
		echo "/* $$fi */" >> $@; \
		sed -e "/^\/\* Public/,/^\/\* Private/!d;/^\/\* Public/d;/^\/\* Private/d;s/\(\/\*\*.*\*\/\)//;/^[ 	]*\/\*\*/,/\*\/[ 	]*$$/d;" $(srcdir)/$$fi >> $@; \
	done
	echo -e "\n#ifdef __cplusplus\n}\n#endif" >> $@
	echo -e "\n#endif /* __LIBZVBI_H__ */" >> $@

_videodev.h: Makefile.am $(structpr) videodev.h
	$(srcdir)/$(structpr) <$(srcdir)/videodev.h \
	video_device.type=VFL_TYPE_ \
	video_device.hardware=VID_HARDWARE_ \
	video_capability.type=VID_TYPE_ \
	video_channel.flags=VIDEO_VC_ \
	video_channel.type=VIDEO_TYPE_ \
	video_tuner.flags=VIDEO_TUNER_ \
	video_tuner.mode=VIDEO_MODE_ \
	video_picture.palette=VIDEO_PALETTE_ \
	video_audio.flags=VIDEO_AUDIO_ \
	video_audio.mode=VIDEO_SOUND_ \
	video_window.flags=VIDEO_WINDOW_ \
	video_capture.flags=VIDEO_CAPTURE_ \
	vbi_format.flags=VBI_ \
	> $@

# XXX v4l2_pix_format.flags has V4L2_FMT_CS_field subset.
# XXX v4l2_timecode.flags has V4L2_TC_USERBITS_field subset.
# XXX v4l2_buffer.flags has V4L2_BUF_FLAG_, V4L2_BUF_ATTR_, V4L2_BUF_REQ_.
_videodev2.h: Makefile.am $(structpr) videodev2.h
	$(srcdir)/$(structpr) <$(srcdir)/videodev2.h \
	v4l2_capability.type=V4L2_TYPE_ \
	v4l2_capability.flags=V4L2_FLAG_ \
	v4l2_pix_format.pixelformat=V4L2_PIX_FMT_ \
	v4l2_pix_format.flags=V4L2_FMT_FLAG_ \
	v4l2_fmtdesc.pixelformat=V4L2_PIX_FMT_ \
	v4l2_timecode.flags=V4L2_TC_FLAG_ \
	v4l2_timecode.type=V4L2_TC_TYPE_ \
	v4l2_requestbuffers.type=V4L2_BUF_TYPE_ \
	v4l2_buffer.type=V4L2_BUF_TYPE_ \
	v4l2_buffer.flags=V4L2_BUF_FLAG_ \
	v4l2_framebuffer.capability=V4L2_FBUF_CAP_ \
	v4l2_framebuffer.flags=V4L2_FBUF_FLAG_ \
	v4l2_captureparm.capability=V4L2_CAP_ \
	v4l2_captureparm.capturemode=V4L2_MODE_ \
	v4l2_zoomcap.capability=V4L2_ZOOM_ \
	v4l2_standard.colorstandard=V4L2_COLOR_STD_, \
		V4L2_COLOR_STD_PAL:colorstandard_data.pal, \
		V4L2_COLOR_STD_NTSC:colorstandard_data.ntsc, \
		V4L2_COLOR_STD_SECAM:colorstandard_data.secam \
	v4l2_standard.transmission=V4L2_TRANSM_STD_ \
	v4l2_input.type=V4L2_INPUT_TYPE_ \
	v4l2_input.capability=V4L2_INPUT_CAP_ \
	v4l2_output.type=V4L2_OUTPUT_TYPE_ \
	v4l2_output.capability=V4L2_OUTPUT_CAP_ \
	v4l2_control.id=V4L2_CID_ \
	v4l2_queryctrl.id=V4L2_CID_ \
	v4l2_queryctrl.type=V4L2_CTRL_TYPE_ \
	v4l2_queryctrl.flags=V4L2_CTRL_FLAG_ \
	v4l2_queryctrl.group=string \
	v4l2_queryctrl.category=V4L2_CTRL_CAT_ \
	v4l2_querymenu.id=V4L2_CID_ \
	v4l2_fxcontrol.id=V4L2_CID_ \
	v4l2_tuner.capability=V4L2_TUNER_CAP_ \
	v4l2_tuner.rxsubchans=V4L2_TUNER_SUB_ \
	v4l2_tuner.audmode=V4L2_TUNER_MODE_ \
	v4l2_modulator.capability=V4L2_TUNER_CAP_ \
	v4l2_modulator.txsubchans=V4L2_TUNER_SUB_ \
	v4l2_audio.capability=V4L2_AUDCAP_ \
	v4l2_audio.mode=V4L2_AUDMODE_ \
	v4l2_vbi_format.sample_format=V4L2_VBI_SF_ \
	v4l2_vbi_format.flags=V4L2_VBI_ \
	v4l2_format.type=V4L2_BUF_TYPE_, \
		V4L2_BUF_TYPE_CAPTURE:fmt.pix, \
		V4L2_BUF_TYPE_VBI:fmt.vbi \
	v4l2_streamparm.type=V4L2_BUF_TYPE_, \
		V4L2_BUF_TYPE_CAPTURE:parm.capture, \
		V4L2_BUF_TYPE_VIDEOOUT:parm.output \
	> $@

# XXX v4l2_timecode.flags has V4L2_TC_USERBITS_field subset.
_videodev2k.h: Makefile.am $(structpr) videodev2k.h
	$(srcdir)/$(structpr) <$(srcdir)/videodev2k.h \
	"v4l2_fract={ fprintf (fp, \"%u/%u\", t->numerator, t->denominator); }" \
	v4l2_capability.driver=string \
	v4l2_capability.card=string \
	v4l2_capability.bus_info=string \
	v4l2_capability.version=hex \
	v4l2_capability.capabilities=V4L2_CAP_ \
	v4l2_pix_format.pixelformat=V4L2_PIX_FMT_ \
	v4l2_fmtdesc.flags=V4L2_FMT_FLAG_ \
	v4l2_fmtdesc.description=string \
	v4l2_fmtdesc.pixelformat=fourcc \
	v4l2_timecode.type=V4L2_TC_TYPE_ \
	v4l2_timecode.flags=V4L2_TC_FLAG_ \
	v4l2_jpegcompression.jpeg_markers=V4L2_JPEG_MARKER_ \
	v4l2_buffer.flags=V4L2_BUF_FLAG_ \
	v4l2_buffer.memory=V4L2_MEMORY_MMAP:m.offset, \
			   V4L2_MEMORY_USERPTR:m.userptr \
	v4l2_framebuffer.capability=V4L2_FBUF_CAP_ \
	v4l2_framebuffer.flags=V4L2_FBUF_FLAG_ \
	v4l2_captureparm.capability=V4L2_CAP_ \
	v4l2_captureparm.capturemode=V4L2_MODE_ \
	v4l2_std_id=V4L2_STD_ \
	v4l2_input.type=V4L2_INPUT_TYPE_ \
	v4l2_input.status=V4L2_IN_ST_ \
	v4l2_output.type=V4L2_OUTPUT_TYPE_ \
	v4l2_control.id=V4L2_CID_ \
	v4l2_queryctrl.id=V4L2_CID_ \
	v4l2_queryctrl.flags=V4L2_CTRL_FLAG_ \
	v4l2_querymenu.id=V4L2_CID_ \
	v4l2_querymenu.index=V4L2_CID_ \
	v4l2_tuner.capability=V4L2_TUNER_CAP_ \
	v4l2_tuner.rxsubchans=V4L2_TUNER_SUB_ \
	v4l2_tuner.audmode=V4L2_TUNER_MODE_ \
	v4l2_modulator.capability=V4L2_TUNER_CAP_ \
	v4l2_modulator.txsubchans=V4L2_TUNER_SUB_ \
	v4l2_audio.capability=V4L2_AUDCAP_ \
	v4l2_audio.mode=V4L2_AUDMODE_ \
	v4l2_vbi_format.sample_format=V4L2_PIX_FMT_ \
	v4l2_vbi_format.flags=V4L2_VBI_ \
	v4l2_format.type=V4L2_BUF_TYPE_VIDEO_CAPTURE:fmt.pix, \
		V4L2_BUF_TYPE_VIDEO_OVERLAY:fmt.win, \
		V4L2_BUF_TYPE_VBI_CAPTURE:fmt.vbi \
	> $@

_bktr.h: Makefile.am $(structpr) ioctl_bt848.h ioctl_meteor.h
	cat $(srcdir)/ioctl_bt848.h $(srcdir)/ioctl_meteor.h | \
	$(srcdir)/$(structpr) \
	> $@
