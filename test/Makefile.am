## Process this file with automake to produce Makefile.in

TESTS = \
	cpptest \
	exoptest \
	hamm \
	raw_decoder

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
	explist \
	export \
	glyph \
	unicode \
	wss \
	$(proxy_programs) \
	$(x_programs)

noinst_SCRIPTS = \
	uclist

EXTRA_DIST = \
	README \
	exoptest \
	uclist \
	sim.c

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	$(X_CFLAGS) \
	-D_REENTRANT \
	-D_GNU_SOURCE

LDADD = \
	$(top_builddir)/src/.libs/libzvbi.a \
	$(LIBS) \
	$(X_LIBS)
