noinst_PROGRAMS = x11font

EXTRA_DIST = README

INCLUDES = -I$(top_srcdir) \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations \
	-D_REENTRANT -D_GNU_SOURCE

LDADD = $(top_builddir)/src/.libs/libzvbi.a $(LIBS)
