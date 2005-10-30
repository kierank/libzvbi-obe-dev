## Process this file with automake to produce Makefile.in

if ENABLE_PROXY
proxy_dirs = daemon
else
proxy_dirs =
endif

EXTRA_DIST = \
	BUGS \
	config.rpath \
	depcomp \
	mkinstalldirs \
	zvbi.spec.in

DISTCLEANFILES = site_def.h

# Attn order matters.
SUBDIRS = \
	m4 \
	src \
	$(proxy_dirs) \
	contrib \
	test \
	po \
	doc

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = zvbi-0.2.pc

dist-hook:
	chown -R 500:100 $(distdir)
