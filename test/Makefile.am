# We have no automatic tests (make check) yet
TESTS = 

noinst_PROGRAMS = unicode glyph explist caption export osc capture

EXTRA_DIST = README uclist sim.c

INCLUDES = -I$(top_srcdir)/src $(COMMON_INCLUDES) $(X_CFLAGS) \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

LDADD = $(top_builddir)/src/.libs/libzvbi.a \
	-lpthread -lm $(PNG_LIB) $(X_LIBS)
