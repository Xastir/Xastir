dnl
dnl
dnl ICE_FIND_XmHTML
dnl
dnl Adapted from a macro by Andreas Zeller.
dnl
AC_DEFUN([ICE_FIND_XmHTML],
[
AC_REQUIRE([AC_PATH_XTRA])
xmhtml_includes=
xmhtml_libraries=
AC_ARG_WITH(XmHTML,
[  --without-XmHTML         do not use XmHTML widgets])
dnl Treat --without-XmHTML like
dnl --without-XmHTML-includes --without-XmHTML-libraries.
if test "$with_XmHTML" = "no"
then
xmhtml_includes=none
xmhtml_libraries=none
fi
AC_ARG_WITH(xmhtml-includes,
[  --with-xmhtml-includes=DIR    Motif include files are in DIR],
xmhtml_includes="$withval")
AC_ARG_WITH(xmhtml-libraries,
[  --with-xmhtml-libraries=DIR   Motif libraries are in DIR],
xmhtml_libraries="$withval")
AC_MSG_CHECKING(for XmHTML)
#
#
# Search the include files.
#
if test "$xmhtml_includes" = ""; then
AC_CACHE_VAL(ice_cv_xmhtml_includes,
[
ice_xmhtml_save_LIBS="$LIBS"
ice_xmhtml_save_CFLAGS="$CFLAGS"
ice_xmhtml_save_CPPFLAGS="$CPPFLAGS"
ice_xmhtml_save_LDFLAGS="$LDFLAGS"
#
LIBS="$X_PRE_LIBS -lXm -lXt -lX11 $X_EXTRA_LIBS $LIBS"
CFLAGS="$X_CFLAGS $CFLAGS"
CPPFLAGS="$X_CFLAGS $CPPFLAGS"
LDFLAGS="$X_LIBS $LDFLAGS"
#
AC_TRY_COMPILE([#include <XmHTML/XmHTML.h>],[int a;],
[
# XmHTML/XmHTML.h is in the standard search path.
ice_cv_xmhtml_includes=
],
[
# XmHTML/XmHTML.h is not in the standard search path.
# Locate it and put its directory in `xmhtml_includes'
#
# /usr/include/Motif* are used on HP-UX (Motif).
# /usr/include/X11* are used on HP-UX (X and Athena).
# /usr/dt is used on Solaris (Motif).
# /usr/openwin is used on Solaris (X and Athena).
# Other directories are just guesses.
ice_cv_xmhtml_includes="none"
for dir in "$x_includes" "${prefix}/include" /usr/include /usr/local/include \
           /usr/include/Motif2.0 /usr/include/Motif1.2 /usr/include/Motif1.1 \
           /usr/include/X11R6 /usr/include/X11R5 /usr/include/X11R4 \
           /usr/dt/include /usr/openwin/include \
           /usr/dt/*/include /opt/*/include /usr/include/Motif* \
           /home/XmHTML/include /usr/XmHTML/include /opt/XmHTML/include \
           /home/XmHTML*/include /usr/XmHTML*/include /opt/XmHTML*/include \
           "${prefix}"/*/include /usr/*/include /usr/local/*/include \
           "${prefix}"/include/* /usr/include/* /usr/local/include/*; do
if test -f "$dir/XmHTML/XmHTML.h"; then
        ice_cv_xmhtml_includes="$dir"
        break
fi
done
])
#
LIBS="$ice_xmhtml_save_LIBS"
CFLAGS="$ice_xmhtml_save_CFLAGS"
CPPFLAGS="$ice_xmhtml_save_CPPFLAGS"
LDFLAGS="$ice_xmhtml_save_LDFLAGS"
])
xmhtml_includes="$ice_cv_xmhtml_includes"
fi
#
#
# Now for the libraries.
#
if test "$xmhtml_libraries" = ""; then
AC_CACHE_VAL(ice_cv_xmhtml_libraries,
[
ice_xmhtml_save_LIBS="$LIBS"
ice_xmhtml_save_CFLAGS="$CFLAGS"
ice_xmhtml_save_CPPFLAGS="$CPPFLAGS"
ice_xmhtml_save_LDFLAGS="$LDFLAGS"
#
LIBS="$X_PRE_LIBS -lXmHTML -lXm -lXt -lX11 $X_EXTRA_LIBS $LIBS"
CFLAGS="$X_CFLAGS $CFLAGS"
CPPFLAGS="$X_CFLAGS $CPPFLAGS"
LDFLAGS="$X_LIBS $LDFLAGS"
#
AC_TRY_LINK([#include <XmHTML/XmHTML.h>],[XmCreateHTML();],
[
# libXm.a is in the standard search path.
ice_cv_xmhtml_libraries=
],
[
# libXm.a is not in the standard search path.
# Locate it and put its directory in `xmhtml_libraries'
#
# /usr/lib/Motif* are used on HP-UX (Motif).
# /usr/lib/X11* are used on HP-UX (X and Athena).
# /usr/dt is used on Solaris (Motif).
# /usr/lesstif is used on Linux (Lesstif).
# /usr/openwin is used on Solaris (X and Athena).
# Other directories are just guesses.
ice_cv_xmhtml_libraries="none"
for dir in "$x_libraries" "${prefix}/lib" /usr/lib /usr/local/lib \
           /usr/lib/Motif2.0 /usr/lib/Motif1.2 /usr/lib/Motif1.1 \
           /usr/lib/X11R6 /usr/lib/X11R5 /usr/lib/X11R4 /usr/lib/X11 \
           /usr/dt/lib /usr/openwin/lib \
           /usr/dt/*/lib /opt/*/lib /usr/lib/Motif* \
           /usr/lesstif*/lib /usr/lib/Lesstif* \
           /home/XmHTML/lib /usr/XmHTML/lib /opt/XmHTML/lib \
           /home/XmHTML*/lib /usr/XmHTML*/lib /opt/XmHTML*/lib \
           "${prefix}"/*/lib /usr/*/lib /usr/local/*/lib \
           "${prefix}"/lib/* /usr/lib/* /usr/local/lib/*; do
if test -d "$dir" && test "`ls $dir/libXmHTML.* 2> /dev/null`" != ""; then
        ice_cv_xmhtml_libraries="$dir"
        break
fi
done
])
#
LIBS="$ice_xmhtml_save_LIBS"
CFLAGS="$ice_xmhtml_save_CFLAGS"
CPPFLAGS="$ice_xmhtml_save_CPPFLAGS"
LDFLAGS="$ice_xmhtml_save_LDFLAGS"
])
#
xmhtml_libraries="$ice_cv_xmhtml_libraries"
fi
#
# Provide an easier way to link
#
# Okay
#
# Let's start by making sure that we completely abandon everything related
# to XmHTML installation if either the library or the includes have not been
# located, OR if there was a problem locating the Motif libraries, which are
# required for the use of XmHTML.  The opening three conditions, if true, will
# bypass all XmHTML config operations; that is to say, if any of these
# conditions is true, we call with_xmhtml "no", and that's the end of the
# game.
#
if test "$with_xmhtml" = "no" ; then
        with_xmhtml="no"
elif test "$xmhtml_includes" = "none" ; then
        with_xmhtml="no"
elif test "$xmhtml_libraries" = "none"; then
        with_xmhtml="no"
else
#
# We now have established that we want to use XmHTML. It's time to set up the
# basic environment, and do some discrete tests to set up the environment.
#
# First, let's set with_xmhtml to "yes" (don't know of this is really
# necessary, but we'll be conservative here).  We also send HAVE_XmHTML
# to config.h and the cache file.
#
        AC_DEFINE(HAVE_XmHTML_H)
        with_xmhtml="yes"
#
# Then let's see if the includes were NOT in the default path (if they were,
# we won't be needing an -I to point at the headers, because the compiler
# will find them by itself).  We've already eliminated the possibility of
# "none", so anything other than "" will definitely be a path.
#
#
        if test "$xmhtml_includes" != ""; then
                include_xmhtml="-I$xmhtml_includes"
        fi
#
# Now that that's out of the way, let's deal with libraries.  Here,
# we check again to see if the variable (xmhtml_libraries this time)
# is an empty string, but this time we have work to do whether the
# test is true or false.  We start with the case of an empty
# string, which means we want to link with XmHTML, but don't need
# a path to the library.
#
# This isn't quite happy yet.  A test for the location of the jpeg
# and Xext libraries should be added.
#
        if test "$xmhtml_libraries" = ""; then
                link_xmhtml="-lXmHTML -lXext -ljpeg -lpng -lz"
        else
                link_xmhtml="-L$xmhtml_libraries -lXmHTML -lXext -ljpeg -lpng -lz"
        fi
#
# We now close the enclosing conditional.
#
fi
#
AC_SUBST(include_xmhtml)
AC_SUBST(link_xmhtml)
#
#
#
xmhtml_libraries_result="$xmhtml_libraries"
xmhtml_includes_result="$xmhtml_includes"
test "$xmhtml_libraries_result" = "" && xmhtml_libraries_result="in default path"
test "$xmhtml_includes_result" = "" && xmhtml_includes_result="in default path"
test "$xmhtml_libraries_result" = "none" && xmhtml_libraries_result="(none)"
test "$xmhtml_includes_result" = "none" && xmhtml_includes_result="(none)"
AC_MSG_RESULT(
  [libraries $xmhtml_libraries_result, headers $xmhtml_includes_result])
])dnl
