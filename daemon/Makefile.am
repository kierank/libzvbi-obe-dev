## Process this file with automake to produce Makefile.in

sbin_PROGRAMS = zvbid
zvbid_SOURCES = proxyd.c
man_MANS = zvbid.1

INCLUDES = -I$(top_srcdir)/src \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

LDADD = $(top_builddir)/src/libzvbi.la $(LIBS)

EXTRA_DIST = $(man_MANS) zvbid.init.in
