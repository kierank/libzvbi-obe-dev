## Process this file with automake to produce Makefile.in

bin_PROGRAMS = zvbi-chains
zvbi_chains_SOURCES = chains.c

sbin_PROGRAMS = zvbid
zvbid_SOURCES = proxyd.c

man_MANS = zvbid.1 zvbi-chains.1

INCLUDES = -I$(top_srcdir) \
	-DLIBZVBI_CHAINS_PATH=\"libzvbi-chains.so\" \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations \
	-D_REENTRANT -D_GNU_SOURCE

LDADD = $(top_builddir)/src/libzvbi.la $(LIBS)

EXTRA_DIST = $(man_MANS) zvbid.init.in
