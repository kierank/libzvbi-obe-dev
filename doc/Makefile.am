## Process this file with automake to produce Makefile.in

EXTRA_DIST = zvbi_525.gif zvbi_525.ps zvbi_625.gif zvbi_625.ps

if HAVE_DOXYGEN

all-local: html-build.stamp

html-build.stamp: Doxyfile
	doxygen Doxyfile
	touch html-build.stamp

clean-local:
	rm -rf html html-build.stamp

endif
