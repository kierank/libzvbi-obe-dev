## Process this file with automake to produce Makefile.in

TESTS = exoptest cpptest raw_decoder

check_PROGRAMS = \
    cpptest \
    raw_decoder

cpptest_SOURCES = cpptest.cc

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
    unicode \
    glyph \
    explist \
    export \
    capture \
    $(x_programs) \
    $(proxy_programs)

EXTRA_DIST = README exoptest uclist sim.c

INCLUDES = -I$(top_srcdir) $(COMMON_INCLUDES) $(X_CFLAGS) \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations \
	-D_REENTRANT -D_GNU_SOURCE

LDADD = $(top_builddir)/src/.libs/libzvbi.a $(LIBS) $(X_LIBS)
