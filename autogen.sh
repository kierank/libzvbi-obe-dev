#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PACKAGE=libzvbi

# automake --copy: no links please (nfs)
GETTEXTIZE_FLAGS="--copy --no-changelog"
am_opt="--copy"

(test -f $srcdir/configure.in) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level directory"
    exit 1
}

. $srcdir/m4/autogen.sh
