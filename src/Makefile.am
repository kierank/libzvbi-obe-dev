## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = libzvbi.la

EXTRA_DIST = wstfont2.xbm ccfont2.xbm

INCLUDES  = -Wall -Wunused -Wmissing-prototypes -Wmissing-declarations
INCLUDES += -D_REENTRANT -D_XOPEN_SOURCE=500 -D_BSD_SOURCE
INCLUDES += $(X_CFLAGS) $(UNICODE_CFLAGS)

libzvbi_la_SOURCES = \
	decoder.c decoder.h sliced.h \
	io.c io.h io-v4l2.c io-v4l.c videodev.h videodev2.h \
	vbi.c vbi.h event.h \
	packet.c teletext.c vt.h \
	caption.c cc.h \
	trigger.c trigger.h \
	search.c search.h ure.c ure.h \
	cache.c cache.h \
	export.c export.h format.h \
	exp-html.c exp-txt.c exp-txt.h \
	exp-vtx.c exp-gfx.c exp-gfx.h exp-templ.c \
	hamm.c hamm.h \
	lang.c lang.h \
	tables.c tables.h bcd.h \
	wss.c wss.h

libzvbiincludedir = $(includedir)
libzvbiinclude_HEADERS = libzvbi.h

libzvbi_la_LIBADD = -lpthread $(PNG_LIB)
libzvbi_la_LDFLAGS = -version-info 1:2:1

# Concat the header files in this order and filter out all lines
#   before "^/* Public", after "^/* Private" and between "^/**"..."**/$"
libzvbi.h: bcd.h event.h format.h lang.h export.h cache.h search.h \
	   sliced.h decoder.h io.h exp-gfx.h exp-txt.h cc.h vt.h tables.h vbi.h
	sed -ne "1,/\*\//p" $(srcdir)/vbi.h > $@
	echo -e "\n/* Generated file, do not edit! */" >> $@
	echo -e "\n#ifndef __LIBZVBI_H__\n#define __LIBZVBI_H__" >> $@
	echo -e "\n#define VBI_VERSION_MAJOR" `echo @VERSION@ \
	 | sed "s/\./\\\\\n#define VBI_VERSION_MINOR /" \
	 | sed "s/\./\\\\\n#define VBI_VERSION_MICRO /"` >> $@
	echo -e "\n#ifdef __cplusplus\nextern "C" {\n#endif\n" >> $@
	echo -e "\ntypedef struct vbi_decoder vbi_decoder;\n" >> $@
	for fi in $^; do \
		echo "/* $$fi */" >> $@; \
		sed -e "/^\/\* Public/,/^\/\* Private/!d;/^\/\* Public/d;/^\/\* Private/d;/^\/\*\*/,/\*\*\/$$/d;" $$fi >> $@; \
	done
	echo -e "\n#ifdef __cplusplus\n}\n#endif" >> $@
	echo -e "\n#endif /* __LIBZVBI_H__ */" >> $@
