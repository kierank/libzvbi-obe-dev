## Process this file with automake to produce Makefile.in

TESTS = exoptest cpptest
noinst_PROGRAMS = unicode glyph explist caption export osc capture cpptest
cpptest_SOURCES = cpptest.cc

EXTRA_DIST = $(TESTS) README uclist sim.c

INCLUDES = -I$(top_srcdir)/src $(COMMON_INCLUDES) $(X_CFLAGS) \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

LDADD = $(top_builddir)/src/.libs/libzvbi.a \
	-lpthread -lm $(PNG_LIB) $(X_LIBS)
