noinst_PROGRAMS = x11font

EXTRA_DIST = README

INCLUDES = -I$(top_srcdir)/src $(COMMON_INCLUDES) \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations
LDADD = $(top_builddir)/src/.libs/libzvbi.a \
	$(PTHREAD_LIB) -lm $(PNG_LIB)
