#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="xfce4-screensaver"

(test -f $srcdir/configure.ac) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

which xdt-autogen || {
    echo "You need to install xdt-autogen from the Xfce Git"
    exit 1
}

REQUIRED_AUTOMAKE_VERSION=1.9
XFCE_DATADIR="$xfce_datadir"

. xdt-autogen

