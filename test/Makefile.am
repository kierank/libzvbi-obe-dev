## Process this file with automake to produce Makefile.in

TESTS = exoptest cpptest

if ENABLE_PROXY
proxy_programs = proxy-test
else
proxy_programs =
endif

noinst_PROGRAMS = \
    unicode \
    glyph \
    explist \
    caption \
    export \
    osc \
    capture \
    cpptest \
    $(proxy_programs)

cpptest_SOURCES = cpptest.cc

EXTRA_DIST = $(TESTS) README uclist sim.c

INCLUDES = -I$(top_srcdir) $(COMMON_INCLUDES) $(X_CFLAGS) \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations \
	-D_REENTRANT -D_GNU_SOURCE

LDADD = $(top_builddir)/src/.libs/libzvbi.a $(LIBS) $(X_LIBS)
