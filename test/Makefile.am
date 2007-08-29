## Process this file with automake to produce Makefile.in

TESTS = \
	cpptest \
	exoptest \
	hamm \
	raw_decoder \
	test-dvb_mux \
	test-vps

check_PROGRAMS = \
	cpptest \
	explist \
	hamm \
	raw_decoder \
	test-dvb_mux

cpptest_SOURCES = cpptest.cc
test_dvb_mux_SOURCES = test-dvb_mux.cc

check_SCRIPTS = \
	exoptest

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
	exoptest \
	uclist

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
