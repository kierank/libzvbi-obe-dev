## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = libzvbi.la

EXTRA_DIST = wstfont2.xbm ccfont2.xbm

INCLUDES  = -W -Wall -Wunused -Wfloat-equal \
	-Wpointer-arith -Wbad-function-cast -Wcast-qual -Wcast-align \
	-Wwrite-strings -Winline -Wmissing-prototypes \
	-Wmissing-declarations
## -Wpadded -Wconversion 
## -D_XOPEN_SOURCE=500 -D_BSD_SOURCE to enable pthread_rwlocks
INCLUDES += -D_REENTRANT -D_POSIX_SOURCE
INCLUDES += $(X_CFLAGS) $(UNICODE_CFLAGS)

libzvbi_la_SOURCES = \
	decoder.c decoder.h sliced.h \
	io.c io.h \
	io-bktr.c \
	io-v4l2k.c videodev2k.h \
	io-v4l2.c videodev2.h \
	io-v4l.c videodev.h \
	vbi.c vbi.h event.h \
	packet.c teletext.c vt.h \
	caption.c cc.h \
	trigger.c trigger.h \
	wss.c wss.h \
	search.c search.h ure.c ure.h \
	cache.c cache.h \
	export.c export.h format.h \
	exp-txt.c exp-txt.h \
	exp-gfx.c exp-gfx.h \
	exp-html.c \
	exp-vtx.c \
	exp-templ.c \
	hamm.c hamm.h \
	lang.c lang.h \
	tables.c tables.h \
	misc.h bcd.h

libzvbiincludedir = $(includedir)
libzvbiinclude_HEADERS = libzvbi.h

libzvbi_la_LIBADD = $(PTHREAD_LIB) $(PNG_LIB)
libzvbi_la_LDFLAGS = -version-info $(LIBZVBI_SO_VERSION)

BUILT_SOURCES = libzvbi.h

LIBZVBI_HDRS = misc.h bcd.h event.h format.h lang.h export.h cache.h search.h \
	   sliced.h decoder.h io.h exp-gfx.h exp-txt.h cc.h vt.h tables.h \
	   vbi.h

# Concat the header files in this order and filter out all lines
#   before "^/* Public", after "^/* Private" and between "^/**"..."**/$"
libzvbi.h: $(LIBZVBI_HDRS)
	sed -ne "1,/\*\//p" $(srcdir)/vbi.h > $@
	echo -e "\n/* Generated file, do not edit! */" >> $@
	echo -e "\n#ifndef __LIBZVBI_H__\n#define __LIBZVBI_H__" >> $@
	echo -e "\n#define VBI_VERSION_MAJOR" `echo @VERSION@ \
	 | sed "s/\./\\\\\n#define VBI_VERSION_MINOR /" \
	 | sed "s/\./\\\\\n#define VBI_VERSION_MICRO /"` >> $@
	echo -e "\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n" >> $@
	echo -e "\ntypedef struct vbi_decoder vbi_decoder;\n" >> $@
	for fi in $(LIBZVBI_HDRS); do \
		echo "/* $$fi */" >> $@; \
		sed -e "/^\/\* Public/,/^\/\* Private/!d;/^\/\* Public/d;/^\/\* Private/d;s/\(\/\*\*.*\*\/\)//;/^[ 	]*\/\*\*/,/\*\/[ 	]*$$/d;" $(srcdir)/$$fi >> $@; \
	done
	echo -e "\n#ifdef __cplusplus\n}\n#endif" >> $@
	echo -e "\n#endif /* __LIBZVBI_H__ */" >> $@
