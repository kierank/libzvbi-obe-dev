## Process this file with automake to produce Makefile.in

if ENABLE_PROXY

noinst_PROGRAMS = proxyd

proxyd_SOURCES = proxyd.c

INCLUDES = -I$(top_srcdir)/src \
	-Wall -Wunused -Wmissing-prototypes -Wmissing-declarations

LDADD = $(top_builddir)/src/.libs/libzvbi.a \
	$(PTHREAD_LIB) $(PNG_LIB) $(UNICODE_LIBS)

endif
