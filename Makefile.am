## Process this file with automake to produce Makefile.in

if ENABLE_PROXY
proxy_dirs = daemon
else
proxy_dirs =
endif

EXTRA_DIST = mkinstalldirs config.rpath BUGS zvbi.spec.in zvbi.spec depcomp

DISTCLEANFILES = site_def.h

SUBDIRS = m4 src $(proxy_dirs) po test doc contrib

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = zvbi-0.2.pc
