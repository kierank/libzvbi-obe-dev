## Process this file with automake to produce Makefile.in

noinst_PROGRAMS = x11font

EXTRA_DIST = README

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-D_REENTRANT \
	-D_GNU_SOURCE

LDADD = \
	$(top_builddir)/src/.libs/libzvbi.a
	$(LIBS)
