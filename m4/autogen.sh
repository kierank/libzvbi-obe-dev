#!/bin/sh
# Run this to generate all the initial makefiles, etc.

DIE=0

if [ -n "$GNOME2_PATH" ]; then
	ACLOCAL_FLAGS="-I $GNOME2_PATH/share/aclocal $ACLOCAL_FLAGS"
	PATH="$GNOME2_PATH/bin:$PATH"
	export PATH
fi

test -n "`autoconf --version </dev/null | grep 2.53`" || {
  echo
  echo "**Error**: You must have 'autoconf 2.53' installed to compile $PACKAGE."
  echo "Download the appropriate package for your distribution,"
  echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
  echo "(a newer version may work but is not tested)"
  DIE=1
}

(grep "^AM_PROG_XML_I18N_TOOLS" $srcdir/configure.in >/dev/null) && {
  (xml-i18n-toolize --version) < /dev/null > /dev/null 2>&1 || {
    echo 
    echo "**Error**: You must have 'xml-i18n-toolize' installed to compile $PACKAGE."
    echo "Get ftp://ftp.gnome.org/pub/GNOME/stable/sources/xml-i18n-tools/xml-i18n-tools-0.6.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
  }
}

(grep "^AM_PROG_LIBTOOL" $srcdir/configure.in >/dev/null) && {
  test -n "`libtool --version </dev/null | grep 1.4`" || {
    echo
    echo "**Error**: You must have 'libtool 1.4.2' installed to compile $PACKAGE."
    echo "Get ftp://ftp.gnu.org/pub/gnu/libtool/libtool-1.4.2.tar.gz"
    echo "(a newer version may work but is not tested)"
    DIE=1
  }
}

grep "^AM_GNU_GETTEXT" $srcdir/configure.in >/dev/null && {
  grep "sed.*POTFILES" $srcdir/configure.in >/dev/null || \
  test -n "`gettext --version </dev/null | grep 0.11`" || {
    echo
    echo "**Error**: You must have 'gettext 0.11.2' installed to compile $PACKAGE."
    echo "Get ftp://ftp.gnu.org/pub/gnu/gettext/gettext-0.11.2.tar.gz"
    echo "(a newer version may work, but is not tested)"
    DIE=1
  }
}

grep "^AM_GNOME_GETTEXT" $srcdir/configure.in >/dev/null && {
  grep "sed.*POTFILES" $srcdir/configure.in >/dev/null || \
  test -n "`gettext --version </dev/null | grep 0.11`" || {
    echo
    echo "**Error**: You must have 'gettext 0.11.2' installed to compile $PACKAGE."
    echo "Get ftp://ftp.gnu.org/pub/gnu/gettext/gettext-0.11.2.tar.gz"
    echo "(a newer version may work, but is not tested)"
    DIE=1
  }
}

test -n "`automake --version </dev/null | grep 1.6`" || {
  echo
  echo "**Error**: You must have 'automake 1.6' installed to compile $PACKAGE."
  echo "Get ftp://ftp.gnu.org/pub/gnu/automake/automake-1.6.tar.gz"
  echo "(a newer version may work, but is not tested)"
  DIE=1
  NO_AUTOMAKE=yes
}

# if no automake, don't bother testing for aclocal
test -n "$NO_AUTOMAKE" || (aclocal --version) < /dev/null > /dev/null 2>&1 || {
  echo
  echo "**Error**: Missing 'aclocal'.  The version of 'automake'"
  echo "installed doesn't appear recent enough."
  echo "Get ftp://ftp.gnu.org/pub/gnu/automake/automake-1.6.tar.gz"
  echo "(a newer version may work, but is not tested)"
  DIE=1
}

if test "$DIE" -eq 1; then
  read -p "Continue? (y/n) " yesno
  test "$yesno" == "y" || exit 1
fi

if test -z "$*"; then
  echo "**Warning**: I am going to run 'configure' with no arguments."
  echo "If you wish to pass any to it, please specify them on the"
  echo $0 "command line."
  echo
fi

case $CC in
xlc )
  am_opt=--include-deps;;
esac

for coin in `find $srcdir -name configure.in -print`
do 
  dr=`dirname $coin`
  if test -f $dr/NO-AUTO-GEN; then
    echo skipping $dr -- flagged as no auto-gen
  else
    echo processing $dr
    macrodirs=`sed -n -e 's,AM_ACLOCAL_INCLUDE(\(.*\)),\1,gp' < $coin`
    ( cd $dr
      macrosdir="`find . -name macros -print` `find . -name macros -print`"
      for i in $macrodirs; do
	if test -f $i/gnome-gettext.m4; then
	  DELETEFILES="$DELETEFILES $i/gnome-gettext.m4"
	fi
      done

      echo "deletefiles is $DELETEFILES"
      aclocalinclude="$ACLOCAL_FLAGS"
      for k in $aclocalinclude; do
  	if test -d $k; then
	  if [ -f $k/gnome.m4 -a "$GNOME_INTERFACE_VERSION" = "1" ]; then
	    rm -f $DELETEFILES
	  fi
        fi
      done
      for k in $macrodirs; do
  	if test -d $k; then
          aclocalinclude="$aclocalinclude -I $k"
	  if [ -f $k/gnome.m4 -a "$GNOME_INTERFACE_VERSION" = "1" ]; then
	    rm -f $DELETEFILES
	  fi
        fi
      done
      if grep "^AM_GNU_GETTEXT" configure.in >/dev/null; then
	if grep "sed.*POTFILES" configure.in >/dev/null; then
	  : do nothing -- we still have an old unmodified configure.in
	else
	  echo "Creating $dr/aclocal.m4 ..."
	  test -r $dr/aclocal.m4 || touch $dr/aclocal.m4
	  echo "Running gettextize...  Ignore non-fatal messages."
	  echo "no" | gettextize $GETTEXTIZE_FLAGS
	  echo "Making $dr/aclocal.m4 writable ..."
	  test -r $dr/aclocal.m4 && chmod u+w $dr/aclocal.m4
        fi
      fi
      if grep "^AM_GNOME_GETTEXT" configure.in >/dev/null; then
	echo "Creating $dr/aclocal.m4 ..."
	test -r $dr/aclocal.m4 || touch $dr/aclocal.m4
	echo "Running gettextize...  Ignore non-fatal messages."
	echo "no" | gettextize --force --copy
	echo "Making $dr/aclocal.m4 writable ..."
	test -r $dr/aclocal.m4 && chmod u+w $dr/aclocal.m4
      fi
      if grep "^AM_PROG_XML_I18N_TOOLS" configure.in >/dev/null; then
        echo "Running xml-i18n-toolize... Ignore non-fatal messages."
	xml-i18n-toolize --copy --force --automake
      fi
      if grep "^AM_PROG_LIBTOOL" configure.in >/dev/null; then
	if test -z "$NO_LIBTOOLIZE" ; then 
	  echo "Running libtoolize..."
	  libtoolize --force --copy
	fi
      fi
      echo "Running aclocal $aclocalinclude ..."
      aclocalmsg=`aclocal $aclocalinclude 2>&1 || echo .`
      if test ! -z "$aclocalmsg" ; then
	echo
	echo "**Error**: aclocal failed. This may mean that you have not"
	echo "installed all of the packages you need, or you may need to"
	echo "set ACLOCAL_FLAGS to include \"-I \$prefix/share/aclocal\""
	echo "for the prefix where you installed the packages whose"
	echo "macros were not found"
	exit 1
      fi

      if grep "^AM_CONFIG_HEADER" configure.in >/dev/null; then
	echo "Running autoheader..."
	autoheader || { echo "**Error**: autoheader failed."; exit 1; }
      fi
      echo "Running automake --gnu $am_opt ..."
      automake --add-missing --gnu $am_opt ||
	{ echo "**Error**: automake failed."; exit 1; }
      echo "Running autoconf ..."
      autoconf || { echo "**Error**: autoconf failed."; exit 1; }
    ) || exit 1
  fi
done

conf_flags="--enable-maintainer-mode --enable-compile-warnings" #--enable-iso-c

if test x$NOCONFIGURE = x; then
  echo Running $srcdir/configure $conf_flags "$@" ...
  $srcdir/configure $conf_flags "$@" || exit 1
else
  echo Skipping configure process.
fi
