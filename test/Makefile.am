## Process this file with automake to produce Makefile.in

TESTS = \
	cpptest \
	exoptest \
	hamm \
	test-raw_decoder \
	test-dvb_mux \
	test-unicode \
	test-vps

check_PROGRAMS = \
	cpptest \
	explist \
	hamm \
	test-raw_decoder \
	test-dvb_mux

cpptest_SOURCES = cpptest.cc

test_dvb_demux_SOURCES = \
	test-dvb_demux.cc \
	test-common.cc test-common.h

test_dvb_mux_SOURCES = \
	test-dvb_mux.cc \
	test-common.cc test-common.h

test_raw_decoder_SOURCES = \
	test-raw_decoder.cc \
	test-common.cc test-common.h

test-unicode: unicode

check_SCRIPTS = \
	exoptest \
	test-unicode

if ENABLE_PROXY
proxy_programs = proxy-test
else
proxy_programs =
endif

if HAVE_X
x_programs = caption osc
else
x_programs =
endif

noinst_PROGRAMS = \
	capture \
	decode \
	explist \
	export \
	glyph \
	sliced2pes \
	test-vps \
	ttxfilter \
	unicode \
	$(proxy_programs) \
	$(x_programs)

capture_SOURCES = \
	capture.c \
	sliced.c sliced.h

caption_SOURCES = \
	caption.c \
	sliced.c sliced.h

decode_SOURCES = \
	decode.c \
	sliced.c sliced.h

export_SOURCES = \
	export.c \
	sliced.c sliced.h

sliced2pes_SOURCES = \
	sliced2pes.c \
	sliced.c sliced.h

ttxfilter_SOURCES = \
	ttxfilter.c \
	sliced.c sliced.h

test_vps_SOURCES = test-vps.c

noinst_SCRIPTS = \
	uclist

EXTRA_DIST = \
	README \
	$(noinst_SCRIPTS) \
	$(check_SCRIPTS) \
	unicode-out-ref.txt

CLEANFILES = \
	unicode-out.txt

AM_CFLAGS = \
	-I$(top_srcdir) \
	$(X_CFLAGS) \
	-D_REENTRANT \
	-D_GNU_SOURCE

AM_CPPFLAGS = $(AM_CFLAGS)

# Libtool creates a wrapper script around the application until the
# library is installed, which is a major annoyance for debugging with
# gdb and valgrind. So we link these apps statically if possible.
if BUILD_STATIC_LIB
LDADD = $(top_builddir)/src/.libs/libzvbi.a
else
LDADD = $(top_builddir)/src/libzvbi.la
endif

LDADD += \
	$(LIBS) \
	$(X_LIBS)

unrename:
	for file in *.cc *.c *.h ; do \
	  case "$$file" in \
	  test-raw_decoder.cc|osc.c) continue ; ;; \
	  esac ; \
	  if grep -q -i vbi3_ $$file ; then \
	    sed 's/vbi3_/vbi_/g;s/VBI3_/VBI_/g' <$$file >tmp ; \
	    mv tmp $$file ; \
	  fi ; \
	done
