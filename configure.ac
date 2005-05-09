# Process this file with autoconf to produce a configure script.

# $Id$
#
# Copyright (C) 2000-2005  The Xastir Group


#########################################################################
# SET VERSION HERE!
#
# AC_INIT():
# It should look something like this:  (xastir, 1.5.1, xastir@xastir.org)
# The revision number must contain at least one '.' and two digits.
# AM_INIT_AUTOMAKE():
# It should look something like this:  (xastir, 1.5.1)
# The revision number must contain at least one '.' and two digits.
#
AC_INIT(xastir, 1.5.1, xastir@xastir.org)
AM_INIT_AUTOMAKE(xastir, 1.5.1)
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
AC_PROG_RANLIB
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
use_festival=yes
use_gpsman=yes
use_err_popups=no
use_rtree=no
AC_ARG_WITH(festival,[  --without-festival       Disable festival features.],use_festival=$withval)

AC_ARG_WITH(gpsman,[  --without-gpsman       Disable gpsman features.],use_gpsman=$withval)

AC_ARG_WITH(errorpopups,[  --with-errorpopups       Send error popups to stderr.],use_err_popups=$withval)

LIBRTREE=""
SUBRTREE=""
AC_ARG_WITH(rtree,[  --with-rtree       Enable spatial indexing of shapefiles for faster rendering.],[use_rtree=$withval])
if test "${use_rtree}" = "yes" ; then
    AC_DEFINE([USE_RTREE],1,[RTree spatial indexing])
    LIBRTREE="-Lrtree -lrtree"
    SUBRTREE="rtree"
fi
AC_SUBST(SUBRTREE)
AC_SUBST(LIBRTREE)


# Now that all the various "use_" variables are set, probe for binaries.
XASTIR_DETECT_BINARIES

# if the probes failed, then make sure the "use_" variables are set to no,
# even if we requested them.
if test "x${festival}" = "xno"; then
  use_festival=no
fi
if test "x${gpsman}" = "xno"; then
  use_gpsman=no
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
  AC_MSG_ERROR(*** No X11!  Install X-Windows development headers/libraries! ***)
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
 


# Our old stuff.  Doesn't work in all cases.
# Checks for header files. 
#AC_CHECK_HEADERS([Xm/Xm.h],MOTIF_INC="yes", AC_MSG_ERROR(*** Cannot find Motif include files:  Please install one of Motif/OpenMotif/Lesstif development packages. ***))

XASTIR_PATH_MOTIF
if test "x$xm_includes" != "x" ; then
    CFLAGS="$CFLAGS -I$xm_includes"
    CPPFLAGS="$CPPFLAGS -I$xm_includes"
fi
if test "x$xm_libraries" != "x" ; then
    LDFLAGS="-L$xm_libraries $LDFLAGS"
fi

AC_SUBST(LDFLAGS) 
AC_SUBST(LIBS) 


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

#Some (Mac?) systems don't define this where it should be, so we kludge
AC_CHECK_TYPES(socklen_t,,,[#include <sys/types.h>
#include <sys/socket.h>])

#Force this into config.h.in
AH_BOTTOM([#ifndef HAVE_SOCKLEN_T
#define socklen_t int
#endif])


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
# this is a GNU extension and not present on all systems.
AC_CHECK_FUNCS([strndup])
 
# Check for intl library (Needed for Cygwin) 
# 
AC_CHECK_LIB(intl, gettext, LIBS="-lintl ${LIBS}") 

 
# Check for Shapelib
use_shapelib=yes
AC_ARG_WITH(shapelib,[  --without-shapelib       Disable shapelib features.],use_shapelib=$withval)
if test "${use_shapelib}" = "yes"; then
  use_shapelib=no
  AC_CHECK_HEADERS(shapefil.h libshp/shapefil.h, [AC_CHECK_LIB(shp, DBFOpen, use_shapelib=yes
  LIBS="$LIBS -lshp"
  AC_CHECK_LIB(shp,DBFGetFieldIndex,
    AC_DEFINE(HAVE_DBFGETFIELDINDEX, , Define to 1 if your `shp' library has DBFGetFieldIndex. ) )
  AC_DEFINE(HAVE_LIBSHP, , Define to 1 if you have the `shp' library (-lshp). )
  break)])
fi


# Check for pcre
use_pcre=yes
AC_ARG_WITH(pcre,[  --without-pcre       Disable pcre features.],use_pcre=$withval)
if test "${use_pcre}" = "yes"; then
  use_pcre=no
  AC_CHECK_HEADERS(pcre.h pcre/pcre.h, [AC_CHECK_LIB(pcre, pcre_compile, use_pcre=yes
  LIBS="$LIBS -lpcre"
  AC_DEFINE(HAVE_LIBPCRE, , Define to 1 if you have the `pcre' library (-lpcre). )
  break)])
fi

 
# Check for XPM
AC_CHECK_HEADERS(X11/xpm.h, AC_SEARCH_LIBS(XpmWriteFileFromPixmap, Xpm, AC_DEFINE(HAVE_LIBXPM, , Define to 1 if you have the `Xpm' library (-lXpm). )))
AC_CHECK_HEADERS(Xm/XpmI.h, AC_SEARCH_LIBS(XmeXpmWriteFileFromPixmap, Xm, AC_DEFINE(HAVE_LIBXPM_IN_XM, , Define to 1 if the `Xm' library (-lXm) is used for Xpm. )))
 
 
# Check for libproj
use_proj=yes
AC_ARG_WITH(libproj,[  --without-libproj       Disable libproj features.],use_proj=$withval)
if test "${use_proj}" = "yes"; then
  use_proj=no
  AC_CHECK_LIB(proj, pj_init, use_proj=yes LIBS="$LIBS -lproj"
    AC_DEFINE(HAVE_PROJ, , Define to 1 if you have the `libproj' library (-lproj). )
  )
fi


#check for libtiff, libgeotiff
if test "${use_proj}" = "yes" ; then
    use_geotiff=yes
    AC_ARG_WITH(geotiff, [  --without-geotiff       Disable geotiff features.],
    use_geotiff=$withval)
    if test "${use_geotiff}" = "yes"; then
        AC_CHECK_LIB(tiff, TIFFClose, use_tiff=yes
LIBS="$LIBS -ltiff"
        AC_DEFINE(HAVE_TIFF, ,Define to 1 if you have the 'tiff' library (-ltiff)))
	if test "${use_tiff}" = "yes"; then
           use_geotiff=no
           AC_CHECK_LIB(geotiff, GTIFNew, use_geotiff=yes
	       LIBS="$LIBS -lgeotiff"
               AC_DEFINE(HAVE_LIBGEOTIFF, ,Define to 1 if you have the `geotiff' library (-lgeotiff).))
        fi
    fi
fi 

# Check for AX.25 library 
use_ax25=yes
AC_ARG_WITH(ax25,[  --without-ax25       Disable ax25 features.],use_ax25=$withval)
if test "${use_ax25}" = "yes"; then
  use_ax25=no
  AC_CHECK_HEADERS(netax25/ax25.h, [AC_CHECK_LIB(ax25, ax25_config_load_ports, use_ax25=yes
  LIBS="$LIBS -lax25"
  AC_DEFINE(HAVE_LIBAX25, , Define to 1 if you have the `ax25' library (-lax25). )
  break)])
fi


# use dbfawk
use_dbfawk=yes
AC_ARG_WITH(dbfawk,[  --without-dbfawk       Disable dbfawk features.],use_dbfawk=$withval)
# dbfawk requires shapelib and pcre.  Make sure:
if test "${use_dbfawk}" = "yes"; then
  if test "${use_pcre}" = "yes" -a "${use_shapelib}" = "yes"; then
    AC_DEFINE(WITH_DBFAWK, , Define to 1 to use dbfawk features. )
  else
    echo "Warning: dbfawk requires both pcre and shapelib."
    use_dbfawk=no
  fi
fi


# Check for GDAL.  Make sure that the GDAL check is towards the
# last, as it can mess up shapelib and libgeotiff currently.
use_gdal=yes
AC_ARG_WITH(gdal,[  --without-gdal       Disable gdal features.],use_gdal=$withval)
if test "${use_gdal}" = "yes"; then
  use_gdal=no
  AC_CHECK_HEADERS(gdal.h, [AC_CHECK_LIB(gdal, GDALAllRegister, use_gdal=yes
  LIBS="$LIBS -lgdal"
  AC_DEFINE(HAVE_LIBGDAL, , Define to 1 if you have the `gdal' library (-lgdal). )
  break)])
fi


AC_ARG_ENABLE(davis, [  --enable-davis       Turn on Davis support],
  [case "${enableval}" in
    yes) davis=true ;;
     no) davis=false ;;
      *) AC_MSG_ERROR(bad value ${enableval} for --enable-davis) ;;
  esac],[davis=false])
AM_CONDITIONAL(DAVIS, test x$davis = xtrue)
if test "${davis}" = "true"; then
  davis=yes
else
  davis=no
fi





# Check for LIBGC, a Garbage Collection library.  If found, allow
# linking it with Xastir, but require the "--with-libgc" flag to
# enable it.
use_libgc=no
AC_ARG_WITH(libgc,[  --with-libgc       Enable libgc features.],use_libgc=$withval)
if test "${use_libgc}" = "yes"; then
    AC_CHECK_HEADERS([gc.h], LIBGC_INC="yes", use_libgc=no)
    AC_CHECK_LIB([gc], [GC_init], , use_libgc=no)
fi





# Check whether profiling was requested.  If so, add the "-pg" flag
# to CFLAGS.  Require "--with-profiling" flag to enable it.
use_profiling=no
AC_ARG_WITH(profiling,[  --with-profiling       Enable profiling features.],use_profiling=$withval)
if test "${use_profiling}" = "yes"; then
    CFLAGS="$CFLAGS -pg"
fi





# Check for Berkeley DB.
XASTIR_BERKELEY_DB_CHK





# Check for ImageMagick.  This check is VERY important to have as
# the last check, as it messes up the previous checks if it fails.
use_imagemagick=yes
AC_ARG_WITH(imagemagick,[  --without-imagemagick       Disable imagemagick features.],use_imagemagick=$withval)
if test "${use_imagemagick}" = "yes"; then
  XASTIR_CHECK_IMAGEMAGICK
fi





# Set XASTIR_DATA_BASE in CPPFLAGS due to Gnu coding standard that requires
# datadir expansion to be deferred until make time.
if test "x${datadir}" = "x"; then 
  CPPFLAGS="$CPPFLAGS -DXASTIR_DATA_BASE=\\\"${ac_default_prefix}/share/xastir\\\""
else 
  CPPFLAGS="$CPPFLAGS -DXASTIR_DATA_BASE=\\\"${datadir}/xastir\\\""
fi 
 

# Add a special option that makes Cygwin link much faster (from
# Henk de Groot)
save_LDFLAGS="$LDFLAGS"
LDFLAGS="-Wl,--no-keep-memory $LDFLAGS"
AC_TRY_LINK([#include <sys/types.h>],
  [/* Stupid useless test for linker flags */
   exit(0);],
  xa_cv_no_keep_memory=yes,
  xa_cv_no_keep_memory=no)
if test "${xa_cv_no_keep_memory}" = "no"; then 
  LDFLAGS="$save_LDFLAGS"
fi

# 
# Assure we have this order for these LIBS: "-lXm -lXt -lX11" by 
# adding them to the beginning.  Add spaces around them for the 
# following steps.  Here I'm adding them to the beginning so the   
# dupe-check portion will take out the later ones that might be in 
# the wrong order.  If they're in the wrong order, we can get 
# "Error: Shell widget xastir has zero width and/or height" when 
# Xastir is started up.   Can also get "Error: attempt to add
# non-widget child "DropSiteManager" to parent", but that can also
# be caused by other problems with libraries.
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
#LIBS=`echo "$LIBS" | awk '{for(i=1;i<=NF;++i) {if (arg[$i]++ == 0) s = s " " $i} print s}'`
changequote([,]) 


# 
# Remove "-g" CFLAGS/CPPFLAGS entries.
#
changequote(,) 
CFLAGS=`echo  "$CFLAGS" | awk '{for(i=1;i<=NF;++i) {if ($i != "-g") s = s " " $i} print s}'`
CPPFLAGS=`echo  "$CPPFLAGS" | awk '{for(i=1;i<=NF;++i) {if ($i != "-g") s = s " " $i} print s}'`
changequote([,]) 


#
# Change the compiler optimization if using Windows/Cygwin in order
# to reduce memory usage and speed up the compile.  Here we change
# "-O2" to "-O1" or " " in CFLAGS/CPPFLAGS to reduce the level of
# optimization.
#
case "$host_os" in
cygwin*)
  changequote(,) 
  CFLAGS=`echo  "$CFLAGS" | awk '{for(i=1;i<=NF;++i) {if ($i != "-O2") s = s " " $i} print s}'`
  CPPFLAGS=`echo  "$CPPFLAGS" | awk '{for(i=1;i<=NF;++i) {if ($i != "-O2") s = s " " $i} print s}'`
  changequote([,]) 
;;
esac
 

# Create the ILIBS defined variable
if test "${use_ax25}" = "yes"; then
  ILIBS="AX25"
fi
if test "${use_festival}" = "yes"; then
  ILIBS="$ILIBS Festival"
fi
if test "${use_gpsman}" = "yes"; then
  ILIBS="$ILIBS GPSMan"
fi
if test "${use_shapelib}" = "yes"; then
  ILIBS="$ILIBS ShapeLib"
fi
if test "${use_imagemagick}" = "yes"; then
  ILIBS="$ILIBS ImageMagick"
fi
if test "${use_proj}" = "yes"; then
  ILIBS="$ILIBS libProj"
fi
if test "${use_geotiff}" = "yes"; then
  ILIBS="$ILIBS GeoTiff"
fi
if test "${use_gdal}" = "yes"; then
  ILIBS="$ILIBS GDAL/OGR"
fi
if test "${use_pcre}" = "yes"; then
  ILIBS="$ILIBS PCRE"
fi
if test "${use_dbfawk}" = "yes"; then
  ILIBS="$ILIBS Dbfawk"
fi
if test "${use_libgc}" = "yes"; then
  ILIBS="$ILIBS libgc"
fi
if test "${use_profiling}" = "yes"; then
  ILIBS="$ILIBS profiling"
fi
if test "${use_err_popups}" = "yes"; then
  ILIBS="$ILIBS error_popups"
fi
AC_DEFINE_UNQUOTED(XASTIR_INSTALLED_LIBS, "`echo $ILIBS`", [Defines the installed libs])


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
    src/rtree/Makefile \
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


# Write out the library information to "summary.log"
echo "$ILIBS" >summary.log


echo ===========================================
echo AC_PACKAGE_NAME AC_PACKAGE_VERSION has been configured using the 
echo following external libraries.
echo  
echo 
echo Building with AX25 .............................. : $use_ax25 
echo Building with Festival .......................... : $use_festival 
echo Building with GPSMan ............................ : $use_gpsman
echo Building with ImageMagick ....................... : $use_imagemagick 
echo Building with libproj ........................... : $use_proj
echo Building with GeoTiff ........................... : $use_geotiff
echo Building with GDAL/OGR .......................... : $use_gdal
echo Building with ShapeLib .......................... : $use_shapelib 
echo Building with pcre .............................. : $use_pcre
echo Building with dbfawk ............................ : $use_dbfawk
echo Building with map caching ....................... : $use_map_cache
echo Building with ErrorPopups ....................... : $use_err_popups
echo "Building with libgc (Debug) ..................... : $use_libgc"
echo "Building with profiling (Debug) ................. : $use_profiling"
echo "Building with rtree indexing (experimental) ..... : $use_rtree"
#echo Building with Davis/MySQL.......... : $davis
echo 
echo AC_PACKAGE_NAME will be installed in $prefix/bin. 
if test "x$xastirpath" != "x" ; then 
        echo Warning: You have an old copy of Xastir at $xastirpath. 
fi 
echo "Type 'make' to build Xastir (Use 'gmake' instead on some systems)." 


