## Process this file with automake to produce Makefile.in

# This is a blank Makefile.am for using gtk-doc.
# Copy this to your project's API docs directory and modify the variables to
# suit your project. See the GTK+ Makefiles in gtk+/docs/reference for examples
# of using the various options.

builddir=$(top_builddir)/doc

# The name of the module, e.g. 'glib'.
DOC_MODULE=zvbi

# The top-level SGML file. Change it if you want.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting functions and macros.
DOC_SOURCE_DIR=$(top_srcdir)/src

# Extra options to supply to gtkdoc-scan.
SCAN_OPTIONS=

# Extra header files for gtkdoc-scan.
EXTRA_HFILES=$(top_builddir)/src/libzvbi.h

# Extra options to supply to gtkdoc-mkdb.
MKDB_OPTIONS=--sgml-mode

# Extra options to supply to gtkdoc-fixref.
FIXXREF_OPTIONS=

# Used for dependencies.
HFILE_GLOB=$(top_srcdir)/src/*.h
CFILE_GLOB=$(top_srcdir)/src/*.c

# Header files to ignore when scanning.
IGNORE_HFILES=

# Images to copy into HTML directory.
HTML_IMAGES = zvbi_525.gif zvbi_625.gif

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
content_files =

# Other files to distribute.
extra_files = zdoc-scan zvbi_525.ps zvbi_625.ps

# CFLAGS and LDFLAGS for compiling scan program. Only needed if your app/lib
# contains GtkObjects/GObjects and you want to document signals and properties.
GTKDOC_CFLAGS =
GTKDOC_LIBS =

GTKDOC_CC=$(LIBTOOL) --mode=compile $(CC)
GTKDOC_LD=$(LIBTOOL) --mode=link $(CC)

# If you need to override some of the declarations, place them in the
# $(DOC_MODULE)-overrides.txt file and uncomment the second line here.
DOC_OVERRIDES =
#DOC_OVERRIDES = $(DOC_MODULE)-overrides.txt



###########################################################################
# Everything below here is generic and you shouldn't need to change it.
###########################################################################

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

DOC_STAMPS=\
	$(builddir)/scan-build.stamp $(builddir)/tmpl-build.stamp \
	$(builddir)/sgml-build.stamp $(builddir)/html-build.stamp \
	$(builddir)/tmpl.stamp $(builddir)/sgml.stamp $(builddir)/html.stamp

EXTRA_DIST = 				\
	$(content_files)		\
	$(extra_files)			\
	$(HTML_IMAGES)			\
	$(DOC_MAIN_SGML_FILE)		\
	$(DOC_MODULE)-sections.txt	\
	$(DOC_MODULE).types		\
	$(DOC_OVERRIDES)

SCANOBJ_FILES = 		\
	$(DOC_MODULE).args 	\
	$(DOC_MODULE).hierarchy \
	$(DOC_MODULE).signals

if ENABLE_GTK_DOC
all-local: copytree html-build.stamp

#### scan ####

scan-build.stamp: $(HFILE_GLOB)
	@echo '*** Scanning header files ***'
	if grep -l '^..*$$' $(DOC_MODULE).types > /dev/null ; then \
	    CC="$(GTKDOC_CC)" LD="$(GTKDOC_LD)" CFLAGS="$(GTKDOC_CFLAGS)" LDFLAGS="$(GTKDOC_LIBS)" gtkdoc-scanobj --module=$(DOC_MODULE) --output-dir=$(builddir) ; \
	else \
	    for i in $(SCANOBJ_FILES) ; do \
               test -f $$i || touch $$i ; \
	    done \
	fi
	$(top_srcdir)/doc/zdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) --ignore-headers="$(IGNORE_HFILES)" $(SCAN_OPTIONS) $(EXTRA_HFILES)
#	gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) --ignore-headers="$(IGNORE_HFILES)" $(SCAN_OPTIONS) $(EXTRA_HFILES)
	touch scan-build.stamp

$(DOC_MODULE)-decl.txt $(SCANOBJ_FILES): scan-build.stamp
	@true

#### templates ####

tmpl-build.stamp: $(DOC_MODULE)-decl.txt $(SCANOBJ_FILES) $(DOC_MODULE)-sections.txt $(DOC_OVERRIDES)
	@echo '*** Rebuilding template files ***'
	gtkdoc-mktmpl --module=$(DOC_MODULE)
	touch tmpl-build.stamp

tmpl.stamp: tmpl-build.stamp
	@true

#### sgml ####

sgml-build.stamp: tmpl.stamp $(CFILE_GLOB) $(srcdir)/tmpl/*.sgml
	@echo '*** Building SGML ***'
	gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) --main-sgml-file=$(DOC_MAIN_SGML_FILE) $(MKDB_OPTIONS)
	touch sgml-build.stamp

sgml.stamp: sgml-build.stamp
	@true

#### html ####

html-build.stamp: sgml.stamp $(DOC_MAIN_SGML_FILE) $(content_files)
	@echo '*** Building HTML ***'
	test -d html || mkdir html
	(cd html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE))
	test -z "$(HTML_IMAGES)" || (for i in "$(HTML_IMAGES)"; do cp $(srcdir)/$$i html/; done)
	@echo '-- Fixing Crossreferences' 
	gtkdoc-fixxref --module-dir=html --html-dir=$(HTML_DIR) $(FIXXREF_OPTIONS)
	touch html-build.stamp

##############

copytree:
	test "`cd $(srcdir); pwd; cd-`" = "`cd $(builddir); pwd; cd-`" || \
	  cp -R $(srcdir)/tmpl $(srcdir)/$(DOC_MODULE)-docs.sgml $(srcdir)/$(DOC_MODULE)-sections.txt \
	    $(srcdir)/$(DOC_MODULE).types ./
else
all-local:
endif

##############

clean-local:
	rm -f *~ *.bak $(SCANOBJ_FILES) *-unused.txt $(DOC_STAMPS)

maintainer-clean-local: clean
	rm -rf sgml html tmpl $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt

install-data-local:
#	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR)
#	(installfiles=`echo $(builddir)/html/*.html`; \
#	if test "$$installfiles" = '$(builddir)/html/*.html'; \
#	then echo '-- Nothing to install' ; \
#	else \
#	  for i in $$installfiles; do \
#	    echo '-- Installing '$$i ; \
#	    $(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR); \
#	  done; \
#	  echo '-- Installing $(builddir)/html/index.sgml' ; \
#	  $(INSTALL_DATA) $(builddir)/html/index.sgml $(DESTDIR)$(TARGET_DIR); \
#	fi)

#
# Require gtk-doc when making dist
#
if ENABLE_GTK_DOC
dist-check-gtkdoc:
else
dist-check-gtkdoc:
	@echo "*** gtk-doc must be installed and enabled in order to make dist"
	@false
endif

dist-hook: dist-check-gtkdoc dist-hook-local all-local
	mkdir $(distdir)/tmpl
	mkdir $(distdir)/sgml
	mkdir $(distdir)/html
	-cp $(builddir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(builddir)/sgml/*.sgml $(distdir)/sgml
	-cp $(builddir)/html/index.sgml $(distdir)/html
	-cp $(builddir)/html/*.html $(builddir)/html/*.css $(distdir)/html

	test -z "$(HTML_IMAGES)" || (for i in "$(HTML_IMAGES)"; do \
	  cp $(srcdir)/$$i $(distdir)/html; \
	done)

.PHONY : dist-hook-local
