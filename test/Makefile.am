TESTS = unicode	glyph explist caption export osc

noinst_PROGRAMS = $(TESTS)

EXTRA_DIST = README uclist

INCLUDES = $(COMMON_INCLUDES) -UNDEBUG \
	-I$(top_srcdir)/src ## libzvbi.h
LDADD = \
	$(top_builddir)/src/.libs/libzvbi.a \
	-lpthread -lm $(PNG_LIB) $(X_LIBS) -lX11
