# Process this file with autoconf to produce a configure script.


# Set version here.  It should look something like this:
# ("xastir", "1.1.3", [])
#
# Also remember to change it in the AC_DEFINE below.
AC_INIT(xastir, 1.1.3, xastir@xastir.org)

AC_PREREQ(2.53)

echo ""
echo "Configuring AC_PACKAGE_NAME AC_PACKAGE_VERSION"
echo ""

AC_CONFIG_SRCDIR([src/xastir.h])
AM_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE

# Take out the dots in order to create the TOCALL
SHORT_VERSION="AC_PACKAGE_VERSION"
SHORT_VERSION=`echo $SHORT_VERSION | sed -e 's/\.//g'`
AC_SUBST(SHORT_VERSION)

AC_DEFINE_UNQUOTED(XASTIR_TOCALL, "APX$SHORT_VERSION", [Defines the version tocall])

echo ""
echo "TOCALL = APX$SHORT_VERSION"
echo ""

# Check for host type
AC_CANONICAL_HOST

# Checks for programs.
#AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_AWK
#AC_PROG_YACC
# AC_PROG_LIBTOOL
#echo "end of program checks"


# Liberated from ethereal's configure.in
#
# Add any platform-specific compiler flags needed.
#
AC_MSG_CHECKING(for platform-specific compiler flags)
if test "x$GCC" = x
then
    #
    # Not GCC - assume it's the vendor's compiler.
    #
    case "$host_os" in 
    hpux*) 
        # 
        # HP's ANSI C compiler; flags suggested by Jost Martin. 
        # "-Ae" for ANSI C plus extensions such as "long long". 
        # "+O2", for optimization.  XXX - works with "-g"? 
        # 
        CFLAGS="-Ae +O2 $CFLAGS" 
        AC_MSG_RESULT(HP ANSI C compiler - added -Ae +O2) 
        ;; 
    darwin*) 
        # 
        # It may be called "cc", but it's really a GCC derivative 
        # with a problematic special precompiler and precompiled 
        # headers; turn off the special precompiler, as some 
        # apparently-legal code won't compile with its precompiled 
        # headers. 
        # 
        CFLAGS="-no-cpp-precomp $CFLAGS" 
        AC_MSG_RESULT(Apple GCC - added -no-cpp-precomp) 
		darwin="yes" 
        ;; 
	linux*) 
        ac_tnc_port='"/dev/ttyS0"' 
        ac_gps_port='"/dev/ttyS1"' 
        CFLAGS="-g -O2 -pipe -pthread" 
		AC_MSG_RESULT(LINUX - added -g -O2 -pipe -pthread) 
		AC_DEFINE(SYS_LINUX, 1, Compiling for Linux OS) 
		;; 
	freebsd*) 
        ac_tnc_port='"/dev/cuaa0"' 
        ac_gps_port='"/dev/cuaa1"' 
		AC_MSG_RESULT(none needed) 
		AC_DEFINE(SYS_FREEBSD, 1, Compiling for FreeBSD) 
		;; 
	openbsd*) 
        ac_tnc_port='"/dev/cuaa0"' 
        ac_gps_port='"/dev/cuaa1"' 
		AC_MSG_RESULT(none needed) 
		AC_DEFINE(SYS_OPENBSD, 1, Compiling for OpenBSD) 
		;; 
	cygwin*) 
        ac_tnc_port='"com1"' 
        ac_gps_port='"com2"' 
		AC_MSG_RESULT(none needed) 
		AC_DEFINE(SYS_CYGWIN, 1, Compiling for Cygwin) 
		cygwin="yes" 
		;; 
    *) 
        AC_MSG_RESULT(none needed) 
        ;; 
    esac 
else 
    case "$host_os" in 
    solaris*) 
        AC_DEFINE(XASTIR_SYSTEM, [Solaris], [Define system type]) 
        ac_tnc_port='"/dev/cua/a"' 
        ac_gps_port='"/dev/cua/b"' 
        # the X11 headers don't automatically include prototype info 
        # and a lot don't include the return type 
        CFLAGS="$CFLAGS -Wno-return-type -DFUNCPROTO=15" 
        AC_MSG_RESULT(GCC on Solaris - added -Wno-return-type -DFUNCPROTO=15) 
        ;; 
    darwin*) 
        AC_DEFINE(XASTIR_SYSTEM, [Darwin], [Define system type]) 
        ac_tnc_port='"/dev/cua/a"' 
        ac_gps_port='"/dev/cua/b"' 
        # 
        # See comments above about Apple's lovely C compiler. 
        # 
        CFLAGS="-no-cpp-precomp $CFLAGS" 
        AC_MSG_RESULT(Apple GCC - added -no-cpp-precomp) 
		darwin="yes" 
        ;; 
	linux*) 
        AC_DEFINE(XASTIR_SYSTEM, "Linux", [Define system type]) 
        ac_tnc_port='"/dev/ttyS0"' 
        ac_gps_port='"/dev/ttyS1"' 
        CFLAGS="-g -O2 -Wall -pipe -pthread" 
		AC_MSG_RESULT(LINUX - added -g -O2 -Wall -pipe -pthread) 
		AC_DEFINE(SYS_LINUX, 1, Compiling for Linux OS) 
		;; 
    freebsd*) 
        AC_DEFINE(XASTIR_SYSTEM, [FreeBSD], [Define system type]) 
        ac_tnc_port='"/dev/cuaa0"' 
        ac_gps_port='"/dev/cuaa1"' 
        AC_MSG_RESULT(none needed) 
        AC_DEFINE(SYS_FREEBSD, 1, Compiling for FreeBSD) 
        ;; 
    openbsd*) 
        AC_DEFINE(XASTIR_SYSTEM, [OpenBSD], [Define system type]) 
        ac_tnc_port='"/dev/cuaa0"' 
        ac_gps_port='"/dev/cuaa1"' 
        AC_MSG_RESULT(none needed) 
        AC_DEFINE(SYS_OPENBSD, 1, Compiling for OpenBSD) 
        ;; 
	cygwin*) 
        AC_DEFINE(XASTIR_SYSTEM, [CYGWIN], [Define system type]) 
        ac_tnc_port='"com1"' 
        ac_gps_port='"com2"' 
		AC_MSG_RESULT(none needed) 
		AC_DEFINE(SYS_CYGWIN, 1, Compiling for Cygwin) 
		cygwin="yes" 
		;; 
    *) 
        AC_MSG_RESULT(none needed) 
        ;; 
    esac 
fi 
 
CXXFLAGS="$CXXFLAGS $CFLAGS" 
 
AC_DEFINE_UNQUOTED(TNC_PORT, $ac_tnc_port, [Define default TNC port.]) 
AC_DEFINE_UNQUOTED(GPS_PORT, $ac_gps_port, [Define default GPS port.]) 
 
AC_DEFINE(_GNU_SOURCE, 1, [This is GNU compliant source, use GNU extensions.]) 
# AC_DEFINE(_REENTRANT, 1, [Use reentrant code if available.]) 
AC_DEFINE_UNQUOTED(STIPPLE, 1, [Legacy stuff, use crowbar and lets keep going]) 
 
# Checks for libraries. 
# 
# 
# Find the X11 include and library directories. 
# 
LIB_X11='' 
LIB_XEXT='' 
LIB_XT='' 
AC_PATH_XTRA 
if test "$no_x" != 'yes' 
then 
  LDFLAGS="$LDFLAGS $X_LIBS" 
  LIB_X11="$X_PRE_LIBS -lX11 $X_EXTRA_LIBS" 
  LIBS="$LIB_X11 $LIBS" 
  CPPFLAGS="$CPPFLAGS $X_CFLAGS" 
 
  AC_DEFINE(HasX11,,Define if you have X11 library) 
 
  AC_CHECK_LIB(Xext,XShmAttach,LIB_XEXT='-lXext',,) 
  echo "checking again for shared memory extension..." 
  AC_CHECK_LIB(Xext,XShmAttach,AC_DEFINE(HasSharedMemory,,X11 server supports shared memory extension),,) 
  AC_CHECK_LIB(Xext,XShapeCombineMask,LIB_XEXT='-lXext',,) 
  echo "checking again for shape extension..." 
  AC_CHECK_LIB(Xext,XShapeCombineMask,AC_DEFINE(HasShape,,X11 server supports shape extension),,) 
 
  AC_CHECK_LIB(Xt,XtSetEventDispatcher,LIB_XT='-lXt',,) 
  LIBS="$LIB_XEXT $LIB_XT $LIBS" 
fi 
if test "$no_x" != 'yes' 
then 
  have_x='yes' 
else 
  have_x='no' 
  AC_MSG_ERROR([Working X-dev not found... This ain't going to work, giving up.])
fi 
AM_CONDITIONAL(HasX11, test "$have_x" = 'yes') 
AC_SUBST(LIB_X11) 
AC_SUBST(LIB_XEXT) 
AC_SUBST(LIB_XT) 
 
 
AC_CHECK_LIB([math], [tan]) 
AC_CHECK_LIB([m], [tan]) 
AC_CHECK_LIB([Xpm], [XpmWriteFileFromPixmap]) 
AC_CHECK_LIB([Xp], [XpGetDocumentData]) 
AC_CHECK_LIB([Xm], [XmTextFindString]) 
AC_CHECK_LIB([Xt], [XtDisplay]) 
AC_CHECK_LIB([pthread],[pthread_mutexattr_init]) 
 
# Check for endian 
AC_C_BIGENDIAN 
 
AC_SUBST(LDFLAGS) 
AC_SUBST(LIBS) 
 
# Checks for header files. 
# 
AC_CHECK_HEADERS([Xm/Xm.h],,AC_MSG_ERROR(*** Motif/Lesstif include files not found.  Please install them first. ***)) 
AC_FUNC_ALLOCA 
AC_HEADER_DIRENT 
AC_HEADER_STDC 
AC_HEADER_SYS_WAIT 
AC_CHECK_HEADERS([argz.h arpa/inet.h fcntl.h libintl.h limits.h locale.h malloc.h netdb.h]) 
AC_CHECK_HEADERS([netinet/in.h nl_types.h stddef.h stdlib.h string.h strings.h sys/file.h]) 
AC_CHECK_HEADERS([sys/ioctl.h sys/param.h sys/socket.h sys/time.h signal.h termios.h unistd.h]) 
 
# Checks for typedefs, structures, and compiler characteristics. 
# 
AC_C_CONST 
AC_TYPE_UID_T 
AC_C_INLINE 
AC_TYPE_PID_T 
AC_TYPE_SIZE_T 
AC_HEADER_TIME 
AC_STRUCT_TM 
AC_TYPE_SIGNAL 
 
# Checks for library functions. 
# 
AC_FUNC_FORK 
AC_PROG_GCC_TRADITIONAL 
AC_FUNC_GETPGRP 
AC_FUNC_MALLOC 
AC_FUNC_MKTIME 
AC_FUNC_MMAP 
AC_FUNC_STAT 
AC_FUNC_STRFTIME 
AC_FUNC_STRTOD 
AC_CHECK_FUNCS([__argz_count __argz_next __argz_stringify alarm dcgettext getcwd getpgrp]) 
AC_CHECK_FUNCS([gethostbyname gethostname gettimeofday getwd inet_ntoa memmove mempcpy]) 
AC_CHECK_FUNCS([memset mkdir munmap pow putenv select setenv setlocale snprintf socket sqrt stpcpy]) 
AC_CHECK_FUNCS([strcasecmp strchr strdup strerror strncasecmp strpbrk strrchr strstr strtol]) 
AC_CHECK_FUNCS([strtoul tzset vsnprintf]) 
 
 
 
# Check for wget 
# 
# Is there a way to find out the path to the executable here and 
# then compile that into the Xastir code? 
# 
AC_CHECK_PROG(wget, [wget --version], yes, no) 
if test "$wget" = "no"; then 
  AC_MSG_WARN(*** Missing wget (or it is not in the path).  Building w/o wget support. ***) 
fi 
# 
# End of wget checks 
 
 
 
# Check for convert 
# 
# Is there a way to find out the path to the executable here and 
# then compile that into the Xastir code? 
# 
AC_CHECK_PROG(convert, [convert], yes, no) 
if test "$convert" = "no"; then 
  AC_MSG_WARN(*** Missing convert (or it is not in the path).  Building w/o convert support. ***) 
fi 
# 
# End of convert checks 
 
 
 
# Check for lp/lpr 
# 
# Is there a way to find out the path to the executable here and 
# then compile that into the Xastir code? 
# 
#AC_CHECK_PROG(lpr, [lpr --version], yes, no) 
#if test "$lpr" = "no"; then 
#  AC_MSG_WARN(*** Missing lpr (or it is not in the path).  Building w/o lpr support. ***) 
#fi 
# 
# End of lpr checks 
 
 
 
# Check for gv 
# 
# Is there a way to find out the path to the executable here and 
# then compile that into the Xastir code? 
# 
AC_CHECK_PROG(gv, [gv -v], yes, no) 
if test "$gv" = "no"; then 
  AC_MSG_WARN(*** Missing gv (or it is not in the path).  Building w/o gv support. ***) 
fi 
# 
# End of gv checks 
 
 
 
# Check for cat 
# 
# Is there a way to find out the path to the executable here and 
# then compile that into the Xastir code? 
# 
AC_CHECK_PROG(cat, [cat --version], yes, no) 
if test "$cat" = "no"; then 
  AC_MSG_WARN(*** Missing cat (or it is not in the path).  Building w/o cat support. ***) 
fi 
# 
# End of cat checks 
 
 
 
# Check for cp 
# 
# Is there a way to find out the path to the executable here and 
# then compile that into the Xastir code? 
# 
AC_CHECK_PROG(cp, [cp --version], yes, no) 
if test "$cp" = "no"; then 
  AC_MSG_WARN(*** Missing cp (or it is not in the path).  Building w/o cp support. ***) 
fi 
# 
# End of cp checks 
 
 
 
# Check for intl library (This is needed for Cygwin) 
# 
AC_CHECK_LIB(intl, gettext, XASTIR_INTL_LIB="-lintl") 
if test "$XASTIR_INTL_LIB" = "-lintl" 
then 
  LIBS="${XASTIR_INTL_LIB} ${LIBS}" 
fi 
# 
# End of intl checks 
 
 
 
# Check for Shapelib library 
# 
# Make sure to check /usr/local/lib & /usr/local/include as well 
# 
# 
use_shapelib=no 
AC_CHECK_HEADERS(libshp/shapefil.h, XASTIR_SHAPELIB_INC="yes") 
if test "$XASTIR_SHAPELIB_INC" = "yes" 
then 
  # This one fails for Cygwin if Shapelib is in /usr/local/lib 
  AC_CHECK_LIB(shp, DBFOpen, XASTIR_SHAPELIB_LIB="-lshp") 
 
  # Try one more location for Cygwin specifically.  Add the path to 
  # the LIBS line as well so that the linker will find it later. 
  if test "$XASTIR_SHAPELIB_LIB" != "-lshp" 
  then 
    AC_CHECK_FILE(/usr/local/lib/libshp.so,XASTIR_SHAPELIB_LIB="-lshp") 
    if test "$XASTIR_SHAPELIB_LIB" = "-lshp" 
    then 
      XASTIR_SHAPE_LIB_PATH="-L/usr/local/lib" 
    fi 
  fi 
 
  if test "$XASTIR_SHAPELIB_LIB" = "-lshp" 
  then 
    AC_DEFINE_UNQUOTED(HAVE_SHAPELIB, 1, [Define if you have ShapeLib]) 
    LIBS="${XASTIR_SHAPE_LIB_PATH} ${XASTIR_SHAPELIB_LIB} ${LIBS}" 
    use_shapelib=yes 
  else 
    AC_MSG_WARN(*** Shapelib library files not found or not in the path.  Building w/o Shapelib support. ***) 
  fi 
else 
  AC_MSG_WARN(*** Shapelib header files not found or not in the path.  Building w/o Shapelib support. ***) 
fi 
# 
# End of Shapelib checks 
 
 
 
# Check for XPM library 
# 
# Note that on some operating systems XPM is included in other 
# libraries (I think Solaris is this way). 
# 
# Note that we might need to include this in the ImageMagick checks 
# below, or else just save away the variables found and then insert 
# them if ImageMagick can't be used. 
# 
# Adapted from a macro by Andreas Zeller. 
# 
# The variables provided are : 
#     link_xpm                (e.g. -L/usr/lesstif/lib -lXm) 
#     include_xpm             (e.g. -I/usr/lesstif/lib) 
#     xpm_libraries           (e.g. /usr/lesstif/lib) 
#     xpm_includes            (e.g. /usr/lesstif/include) 
# 
# The link_xpm and include_xpm variables should be fit to put on 
# your application's link line in your Makefile. 
# 
# 
#AC_REQUIRE([AC_PATH_XTRA]) 
xpm_includes= 
xpm_libraries= 
AC_ARG_WITH(xpm, 
[  --without-xpm         do not use Xpm]) 
# Treat --without-xpm like 
# --without-xpm-includes --without-xpm-libraries. 
if test "$with_xpm" = "no" 
then 
    xpm_includes=no 
    xpm_libraries=no 
fi 
AC_ARG_WITH(xpm-includes, 
    [  --with-xpm-includes=DIR    Xpm include files are in DIR], xpm_includes="$withval") 
AC_ARG_WITH(xpm-libraries, 
    [  --with-xpm-libraries=DIR   Xpm libraries are in DIR], xpm_libraries="$withval") 
if test "$xpm_includes" = "no" && test "$xpm_libraries" = "no" 
then 
    with_xpm="no" 
fi 
 
AC_MSG_CHECKING([for Xpm]) 
if test "$with_xpm" != "no" 
then 
    # 
    # 
    # Search the include files. 
    # 
    if test "$xpm_includes" = "" 
    then 
        AC_CACHE_VAL(ac_cv_xpm_includes, 
        [ 
        ac_xpm_save_CFLAGS="$CFLAGS" 
        ac_xpm_save_CPPFLAGS="$CPPFLAGS" 
        # 
        CFLAGS="$X_CFLAGS $CFLAGS" 
        CPPFLAGS="$X_CFLAGS $CPPFLAGS" 
        # 
        AC_TRY_COMPILE([#include <X11/xpm.h>],[int a;], 
        [ 
        # X11/xpm.h is in the standard search path. 
        ac_cv_xpm_includes= 
        ], 
    [ 
    # Else clause for first AC_TRY_COMPILE statement 
    # 
    AC_TRY_COMPILE([#include <Xm/XpmI.h>],[int a;], 
    [ 
    # Xm/XpmI.h is in the Solaris standard search path. 
    ac_cv_xpm_includes= 
    xpm_libraries="/usr/lib/libXm.so" 
    xpm_skip_linking=1 
    ], 
        [ 
        # X11/xpm.h or Xm/XpmI.h are not in the standard search 
        # path. 
        # Locate one of them and put its directory in `xpm_includes' 
        # 
        # Other directories are just guesses. 
        for dir in "$x_includes" "${prefix}/include" /usr/include /usr/local/include \ 
                   /usr/include/Motif2.0 /usr/include/Motif1.2 /usr/include/Motif1.1 \ 
                   /usr/include/X11R6 /usr/include/X11R5 /usr/include/X11R4 \ 
                   /usr/dt/include /usr/openwin/include /usr/X11R6/include \ 
                   /usr/dt/*/include /opt/*/include /usr/include/Xpm* \ 
                   "${prefix}"/*/include /usr/*/include /usr/local/*/include \ 
                   "${prefix}"/include/* /usr/include/* /usr/local/include/* 
        do 
            if test -f "$dir/X11/xpm.h" 
            then 
                  ac_cv_xpm_includes="$dir" 
        else 
          if test -f "$dir/Xm/XpmI.h" 
          then 
            ac_cv_xpm_includes="$dir" 
            xpm_libraries="/usr/lib/libXm.so" 
          fi 
            fi 
        done 
        ]) ]) 
        # 
        CFLAGS="$ac_xpm_save_CFLAGS" 
        CPPFLAGS="$ac_xpm_save_CPPFLAGS" 
        ]) 
        xpm_includes="$ac_cv_xpm_includes" 
    fi 
 
    if test -z "$xpm_includes" 
    then 
        xpm_includes_result="default path" 
        XPM_CFLAGS="" 
    else 
        if test "$xpm_includes" = "no" 
        then 
            xpm_includes_result="told not to use them" 
            XPM_CFLAGS="-DNO_XPM" 
        else 
            xpm_includes_result="$xpm_includes" 
            XPM_CFLAGS="-I$xpm_includes" 
        fi 
    fi 
    # 
    # 
    # Now for the libraries. 
    # 
    if test "$xpm_libraries" = "" 
    then 
        AC_CACHE_VAL(ac_cv_xpm_libraries, 
        [ 
        ac_xpm_save_LIBS="$LIBS" 
        ac_xpm_save_CFLAGS="$CFLAGS" 
        ac_xpm_save_CPPFLAGS="$CPPFLAGS" 
        # 
        LIBS="-lXpm $X_LIBS -lX11 $X_EXTRA_LIBS $LIBS" 
        CFLAGS="$XPM_CFLAGS $X_CFLAGS $CFLAGS" 
        CPPFLAGS="$XPM_CFLAGS $X_CFLAGS $CPPFLAGS" 
        # 
        AC_TRY_LINK([#include <X11/xpm.h>],[XpmAttributesSize();], 
        [ 
        # libXpm.a is in the standard search path. 
        ac_cv_xpm_libraries= 
        ], 
    [ 
    # Else clause of AC_TRY_LINK 
    AC_TRY_LINK([#include <Xm/XpmI.h>],[XpmAttributesSize();], 
    [ 
    # libXm.a is in the standard search path for Solaris. 
    ac_cv_xpm_libraries= 
    ], 
        [ 
        # libXpm.a or libXm.a are not in the standard search path. 
        # Locate one of them and put its directory in 
        # `xpm_libraries' 
        # 
        # Other directories are just guesses. 
        for dir in "$x_libraries" "${prefix}/lib" /usr/lib /usr/local/lib \ 
                   /usr/lib/Xlt2.0 /usr/lib/Xlt1.2 /usr/lib/Xlt1.1 \ 
                   /usr/lib/X11R6 /usr/lib/X11R5 /usr/lib/X11R4 /usr/lib/X11 \ 
                   /usr/dt/lib /usr/openwin/lib /usr/X11/lib \ 
                   /usr/dt/*/lib /opt/*/lib /usr/lib/Xpm* \ 
                   /usr/lesstif*/lib /usr/lib/Lesstif* \ 
                   "${prefix}"/*/lib /usr/*/lib /usr/local/*/lib \ 
                   "${prefix}"/lib/* /usr/lib/* /usr/local/lib/*; do 
            for ext in "sl" "so" "a"; do 
                  if test -d "$dir" && test -f "$dir/libXpm.$ext" 
          then 
                    ac_cv_xpm_libraries="$dir" 
                    break 2 
          fi 
          if test -d "$dir" && test -f "$dir/libXm.$ext" 
          then 
            ac_cv_xpm_libraries="$dir" 
            break 2 
          fi 
            done 
        done 
        ]) ]) 
        # 
        LIBS="$ac_xpm_save_LIBS" 
        CFLAGS="$ac_xpm_save_CFLAGS" 
        CPPFLAGS="$ac_xpm_save_CPPFLAGS" 
        ]) 
        # 
        xpm_libraries="$ac_cv_xpm_libraries" 
    fi 
    if test -z "$xpm_libraries" 
    then 
        xpm_libraries_result="default path" 
        XPM_LIBS="-lXpm" 
    else 
        if test "$xpm_libraries" = "no" 
        then 
            xpm_libraries_result="told not to use it" 
            XPM_LIBS="" 
        else 
        if test "$xpm_skip_linking" = "1" 
        then 
            xpm_libraries_result="default path" 
            XPM_LIBS="" 
        else 
            xpm_libraries_result="$xpm_libraries" 
                XPM_LIBS="-L$xpm_libraries -lXpm" 
        fi 
        fi 
    fi 
 
    if test "$xpm_skip_linking" = "1" 
    then 
        # JMT - this is only done if we are running Solaris 
        AC_DEFINE([HAVE_XPMI], 1, [Define if Xm/XmpI.h is used.]) 
        AC_DEFINE([HAVE_XPM], 1, [Define if XPM is present.]) 
    else 
# 
# Make sure, whatever we found out, we can link. 
# 
    ac_xpm_save_LIBS="$LIBS" 
    ac_xpm_save_CFLAGS="$CFLAGS" 
    ac_xpm_save_CPPFLAGS="$CPPFLAGS" 
    # 
    LIBS="$XPM_LIBS -lXpm $X_LIBS -lX11 $X_EXTRA_LIBS $LIBS" 
    CFLAGS="$XPM_CFLAGS $X_CFLAGS $CFLAGS" 
    CPPFLAGS="$XPM_CFLAGS $X_CFLAGS $CPPFLAGS" 
 
    AC_TRY_LINK([#include <X11/xpm.h>],[XpmAttributesSize();], 
        [ 
        # 
        # link passed 
        # 
        AC_DEFINE([HAVE_XPM], 1, [Define if XPM is present.]) 
        ], 
        [ 
        # 
        # link failed 
        # 
        xpm_libraries_result="test link failed" 
        xpm_includes_result="test link failed" 
        with_xpm="no" 
        XPM_CFLAGS="-DNO_XPM" 
        XPM_LIBS="" 
        ]) # AC_TRY_LINK 
 
    LIBS="$ac_xpm_save_LIBS" 
    CFLAGS="$ac_xpm_save_CFLAGS $XPM_CFLAGS" 
    CPPFLAGS="$ac_xpm_save_CPPFLAGS" 
    fi 
else 
    xpm_libraries_result="told not to use it" 
    xpm_includes_result="told not to use them" 
    XPM_CFLAGS="-DNO_XPM" 
    XPM_LIBS="" 
fi 
# 
# For the case of apple-darwin, we don't want -lXpm 
# included due to other libraries also providing the 
# same interfaces 
# 
case "$host" in 
   *-*apple-darwin* ) 
      XPM_LIBS="" 
      ;; 
   * ) 
      ;; 
esac 
AC_MSG_RESULT([libraries $xpm_libraries_result, headers $xpm_includes_result]) 
AC_SUBST(XPM_CFLAGS) 
AC_SUBST(XPM_LIBS) 
AC_SUBST(LIBS) 
AC_SUBST(CFLAGS) 
# 
# End of XPM checks 
 
 
 
# Check for ImageMagick 
# 
save_cppflags="$CPPFLAGS" 
save_cxxflags="$CXXFLAGS" 
save_libs="$LIBS" 
save_ldflags="$LDFLAGS" 
# 
AC_CHECK_PROG(use_imagemagick, [Magick-config --version], yes, no) 
if test "$use_imagemagick" = "yes"; then 
  magickversion=`Magick-config --version` 
  magickmajor=`echo $magickversion | cut -d '.' -f 1` 
  magickminor=`echo $magickversion | cut -d '.' -f 2` 
  magicktiny=`echo $magickversion | cut -d '.' -f 3` 
  if test "$magickmajor" -lt 5; then 
    magickold="yes"; 
  elif test "$magickmajor" -eq 5 -a "$magickminor" -lt 4; then 
    magickold="yes"; 
  elif test "$magickmajor" -eq 5 -a "$magickminor" -eq 4 -a "$magicktiny" -lt 7; then 
    magickold="yes"; 
  fi 
fi 
# 
if test "$use_imagemagick" = "no"; then 
  AC_MSG_WARN(*** Missing Magick-config (or it is not in the path).  Building w/o ImageMagick support. ***) 
else 
  if test "$magickold" = "yes"; then 
    AC_MSG_WARN(*** Old ImageMagick version ($magickversion) found.  Please upgrade to 5.4.7 or newer. ***) 
  fi 
  CPPFLAGS="$CPPFLAGS `Magick-config --cppflags`" 
  CXXFLAGS="$CXXFLAGS `Magick-config --cflags`" 
  LDFLAGS="$LDFLAGS `Magick-config --ldflags`" 
  LIBS="`Magick-config --libs` $LIBS" 
  AC_CHECK_HEADER(magick/api.h, use_imagemagick="yes", use_imagemagick="no") 
  if test "$use_imagemagick" = "no"; then 
    AC_MSG_WARN(*** Missing ImageMagick include files.  Building w/o ImageMagick support. ***) 
  else 
    AC_CHECK_LIB([Magick], [WriteImage], 
    AC_DEFINE(HAVE_IMAGEMAGICK, 1, [Imagemagick image library]) gpslc="\$(GPSL)", use_imagemagick="no") 
    if test "$use_imagemagick" = "no"; then 
        AC_MSG_WARN(*** Missing ImageMagick library.  Building w/o ImageMagick support. ***) 
    fi 
  fi 
fi 
# 
if test "$use_imagemagick" = "no"; then 
  CPPFLAGS=$save_cppflags 
  CXXFLAGS=$save_cxxflags 
  LIBS=$save_libs 
  LDFLAGS=$save_ldflags 
fi 
# 
# End of ImageMagick checks 
 
 
 
# Check for libgeotiff/libtiff/libproj libraries 
# 
# Make sure to check /usr/local/lib & /usr/local/include as well 
# 
use_geotiff=no 
AC_CHECK_LIB(tiff, TIFFClose, XASTIR_TIFF_LIB="-ltiff") 
AC_CHECK_LIB(geotiff, GTIFNew, XASTIR_GEOTIFF_LIB="-lgeotiff") 
AC_CHECK_LIB(proj, pj_init, XASTIR_PROJ_LIB="-lproj") 
if test "$XASTIR_PROJ_LIB" = "-lproj" 
then 
  if test "$XASTIR_TIFF_LIB" = "-ltiff" 
  then 
    if test "$XASTIR_GEOTIFF_LIB" = "-lgeotiff" 
    then 
      AC_DEFINE_UNQUOTED(HAVE_GEOTIFF, 1, [Define if you have GeoTiff Package]) 
      LIBS="${XASTIR_GEOTIFF_LIB} ${XASTIR_PROJ_LIB} ${LIBS} ${XASTIR_TIFF_LIB} " 
      use_geotiff=yes 
    fi 
  fi 
fi 
# 
# End of libgeotiff/libtiff/libproj checks 
 
 
 
# Check for Festival 
# 
# Make sure to check /usr/local/bin & /usr/local/include as well 
# 
AC_MSG_CHECKING([for Festival Speech Synthesizer Support]) 
if test -f "/usr/bin/festival" || test -f "/usr/local/bin/festival"; 
then 
  AC_MSG_RESULT(yes) 
  AC_DEFINE_UNQUOTED(HAVE_FESTIVAL, 1, [Define if you have Festival]) 
  AC_SUBST(CFLAGS) 
  use_festival=yes 
else 
  AC_MSG_RESULT(no) 
  use_festival=no 
fi 
# 
# End of Festival checks 
 
 
 
# Check for AX.25 library 
# 
# Make sure to check /usr/local/lib & /usr/local/include as well 
# 
has_axlib="no" 
xastir_has_axlib_inc="no" 
AC_CHECK_HEADERS(netax25/axlib.h, xastir_has_axlib_inc="yes") 
if test "$xastir_has_axlib_inc" = "yes" 
then 
  AC_CHECK_LIB(ax25, ax25_aton, xastir_has_ax25="yes") 
  if test "$xastir_has_ax25" = "yes" 
  then 
    echo "Found libax25" 
    has_axlib="yes" 
    LIBS="$LIBS -lax25" 
    AC_DEFINE(HAVE_AX25, 1, [Define if using libax25]) 
  else 
    has_axlib = "no" 
    AC_DEFINE(HAVE_AX25, 0, [Define if using libax25]) 
  fi 
fi 
# 
# End of AX.25 checks 
 
 
 
# Set XASTIR_DATA_BASE in config.h. 
if test "x${prefix}" = "xNONE"; then 
  AC_DEFINE_UNQUOTED(XASTIR_DATA_BASE, "${ac_default_prefix}/xastir", [Stored data]) 
else 
  AC_DEFINE_UNQUOTED(XASTIR_DATA_BASE, "${prefix}/xastir", [Stored data]) 
fi 
 
 
 
# 
# Assure we have this order for these LIBS: "-lXm -lXt -lX11" by 
# adding them to the beginning.  Add spaces around them for the 
# following steps.  Here I'm adding them to the beginning so the   
# dupe-check portion will take out the later ones that might be in 
# the wrong order.  If they're in the wrong order, we can get 
# "Error: Shell widget xastir has zero width and/or height" when 
# Xastir is started up. 
# 
LIBS=" -lXm -lXt -lX11 $LIBS" 
  
 
 
# 
# Remove duplicate entries.  Thanks to Paul Lutt, ke7xt, for this! 
# 
changequote(,) 
CFLAGS=`echo  "$CFLAGS" | awk '{for(i=1;i<=NF;++i) {if (arg[$i]++ == 0) s = s " " $i} print s}'` 
CPPFLAGS=`echo  "$CPPFLAGS" | awk '{for(i=1;i<=NF;++i) {if (arg[$i]++ == 0) s = s " " $i} print s}'` 
LDFLAGS=`echo "$LDFLAGS" | awk '{for(i=1;i<=NF;++i) {if (arg[$i]++ == 0) s = s " " $i} print s}'` 
# 
# We probably don't want to get rid of dupes on the LIBS line, 'cuz 
# we may have to have multiple instances of them just before the 
# libraries that they call out. 
#LIBS=`echo  "$LIBS" | awk '{for(i=1;i<=NF;++i) {if (arg[$i]++ == 0) s = s " " $i} print s}'` 
changequote([,]) 
  
 
 
# 
# Remove extraneous spaces from output variables (asthetic) 
# 
X_CFLAGS=`echo $X_CFLAGS | sed -e 's/  */ /g'` 
X_PRE_LIBS=`echo $X_PRE_LIBS | sed -e 's/  */ /g'` 
X_LIBS=`echo $X_LIBS | sed -e 's/  */ /g'` 
X_EXTRA_LIBS=`echo $X_EXTRA_LIBS | sed -e 's/  */ /g'` 
 
CC=`echo $CC | sed -e 's/  */ /g'` 
CFLAGS=`echo $CFLAGS | sed -e 's/  */ /g'` 
CPPFLAGS=`echo $CPPFLAGS | sed -e 's/  */ /g'` 
CXXFLAGS=`echo $CXXFLAGS | sed -e 's/  */ /g'` 
LDFLAGS=`echo $LDFLAGS | sed -e 's/  */ /g'` 
TESTED_LIBS=`echo $LIBS | sed -e 's/  */ /g'` 
MAGICK_DEP_LIBS=`echo $MAGICK_DEP_LIBS | sed -e 's/  */ /g'` 
LIBS=`echo $LIBS | sed -e 's/  */ /g'` 
 
AC_SUBST(LDFLAGS) 
AC_SUBST(LIBS) 
#AC_SUBST(CPPFLAGS) 
AC_SUBST(X_CFLAGS) 
#AC_SUBST(LDFLAGS) 
#AC_SUBST(X_PRE_LIBS) 
#AC_SUBST(X_LIBS) 
#AC_SUBST(X_EXTRA_LIBS) 
 
AC_CONFIG_FILES([Makefile \
    callpass/Makefile \
    config/Makefile \
    help/Makefile \
    intl/Makefile \
    m4/Makefile \
    scripts/Makefile \
    src/Makefile \
    symbols/Makefile \
    xastir.spec]) 
 
AC_OUTPUT 
 
echo "" 
echo "        LIBS: $LIBS" 
echo "     LDFLAGS: $LDFLAGS" 
echo "      CFLAGS: $CFLAGS" 
echo "    CPPFLAGS: $CPPFLAGS" 
echo "      X_LIBS: $X_LIBS" 
echo "  X_PRE_LIBS: $X_PRE_LIBS" 
echo "X_EXTRA_LIBS: $X_EXTRA_LIBS" 
 
echo 
echo "AC_PACKAGE_NAME AC_PACKAGE_VERSION" 
 
echo 
echo Building with AX25.......... : $has_axlib 
echo Building with GeoTiff....... : $use_geotiff 
echo Building with ShapeLib...... : $use_shapelib 
echo Building with ImageMagick... : $use_imagemagick 
echo Building with Festival...... : $use_festival 
echo 
echo Xastir will be installed in $prefix/bin. 
if test "x$xastirpath" != "x" ; then 
        echo Warning: You have an old copy of Xastir at $xastirpath. 
fi 
echo "Type 'make' to build Xastir (Use 'gmake' instead on some systems)." 
 
