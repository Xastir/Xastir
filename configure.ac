# Process this file with autoconf to produce a configure script.

# $Id$


#########################################################################
# SET VERSION HERE!
#
# AC_INIT():
# It should look something like this:  (xastir, 1.2.1, xastir@xastir.org)
# The revision number must contain at least one '.' and two digits.
# AM_INIT_AUTOMAKE():
# It should look something like this:  (xastir, 1.2.1)
# The revision number must contain at least one '.' and two digits.
#
AC_INIT(xastir, 1.2.1, xastir@xastir.org)
AM_INIT_AUTOMAKE(xastir, 1.2.1)
#########################################################################


AC_PREREQ(2.53)

AC_CONFIG_SRCDIR([src/xastir.h])
AM_CONFIG_HEADER(config.h)

echo ""
echo "Configuring AC_PACKAGE_NAME AC_PACKAGE_VERSION"
echo ""

# Take out the dots in order to create the TOCALL
AC_DEFINE_UNQUOTED(XASTIR_TOCALL, "`echo AC_PACKAGE_VERSION | sed -e 's/^/APX/' -e 's/\.//g'`", [Defines the version tocall])

# Check for host type
AC_CANONICAL_HOST

# Define _GNU_SOURCE if appropriate
# doesn't work with older (heh) autoconfs
AC_GNU_SOURCE

# Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_AWK
#AC_PROG_YACC

# test for devices
XASTIR_DETECT_DEVICES

# add includes and libs
XASTIR_ADD_SEARCH_PATHS

# check for pthread first
ACX_PTHREAD

# add compiler flags 
XASTIR_COMPILER_FLAGS

# set XASTIR_SYSTEM
XASTIR_SET_SYSTEM

# Check for binaries
use_festival=no
use_gpsman=no
XASTIR_DETECT_BINARIES
if test "x${festival}" != "xno"; then
  use_festival=yes
fi
if test "x${gpsman}" != "xno"; then
  echo GPSMAN is ${gpsman}
  use_gpsman=yes
fi

# JMT - XXX - what is this and why is it relevant?
#AC_DEFINE(_REENTRANT, 1, [Use reentrant code if available.]) 
AC_DEFINE_UNQUOTED(STIPPLE, 1, [Legacy stuff, use crowbar and lets keep going]) 

# Checks for libraries. 
# 
# 
# Find the X11 include and library directories. 
# 
AC_PATH_XTRA
if test "$no_x" = "yes"; then
  AC_MSG_ERROR(*** Cannot find X11 libraries:  Please Install X-Windows ***)
fi

CPPFLAGS="$CPPFLAGS $X_CFLAGS" 
LDFLAGS="$LDFLAGS $X_LIBS"
LIBS="$LIBS $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"

AC_SEARCH_LIBS(tan,math m)
AC_CHECK_LIB([Xext], [XextAddDisplay])
AC_CHECK_LIB([Xp], [XpGetDocumentData]) 
AC_CHECK_LIB([Xt], [XtDisplay]) 
AC_CHECK_LIB([Xm], [XmTextFindString]) 
AC_CHECK_LIB([curl], [curl_global_init])
# JMT - XXX - why is this here?
AC_CHECK_LIB(compat, main, 
    [LIBCOMPAT="-lcompat"
    AC_SUBST(LIBCOMPAT)])
 
# Check for sched_yield in librt (Needed for Solaris)
AC_CHECK_LIB(rt, sched_yield, LIBS="-lrt $LIBS")
 
# Check for endian 
AC_C_BIGENDIAN 
 
AC_SUBST(LDFLAGS) 
AC_SUBST(LIBS) 

# Checks for header files. 
AC_CHECK_HEADERS([Xm/Xm.h],MOTIF_INC="yes", AC_MSG_ERROR(*** Cannot find Motif include files:  Please install Motif, OpenMotif, or Lesstif development package. ***))
#
AC_FUNC_ALLOCA 
AC_HEADER_DIRENT 
AC_HEADER_STDC 
AC_HEADER_SYS_WAIT 
AC_CHECK_HEADERS([argz.h arpa/inet.h fcntl.h libintl.h limits.h locale.h malloc.h math.h netdb.h]) 
AC_CHECK_HEADERS([netinet/in.h nl_types.h stdarg.h stddef.h stdlib.h string.h strings.h])
AC_CHECK_HEADERS([sys/file.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h signal.h])
AC_CHECK_HEADERS([termios.h unistd.h]) 

# Checks for typedefs, structures, and compiler characteristics. 
# 
AC_C_CONST 
AC_TYPE_UID_T 
AC_C_INLINE 
AC_TYPE_PID_T 
AC_TYPE_SIZE_T 
AC_HEADER_TIME 
#AC_STRUCT_TM 
AC_TYPE_SIGNAL 
AC_STRUCT_TIMEZONE

AC_CHECK_TYPES(socklen_t,,,[#include <sys/types.h>
#include <sys/socket.h>])

# Check for tm_gmtoff in tm
AC_CHECK_GMTOFF
 
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
AC_CHECK_FUNCS([__argz_count __argz_next __argz_stringify alarm dcgettext])
AC_CHECK_FUNCS([getcwd getpgrp gethostbyname gethostname gettimeofday])
AC_CHECK_FUNCS([getwd inet_ntoa memmove mempcpy memset mkdir munmap pow])
AC_CHECK_FUNCS([putenv select setenv setlocale settimeofday signal])
AC_CHECK_FUNCS([snprintf socket sqrt stpcpy strcasecmp strchr strdup])
AC_CHECK_FUNCS([strerror strncasecmp strpbrk strptime strrchr strstr])
AC_CHECK_FUNCS([strtol strtoul tzset vsnprintf])
 
# Check for intl library (Needed for Cygwin) 
# 
AC_CHECK_LIB(intl, gettext, LIBS="-lintl ${LIBS}") 
 
# Check for Shapelib
use_shapelib=no
AC_CHECK_HEADERS(shapefil.h libshp/shapefil.h, [AC_CHECK_LIB(shp, DBFOpen, use_shapelib=yes
LIBS="$LIBS -lshp"
AC_CHECK_LIB(shp,DBFGetFieldIndex,
  AC_DEFINE(HAVE_DBFGETFIELDINDEX, , Define to 1 if your `shp' library has DBFGetFieldIndex. ) )
AC_DEFINE(HAVE_LIBSHP, , Define to 1 if you have the `shp' library (-lshp). )
break)])
 
# Check for pcre
use_pcre=no
AC_CHECK_HEADERS(pcre.h pcre/pcre.h, [AC_CHECK_LIB(pcre, pcre_compile, use_pcre=yes
LIBS="$LIBS -lpcre"
AC_DEFINE(HAVE_LIBPCRE, , Define to 1 if you have the `pcre' library (-lpcre). )
break)])
 
# Check for XPM
AC_CHECK_HEADERS(X11/xpm.h, AC_SEARCH_LIBS(XpmWriteFileFromPixmap, Xpm, AC_DEFINE(HAVE_LIBXPM, , Define to 1 if you have the `Xpm' library (-lXpm). )))
AC_CHECK_HEADERS(Xm/XpmI.h, AC_SEARCH_LIBS(XmeXpmWriteFileFromPixmap, Xm, AC_DEFINE(HAVE_LIBXPM_IN_XM, , Define to 1 if the `Xm' library (-lXm) is used for Xpm. )))
 
# Check for ImageMagick 
XASTIR_CHECK_IMAGEMAGICK
 
# Check for libproj
use_proj=no
AC_CHECK_LIB(proj, pj_init, use_proj=yes LIBS="$LIBS -lproj"
AC_DEFINE(HAVE_LIBPROJ, , Define to 1 if you have the `libproj' library (-lproj). )
)
 
# Check for libgeotiff and required libraries
use_geotiff=no
AC_CHECK_LIB(tiff, TIFFClose, AC_CHECK_LIB(proj, pj_init, AC_CHECK_LIB(geotiff, GTIFNew, use_geotiff=yes
LIBS="$LIBS -ltiff -lgeotiff"
AC_DEFINE(HAVE_LIBGEOTIFF, , Define to 1 if you have the `geotiff' library (-lgeotiff). )
)))
 
# Check for AX.25 library 
has_axlib=no
AC_CHECK_HEADERS(netax25/ax25.h, [AC_CHECK_LIB(ax25, ax25_config_load_ports, has_axlib=yes
LIBS="$LIBS -lax25"
AC_DEFINE(HAVE_LIBAX25, , Define to 1 if you have the `ax25' library (-lax25). )
break)])

# use dbfawk
use_dbfawk=no
AC_ARG_WITH(dbfawk,[  --with-dbfawk       Enable experimental dbfawk features.],use_dbfawk=yes)
# dbfawk requires shapelib and pcre.  Make sure:
if test "${use_dbfawk}" = "yes"; then
  if test "${use_pcre}" = "yes" -a "${use_shapelib}" = "yes"; then
    AC_DEFINE(WITH_DBFAWK, , Define to 1 to use experimental dbfawk features. )
  else
    echo "Warning: dbfawk requires both pcre and shapelib."
    use_dbfawk=no
  fi
fi


# Check for GDAL.  Make sure that GDAL is the last library, as it
# can mess up shapelib and libgeotiff currently.
use_gdal=no
AC_CHECK_HEADERS(gdal.h, [AC_CHECK_LIB(gdal, GDALAllRegister, use_gdal=yes
LIBS="$LIBS -lgdal"
AC_DEFINE(HAVE_LIBGDAL, , Define to 1 if you have the `gdal' library (-lgdal). )
break)])
 
 
# Set XASTIR_DATA_BASE in CPPFLAGS due to Gnu coding standard that requires
# datadir expansion to be deferred until make time.
if test "x${datadir}" = "x"; then 
  CPPFLAGS="$CPPFLAGS -DXASTIR_DATA_BASE=\\\"${ac_default_prefix}/share/xastir\\\""
else 
  CPPFLAGS="$CPPFLAGS -DXASTIR_DATA_BASE=\\\"${datadir}/xastir\\\""
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
# Don't get rid of dupes on the LIBS line.  We may need the same
# "-Llibdir" called out several times, immediately prior to each
# "-llibrary" that needs it.
# 
changequote(,) 
CFLAGS=`echo  "$CFLAGS" | awk '{for(i=1;i<=NF;++i) {if (arg[$i]++ == 0) s = s " " $i} print s}'` 
CPPFLAGS=`echo  "$CPPFLAGS" | awk '{for(i=1;i<=NF;++i) {if (arg[$i]++ == 0) s = s " " $i} print s}'` 
LDFLAGS=`echo "$LDFLAGS" | awk '{for(i=1;i<=NF;++i) {if (arg[$i]++ == 0) s = s " " $i} print s}'` 
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
    m4/Makefile \
    scripts/Makefile \
    src/Makefile \
    symbols/Makefile \
    xastir.spec]) 
 
AC_OUTPUT 

# Please leave these in as they're very useful for debug when we
# port to new platforms!  Leave them commented out unless doing
# debugging on the configure scripts.
#
#echo "" 
#echo "        LIBS: $LIBS" 
#echo "     LDFLAGS: $LDFLAGS" 
#echo "      CFLAGS: $CFLAGS" 
#echo "    CPPFLAGS: $CPPFLAGS" 
#echo "      X_LIBS: $X_LIBS" 
#echo "  X_PRE_LIBS: $X_PRE_LIBS" 
#echo "X_EXTRA_LIBS: $X_EXTRA_LIBS" 


echo ===========================================
echo AC_PACKAGE_NAME AC_PACKAGE_VERSION has been configured using the 
echo following external libraries.
echo  
echo 
echo Building with AX25.......... : $has_axlib 
echo Building with Festival...... : $use_festival 
echo Building with GPSMan........ : $use_gpsman
echo Building with ShapeLib...... : $use_shapelib 
echo Building with ImageMagick... : $use_imagemagick 
echo Building with libproj....... : $use_proj
echo Building with GeoTiff....... : $use_geotiff 
echo Building with GDAL.......... : $use_gdal
echo Building with pcre.......... : $use_pcre
echo Building with dbfawk........ : $use_dbfawk
echo 
echo AC_PACKAGE_NAME will be installed in $prefix/bin. 
if test "x$xastirpath" != "x" ; then 
        echo Warning: You have an old copy of Xastir at $xastirpath. 
fi 
echo "Type 'make' to build Xastir (Use 'gmake' instead on some systems)." 

 
