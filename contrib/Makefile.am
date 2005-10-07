## Process this file with automake to produce Makefile.in

bin_PROGRAMS = zvbi-ntsc-cc
zvbi_ntsc_cc_SOURCES = ntsc-cc.c

man_MANS = \
	zvbi-ntsc-cc.1

noinst_PROGRAMS = x11font

EXTRA_DIST = \
	$(man_MANS) \
	README

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	$(X_CFLAGS) \
	-D_REENTRANT \
	-D_GNU_SOURCE

LDADD = \
	$(top_builddir)/src/.libs/libzvbi.a \
	$(LIBS) \
	$(X_LIBS)
