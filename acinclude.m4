# acinclude.m4 for Xastir
#
# Copyright (C) 2000-2004  The Xastir Group
#
# $Id$

# test for devices.  Avoid the tests on Cygwin as they hang on some
# WinXP boxes.
#
AC_DEFUN([XASTIR_DETECT_DEVICES],
[
AC_MSG_CHECKING([for devices])
if test -d /proc/registry ; then
ac_tnc_port=/dev/ttyS0
ac_gps_port=/dev/ttyS1
elif test -c /dev/cuaa0 ; then
ac_tnc_port=/dev/cuaa0
ac_gps_port=/dev/cuaa1
elif test -c /dev/ttyS0 ; then
ac_tnc_port=/dev/ttyS0
ac_gps_port=/dev/ttyS1
elif test -c /dev/cua/a ; then
ac_tnc_port=/dev/cua/a
ac_gps_port=/dev/cua/b
else
ac_tnc_port=none
ac_gps_port=none
fi

AC_DEFINE_UNQUOTED([TNC_PORT], "$ac_tnc_port", [Default TNC port.])
AC_DEFINE_UNQUOTED([GPS_PORT], "$ac_gps_port", [Default GPS port.])
AC_MSG_RESULT(found $ac_tnc_port and $ac_gps_port)
])

# add search paths
AC_DEFUN([XASTIR_ADD_SEARCH_PATHS],
[
AC_MSG_CHECKING([for search paths])

test -d /usr/local/include && CPPFLAGS="-I/usr/local/include $CPPFLAGS"
test -d /usr/local/lib && LDFLAGS="-L/usr/local/lib $LDFLAGS"

for d in /sw /opt /opt/local /usr/dt/share /usr/sfw /opt/sfw; do
test -d $d/include && CPPFLAGS="$CPPFLAGS -I$d/include"
test -d $d/lib && LDFLAGS="$LDFLAGS -L$d/lib"
done

AC_MSG_RESULT([done])
])

# add compiler flags
AC_DEFUN([XASTIR_COMPILER_FLAGS],
[
AC_MSG_CHECKING([for compiler flags])
# notes -- gcc only! HPUX doesn't work for this so no "-Ae +O2"
# everybody likes "-g -O2", right?

for f in -g -O2; do
# eventually write a test for these
# gcc already checks for -g earlier!
echo $CFLAGS | grep -- $f - > /dev/null || CFLAGS="$CFLAGS $f"
done

# brutal!
# check for sed maybe?
if test "$ac_cv_prog_ac_ct_CC" = "gcc"; then
gcc --help | sed -e "/^[^ ]/d" -e "/^ [^ ]/d" -e "/^  [^-]/d" -e "s/  //" -e "s/ .*//" > gccflags

# I need a test for -Wno-return-type and -DFUNCPROTO=15
# before adding them

for f in -no-cpp-precomp -pipe; do
grep -- $f gccflags > /dev/null && CFLAGS="$CFLAGS $f"
done

# delete temporary file
rm -f gccflags

# add any other flags that aren't added earlier
for f in -Wall; do
echo $CFLAGS | grep -- $f - > /dev/null || CFLAGS="$CFLAGS $f"
done

# end gcc-specific checks
fi

# add any pthread flags now
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

AC_MSG_RESULT(using $CFLAGS)
])

# set XASTIR_SYSTEM
# JMT - is this really necessary?
AC_DEFUN([XASTIR_SET_SYSTEM],
[
AC_MSG_CHECKING([for system])

case "$host_os" in
cygwin*)
system=Cygnus
;;
darwin*)
system=Darwin
;;
freebsd*)
system=FreeBSD
;;
hpux*)
system=HP/UX
;;
linux*)
system=Linux
;;
netbsd*)
system=NetBSD
;;
openbsd*)
system=OpenBSD
;;
solaris*)
system=Solaris
;;
*)
system=unknown
;;
esac

AC_DEFINE_UNQUOTED([XASTIR_SYSTEM], "$system", [Define system type.])
AC_MSG_RESULT($system)
])

AC_DEFUN([XASTIR_DETECT_BINARIES],
[
BINPATH=$PATH

for d in / /usr /usr/local /usr/X11 /usr/X11R6 /usr/sfw /opt/sfw /sw; do
test -d $d/bin && echo $BINPATH | grep -- $d/bin - > /dev/null || BINPATH="$BINPATH:$d/bin"
done

# it would be much nicer to do this in a for loop
AC_PATH_PROG(wget, [wget --version], no, $BINPATH)
if test "$wget" != "no"; then
  AC_DEFINE_UNQUOTED(HAVE_WGET, 1, [Define if you have wget]) 
  AC_DEFINE_UNQUOTED(WGET_PATH, "${wget}", [Path to wget]) 
fi

AC_PATH_PROG(convert, [convert --version], no, $BINPATH)
AC_CHECK_FILE(/usr/bin/convert.exe, convert="/usr/bin/convert")
if test "$convert" != "no"; then
  AC_DEFINE_UNQUOTED(HAVE_CONVERT, 1, [Define if you have convert]) 
  AC_DEFINE_UNQUOTED(CONVERT_PATH, "${convert}", [Path to convert]) 
fi
 
AC_PATH_PROG(lpr, [lpr /dev/null], no, $BINPATH)
if test "$lpr" != "no"; then
  AC_DEFINE_UNQUOTED(HAVE_LPR, 1, [Define if you have lpr]) 
  AC_DEFINE_UNQUOTED(LPR_PATH, "${lpr}", [Path to lpr]) 
fi
 
AC_PATH_PROG(gv, [gv --version], no, $BINPATH)
if test "$gv" != "no"; then
  AC_DEFINE_UNQUOTED(HAVE_GV, 1, [Define if you have gv]) 
  AC_DEFINE_UNQUOTED(GV_PATH, "${gv}", [Path to gv]) 
fi
 
AC_PATH_PROG(cp, [cp], no, $BINPATH)
if test "$cp" != "no"; then
  AC_DEFINE_UNQUOTED(HAVE_CP, 1, [Define if you have cp]) 
  AC_DEFINE_UNQUOTED(CP_PATH, "${cp}", [Path to cp]) 
fi
 
AC_PATH_PROG(cat, [cat], no, $BINPATH)
if test "$cat" != "no"; then
  AC_DEFINE_UNQUOTED(HAVE_CAT, 1, [Define if you have cat])
  AC_DEFINE_UNQUOTED(CAT_PATH, "${cat}", [Path to cat])
fi

if test "$use_festival" != "no"; then 
 AC_PATH_PROG(festival, [festival], no, $BINPATH)
 if test "$festival" != "no"; then
   AC_DEFINE_UNQUOTED(HAVE_FESTIVAL, 1, [Define if you have festival])
   AC_DEFINE_UNQUOTED(FESTIVAL_PATH, "${festival}", [Path to festival])
 fi
fi

if test "$use_gpsman" != "no"; then
 AC_PATH_PROG(gpsman, [gpsman haslib gpsmanshp], no, $BINPATH)
 if test "$gpsman" != "no"; then
   AC_DEFINE_UNQUOTED(HAVE_GPSMAN, 1, [Define if you have gpsman])
   AC_DEFINE_UNQUOTED(GPSMAN_PATH, "${gpsman}", [Path to gpsman])
 fi
fi

if test "$use_err_popups" != "no"; then
 AC_DEFINE_UNQUOTED(HAVE_ERROR_POPUPS, 1, [Define if you have error popups enabled])
fi

])

# JMT - this is pure evil and will not be edited at the present time
AC_DEFUN([XASTIR_CHECK_IMAGEMAGICK],
[

# Check for ImageMagick 
# 
save_cppflags="$CPPFLAGS" 
save_cxxflags="$CXXFLAGS" 
save_libs="$LIBS" 
save_ldflags="$LDFLAGS" 
#
# First look for the needed Magick-config script, which tells us all
# of the build options we need.
#
AC_CHECK_PROG(use_imagemagick, [Magick-config --version], yes, no) 
if test "$use_imagemagick" = "yes"; then
  MAGIC_BIN="Magick-config"
else
  #
  # Test for MacOSX/Fink directories under "/sw".
  #
  AC_CHECK_FILE(/sw/bin/Magick-config, use_imagemagick="yes") 
  if test "$use_imagemagick" = "yes"; then
    MAGIC_BIN="/sw/bin/Magick-config"
  else
    AC_MSG_WARN(*** Cannot find Magick-config:  Building w/o ImageMagick support. ***) 
  fi
fi
#
if test "$use_imagemagick" = "yes"; then
  #
  # Compute the ImageMagick revision number
  #
  magickversion=`${MAGIC_BIN} --version` 
  magickmajor=`echo $magickversion | cut -d '.' -f 1` 
  magickminor=`echo $magickversion | cut -d '.' -f 2` 
  magicktiny=`echo $magickversion | cut -d '.' -f 3` 
  if test "$magickmajor" -lt 5; then 
    magickold="yes"; 
  elif test "$magickmajor" -eq 5 -a "$magickminor" -lt 4; then 
    magickold="yes"; 
  elif test "$magickmajor" -eq 5 -a "$magickminor" -eq 4 -a "$magicktiny" -lt 9; then 
    magickold="yes"; 
  fi 
  #
  # Figure out the build options using the Magick-config script
  #
  CPPFLAGS="$CPPFLAGS `${MAGIC_BIN} --cppflags`" 
  CXXFLAGS="$CXXFLAGS `${MAGIC_BIN} --cflags`" 
  LDFLAGS="$LDFLAGS `${MAGIC_BIN} --ldflags`" 
  LIBS="${MAGIC_LIB_DIR} `${MAGIC_BIN} --libs` $LIBS" 
  # 
  # For the case of apple-darwin, we don't want to check the
  # headers/libraries 'cuz the standard macros won't find them anyway.
  #
  if test "$darwin" = "yes"; then
    AC_DEFINE(HAVE_IMAGEMAGICK, 1, [Imagemagick image library])
  else
    AC_CHECK_HEADER(magick/api.h, use_imagemagick="yes", use_imagemagick="no")
    if test "$use_imagemagick" = "no"; then
      AC_MSG_WARN(*** Cannot find ImageMagick include files:  Building w/o ImageMagick support. ***)
    else
      AC_CHECK_LIB([Magick], [WriteImage], AC_DEFINE(HAVE_IMAGEMAGICK, 1, [Imagemagick image library]), use_imagemagick="no")
      if test "$use_imagemagick" = "no"; then
        AC_MSG_WARN(*** Cannot find ImageMagick library files:  Building w/o ImageMagick support. ***)
      fi
    fi
  fi
  #
#  if test "$magickold" = "yes"; then 
# This used to be important, as some versions didn't support the
# Tigermap intensity slider.
#    AC_MSG_WARN(*********************************************************)
#    AC_MSG_WARN(***     Old ImageMagick version ($magickversion) found.        ***)
#    AC_MSG_WARN(*** Upgrade to 5.4.9 or newer for full functionality. ***)
#    AC_MSG_WARN(*********************************************************) 
#  fi 
else
  #
  # No ImageMagick found.  Restore variables.
  #
  CPPFLAGS=$save_cppflags 
  CXXFLAGS=$save_cxxflags 
  LIBS=$save_libs 
  LDFLAGS=$save_ldflags 
fi 
# 
# End of ImageMagick checks 

])

# things grabbed elsewhere

# this is from Squeak-3.2-4's acinclude.m4
AC_DEFUN([AC_CHECK_GMTOFF],
[AC_CACHE_CHECK([for gmtoff in struct tm], ac_cv_tm_gmtoff,
  AC_TRY_COMPILE([#include <time.h>],[struct tm tm; tm.tm_gmtoff;],
    ac_cv_tm_gmtoff="yes", ac_cv_tm_gmtoff="no"))
test "$ac_cv_tm_gmtoff" != "no" && AC_DEFINE(HAVE_TM_GMTOFF,,X)])

dnl Available from the GNU Autoconf Macro Archive at:
dnl http://www.gnu.org/software/ac-archive/htmldoc/acx_pthread.html
dnl
AC_DEFUN([ACX_PTHREAD], [
AC_REQUIRE([AC_CANONICAL_HOST])
AC_LANG_SAVE
AC_LANG_C
acx_pthread_ok=no

# We used to check for pthread.h first, but this fails if pthread.h
# requires special compiler flags (e.g. on True64 or Sequent).
# It gets checked for in the link test anyway.

# First of all, check if the user has set any of the PTHREAD_LIBS,
# etcetera environment variables, and if threads linking works using
# them:
if test x"$PTHREAD_LIBS$PTHREAD_CFLAGS" != x; then
        save_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
        save_LIBS="$LIBS"
        LIBS="$PTHREAD_LIBS $LIBS"
        AC_MSG_CHECKING([for pthread_join in LIBS=$PTHREAD_LIBS with CFLAGS=$PTHREAD_CFLAGS])
        AC_TRY_LINK_FUNC(pthread_join, acx_pthread_ok=yes)
        AC_MSG_RESULT($acx_pthread_ok)
        if test x"$acx_pthread_ok" = xno; then
                PTHREAD_LIBS=""
                PTHREAD_CFLAGS=""
        fi
        LIBS="$save_LIBS"
        CFLAGS="$save_CFLAGS"
fi

# We must check for the threads library under a number of different
# names; the ordering is very important because some systems
# (e.g. DEC) have both -lpthread and -lpthreads, where one of the
# libraries is broken (non-POSIX).

# Create a list of thread flags to try.  Items starting with a "-" are
# C compiler flags, and other items are library names, except for "none"
# which indicates that we try without any flags at all.

acx_pthread_flags="pthreads none -Kthread -kthread lthread -pthread -pthreads -mthreads pthread --thread-safe -mt"

# The ordering *is* (sometimes) important.  Some notes on the
# individual items follow:

# pthreads: AIX (must check this before -lpthread)
# none: in case threads are in libc; should be tried before -Kthread and
#       other compiler flags to prevent continual compiler warnings
# -Kthread: Sequent (threads in libc, but -Kthread needed for pthread.h)
# -kthread: FreeBSD kernel threads (preferred to -pthread since SMP-able)
# lthread: LinuxThreads port on FreeBSD (also preferred to -pthread)
# -pthread: Linux/gcc (kernel threads), BSD/gcc (userland threads)
# -pthreads: Solaris/gcc
# -mthreads: Mingw32/gcc, Lynx/gcc
# -mt: Sun Workshop C (may only link SunOS threads [-lthread], but it
#      doesn't hurt to check since this sometimes defines pthreads too;
#      also defines -D_REENTRANT)
# pthread: Linux, etcetera
# --thread-safe: KAI C++

case "${host_cpu}-${host_os}" in
        *solaris*)

        # On Solaris (at least, for some versions), libc contains stubbed
        # (non-functional) versions of the pthreads routines, so link-based
        # tests will erroneously succeed.  (We need to link with -pthread or
        # -lpthread.)  (The stubs are missing pthread_cleanup_push, or rather
        # a function called by this macro, so we could check for that, but
        # who knows whether they'll stub that too in a future libc.)  So,
        # we'll just look for -pthreads and -lpthread first:

        acx_pthread_flags="-pthread -pthreads pthread -mt $acx_pthread_flags"
        ;;
esac

if test x"$acx_pthread_ok" = xno; then
for flag in $acx_pthread_flags; do

        case $flag in
                none)
                AC_MSG_CHECKING([whether pthreads work without any flags])
                ;;

                -*)
                AC_MSG_CHECKING([whether pthreads work with $flag])
                PTHREAD_CFLAGS="$flag"
                ;;

                *)
                AC_MSG_CHECKING([for the pthreads library -l$flag])
                PTHREAD_LIBS="-l$flag"
                ;;
        esac

        save_LIBS="$LIBS"
        save_CFLAGS="$CFLAGS"
        LIBS="$PTHREAD_LIBS $LIBS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

        # Check for various functions.  We must include pthread.h,
        # since some functions may be macros.  (On the Sequent, we
        # need a special flag -Kthread to make this header compile.)
        # We check for pthread_join because it is in -lpthread on IRIX
        # while pthread_create is in libc.  We check for pthread_attr_init
        # due to DEC craziness with -lpthreads.  We check for
        # pthread_cleanup_push because it is one of the few pthread
        # functions on Solaris that doesn't have a non-functional libc stub.
        # We try pthread_create on general principles.
        AC_TRY_LINK([#include <pthread.h>],
                    [pthread_t th; pthread_join(th, 0);
                     pthread_attr_init(0); pthread_cleanup_push(0, 0);
                     pthread_create(0,0,0,0); pthread_cleanup_pop(0); ],
                    [acx_pthread_ok=yes])

        LIBS="$save_LIBS"
        CFLAGS="$save_CFLAGS"

        AC_MSG_RESULT($acx_pthread_ok)
        if test "x$acx_pthread_ok" = xyes; then
                break;
        fi

        PTHREAD_LIBS=""
        PTHREAD_CFLAGS=""
done
fi

# Various other checks:
if test "x$acx_pthread_ok" = xyes; then
        save_LIBS="$LIBS"
        LIBS="$PTHREAD_LIBS $LIBS"
        save_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

        # Detect AIX lossage: threads are created detached by default
        # and the JOINABLE attribute has a nonstandard name (UNDETACHED).
        AC_MSG_CHECKING([for joinable pthread attribute])
        AC_TRY_LINK([#include <pthread.h>],
                    [int attr=PTHREAD_CREATE_JOINABLE;],
                    ok=PTHREAD_CREATE_JOINABLE, ok=unknown)
        if test x"$ok" = xunknown; then
                AC_TRY_LINK([#include <pthread.h>],
                            [int attr=PTHREAD_CREATE_UNDETACHED;],
                            ok=PTHREAD_CREATE_UNDETACHED, ok=unknown)
        fi
        if test x"$ok" != xPTHREAD_CREATE_JOINABLE; then
                AC_DEFINE(PTHREAD_CREATE_JOINABLE, $ok,
                          [Define to the necessary symbol if this constant
                           uses a non-standard name on your system.])
        fi
        AC_MSG_RESULT(${ok})
        if test x"$ok" = xunknown; then
                AC_MSG_WARN([we do not know how to create joinable pthreads])
        fi

        AC_MSG_CHECKING([if more special flags are required for pthreads])
        flag=no
        case "${host_cpu}-${host_os}" in
                *-aix* | *-freebsd*)     flag="-D_THREAD_SAFE";;
                *solaris* | *-osf* | *-hpux*) flag="-D_REENTRANT";;
        esac
        AC_MSG_RESULT(${flag})
        if test "x$flag" != xno; then
                PTHREAD_CFLAGS="$flag $PTHREAD_CFLAGS"
        fi

        LIBS="$save_LIBS"
        CFLAGS="$save_CFLAGS"

        # More AIX lossage: must compile with cc_r
        AC_CHECK_PROG(PTHREAD_CC, cc_r, cc_r, ${CC})
else
        PTHREAD_CC="$CC"
fi

AC_SUBST(PTHREAD_LIBS)
AC_SUBST(PTHREAD_CFLAGS)
AC_SUBST(PTHREAD_CC)

# Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
if test x"$acx_pthread_ok" = xyes; then
        ifelse([$1],,AC_DEFINE(HAVE_PTHREAD,1,[Define if you have POSIX threads libraries and header files.]),[$1])
        :
else
        acx_pthread_ok=no
        $2
fi
AC_LANG_RESTORE
])dnl ACX_PTHREAD


#
AC_DEFUN([XASTIR_PATH_MOTIF], [
# New stuff to check for Motif/Lesstif.  Shamelessly borrowed from
# the opendx project. Opendx in turn snarfed their test from AC_PATH_X.

# Allow "--with-motif-includes" and "--with-motif-libs" so user can 
# force a specific set of includes.
AC_ARG_WITH(motif-includes, [  --with-motif-includes   set path for motif includes (default none)],[with_motif_includes=$withval], [with_motif_includes=])
if test "$with_motif_includes" != "yes" && test -z "$with_motif_includes"
then
        with_motif_includes=
fi

AC_ARG_WITH(motif-libs, [  --with-motif-libs       set path for motif libraries (default none)],[with_motif_libs=$withval], [with_motif_libs=])
if test "$with_motif_libs" != "yes" && test -z "$with_motif_libs"
then
  with_motif_libs=
fi

  # Guess where to find include files, by looking for this one Xm .h file.
  test -z "$xm_direct_test_include" && xm_direct_test_include=Xm/Xm.h

  # First, try using that file with no special directory specified.
AC_MSG_CHECKING([for Motif headers])
AC_PREPROC_IFELSE(
 [AC_LANG_SOURCE([[#include <$xm_direct_test_include>]])],
 [
  # We can compile using X headers with no special include directory.
  xm_includes=
  AC_MSG_RESULT([in default path])
 ],[
# that test didn't work, we need to hunt a little
# Look for the header file in a standard set of common directories.
# Check X11 before X11Rn because it is often a symlink to the current release.
  for ac_dir in               \
    /usr/X11/include          \
    /usr/X11R6/include        \
    /usr/X11R5/include        \
    /usr/X11R4/include        \
                              \
    /usr/include/X11          \
    /usr/include/X11R6        \
    /usr/include/X11R5        \
    /usr/include/X11R4        \
                              \
    /usr/local/X11/include    \
    /usr/local/X11R6/include  \
    /usr/local/X11R5/include  \
    /usr/local/X11R4/include  \
                              \
    /usr/local/include/X11    \
    /usr/local/include/X11R6  \
    /usr/local/include/X11R5  \
    /usr/local/include/X11R4  \
                              \
    /usr/X386/include         \
    /usr/x386/include         \
    /usr/XFree86/include/X11  \
                              \
    /usr/include              \
    /usr/local/include        \
    /usr/unsupported/include  \
    /usr/athena/include       \
    /usr/local/x11r5/include  \
    /usr/lpp/Xamples/include  \
                              \
    /usr/openwin/include      \
    /usr/openwin/share/include \
    "$with_motif_includes"    \
    ; \
  do
    if test -r "$ac_dir/$xm_direct_test_include"; then
      xm_includes=$ac_dir
      AC_MSG_RESULT([in $xm_includes])
      break
    fi
  done
  if test "x$xm_includes" = "x"; then
    AC_MSG_ERROR([**** NO MOTIF HEADERS FOUND **** install Motif  development headers or use --with-motif-includes to specify location of Xm/Xm.h ])
  fi
])
  # Check for the libraries.

  AC_MSG_CHECKING([for Motif libraries])
  test -z "$xm_direct_test_library" && xm_direct_test_library=Xm
  test -z "$xm_direct_test_function" && xm_direct_test_function=XmGetDestination

  # See if we find them without any special options.
  # Don't add to $LIBS permanently.
  ac_save_LIBS="$LIBS"
  LIBS="-l$xm_direct_test_library $LIBS"
AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[${xm_direct_test_function}()]])],
 [
  LIBS="$ac_save_LIBS"
  # We can link Motif programs with no special library path.
  xm_libraries=
  AC_MSG_RESULT([in default path])
 ],[
 LIBS="$ac_save_LIBS"
# First see if replacing the include by lib works.
# Check X11 before X11Rn because it is often a symlink to the current release.
for ac_dir in `echo "$xm_includes" | sed s/include/lib/` \
    /usr/X11/lib          \
    /usr/X11R6/lib        \
    /usr/X11R5/lib        \
    /usr/X11R4/lib        \
                          \
    /usr/lib/X11          \
    /usr/lib/X11R6        \
    /usr/lib/X11R5        \
    /usr/lib/X11R4        \
                          \
    /usr/local/X11/lib    \
    /usr/local/X11R6/lib  \
    /usr/local/X11R5/lib  \
    /usr/local/X11R4/lib  \
                          \
    /usr/local/lib/X11    \
    /usr/local/lib/X11R6  \
    /usr/local/lib/X11R5  \
    /usr/local/lib/X11R4  \
                          \
    /usr/X386/lib         \
    /usr/x386/lib         \
    /usr/XFree86/lib/X11  \
                          \
    /usr/lib              \
    /usr/local/lib        \
    /usr/unsupported/lib  \
    /usr/athena/lib       \
    /usr/local/x11r5/lib  \
    /usr/lpp/Xamples/lib  \
    /lib/usr/lib/X11	  \
                          \
    /usr/openwin/lib      \
    /usr/openwin/share/lib \
    "$with_motif_libs"     \
    ; \
do
dnl Don't even attempt the hair of trying to link an X program!
  for ac_extension in a so sl; do
    if test -r $ac_dir/lib${xm_direct_test_library}.$ac_extension; then
      xm_libraries=$ac_dir
      AC_MSG_RESULT([in $xm_libraries])
      break 2
    fi
  done
done
if test "x$xm_libraries" = "x"; then
 AC_MSG_ERROR([**** MOTIF LIBRARIES NOT FOUND **** Install Motif development headers/libraries or use --with-motif-libraries to specify path to libXm.a ])
fi
])
])

# From Cyrus imap distribution (KB3EGH)
dnl $Id$

dnl These are the Cyrus Berkeley DB macros.  In an ideal world these would be
dnl identical to the above.

dnl They are here so that they can be shared between Cyrus IMAPd
dnl and Cyrus SASL with relative ease.

dnl The big difference between this and the ones above is that we don't assume
dnl that we know the name of the library, and we try a lot of permutations
dnl instead.  We also assume that DB4 is acceptable.

dnl When we're done, there will be a BDB_LIBADD and a BDB_INCADD which should
dnl be used when necessary.  We should probably be smarter about our RPATH
dnl handling.

dnl Call these with XASTIR_BERKELEY_DB_CHK.

dnl We will also set $dblib to "berkeley" if we are successful, "no" otherwise.

dnl this is unbelievably painful due to confusion over what db-3 should be
dnl named and where the db-3 header file is located.  arg.
AC_DEFUN([XASTIR_BERKELEY_DB_CHK_LIB],
[
	BDB_SAVE_LDFLAGS=$LDFLAGS

	if test -d $with_bdb_lib; then
	    XASTIR_ADD_LIBPATH_TO($with_bdb_lib, LDFLAGS)
	    XASTIR_ADD_LIBPATH_TO($with_bdb_lib, BDB_LIBADD)
	else
	    BDB_LIBADD=""
	fi

	saved_LIBS=$LIBS
# Removed db-3.3 db3.3 db33 db-3.2 db3.2 db32 db-3.1 db3.1 db31 db-3 db30 db3 
# from the probe.  The map_cache.c code explicitly bombs if it doesn't have
# version 4 or above, so why probe for version 3?

# it would be nice if this could be done with AC_SEARCH_LIBS but that doesn't
# work as it appears that there is some C++-type name mangling going on,
# and just probing for a library that contains "db_create" fails.  One needs
# to specify the function call with the full prototype for it to be found.
        BDB_LIB_FOUND="none"
        AC_MSG_CHECKING([for a library containing db_create])
        for dbname in db-4.2 db4.2 db42 db-4.1 db4.1 db41 db-4.0 db4.0 db-4 db40 db4 db
          do
	    LIBS="$saved_LIBS -l$dbname"
	    AC_TRY_LINK(
            [#include <db.h>],
	    [db_create(NULL, NULL, 0);],
	    [BDB_LIBADD="$BDB_LIBADD -l$dbname"; dblib="berkeley"; dbname=db;
                BDB_LIB_FOUND="-l$dbname"],
            dblib="no")
#         STOP if we find one.  Otherwise we'll keep stepping through the 
#         list and resetting dblib to "no" over and over.
          if test $dblib = "berkeley" ; then
            break;
          fi
          done
        AC_MSG_RESULT([$BDB_LIB_FOUND])

# Commented out because the map_cache code is not actually set up to use
# db_open instead of db_create.  Probing in this way could actually be 
# dangerous.
#        if test "$dblib" = "no"; then
#	    LIBS="$saved_LIBS -ldb"
#	    AC_TRY_LINK([#include <db.h>],
#	    [db_open(NULL, 0, 0, 0, NULL, NULL, NULL);],
#	    BDB_LIBADD="$BDB_LIBADD -ldb"; dblib="berkeley"; dbname=db,
#            dblib="no")
#        fi
	LIBS=$saved_LIBS

	LDFLAGS=$BDB_SAVE_LDFLAGS
])

AC_DEFUN([XASTIR_BERKELEY_DB_OPTS],
[
AC_ARG_WITH(bdb-libdir,
	[  --with-bdb-libdir=DIR   Berkeley DB lib files are in DIR],
	with_bdb_lib=$withval,
	[ test "${with_bdb_lib+set}" = set || with_bdb_lib=none])
AC_ARG_WITH(bdb-incdir,
	[  --with-bdb-incdir=DIR   Berkeley DB include files are in DIR],
	with_bdb_inc=$withval,
	[ test "${with_bdb_inc+set}" = set || with_bdb_inc=none ])
])

AC_DEFUN([XASTIR_BERKELEY_DB_CHK],
[
	AC_REQUIRE([XASTIR_BERKELEY_DB_OPTS])

	xastir_save_CPPFLAGS=$CPPFLAGS

	if test -d $with_bdb_inc; then
	    CPPFLAGS="$CPPFLAGS -I$with_bdb_inc"
	    BDB_INCADD="-I$with_bdb_inc"
	else
	    BDB_INCADD=""
	fi

	dnl Note that FreeBSD puts it in a weird place 
        dnl (/usr/local/include/db42)
        dnl (but they should use with-bdb-incdir)
# Commented out because it doesn't distinguish between versions of db.h
# that can work with xastir and versions that can't.  It is possible to 
# have multiple versions of db installed in different places, pick up the 
# header for one and the library for another.  Bleah.
#        AC_CHECK_HEADER(db.h,
#                        [XASTIR_BERKELEY_DB_CHK_LIB()],
#                        dblib="no")
#
# Do this instead --- check to see if the db.h we find first in the search
# path will actually pass the test we do in map_cache.c.  Don't even bother
# looking for a library if not.  
        AC_MSG_CHECKING([if db.h is exists and is usable])
        AC_TRY_COMPILE([#include <db.h>],
                       [#if (DB_VERSION_MAJOR < 4 )
                        #error DB_VERSION_MAJOR < 4
                        #endif],
                        [AC_MSG_RESULT([yes])
                        XASTIR_BERKELEY_DB_CHK_LIB()],
                        [AC_MSG_RESULT([no]); dblib="no"])
	CPPFLAGS=$xastir_save_CPPFLAGS

    use_map_cache="no"
    if test "${dblib}" = "berkeley"; then
        LIBS="$BDB_LIBADD $LIBS"
	CPPFLAGS="$CPPFLAGS $BDB_INCADD"
        AC_DEFINE(USE_MAP_CACHE, 1, [Berkeley DB Map Caching])
        use_map_cache="yes"
    fi
 
])

dnl add -L(arg), and possibly (runpath switch)(arg), to LDFLAGS
dnl (so the runpath for shared libraries is set).
AC_DEFUN([XASTIR_ADD_LIBPATH], [
  AC_REQUIRE([XASTIR_GUESS_RUNPATH_SWITCH])
  # this is XASTIR ADD LIBPATH
  if test "$xastir_runpath_switch" = "none" ; then
        LDFLAGS="-L$1 ${LDFLAGS}"
  else
        LDFLAGS="-L$1 $xastir_runpath_switch$1 ${LDFLAGS}"
  fi
])

dnl add -L(1st arg), and possibly (runpath switch)(1st arg), to (2nd arg)
dnl (so the runpath for shared libraries is set).
AC_DEFUN([XASTIR_ADD_LIBPATH_TO], [
  AC_REQUIRE([XASTIR_GUESS_RUNPATH_SWITCH])
  # this is XASTIR ADD LIBPATH TO
  if test "$xastir_runpath_switch" = "none" ; then
        $2="-L$1 ${$2}"
  else
        $2="-L$1 ${$2} $xastir_runpath_switch$1"
  fi
])

dnl runpath initialization
AC_DEFUN([XASTIR_GUESS_RUNPATH_SWITCH], [
   # XASTIR GUESS RUNPATH SWITCH
  AC_CACHE_CHECK(for runpath switch, xastir_runpath_switch, [
    # first, try -R
    SAVE_LDFLAGS="${LDFLAGS}"
    LDFLAGS="-R /usr/lib"
    AC_TRY_LINK([],[],[xastir_runpath_switch="-R"], [
        LDFLAGS="-Wl,-rpath,/usr/lib"
    AC_TRY_LINK([],[],[xastir_runpath_switch="-Wl,-rpath,"],
    [xastir_runpath_switch="none"])
    ])
  LDFLAGS="${SAVE_LDFLAGS}"
  ])])
