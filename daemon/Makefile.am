## Process this file with automake to produce Makefile.in

if ENABLE_PROXY

bin_PROGRAMS = proxyd

proxyd_SOURCES = proxyd.c

INCLUDES = -I$(top_srcdir)/src \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

LDADD = $(top_builddir)/src/libzvbi.la

endif
