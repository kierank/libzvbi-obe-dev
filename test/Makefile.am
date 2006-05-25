## Process this file with automake to produce Makefile.in

TESTS = \
	cpptest \
	exoptest \
	hamm \
	raw_decoder \
	test-vps

check_PROGRAMS = \
	cpptest \
	hamm \
	raw_decoder

cpptest_SOURCES = cpptest.cc

check_SCRIPTS = \
	exoptest

# exoptest uses explist, build that first
# XXX requires a make before make dist
exoptest : explist

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

LDADD = \
	$(top_builddir)/src/.libs/libzvbi.a \
	$(LIBS) \
	$(X_LIBS)
