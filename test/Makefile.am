TESTS = unicode	glyph explist caption export osc capture

noinst_PROGRAMS = $(TESTS)

EXTRA_DIST = README uclist sim.c

INCLUDES = -I$(top_srcdir)/src $(COMMON_INCLUDES) \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations
LDADD = $(top_builddir)/src/.libs/libzvbi.a \
	-lpthread -lm $(PNG_LIB) $(X_LIBS) -lX11
