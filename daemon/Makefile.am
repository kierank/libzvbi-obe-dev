## Process this file with automake to produce Makefile.in

if ENABLE_PROXY

sbin_PROGRAMS = zvbid

zvbid_SOURCES = proxyd.c

INCLUDES = -I$(top_srcdir)/src \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

LDADD = $(top_builddir)/src/libzvbi.la

EXTRA_DIST = zvbid.init.in

endif
