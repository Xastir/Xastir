# acinclude.m4 for Xastir

# test for devices
AC_DEFUN([XASTIR_DETECT_DEVICES],
[
AC_MSG_CHECKING([for devices])
if test -c com1 ; then
ac_tnc_port=com1
ac_gps_port=com2
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
gcc --help | sed -e "/^[^ ]/d" -e "/^ [^ ]/d" -e "/^  [^-]/d" -e "s/  //" -e "s/ .*//" > gccflags

# I need a test for -Wno-return-type and -DFUNCPROTO=15
# before adding them

for f in -no-cpp-precomp -pipe; do
grep -- $f gccflags > /dev/null && CFLAGS="$CFLAGS $f"
done

# add any pthread flags now
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

AC_MSG_RESULT(using $CFLAGS)
rm -f gccflags
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
 
AC_PATH_PROG(festival, [festival], no, $BINPATH)
if test "$festival" != "no"; then
  AC_DEFINE_UNQUOTED(HAVE_FESTIVAL, 1, [Define if you have festival])
  AC_DEFINE_UNQUOTED(FESTIVAL_PATH, "${festival}", [Path to festival])
fi

AC_PATH_PROG(gpsman, [gpsman haslib gpsmanshp], no, $BINPATH)
if test "$gpsman" != "no"; then
  AC_DEFINE_UNQUOTED(HAVE_GPSMAN, 1, [Define if you have gpsman])
  AC_DEFINE_UNQUOTED(GPSMAN_PATH, "${gpsman}", [Path to gpsman])
fi

])

# JMT - I have tamed the evil
AC_DEFUN([XASTIR_CHECK_IMAGEMAGICK],
[
use_imagemagick="mu"
old_version="5.4.9"
AC_CHECK_PROG(magick_config, [Magick-config --version], yes, no)
if test "$magick_config" == "yes"; then
  AC_MSG_CHECKING([ImageMagick version])
  magick_version=`Magick-config --version`

  magick_major=`echo $magick_version | cut -d '.' -f 1`
  magick_minor=`echo $magick_version | cut -d '.' -f 2`
  magick_patch=`echo $magick_version | cut -d '.' -f 3`

  old_major=`echo $old_version | cut -d '.' -f 1` 
  old_minor=`echo $old_version | cut -d '.' -f 2` 
  old_patch=`echo $old_version | cut -d '.' -f 3` 

  if test "$magick_major" -lt "$old_major" || \
     test "$magick_major" -eq "$old_major" -a "$magick_minor" -lt "$old_minor" || \
     test "$magick_major" -eq "$old_major" -a "$magick_minor" -eq "$old_minor" -a "$magick_patch" -lt "$old_patch"; then 
     magick_version="$magick_version, upgrade for full functionality"
  fi 
  AC_MSG_RESULT($magick_version)

  # check for presence of header
  if test "$use_imagemagick" == "mu"; then
    save_CFLAGS="$CFLAGS"
    save_CPPFLAGS="$CPPFLAGS" 
    save_LDFLAGS="$LDFLAGS" 
  
    CFLAGS="$CFLAGS `Magick-config --cflags`"
    CPPFLAGS="$CPPFLAGS `Magick-config --cppflags`"
    LDFLAGS="$LDFLAGS `Magick-config --ldflags`"
  
    AC_CHECK_HEADER(magick/api.h, [], [use_imagemagick="no"
CFLAGS=$save_CFLAGS
CPPFLAGS=$save_CPPFLAGS 
LDFLAGS=$save_LDFLAGS])
  fi

  # are all the libraries present
  for lib in `Magick-config --libs`; do
    echo $lib | grep -- ^-l >/dev/null && \
      rev_libs="`echo $lib | sed -e s/^-l//` $rev_libs"
  done 

  save_LIBS="$LIBS" 
  for lib in $rev_libs; do
    if test "$use_imagemagick" == "mu"; then 
      AC_CHECK_LIB($lib, main, [], [use_imagemagick="no"
CFLAGS=$save_CFLAGS
CPPFLAGS=$save_CPPFLAGS 
LDFLAGS=$save_LDFLAGS
LIBS=$save_LIBS])
    fi
  done

  # perform final link check
  if test "$use_imagemagick" == "mu"; then
    AC_CHECK_LIB([Magick], [WriteImage], [use_imagemagick="yes"
AC_DEFINE(HAVE_IMAGEMAGICK, 1, [Imagemagick image library])], [use_imagemagick="no"
CFLAGS=$save_CFLAGS
CPPFLAGS=$save_CPPFLAGS 
LDFLAGS=$save_LDFLAGS 
LIBS=$save_LIBS])
  fi
else
  use_imagemagick="no"
fi

AC_MSG_CHECKING([for ImageMagick])
AC_MSG_RESULT($use_imagemagick)
])

# things grabbed elsewhere

# this is from Squeak-3.2-4's acinclude.m4
AC_DEFUN(AC_CHECK_GMTOFF,
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
